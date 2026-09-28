#include "HSemanticSegmentation.h"
#include "HSemanticSegmentationWdg.h"

#include "HalconDef.h"
#include "HalconImageInterop.h"
#include "XvXmlUtils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <exception>
#include <limits>
#include <memory>

using namespace XvCore;

namespace
{
constexpr int HashChunkSize=1024*1024;

QString halconError(const HException &exception)
{
    const QString message=QString::fromLocal8Bit(exception.ErrorMessage().TextA());
    return message.isEmpty()
            ?QString("Halcon error %1").arg(exception.ErrorCode())
            :message;
}

HString halconPath(const QString &path)
{
#ifdef Q_OS_WIN
    const std::wstring value=QDir::toNativeSeparators(path).toStdWString();
    return HString(value.c_str());
#else
    const QByteArray value=QFile::encodeName(path);
    return HString::FromLocal8bit(value.constData());
#endif
}

QString fromHalconString(const HString &value)
{
    return QString::fromUtf8(value.ToUtf8());
}

bool hasNonWhitespaceText(const QDomElement &element)
{
    for(QDomNode node=element.firstChild();!node.isNull();node=node.nextSibling())
    {
        if((node.isText() || node.isCDATASection())
                && !node.nodeValue().trimmed().isEmpty())
        {
            return true;
        }
    }
    return false;
}

bool normalizedFile(const QString &path,const QString &suffix,
                    QString &normalized,qint64 &length,QByteArray &digest,
                    QString &error)
{
    QFileInfo info(path);
    if(path.trimmed().isEmpty() || !info.exists() || !info.isFile()
            || !info.isReadable())
    {
        error=QString("asset file is missing or unreadable: %1").arg(path);
        return false;
    }
    if(info.suffix().compare(suffix,Qt::CaseInsensitive)!=0)
    {
        error=QString("asset file must use .%1: %2").arg(suffix,path);
        return false;
    }
    normalized=info.canonicalFilePath();
    if(normalized.isEmpty()) normalized=info.absoluteFilePath();
    length=info.size();
    if(length<=0)
    {
        error=QString("asset file is empty: %1").arg(normalized);
        return false;
    }

    QFile file(normalized);
    if(!file.open(QIODevice::ReadOnly))
    {
        error=QString("cannot read asset '%1': %2")
                .arg(normalized,file.errorString());
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd())
    {
        const QByteArray chunk=file.read(HashChunkSize);
        if(chunk.isEmpty() && file.error()!=QFile::NoError)
        {
            error=QString("cannot hash asset '%1': %2")
                    .arg(normalized,file.errorString());
            return false;
        }
        hash.addData(chunk);
    }
    digest=hash.result().toHex();
    return digest.size()==64;
}

bool dictHasKey(const HDict &dict,const char *key)
{
    const HTuple exists=dict.GetDictParam("key_exists",key);
    return exists.Length()==1 && exists.L()!=0;
}

bool dictString(const HDict &dict,const char *key,QString &value,QString &error)
{
    if(!dictHasKey(dict,key))
    {
        error=QString("preprocess dictionary is missing '%1'").arg(key);
        return false;
    }
    const HTuple tuple=dict.GetDictTuple(key);
    if(tuple.Length()!=1)
    {
        error=QString("preprocess key '%1' must contain one string").arg(key);
        return false;
    }
    value=fromHalconString(tuple.S());
    if(value.isEmpty())
    {
        error=QString("preprocess key '%1' is empty").arg(key);
        return false;
    }
    return true;
}

bool dictInteger(const HDict &dict,const char *key,int &value,QString &error)
{
    if(!dictHasKey(dict,key))
    {
        error=QString("preprocess dictionary is missing '%1'").arg(key);
        return false;
    }
    const HTuple tuple=dict.GetDictTuple(key);
    if(tuple.Length()!=1)
    {
        error=QString("preprocess key '%1' must contain one integer").arg(key);
        return false;
    }
    const Hlong parsed=tuple.L();
    if(parsed<std::numeric_limits<int>::min()
            || parsed>std::numeric_limits<int>::max())
    {
        error=QString("preprocess key '%1' is outside int range").arg(key);
        return false;
    }
    value=int(parsed);
    return true;
}

bool dictReal(const HDict &dict,const char *key,double &value,QString &error)
{
    if(!dictHasKey(dict,key))
    {
        error=QString("preprocess dictionary is missing '%1'").arg(key);
        return false;
    }
    const HTuple tuple=dict.GetDictTuple(key);
    if(tuple.Length()!=1)
    {
        error=QString("preprocess key '%1' must contain one number").arg(key);
        return false;
    }
    value=tuple.D();
    if(!qIsFinite(value))
    {
        error=QString("preprocess key '%1' must be finite").arg(key);
        return false;
    }
    return true;
}

QVector<QRgb> classPalette(const QVector<qint32> &classIds,
                           const QStringList &classNames)
{
    QVector<QRgb> colors;
    colors.reserve(classIds.size());
    for(qsizetype index=0;index<classIds.size();++index)
    {
        if(classNames.at(index).compare("background",Qt::CaseInsensitive)==0)
        {
            colors.append(qRgba(0,0,0,0));
            continue;
        }
        const quint32 seed=quint32(classIds.at(index))*2654435761u;
        const int hue=int(seed%360u);
        const int saturation=155+int((seed>>9)%81u);
        const int value=185+int((seed>>17)%56u);
        QColor color=QColor::fromHsv(hue,saturation,value);
        colors.append(qRgba(color.red(),color.green(),color.blue(),255));
    }
    return colors;
}

struct PreprocessConfig
{
    int width=0;
    int height=0;
    int channels=0;
    double rangeMin=0.0;
    double rangeMax=0.0;
};

struct ModelState
{
    HDlModel model;
    PreprocessConfig preprocess;
    QVector<qint32> classIds;
    QStringList classNames;
    QVector<QRgb> classColors;
};

bool modelInteger(const HDlModel &model,const char *key,int &value,QString &error)
{
    const HTuple tuple=model.GetDlModelParam(key);
    if(tuple.Length()!=1)
    {
        error=QString("model parameter '%1' must contain one integer").arg(key);
        return false;
    }
    const Hlong parsed=tuple.L();
    if(parsed<std::numeric_limits<int>::min()
            || parsed>std::numeric_limits<int>::max())
    {
        error=QString("model parameter '%1' is outside int range").arg(key);
        return false;
    }
    value=int(parsed);
    return true;
}

bool modelReal(const HDlModel &model,const char *key,double &value,QString &error)
{
    const HTuple tuple=model.GetDlModelParam(key);
    if(tuple.Length()!=1)
    {
        error=QString("model parameter '%1' must contain one number").arg(key);
        return false;
    }
    value=tuple.D();
    if(!qIsFinite(value))
    {
        error=QString("model parameter '%1' must be finite").arg(key);
        return false;
    }
    return true;
}

bool parseAssets(const QString &modelPath,const QString &preprocessPath,
                 HSemanticSegmentation::Runtime runtime,
                 std::shared_ptr<ModelState> &state,QString &normalizedModel,
                 QString &normalizedPreprocess,qint64 &modelLength,
                 qint64 &preprocessLength,QByteArray &modelDigest,
                 QByteArray &preprocessDigest,QString &error)
{
    if(runtime!=HSemanticSegmentation::Cpu
            && runtime!=HSemanticSegmentation::Gpu)
    {
        error="semantic segmentation runtime is invalid";
        return false;
    }
    if(!normalizedFile(modelPath,"hdl",normalizedModel,modelLength,
                       modelDigest,error)
            || !normalizedFile(preprocessPath,"hdict",normalizedPreprocess,
                               preprocessLength,preprocessDigest,error))
    {
        return false;
    }

    try
    {
        auto candidate=std::make_shared<ModelState>();
        candidate->model=HDlModel(halconPath(normalizedModel));
        const QString modelType=fromHalconString(
                    candidate->model.GetDlModelParam("type").S());
        if(modelType!="segmentation")
        {
            error=QString("model type must be 'segmentation', got '%1'")
                    .arg(modelType);
            return false;
        }

        HDict preprocess(halconPath(normalizedPreprocess),HTuple(),HTuple());
        QString preprocessModelType;
        QString normalizationType;
        QString domainHandling;
        if(!dictString(preprocess,"model_type",preprocessModelType,error)
                || !dictString(preprocess,"normalization_type",
                               normalizationType,error)
                || !dictString(preprocess,"domain_handling",domainHandling,error)
                || !dictInteger(preprocess,"image_width",
                                candidate->preprocess.width,error)
                || !dictInteger(preprocess,"image_height",
                                candidate->preprocess.height,error)
                || !dictInteger(preprocess,"image_num_channels",
                                candidate->preprocess.channels,error)
                || !dictReal(preprocess,"image_range_min",
                             candidate->preprocess.rangeMin,error)
                || !dictReal(preprocess,"image_range_max",
                             candidate->preprocess.rangeMax,error))
        {
            return false;
        }
        if(preprocessModelType!="segmentation")
        {
            error=QString("preprocess model_type must be 'segmentation', got '%1'")
                    .arg(preprocessModelType);
            return false;
        }
        if(normalizationType!="none")
        {
            error=QString("unsupported normalization_type '%1'; "
                          "the current implementation supports only 'none'")
                    .arg(normalizationType);
            return false;
        }
        if(domainHandling!="full_domain")
        {
            error=QString("unsupported domain_handling '%1'; QImage input "
                          "requires 'full_domain'").arg(domainHandling);
            return false;
        }
        if(candidate->preprocess.width<=0 || candidate->preprocess.height<=0
                || (candidate->preprocess.channels!=1
                    && candidate->preprocess.channels!=3)
                || candidate->preprocess.rangeMin>=candidate->preprocess.rangeMax)
        {
            error="preprocess image dimensions, channels, or range are invalid";
            return false;
        }

        int modelWidth=0;
        int modelHeight=0;
        int modelChannels=0;
        double modelRangeMin=0.0;
        double modelRangeMax=0.0;
        if(!modelInteger(candidate->model,"image_width",modelWidth,error)
                || !modelInteger(candidate->model,"image_height",modelHeight,error)
                || !modelInteger(candidate->model,"image_num_channels",
                                 modelChannels,error)
                || !modelReal(candidate->model,"image_range_min",modelRangeMin,error)
                || !modelReal(candidate->model,"image_range_max",modelRangeMax,error))
        {
            return false;
        }
        if(modelWidth!=candidate->preprocess.width
                || modelHeight!=candidate->preprocess.height
                || modelChannels!=candidate->preprocess.channels
                || qAbs(modelRangeMin-candidate->preprocess.rangeMin)>1e-12
                || qAbs(modelRangeMax-candidate->preprocess.rangeMax)>1e-12)
        {
            error="model and preprocess image specifications do not match";
            return false;
        }

        const HTuple ids=candidate->model.GetDlModelParam("class_ids");
        HTuple names;
        try
        {
            names=candidate->model.GetDlModelParam("class_names");
        }
        catch(const HException &)
        {
            names=HTuple();
        }
        if(ids.Length()<=0 || (names.Length()!=0 && names.Length()!=ids.Length()))
        {
            error="model class_ids must be non-empty and class_names must align";
            return false;
        }
        QSet<qint32> uniqueIds;
        for(Hlong index=0;index<ids.Length();++index)
        {
            const Hlong id=ids[index].L();
            if(id<std::numeric_limits<qint32>::min()
                    || id>std::numeric_limits<qint32>::max()
                    || uniqueIds.contains(qint32(id)))
            {
                error="model class_ids contain a duplicate or out-of-range value";
                return false;
            }
            const QString name=names.Length()==0
                    ?QString("Class %1").arg(id)
                    :fromHalconString(names[index].S()).trimmed();
            if(name.isEmpty())
            {
                error="model class_names contain an empty value";
                return false;
            }
            uniqueIds.insert(qint32(id));
            candidate->classIds.append(qint32(id));
            candidate->classNames.append(name);
        }
        candidate->classColors=classPalette(candidate->classIds,
                                            candidate->classNames);
        candidate->model.SetDlModelParam(
                    HString("runtime"),
                    HTuple(runtime==HSemanticSegmentation::Cpu?"cpu":"gpu"));
        candidate->model.SetDlModelParam("batch_size",1.0);
        state=candidate;
        return true;
    }
    catch(const HException &exception)
    {
        error=halconError(exception);
    }
    catch(const std::exception &exception)
    {
        error=QString::fromLocal8Bit(exception.what());
    }
    catch(...)
    {
        error="unknown error while loading semantic segmentation assets";
    }
    return false;
}

bool preprocessImage(const QImage &input,const PreprocessConfig &config,
                     HImage &output,QString &error)
{
    HImage image;
    if(!XvHalconImageInterop::toHalcon(input,image,&error)) return false;
    const Hlong channels=image.CountChannels().L();
    if(config.channels==1 && channels==3)
    {
        image=image.Rgb1ToGray();
    }
    else if(config.channels==3 && channels==1)
    {
        image=image.Compose3(image,image);
    }
    else if(channels!=config.channels)
    {
        error=QString("input channel count %1 cannot be converted to %2")
                .arg(channels).arg(config.channels);
        return false;
    }
    image=image.FullDomain().ZoomImageSize(config.width,config.height,"bilinear")
            .ConvertImageType("real");
    const double scale=(config.rangeMax-config.rangeMin)/255.0;
    output=image.ScaleImage(scale,config.rangeMin);
    return true;
}

bool buildDisplayImages(const QImage &input,const XSegmentationResult &segmentation,
                        double opacity,QImage &colorMask,QImage &overlay,
                        QImage &confidence,QString &error)
{
    if(!qIsFinite(opacity) || opacity<0.0 || opacity>1.0
            || segmentation.isEmpty()
            || input.width()!=segmentation.width()
            || input.height()!=segmentation.height())
    {
        error="cannot render invalid segmentation result";
        return false;
    }
    const QImage rgb=input.convertToFormat(QImage::Format_RGB888);
    if(rgb.isNull())
    {
        error="cannot convert input image to RGB888 for overlay";
        return false;
    }
    QImage candidateMask(rgb.size(),QImage::Format_RGB888);
    QImage candidateOverlay(rgb.size(),QImage::Format_RGB888);
    QImage candidateConfidence(rgb.size(),QImage::Format_Grayscale8);
    if(candidateMask.isNull() || candidateOverlay.isNull()
            || candidateConfidence.isNull())
    {
        error="cannot allocate semantic segmentation display images";
        return false;
    }

    QHash<qint32,int> classIndexes;
    for(qsizetype index=0;index<segmentation.classIds().size();++index)
        classIndexes.insert(segmentation.classIds().at(index),int(index));

    for(int row=0;row<rgb.height();++row)
    {
        const uchar *sourceLine=rgb.constScanLine(row);
        uchar *maskLine=candidateMask.scanLine(row);
        uchar *overlayLine=candidateOverlay.scanLine(row);
        uchar *confidenceLine=candidateConfidence.scanLine(row);
        for(int column=0;column<rgb.width();++column)
        {
            const int pixelIndex=row*rgb.width()+column;
            const int colorIndex=classIndexes.value(
                        segmentation.labels().at(pixelIndex),-1);
            if(colorIndex<0)
            {
                error="segmentation contains an unknown class ID";
                return false;
            }
            const QRgb classColor=segmentation.classColors().at(colorIndex);
            const int byteIndex=column*3;
            maskLine[byteIndex]=uchar(qRed(classColor));
            maskLine[byteIndex+1]=uchar(qGreen(classColor));
            maskLine[byteIndex+2]=uchar(qBlue(classColor));
            const bool background=qAlpha(classColor)==0;
            for(int channel=0;channel<3;++channel)
            {
                const int sourceValue=sourceLine[byteIndex+channel];
                const int maskValue=maskLine[byteIndex+channel];
                overlayLine[byteIndex+channel]=background
                        ?uchar(sourceValue)
                        :uchar(qRound(sourceValue*(1.0-opacity)
                                      +maskValue*opacity));
            }
            const float confidenceValue=segmentation.confidences().at(pixelIndex);
            confidenceLine[column]=uchar(qRound(confidenceValue*255.0f));
        }
    }
    colorMask=candidateMask;
    overlay=candidateOverlay;
    confidence=candidateConfidence;
    return true;
}

bool parseLength(const QDomElement &element,qint64 &length,QString &error)
{
    bool ok=false;
    const QString text=element.attribute("length");
    const qint64 parsed=text.toLongLong(&ok,10);
    if(!ok || parsed<=0 || QString::number(parsed)!=text)
    {
        error=QString("<%1> length must be a canonical positive integer")
                .arg(element.tagName());
        return false;
    }
    length=parsed;
    return true;
}

bool parseDigest(const QDomElement &element,QByteArray &digest,QString &error)
{
    const QString text=element.attribute("sha256");
    static const QRegularExpression expression("^[0-9a-f]{64}$");
    if(!expression.match(text).hasMatch())
    {
        error=QString("<%1> sha256 must be 64 lowercase hexadecimal characters")
                .arg(element.tagName());
        return false;
    }
    digest=text.toLatin1();
    return true;
}
}

