#include "NObjectDetection.h"
#include "NOnnxOperatorWdg.h"

#include "XLanguage.h"

#include <cmath>
#include <memory>

using namespace XvCore;

NObjectDetection::NObjectDetection(QObject *parent)
    :NOnnxBase(parent),m_param(new NObjectDetectionParam()),
      m_result(new NObjectDetectionResult())
{
    _funcRole="NObjectDetection";
    _funcName=getLang("XvFuncSystem_NObjectDetection_Name","ONNX目标检测");
    _funcType=EXvFuncType::MachineLearning;
}

NObjectDetection::~NObjectDetection()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

void NObjectDetection::setMode(Mode mode)
{
    if(m_mode==mode) return;
    m_mode=mode;
    if(mode==Yolov3 || mode==Yolov5 || mode==Yolov5Face)
        setResizeMode(Letterbox);
    if(mode==Yolov5Face && m_classNames.trimmed().isEmpty())
        m_classNames="Face";
}

QStringList NObjectDetection::persistentPropertyNames() const
{
    return QStringList{"mode","normalizedCoordinates","classNames"}
            +commonPersistentPropertyNames();
}

bool NObjectDetection::validateConfiguration(QString &error) const
{
    if(m_mode<Generic || m_mode>Yolov5Face)
    {
        error="NObjectDetection mode is invalid";
        return false;
    }
    const double confidence=m_param->confidenceThreshold->value();
    const double iou=m_param->iouThreshold->value();
    if(!std::isfinite(confidence) || confidence<0.0 || confidence>1.0
            || !std::isfinite(iou) || iou<0.0 || iou>1.0
            || m_param->maximumDetections->value()<=0
            || m_param->maximumDetections->value()>100000)
    {
        error="detection thresholds or maximum count are invalid";
        return false;
    }
    return validateCommonConfiguration(error);
}

bool NObjectDetection::appendPersistentData(QDomDocument &doc,
                                             QDomElement &functionElement,
                                             QString &error) const
{
    return validateConfiguration(error)
            && NOnnxBase::appendPersistentData(doc,functionElement,error);
}

bool NObjectDetection::readPersistentData(const QDomElement &dataElement,
                                           QString &error)
{
    return validateConfiguration(error)
            && NOnnxBase::readPersistentData(dataElement,error);
}

void NObjectDetection::clearResults()
{
    m_result->outputImage->setValue(QImage());
    m_result->detectionCount->setValue(0);
    m_result->detections->clear();
}

EXvFuncRunStatus NObjectDetection::run()
{
    clearResults();
    QString error;
    if(!validateConfiguration(error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    const QImage input=m_param->inputImage->value();
    XOnnxTensorData tensor;
    XvOnnx::ImageTransform transform;
    if(!prepareImageInput(input,tensor,transform,error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    QList<XOnnxTensorData> outputs;
    if(!executeModel({tensor},outputs,error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    QVector<XvOnnx::DetectionValue> decoded;
    if(!XvOnnx::decodeDetections(outputs,
                                 static_cast<XvOnnx::DetectionDecoder>(m_mode),
                                 transform,m_normalizedCoordinates,
                                 m_param->confidenceThreshold->value(),
                                 m_param->iouThreshold->value(),
                                 m_param->classAgnosticNms->value(),
                                 m_param->maximumDetections->value(),
                                 XvOnnx::parseClassNames(m_classNames),decoded,error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    XObjectList candidate("candidateDetections",XDetectionResult::type());
    for(const XvOnnx::DetectionValue &item:decoded)
    {
        auto value=std::make_unique<XDetectionResult>(
                    item.x,item.y,item.width,item.height,
                    item.classId,item.className,item.score);
        if(!candidate.addValue(value.get()))
        {
            setRunMsg(getLang("XvFuncSystem_NObjectDetection_PublishFailed",
                              "无法发布ONNX检测结果"));
            return EXvFuncRunStatus::Error;
        }
        value.release();
    }
    if(!m_result->detections->setData(&candidate))
    {
        setRunMsg(getLang("XvFuncSystem_NObjectDetection_ListFailed",
                          "无法发布ONNX检测结果列表"));
        return EXvFuncRunStatus::Error;
    }
    m_result->outputImage->setValue(input.copy());
    m_result->detectionCount->setValue(static_cast<int>(decoded.size()));
    setRunMsg(getLang("XvFuncSystem_NObjectDetection_RunOk","ONNX目标检测成功"));
    return EXvFuncRunStatus::Ok;
}

void NObjectDetection::onShowFunc()
{
    if(!m_widget) m_widget=new NOnnxOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
