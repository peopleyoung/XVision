#include "HObjectDetection.h"
#include "HObjectDetectionWdg.h"

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

#include <algorithm>
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
    value=fromHalconString(tuple.S()).trimmed();
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

struct PreprocessConfig
{
    int width=0;
    int height=0;
    int channels=0;
    double rangeMin=0.0;
    double rangeMax=0.0;
};

struct ModelInfo
{
    PreprocessConfig preprocess;
    QVector<qint32> classIds;
    QStringList classNames;
    QHash<qint32,QString> classNameById;
};

bool parseAssets(const QString &modelPath,const QString &preprocessPath,
                 std::shared_ptr<ModelInfo> &info,QString &normalizedModel,
                 QString &normalizedPreprocess,qint64 &modelLength,
                 qint64 &preprocessLength,QByteArray &modelDigest,
                 QByteArray &preprocessDigest,QString &error)
{
    if(!normalizedFile(modelPath,"hdl",normalizedModel,modelLength,
                       modelDigest,error)
            || !normalizedFile(preprocessPath,"hdict",normalizedPreprocess,
                               preprocessLength,preprocessDigest,error))
    {
        return false;
    }

    try
    {
        auto candidate=std::make_shared<ModelInfo>();
        HDlModel model(halconPath(normalizedModel));
        const QString modelType=fromHalconString(
                    model.GetDlModelParam("type").S());
        if(modelType!="detection")
        {
            error=QString("model type must be 'detection', got '%1'")
                    .arg(modelType);
            return false;
        }
        const QString instanceType=fromHalconString(
                    model.GetDlModelParam("instance_type").S());
        if(instanceType!="rectangle1")
        {
            error=QString("unsupported detection instance_type '%1'; "
                          "the current implementation supports rectangle1")
                    .arg(instanceType);
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
        if(preprocessModelType!="detection" || normalizationType!="none"
                || domainHandling!="full_domain")
        {
            error=QString("unsupported detection preprocessing: model_type=%1, "
                          "normalization_type=%2, domain_handling=%3")
                    .arg(preprocessModelType,normalizationType,domainHandling);
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
        if(!modelInteger(model,"image_width",modelWidth,error)
                || !modelInteger(model,"image_height",modelHeight,error)
                || !modelInteger(model,"image_num_channels",modelChannels,error)
                || !modelReal(model,"image_range_min",modelRangeMin,error)
                || !modelReal(model,"image_range_max",modelRangeMax,error))
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

        const HTuple ids=model.GetDlModelParam("class_ids");
        HTuple names;
        try
        {
            names=model.GetDlModelParam("class_names");
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
            if(id<0 || id>std::numeric_limits<qint32>::max()
                    || uniqueIds.contains(qint32(id)))
            {
                error="model class_ids contain a duplicate or invalid value";
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
            candidate->classNameById.insert(qint32(id),name);
        }
        info=candidate;
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
        error="unknown error while loading object detection assets";
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

bool parseDetections(const HDict &dlResult,const ModelInfo &info,
                     int inputWidth,int inputHeight,
                     XObjectList &detections,QString &error)
{
    const char *keys[]={"bbox_row1","bbox_col1","bbox_row2","bbox_col2",
                        "bbox_class_id","bbox_confidence"};
    HTuple values[6];
    for(int index=0;index<6;++index)
    {
        if(!dictHasKey(dlResult,keys[index]))
        {
            error=QString("detection result is missing '%1'").arg(keys[index]);
            return false;
        }
        values[index]=dlResult.GetDictTuple(keys[index]);
    }
    const Hlong count=values[0].Length();
    for(int index=1;index<6;++index)
    {
        if(values[index].Length()!=count)
        {
            error="detection result tuple lengths do not match";
            return false;
        }
    }

    const double scaleX=double(inputWidth)/double(info.preprocess.width);
    const double scaleY=double(inputHeight)/double(info.preprocess.height);
    for(Hlong index=0;index<count;++index)
    {
        const double row1=values[0][index].D();
        const double column1=values[1][index].D();
        const double row2=values[2][index].D();
        const double column2=values[3][index].D();
        const Hlong classIdValue=values[4][index].L();
        const double score=values[5][index].D();
        if(!qIsFinite(row1) || !qIsFinite(column1) || !qIsFinite(row2)
                || !qIsFinite(column2) || !qIsFinite(score)
                || row2<row1 || column2<column1 || classIdValue<0
                || classIdValue>std::numeric_limits<qint32>::max()
                || score<0.0 || score>1.0)
        {
            error="detection result contains an invalid box, class, or score";
            return false;
        }
        const qint32 classId=qint32(classIdValue);
        const QString className=info.classNameById.value(classId);
        if(className.isEmpty())
        {
            error=QString("detection result contains unknown class ID %1")
                    .arg(classId);
            return false;
        }
        const double x1=std::clamp(column1*scaleX,0.0,double(inputWidth));
        const double y1=std::clamp(row1*scaleY,0.0,double(inputHeight));
        const double x2=std::clamp((column2+1.0)*scaleX,0.0,
                                   double(inputWidth));
        const double y2=std::clamp((row2+1.0)*scaleY,0.0,
                                   double(inputHeight));
        auto detection=new XDetectionResult();
        detection->setObjectName(QString("detection%1").arg(index));
        if(!detection->setValue(x1,y1,x2-x1,y2-y1,int(classId),className,score)
                || !detections.addValue(detection))
        {
            delete detection;
            error="detection result cannot be represented in input coordinates";
            return false;
        }
    }
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

class XvCore::HObjectDetectionPrivate
{
public:
    mutable QMutex mutex;
    std::shared_ptr<ModelInfo> info;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
};

HObjectDetection::HObjectDetection(QObject *parent)
    :XvFunc(parent),d(new HObjectDetectionPrivate)
{
    _funcRole="HObjectDetection";
    _funcName=getLang("XvFuncSystem_HObjectDetection_Name","目标检测(H)");
    _funcType=EXvFuncType::DefectDetection;
    param=new HObjectDetectionParam();
    result=new HObjectDetectionResult();
}

HObjectDetection::~HObjectDetection()
{
    delete m_frm;
    m_frm=nullptr;
    delete param;
    param=nullptr;
    delete result;
    result=nullptr;
}

void HObjectDetection::setModelPath(const QString &path)
{
    m_modelPath=path;
    QMutexLocker locker(&d->mutex);
    d->info.reset();
    d->modelDigest.clear();
    d->modelLength=0;
}

void HObjectDetection::setPreprocessPath(const QString &path)
{
    m_preprocessPath=path;
    QMutexLocker locker(&d->mutex);
    d->info.reset();
    d->preprocessDigest.clear();
    d->preprocessLength=0;
}

void HObjectDetection::setRuntime(Runtime runtime)
{
    m_runtime=runtime;
}

bool HObjectDetection::configureAssets(const QString &modelPath,
                                       const QString &preprocessPath,
                                       QString &error)
{
    error.clear();
    std::shared_ptr<ModelInfo> info;
    QString normalizedModel;
    QString normalizedPreprocess;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    if(!parseAssets(modelPath,preprocessPath,info,normalizedModel,
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
    d->info=info;
    return true;
}

bool HObjectDetection::hasConfiguredAssets() const
{
    QMutexLocker locker(&d->mutex);
    return !m_modelPath.isEmpty() && !m_preprocessPath.isEmpty()
            && d->modelLength>0 && d->preprocessLength>0
            && d->modelDigest.size()==64 && d->preprocessDigest.size()==64;
}

QString HObjectDetection::assetSummary() const
{
    QMutexLocker locker(&d->mutex);
    if(!d->info)
    {
        const bool configured=!m_modelPath.isEmpty() && !m_preprocessPath.isEmpty()
                && d->modelLength>0 && d->preprocessLength>0
                && d->modelDigest.size()==64
                && d->preprocessDigest.size()==64;
        return configured
                ?getLang("XvFuncSystem_HObjectDetection_LoadOnRun",
                         "已配置，将在运行时加载")
                :QString();
    }
    return getLang("XvFuncSystem_HObjectDetection_AssetSummary",
                   "%1x%2，%3通道，%4类别")
            .arg(d->info->preprocess.width)
            .arg(d->info->preprocess.height)
            .arg(d->info->preprocess.channels)
            .arg(d->info->classIds.size());
}

QStringList HObjectDetection::classSummary() const
{
    QMutexLocker locker(&d->mutex);
    if(!d->info) return {};
    QStringList summary;
    for(qsizetype index=0;index<d->info->classIds.size();++index)
    {
        summary.append(QString("%1: %2")
                       .arg(d->info->classIds.at(index))
                       .arg(d->info->classNames.at(index)));
    }
    return summary;
}

QPixmap HObjectDetection::funcIcon()
{
    return QPixmap(":/images/HObjectDetection.svg");
}

void HObjectDetection::onShowFunc()
{
    if(!m_frm) m_frm=new HObjectDetectionWdg(this);
    m_frm->show();
    m_frm->raise();
}

void HObjectDetection::clearResults()
{
    if(!result) return;
    result->outputImage->setValue(QImage());
    result->detectionCount->setValue(0);
    result->detections->clear();
}

EXvFuncRunStatus HObjectDetection::run()
{
    clearResults();
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_InvalidRuntime",
                          "目标检测运行设备无效"));
        return EXvFuncRunStatus::Error;
    }
    const double minConfidence=param->minConfidence->value();
    const double maxOverlap=param->maxOverlap->value();
    const int maxNumDetections=param->maxNumDetections->value();
    if(!qIsFinite(minConfidence) || minConfidence<0.0 || minConfidence>1.0
            || !qIsFinite(maxOverlap) || maxOverlap<0.0 || maxOverlap>1.0
            || maxNumDetections<1 || maxNumDetections>10000)
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_InvalidParameters",
                          "目标检测参数超出支持范围"));
        return EXvFuncRunStatus::Error;
    }
    const QImage input=param->inputImage->value();
    if(input.isNull())
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_EmptyImage",
                          "目标检测输入图像为空"));
        return EXvFuncRunStatus::Error;
    }

    try
    {
        std::shared_ptr<ModelInfo> info;
        QByteArray expectedModelDigest;
        QByteArray expectedPreprocessDigest;
        qint64 expectedModelLength=0;
        qint64 expectedPreprocessLength=0;
        {
            QMutexLocker locker(&d->mutex);
            info=d->info;
            expectedModelDigest=d->modelDigest;
            expectedPreprocessDigest=d->preprocessDigest;
            expectedModelLength=d->modelLength;
            expectedPreprocessLength=d->preprocessLength;
        }
        if(expectedModelDigest.isEmpty() || expectedPreprocessDigest.isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_AssetsMissing",
                              "尚未配置有效的目标检测模型和预处理文件"));
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
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_AssetsChanged",
                              "目标检测资产缺失或已被替换")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }
        if(!info)
        {
            if(!parseAssets(m_modelPath,m_preprocessPath,info,normalizedModel,
                            normalizedPreprocess,modelLength,preprocessLength,
                            modelDigest,preprocessDigest,error)
                    || modelDigest!=expectedModelDigest
                    || preprocessDigest!=expectedPreprocessDigest)
            {
                setRunMsg(getLang("XvFuncSystem_HObjectDetection_LoadFailed",
                                  "目标检测资产加载失败")
                          +QString(": %1").arg(error));
                return EXvFuncRunStatus::Error;
            }
            QMutexLocker locker(&d->mutex);
            d->info=info;
        }

        HImage preprocessed;
        if(!preprocessImage(input,info->preprocess,preprocessed,error))
        {
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_PreprocessFailed",
                              "目标检测图像预处理失败")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }

        HDlModel model(halconPath(normalizedModel));
        model.SetDlModelParam(HString("runtime"),
                              HTuple(m_runtime==Cpu?"cpu":"gpu"));
        model.SetDlModelParam(HString("batch_size"),HTuple(Hlong(1)));
        model.SetDlModelParam(HString("min_confidence"),HTuple(minConfidence));
        model.SetDlModelParam(HString("max_overlap"),HTuple(maxOverlap));
        model.SetDlModelParam(
                    HString("max_overlap_class_agnostic"),
                    HTuple(param->maxOverlapClassAgnostic->value()
                           ?"true":"false"));
        model.SetDlModelParam(HString("max_num_detections"),
                              HTuple(Hlong(maxNumDetections)));

        HDict sample;
        sample.SetDictObject(preprocessed,"image");
        HDictArray samples(&sample,1);
        const HDictArray results=model.ApplyDlModel(samples,HTuple());
        if(results.Length()!=1)
        {
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_InvalidResult",
                              "HALCON返回了无效的目标检测结果"));
            return EXvFuncRunStatus::Error;
        }
        XObjectList candidate("detections",XDetectionResult::type());
        if(!parseDetections(results.Tools()[0],*info,input.width(),input.height(),
                            candidate,error))
        {
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_InvalidResult",
                              "HALCON返回了无效的目标检测结果")
                      +QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }
        if(!result->detections->setData(&candidate))
        {
            setRunMsg(getLang("XvFuncSystem_HObjectDetection_ResultUpdateFailed",
                              "目标检测结果更新失败"));
            clearResults();
            return EXvFuncRunStatus::Error;
        }
        result->outputImage->setValue(input.copy());
        result->detectionCount->setValue(int(candidate.count()));
        setRunMsg(QString(getLang("XvFuncSystem_HObjectDetection_RunOk",
                                 "目标检测完成，共%1个结果"))
                  .arg(candidate.count()));
        return EXvFuncRunStatus::Ok;
    }
    catch(const HException &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_HalconFailed",
                          "HALCON目标检测失败")
                  +QString(": %1").arg(halconError(exception)));
    }
    catch(const std::exception &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_RunFailed",
                          "目标检测失败")
                  +QString(": %1").arg(QString::fromLocal8Bit(exception.what())));
    }
    catch(...)
    {
        setRunMsg(getLang("XvFuncSystem_HObjectDetection_RunFailed",
                          "目标检测失败"));
    }
    clearResults();
    return EXvFuncRunStatus::Error;
}