class XvCore::HSemanticSegmentationPrivate
{
public:
    mutable QMutex mutex;
    std::shared_ptr<ModelState> state;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
};

HSemanticSegmentation::HSemanticSegmentation(QObject *parent)
    :XvFunc(parent),d(new HSemanticSegmentationPrivate)
{
    _funcRole="HSemanticSegmentation";
    _funcName=getLang("XvFuncSystem_HSemanticSegmentation_Name","语义分割(H)");
    _funcType=EXvFuncType::ImageProcessing;
    param=new HSemanticSegmentationParam();
    result=new HSemanticSegmentationResult();
}

HSemanticSegmentation::~HSemanticSegmentation()
{
    delete m_frm;
    m_frm=nullptr;
    delete param;
    param=nullptr;
    delete result;
    result=nullptr;
}

void HSemanticSegmentation::setModelPath(const QString &path)
{
    m_modelPath=path;
    QMutexLocker locker(&d->mutex);
    d->state.reset();
    d->modelDigest.clear();
    d->modelLength=0;
}

void HSemanticSegmentation::setPreprocessPath(const QString &path)
{
    m_preprocessPath=path;
    QMutexLocker locker(&d->mutex);
    d->state.reset();
    d->preprocessDigest.clear();
    d->preprocessLength=0;
}

