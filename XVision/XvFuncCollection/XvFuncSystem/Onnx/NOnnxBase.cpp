#include "NOnnxBase.h"

#include "XLanguage.h"
#include "XvXmlUtils.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

#include <cmath>
#include <limits>
#include <utility>

using namespace XvCore;

namespace
{
bool hasNonWhitespaceText(const QDomElement &element)
{
    for(QDomNode node=element.firstChild();!node.isNull();node=node.nextSibling())
        if((node.isText() || node.isCDATASection())
                && !node.nodeValue().trimmed().isEmpty()) return true;
    return false;
}

bool modelFile(const QString &path,QString &normalized,qint64 &length,
               QByteArray &digest,QString &error)
{
    const QFileInfo info(path);
    if(path.trimmed().isEmpty() || !info.isAbsolute() || !info.isFile()
            || !info.isReadable() || info.suffix().compare("onnx",Qt::CaseInsensitive)!=0)
    {
        error="ONNX model path must be an absolute readable .onnx file";
        return false;
    }
    normalized=info.canonicalFilePath();
    if(normalized.isEmpty()) normalized=info.absoluteFilePath();
    QFile file(normalized);
    if(!file.open(QIODevice::ReadOnly))
    {
        error=QString("cannot open ONNX model: %1").arg(file.errorString());
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    length=0;
    while(!file.atEnd())
    {
        const QByteArray block=file.read(1024*1024);
        if(block.isEmpty() && file.error()!=QFileDevice::NoError)
        {
            error=QString("cannot read ONNX model: %1").arg(file.errorString());
            return false;
        }
        if(length>std::numeric_limits<qint64>::max()-block.size())
        {
            error="ONNX model length overflows";
            return false;
        }
        length+=block.size();
        hash.addData(block);
    }
    if(length<=0)
    {
        error="ONNX model must not be empty";
        return false;
    }
    digest=hash.result().toHex();
    return true;
}

bool parseLength(const QString &text,qint64 &length)
{
    if(text.isEmpty() || (text.size()>1 && text.startsWith('0'))) return false;
    bool ok=false;
    const qulonglong value=text.toULongLong(&ok,10);
    if(!ok || value==0 || value>static_cast<qulonglong>(std::numeric_limits<qint64>::max()))
        return false;
    length=static_cast<qint64>(value);
    return true;
}

bool normalizationValues(const QString &text,bool requirePositive,
                         const QString &name,QString &error)
{
    if(text.size()>4096)
    {
        error=QString("ONNX %1 values are too long").arg(name);
        return false;
    }
    const QStringList parts=text.split(',',Qt::KeepEmptyParts);
    if(parts.size()!=1 && parts.size()!=3)
    {
        error=QString("ONNX %1 requires one or three values").arg(name);
        return false;
    }
    for(const QString &part:parts)
    {
        bool ok=false;
        const double value=part.trimmed().toDouble(&ok);
        if(part.trimmed().isEmpty() || !ok || !std::isfinite(value)
                || (requirePositive && value<=0.0))
        {
            error=QString("ONNX %1 contains an invalid value").arg(name);
            return false;
        }
    }
    return true;
}
}

class NOnnxBase::Private
{
public:
    mutable QMutex mutex;
    mutable OnnxSession session;
    qint64 modelLength=0;
    QByteArray modelDigest;
};

NOnnxBase::NOnnxBase(QObject *parent):XvFunc(parent),d(std::make_unique<Private>())
{
}

NOnnxBase::~NOnnxBase()=default;

void NOnnxBase::setModelPath(const QString &path)
{
    if(m_modelPath==path) return;
    QMutexLocker locker(&d->mutex);
    m_modelPath=path;
    d->modelLength=0;
    d->modelDigest.clear();
    d->session.clear();
}

QStringList NOnnxBase::commonPersistentPropertyNames() const
{
    return {"modelPath","inputLayout","inputWidth","inputHeight",
            "channelOrder","resizeMode","paddingValue","pixelScale",
            "meanValues","stdValues"};
}

bool NOnnxBase::validateCommonConfiguration(QString &error) const
{
    if(m_inputLayout<Auto || m_inputLayout>Nhwc
            || m_channelOrder<Rgb || m_channelOrder>Bgr
            || m_resizeMode<Stretch || m_resizeMode>Letterbox)
    {
        error="ONNX preprocessing enum value is invalid";
        return false;
    }
    if(m_inputWidth<0 || m_inputWidth>32768
            || m_inputHeight<0 || m_inputHeight>32768
            || m_paddingValue<0 || m_paddingValue>255
            || !std::isfinite(m_pixelScale) || std::abs(m_pixelScale)>1000000.0)
    {
        error="ONNX preprocessing numeric value is invalid";
        return false;
    }
    if(!normalizationValues(m_meanValues,false,"mean",error)
            || !normalizationValues(m_stdValues,true,"standard deviation",error))
        return false;
    error.clear();
    return true;
}

bool NOnnxBase::configureModel(const QString &path,QString &error)
{
    error.clear();
    QString normalized;
    qint64 length=0;
    QByteArray digest;
    if(!modelFile(path,normalized,length,digest,error)) return false;
    OnnxSession candidate;
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    if(!candidate.load(normalized,&error)) return false;
    QString loadedPath;
    qint64 loadedLength=0;
    QByteArray loadedDigest;
    if(!modelFile(normalized,loadedPath,loadedLength,loadedDigest,error)
            || loadedPath!=normalized || loadedLength!=length
            || loadedDigest!=digest)
    {
        if(error.isEmpty()) error="ONNX model changed while it was being configured";
        return false;
    }
#endif
    QMutexLocker locker(&d->mutex);
    m_modelPath=normalized;
    d->modelLength=length;
    d->modelDigest=digest;
    d->session=std::move(candidate);
    return true;
}

bool NOnnxBase::hasConfiguredModel() const
{
    QMutexLocker locker(&d->mutex);
    return !m_modelPath.isEmpty() && d->modelLength>0 && d->modelDigest.size()==64;
}

QString NOnnxBase::modelSummary() const
{
    QMutexLocker locker(&d->mutex);
    if(m_modelPath.isEmpty() || d->modelLength<=0 || d->modelDigest.size()!=64)
        return getLang("XvFuncSystem_NOnnx_ModelNotConfigured","尚未配置ONNX模型");
#if !defined(XVISION_ENABLE_ONNXRUNTIME)
    return getLang("XvFuncSystem_NOnnx_ModelConfiguredBackendDisabled",
                   "模型已配置，ONNX Runtime后端未启用");
#else
    const QList<XOnnxTensorInfo> inputs=d->session.inputs();
    const QList<XOnnxTensorInfo> outputs=d->session.outputs();
    return getLang("XvFuncSystem_NOnnx_ModelSummary","%1个输入，%2个输出")
            .arg(inputs.size()).arg(outputs.size());
#endif
}

XvOnnx::ImagePreprocessConfig NOnnxBase::imagePreprocessConfig() const
{
    XvOnnx::ImagePreprocessConfig config;
    config.layout=static_cast<XvOnnx::TensorLayout>(m_inputLayout);
    config.channelOrder=static_cast<XvOnnx::ChannelOrder>(m_channelOrder);
    config.resizeMode=static_cast<XvOnnx::ResizeMode>(m_resizeMode);
    config.inputWidth=m_inputWidth;
    config.inputHeight=m_inputHeight;
    config.paddingValue=m_paddingValue;
    config.pixelScale=m_pixelScale;
    config.meanValues=m_meanValues;
    config.stdValues=m_stdValues;
    return config;
}

QList<XOnnxTensorInfo> NOnnxBase::modelInputs(QString &error) const
{
    QMutexLocker locker(&d->mutex);
    if(m_modelPath.isEmpty() || d->modelDigest.size()!=64)
    {
        error="ONNX model is not configured";
        return {};
    }
#if !defined(XVISION_ENABLE_ONNXRUNTIME)
    error="ONNX Runtime backend is disabled; configure with XVISION_ENABLE_ONNXRUNTIME=ON";
    return {};
#else
    const QList<XOnnxTensorInfo> values=d->session.inputs();
    if(values.isEmpty()) error="ONNX model has no tensor inputs";
    else error.clear();
    return values;
#endif
}

QList<XOnnxTensorInfo> NOnnxBase::modelOutputs(QString &error) const
{
    QMutexLocker locker(&d->mutex);
    if(m_modelPath.isEmpty() || d->modelDigest.size()!=64)
    {
        error="ONNX model is not configured";
        return {};
    }
#if !defined(XVISION_ENABLE_ONNXRUNTIME)
    error="ONNX Runtime backend is disabled; configure with XVISION_ENABLE_ONNXRUNTIME=ON";
    return {};
#else
    const QList<XOnnxTensorInfo> values=d->session.outputs();
    if(values.isEmpty()) error="ONNX model has no tensor outputs";
    else error.clear();
    return values;
#endif
}

bool NOnnxBase::prepareImageInput(const QImage &image,XOnnxTensorData &tensor,
                                  XvOnnx::ImageTransform &transform,
                                  QString &error) const
{
    if(!validateCommonConfiguration(error)) return false;
    const QList<XOnnxTensorInfo> inputs=modelInputs(error);
    if(inputs.size()!=1)
    {
        if(error.isEmpty()) error="ONNX image operator requires exactly one model input";
        return false;
    }
    return XvOnnx::preprocessImage(image,inputs.first(),imagePreprocessConfig(),
                                   tensor,transform,error);
}

bool NOnnxBase::executeModel(const QList<XOnnxTensorData> &inputs,
                             QList<XOnnxTensorData> &outputs,
                             QString &error) const
{
    QMutexLocker locker(&d->mutex);
    if(m_modelPath.isEmpty() || d->modelLength<=0 || d->modelDigest.size()!=64)
    {
        error="ONNX model is not configured";
        return false;
    }
    QString normalized;
    qint64 length=0;
    QByteArray digest;
    if(!modelFile(m_modelPath,normalized,length,digest,error)
            || normalized!=m_modelPath || length!=d->modelLength
            || digest!=d->modelDigest)
    {
        if(error.isEmpty()) error="ONNX model file changed after configuration";
        return false;
    }
    return d->session.run(inputs,outputs,&error);
}

bool NOnnxBase::appendPersistentData(QDomDocument &doc,
                                     QDomElement &functionElement,
                                     QString &error) const
{
    if(!validateCommonConfiguration(error)) return false;
    QMutexLocker locker(&d->mutex);
    if(m_modelPath.isEmpty())
    {
        if(d->modelLength!=0 || !d->modelDigest.isEmpty())
        {
            error="unconfigured ONNX model has stale asset metadata";
            return false;
        }
        return true;
    }
    QString normalized;
    qint64 length=0;
    QByteArray digest;
    if(!modelFile(m_modelPath,normalized,length,digest,error)
            || normalized!=m_modelPath || length!=d->modelLength
            || digest!=d->modelDigest)
    {
        if(error.isEmpty()) error="ONNX model file changed after configuration";
        return false;
    }
    QDomElement persistent=doc.createElement("PersistentData");
    QDomElement model=doc.createElement("OnnxModel");
    model.setAttribute("format","onnx-runtime");
    model.setAttribute("version","1");
    model.setAttribute("length",QString::number(length));
    model.setAttribute("sha256",QString::fromLatin1(digest));
    persistent.appendChild(model);
    functionElement.appendChild(persistent);
    return true;
}

bool NOnnxBase::readPersistentData(const QDomElement &dataElement,QString &error)
{
    if(!validateCommonConfiguration(error)) return false;
    if(m_modelPath.isEmpty())
    {
        if(!dataElement.isNull())
        {
            error="unconfigured ONNX operator must not contain <PersistentData>";
            return false;
        }
        QMutexLocker locker(&d->mutex);
        d->modelLength=0;
        d->modelDigest.clear();
        d->session.clear();
        return true;
    }
    if(dataElement.isNull()
            || !XvXml::validateAttributes(dataElement,{}, {},error)
            || !XvXml::validateChildren(dataElement,{"OnnxModel"},{"OnnxModel"},error)
            || hasNonWhitespaceText(dataElement))
    {
        if(error.isEmpty()) error="configured ONNX operator requires valid model metadata";
        return false;
    }
    const QDomElement model=XvXml::singleChild(dataElement,"OnnxModel");
    if(!XvXml::validateAttributes(model,{"format","version","length","sha256"},
                                  {"format","version","length","sha256"},error)
            || !XvXml::validateChildren(model,{}, {},error)
            || hasNonWhitespaceText(model)
            || model.attribute("format")!="onnx-runtime"
            || model.attribute("version")!="1")
    {
        if(error.isEmpty()) error="ONNX model metadata format or version is invalid";
        return false;
    }
    qint64 expectedLength=0;
    const QString digestText=model.attribute("sha256");
    static const QRegularExpression digestPattern("^[0-9a-f]{64}$");
    if(!parseLength(model.attribute("length"),expectedLength)
            || !digestPattern.match(digestText).hasMatch())
    {
        error="ONNX model length or SHA-256 metadata is invalid";
        return false;
    }
    QString normalized;
    qint64 length=0;
    QByteArray digest;
    if(!modelFile(m_modelPath,normalized,length,digest,error)
            || normalized!=m_modelPath || length!=expectedLength
            || digest!=digestText.toLatin1())
    {
        if(error.isEmpty()) error="ONNX model metadata does not match the file";
        return false;
    }
    OnnxSession candidate;
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    if(!candidate.load(normalized,&error)) return false;
    QString loadedPath;
    qint64 loadedLength=0;
    QByteArray loadedDigest;
    if(!modelFile(normalized,loadedPath,loadedLength,loadedDigest,error)
            || loadedPath!=normalized || loadedLength!=length
            || loadedDigest!=digest)
    {
        if(error.isEmpty()) error="ONNX model changed while it was being loaded";
        return false;
    }
#endif
    QMutexLocker locker(&d->mutex);
    m_modelPath=normalized;
    d->modelLength=length;
    d->modelDigest=digest;
    d->session=std::move(candidate);
    return true;
}