bool HObjectDetection::appendPersistentData(
        QDomDocument &doc,QDomElement &functionElement,QString &error) const
{
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        error="object detection runtime is invalid";
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
        error="object detection asset configuration is incomplete";
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
        if(error.isEmpty()) error="object detection assets changed since configuration";
        return false;
    }

    QDomElement dataElement=doc.createElement("PersistentData");
    QDomElement assetsElement=doc.createElement("ObjectDetectionAssets");
    assetsElement.setAttribute("format","halcon-dl-detection");
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

bool HObjectDetection::readPersistentData(const QDomElement &dataElement,
                                          QString &error)
{
    if(m_runtime!=Cpu && m_runtime!=Gpu)
    {
        error="object detection runtime is invalid";
        return false;
    }
    if(dataElement.isNull())
    {
        if(!m_modelPath.isEmpty() || !m_preprocessPath.isEmpty())
        {
            error="configured object detection paths require asset metadata";
            return false;
        }
        QMutexLocker locker(&d->mutex);
        d->info.reset();
        d->modelDigest.clear();
        d->preprocessDigest.clear();
        d->modelLength=0;
        d->preprocessLength=0;
        return true;
    }
    if(m_modelPath.isEmpty() || m_preprocessPath.isEmpty())
    {
        error="object detection asset metadata requires both paths";
        return false;
    }
    if(!XvXml::validateAttributes(dataElement,{}, {},error)
            || !XvXml::validateChildren(dataElement,
                    {"ObjectDetectionAssets"},{"ObjectDetectionAssets"},error)
            || hasNonWhitespaceText(dataElement))
    {
        if(error.isEmpty()) error="<PersistentData> contains unsupported text";
        return false;
    }
    const QDomElement assets=XvXml::singleChild(
                dataElement,"ObjectDetectionAssets");
    if(!XvXml::validateAttributes(assets,{"format","version"},
                                  {"format","version"},error)
            || !XvXml::validateChildren(assets,{"Model","Preprocess"},
                                        {"Model","Preprocess"},error)
            || hasNonWhitespaceText(assets)
            || assets.attribute("format")!="halcon-dl-detection"
            || assets.attribute("version")!="1")
    {
        if(error.isEmpty()) error="unsupported object detection asset format or version";
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
        if(error.isEmpty()) error="object detection asset metadata contains text";
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

    std::shared_ptr<ModelInfo> info;
    QString normalizedModel;
    QString normalizedPreprocess;
    qint64 modelLength=0;
    qint64 preprocessLength=0;
    QByteArray modelDigest;
    QByteArray preprocessDigest;
    if(!parseAssets(m_modelPath,m_preprocessPath,info,normalizedModel,
                    normalizedPreprocess,modelLength,preprocessLength,
                    modelDigest,preprocessDigest,error)
            || modelLength!=expectedModelLength
            || preprocessLength!=expectedPreprocessLength
            || modelDigest!=expectedModelDigest
            || preprocessDigest!=expectedPreprocessDigest)
    {
        if(error.isEmpty()) error="object detection asset metadata does not match files";
        return false;
    }

    QMutexLocker locker(&d->mutex);
    m_modelPath=normalizedModel;
    m_preprocessPath=normalizedPreprocess;
    d->info=info;
    d->modelLength=modelLength;
    d->preprocessLength=preprocessLength;
    d->modelDigest=modelDigest;
    d->preprocessDigest=preprocessDigest;
    return true;
}