void HSemanticSegmentation::setRuntime(Runtime runtime)
{
    if(m_runtime==runtime) return;
    m_runtime=runtime;
    QMutexLocker locker(&d->mutex);
    d->state.reset();
}

bool HSemanticSegmentation::configureAssets(const QString &modelPath,
                                            const QString &preprocessPath,
                                            QString &error)
{
    error.clear();
    std::shared_ptr<ModelState> state;
    QString normalizedModel;
    QString normalizedPreprocess;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    if(!parseAssets(modelPath,preprocessPath,m_runtime,state,normalizedModel,
                    normalizedPreprocess,modelLength,preprocessLength,
                    modelDigest,preprocessDigest,error))
    {
        return false;
    }
    QMutexLocker locker(&d->mutex);
    m_modelPath=normalizedModel;
    m_preprocessPath=normalizedPreprocess;
    d->modelLength=modelLength;
    d->preprocessLength=preprocessLength;
    d->modelDigest=modelDigest;
    d->preprocessDigest=preprocessDigest;
    d->state=state;
    return true;
}

bool HSemanticSegmentation::hasConfiguredAssets() const
{
    QMutexLocker locker(&d->mutex);
    return !m_modelPath.isEmpty() && !m_preprocessPath.isEmpty()
            && d->modelLength>0 && d->preprocessLength>0
            && d->modelDigest.size()==64 && d->preprocessDigest.size()==64;
}

QString HSemanticSegmentation::assetSummary() const
{
    QMutexLocker locker(&d->mutex);
    if(!d->state)
    {
        const bool configured=!m_modelPath.isEmpty() && !m_preprocessPath.isEmpty()
                && d->modelLength>0 && d->preprocessLength>0
                && d->modelDigest.size()==64 && d->preprocessDigest.size()==64;
        return configured
                ?getLang("XvFuncSystem_HSemanticSegmentation_LoadOnRun",
                         "已配置，将在运行时加载")
                :QString();
    }
    return getLang("XvFuncSystem_HSemanticSegmentation_AssetSummary",
                   "%1x%2，%3通道，%4类别")
            .arg(d->state->preprocess.width)
            .arg(d->state->preprocess.height)
            .arg(d->state->preprocess.channels)
            .arg(d->state->classIds.size());
}

QStringList HSemanticSegmentation::classSummary() const
{
    QMutexLocker locker(&d->mutex);
    if(!d->state) return {};
    QStringList summary;
    for(qsizetype index=0;index<d->state->classIds.size();++index)
    {
        summary.append(QString("%1: %2")
                       .arg(d->state->classIds.at(index))
                       .arg(d->state->classNames.at(index)));
    }
    return summary;
}

QPixmap HSemanticSegmentation::funcIcon()
{
    return QPixmap(":/images/HModelMatch.svg");
}

void HSemanticSegmentation::onShowFunc()
{
    if(!m_frm) m_frm=new HSemanticSegmentationWdg(this);
    m_frm->show();
    m_frm->raise();
}

void HSemanticSegmentation::clearResults()
{
    if(!result) return;
    result->segmentation->clear();
    result->colorMask->setValue(QImage());
    result->overlayImage->setValue(QImage());
    result->confidenceImage->setValue(QImage());
}

EXvFuncRunStatus HSemanticSegmentation::run()
{
    clearResults();
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_InvalidRuntime",
                          "语义分割运行设备无效"));
        return EXvFuncRunStatus::Error;
    }
    if(!qIsFinite(m_overlayOpacity)
            || m_overlayOpacity<0.0 || m_overlayOpacity>1.0)
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_InvalidOpacity",
                          "语义分割叠加透明度必须位于0到1之间"));
        return EXvFuncRunStatus::Error;
    }
    const QImage input=param->inputImage->value();
    if(input.isNull())
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_EmptyImage",
                          "语义分割输入图像为空"));
        return EXvFuncRunStatus::Error;
    }

    try
    {
        std::shared_ptr<ModelState> state;
        QByteArray expectedModelDigest;
        QByteArray expectedPreprocessDigest;
        qint64 expectedModelLength=0;
        qint64 expectedPreprocessLength=0;
        {
            QMutexLocker locker(&d->mutex);
            state=d->state;
            expectedModelDigest=d->modelDigest;
            expectedPreprocessDigest=d->preprocessDigest;
            expectedModelLength=d->modelLength;
            expectedPreprocessLength=d->preprocessLength;
        }
        if(expectedModelDigest.isEmpty() || expectedPreprocessDigest.isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_AssetsMissing",
                              "尚未配置有效的语义分割模型和预处理文件"));
            return EXvFuncRunStatus::Error;
        }

        QString normalizedModel;
        QString normalizedPreprocess;
        qint64 modelLength=0;
        qint64 preprocessLength=0;
        QByteArray modelDigest;
        QByteArray preprocessDigest;
        QString error;
        if(!normalizedFile(m_modelPath,"hdl",normalizedModel,modelLength,
                           modelDigest,error)
                || !normalizedFile(m_preprocessPath,"hdict",normalizedPreprocess,
                                   preprocessLength,preprocessDigest,error)
                || modelLength!=expectedModelLength
                || preprocessLength!=expectedPreprocessLength
                || modelDigest!=expectedModelDigest
                || preprocessDigest!=expectedPreprocessDigest)
        {
            if(error.isEmpty()) error="asset digest or length does not match";
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_AssetsChanged",
                              "语义分割资产缺失或已被替换")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }
        if(!state)
        {
            if(!parseAssets(m_modelPath,m_preprocessPath,m_runtime,state,
                            normalizedModel,normalizedPreprocess,modelLength,
                            preprocessLength,modelDigest,preprocessDigest,error)
                    || modelDigest!=expectedModelDigest
                    || preprocessDigest!=expectedPreprocessDigest)
            {
                setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_LoadFailed",
                                  "语义分割资产加载失败")
                          +QString(": %1").arg(error));
                return EXvFuncRunStatus::Error;
            }
            QMutexLocker locker(&d->mutex);
            d->state=state;
        }

        HImage preprocessed;
        if(!preprocessImage(input,state->preprocess,preprocessed,error))
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_PreprocessFailed",
                              "语义分割图像预处理失败")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }
        HDict sample;
        sample.SetDictObject(preprocessed,"image");
        HDictArray samples(&sample,1);
        HTuple requestedOutputs("segmentation_image");
        requestedOutputs.Append("segmentation_confidence");
        const HDictArray results=state->model.ApplyDlModel(samples,
                                                           requestedOutputs);
        if(results.Length()!=1)
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_InvalidResult",
                              "HALCON返回了无效的语义分割结果"));
            return EXvFuncRunStatus::Error;
        }
        const HDict &dlResult=results.Tools()[0];
        HImage labelImage(dlResult.GetDictObject("segmentation_image"));
        HImage confidenceImage(dlResult.GetDictObject("segmentation_confidence"));
        labelImage=labelImage.ZoomImageSize(input.width(),input.height(),
                                            "nearest_neighbor");
        confidenceImage=confidenceImage.ZoomImageSize(input.width(),input.height(),
                                                       "bilinear");

        QVector<qint32> labels;
        QVector<float> confidences;
        int labelWidth=0;
        int labelHeight=0;
        int confidenceWidth=0;
        int confidenceHeight=0;
        if(!XvHalconImageInterop::toInt32Pixels(
                    labelImage,labels,labelWidth,labelHeight,&error)
                || !XvHalconImageInterop::toFloatPixels(
                    confidenceImage,confidences,confidenceWidth,
                    confidenceHeight,&error)
                || labelWidth!=input.width() || labelHeight!=input.height()
                || confidenceWidth!=input.width()
                || confidenceHeight!=input.height())
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_InvalidResult",
                              "HALCON返回了无效的语义分割结果")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }

        XSegmentationResult candidate("segmentation");
        if(!candidate.setValue(input.width(),input.height(),labels,confidences,
                               state->classIds,state->classNames,
                               state->classColors))
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_InvalidResult",
                              "HALCON返回了无效的语义分割结果"));
            return EXvFuncRunStatus::Error;
        }
        QImage colorMask;
        QImage overlay;
        QImage confidence;
        if(!buildDisplayImages(input,candidate,m_overlayOpacity,colorMask,
                               overlay,confidence,error)
                || !result->segmentation->setData(&candidate))
        {
            setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_ResultUpdateFailed",
                              "语义分割结果更新失败")
                      +QString(": %1").arg(error));
            clearResults();
            return EXvFuncRunStatus::Error;
        }
        result->colorMask->setValue(colorMask);
        result->overlayImage->setValue(overlay);
        result->confidenceImage->setValue(confidence);
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_RunOk",
                          "语义分割完成"));
        return EXvFuncRunStatus::Ok;
    }
    catch(const HException &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_HalconFailed",
                          "HALCON语义分割失败")
                  +QString(": %1").arg(halconError(exception)));
    }
    catch(const std::exception &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_RunFailed",
                          "语义分割失败")
                  +QString(": %1").arg(QString::fromLocal8Bit(exception.what())));
    }
    catch(...)
    {
        setRunMsg(getLang("XvFuncSystem_HSemanticSegmentation_RunFailed",
                          "语义分割失败"));
    }
    clearResults();
    return EXvFuncRunStatus::Error;
}

bool HSemanticSegmentation::appendPersistentData(
        QDomDocument &doc,QDomElement &functionElement,QString &error) const
{
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        error="semantic segmentation runtime is invalid";
        return false;
    }
    if(!qIsFinite(m_overlayOpacity)
            || m_overlayOpacity<0.0 || m_overlayOpacity>1.0)
    {
        error="semantic segmentation overlay opacity must be in [0,1]";
        return false;
    }
    QByteArray expectedModelDigest;
    QByteArray expectedPreprocessDigest;
    qint64 expectedModelLength=0;
    qint64 expectedPreprocessLength=0;
    {
        QMutexLocker locker(&d->mutex);
        expectedModelDigest=d->modelDigest;
        expectedPreprocessDigest=d->preprocessDigest;
        expectedModelLength=d->modelLength;
        expectedPreprocessLength=d->preprocessLength;
    }
    const bool pathsEmpty=m_modelPath.isEmpty() && m_preprocessPath.isEmpty();
    const bool metadataEmpty=expectedModelDigest.isEmpty()
            && expectedPreprocessDigest.isEmpty()
            && expectedModelLength==0 && expectedPreprocessLength==0;
    if(pathsEmpty && metadataEmpty) return true;
    if(m_modelPath.isEmpty() || m_preprocessPath.isEmpty()
            || expectedModelDigest.size()!=64
            || expectedPreprocessDigest.size()!=64
            || expectedModelLength<=0 || expectedPreprocessLength<=0)
    {
        error="semantic segmentation asset configuration is incomplete";
        return false;
    }

    QString normalized;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    if(!normalizedFile(m_modelPath,"hdl",normalized,modelLength,modelDigest,error)
            || !normalizedFile(m_preprocessPath,"hdict",normalized,
                               preprocessLength,preprocessDigest,error)
            || modelLength!=expectedModelLength
            || preprocessLength!=expectedPreprocessLength
            || modelDigest!=expectedModelDigest
            || preprocessDigest!=expectedPreprocessDigest)
    {
        if(error.isEmpty()) error="semantic segmentation assets changed since configuration";
        return false;
    }

    QDomElement dataElement=doc.createElement("PersistentData");
    QDomElement assetsElement=doc.createElement("SemanticSegmentationAssets");
    assetsElement.setAttribute("format","halcon-dl-segmentation");
    assetsElement.setAttribute("version","1");
    QDomElement modelElement=doc.createElement("Model");
    modelElement.setAttribute("length",QString::number(modelLength));
    modelElement.setAttribute("sha256",QString::fromLatin1(modelDigest));
    QDomElement preprocessElement=doc.createElement("Preprocess");
    preprocessElement.setAttribute("length",QString::number(preprocessLength));
    preprocessElement.setAttribute("sha256",QString::fromLatin1(preprocessDigest));
    assetsElement.appendChild(modelElement);
    assetsElement.appendChild(preprocessElement);
    dataElement.appendChild(assetsElement);
    functionElement.appendChild(dataElement);
    return true;
}

bool HSemanticSegmentation::readPersistentData(const QDomElement &dataElement,
                                               QString &error)
{
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        error="semantic segmentation runtime is invalid";
        return false;
    }
    if(!qIsFinite(m_overlayOpacity)
            || m_overlayOpacity<0.0 || m_overlayOpacity>1.0)
    {
        error="semantic segmentation overlay opacity must be in [0,1]";
        return false;
    }
    if(dataElement.isNull())
    {
        if(!m_modelPath.isEmpty() || !m_preprocessPath.isEmpty())
        {
            error="configured semantic segmentation paths require asset metadata";
            return false;
        }
        QMutexLocker locker(&d->mutex);
        d->state.reset();
        d->modelDigest.clear();
        d->preprocessDigest.clear();
        d->modelLength=0;
        d->preprocessLength=0;
        return true;
    }
    if(m_modelPath.isEmpty() || m_preprocessPath.isEmpty())
    {
        error="semantic segmentation asset metadata requires both paths";
        return false;
    }
    if(!XvXml::validateAttributes(dataElement,{}, {},error)
            || !XvXml::validateChildren(dataElement,
                    {"SemanticSegmentationAssets"},
                    {"SemanticSegmentationAssets"},error)
            || hasNonWhitespaceText(dataElement))
    {
        if(error.isEmpty()) error="<PersistentData> contains unsupported text";
        return false;
    }
    const QDomElement assets=XvXml::singleChild(
                dataElement,"SemanticSegmentationAssets");
    if(!XvXml::validateAttributes(assets,{"format","version"},
                                  {"format","version"},error)
            || !XvXml::validateChildren(assets,{"Model","Preprocess"},
                                        {"Model","Preprocess"},error)
            || hasNonWhitespaceText(assets)
            || assets.attribute("format")!="halcon-dl-segmentation"
            || assets.attribute("version")!="1")
    {
        if(error.isEmpty()) error="unsupported semantic segmentation asset format or version";
        return false;
    }
    const QDomElement modelElement=XvXml::singleChild(assets,"Model");
    const QDomElement preprocessElement=XvXml::singleChild(assets,"Preprocess");
    if(!XvXml::validateAttributes(modelElement,{"length","sha256"},
                                  {"length","sha256"},error)
            || !XvXml::validateAttributes(preprocessElement,
                                          {"length","sha256"},
                                          {"length","sha256"},error)
            || !XvXml::validateChildren(modelElement,{}, {},error)
            || !XvXml::validateChildren(preprocessElement,{}, {},error)
            || hasNonWhitespaceText(modelElement)
            || hasNonWhitespaceText(preprocessElement))
    {
        if(error.isEmpty()) error="semantic segmentation asset metadata contains text";
        return false;
    }
    qint64 expectedModelLength=0;
    qint64 expectedPreprocessLength=0;
    QByteArray expectedModelDigest;
    QByteArray expectedPreprocessDigest;
    if(!parseLength(modelElement,expectedModelLength,error)
            || !parseLength(preprocessElement,expectedPreprocessLength,error)
            || !parseDigest(modelElement,expectedModelDigest,error)
            || !parseDigest(preprocessElement,expectedPreprocessDigest,error))
    {
        return false;
    }

    std::shared_ptr<ModelState> state;
    QString normalizedModel;
    QString normalizedPreprocess;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    if(!parseAssets(m_modelPath,m_preprocessPath,m_runtime,state,normalizedModel,
                    normalizedPreprocess,modelLength,preprocessLength,
                    modelDigest,preprocessDigest,error)
            || modelLength!=expectedModelLength
            || preprocessLength!=expectedPreprocessLength
            || modelDigest!=expectedModelDigest
            || preprocessDigest!=expectedPreprocessDigest)
    {
        if(error.isEmpty()) error="semantic segmentation asset metadata does not match files";
        return false;
    }
    QMutexLocker locker(&d->mutex);
    m_modelPath=normalizedModel;
    m_preprocessPath=normalizedPreprocess;
    d->state=state;
    d->modelLength=modelLength;
    d->preprocessLength=preprocessLength;
    d->modelDigest=modelDigest;
    d->preprocessDigest=preprocessDigest;
    return true;
}
