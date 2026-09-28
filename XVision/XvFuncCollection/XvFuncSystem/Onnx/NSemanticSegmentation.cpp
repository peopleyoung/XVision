#include "NSemanticSegmentation.h"
#include "NOnnxOperatorWdg.h"

#include "XLanguage.h"

#include <QColor>

#include <cmath>

using namespace XvCore;

NSemanticSegmentation::NSemanticSegmentation(QObject *parent)
    :NOnnxBase(parent),m_param(new NSemanticSegmentationParam()),
      m_result(new NSemanticSegmentationResult())
{
    _funcRole="NSemanticSegmentation";
    _funcName=getLang("XvFuncSystem_NSemanticSegmentation_Name","ONNX语义分割");
    _funcType=EXvFuncType::Other;
}

NSemanticSegmentation::~NSemanticSegmentation()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

void NSemanticSegmentation::setMode(Mode mode)
{
    if(m_mode==mode) return;
    m_mode=mode;
    if(mode==Human && m_classNames.trimmed().isEmpty())
        m_classNames="Background\nHuman";
}

QStringList NSemanticSegmentation::persistentPropertyNames() const
{
    return QStringList{"mode","outputLayout","classNames"}
            +commonPersistentPropertyNames();
}

bool NSemanticSegmentation::validateConfiguration(QString &error) const
{
    if((m_mode!=Generic && m_mode!=Human)
            || m_outputLayout<Auto || m_outputLayout>Nhwc)
    {
        error="NSemanticSegmentation mode or output layout is invalid";
        return false;
    }
    const double threshold=m_param->binaryThreshold->value();
    const double opacity=m_param->overlayOpacity->value();
    if(!std::isfinite(threshold) || threshold<0.0 || threshold>1.0
            || !std::isfinite(opacity) || opacity<0.0 || opacity>1.0)
    {
        error="segmentation threshold or opacity is invalid";
        return false;
    }
    return validateCommonConfiguration(error);
}

bool NSemanticSegmentation::appendPersistentData(QDomDocument &doc,
                                                  QDomElement &functionElement,
                                                  QString &error) const
{
    return validateConfiguration(error)
            && NOnnxBase::appendPersistentData(doc,functionElement,error);
}

bool NSemanticSegmentation::readPersistentData(const QDomElement &dataElement,
                                                QString &error)
{
    return validateConfiguration(error)
            && NOnnxBase::readPersistentData(dataElement,error);
}

void NSemanticSegmentation::clearResults()
{
    m_result->segmentation->clear();
    m_result->colorMask->setValue(QImage());
    m_result->overlayImage->setValue(QImage());
    m_result->confidenceImage->setValue(QImage());
}

EXvFuncRunStatus NSemanticSegmentation::run()
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
    if(!executeModel({tensor},outputs,error) || outputs.size()!=1)
    {
        if(error.isEmpty()) error="segmentation model must return exactly one output";
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    XvOnnx::SegmentationValue decoded;
    if(!XvOnnx::decodeSegmentation(outputs.first(),
                                   static_cast<XvOnnx::TensorLayout>(m_outputLayout),
                                   input.width(),input.height(),
                                   m_param->binaryThreshold->value(),
                                   XvOnnx::parseClassNames(m_classNames),decoded,error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    XSegmentationResult candidate("candidateSegmentation");
    if(!candidate.setValue(decoded.width,decoded.height,decoded.labels,
                           decoded.confidences,decoded.classIds,
                           decoded.classNames,decoded.classColors))
    {
        setRunMsg(getLang("XvFuncSystem_NSemanticSegmentation_PublishFailed",
                          "无法发布ONNX分割结果"));
        return EXvFuncRunStatus::Error;
    }
    QImage mask(decoded.width,decoded.height,QImage::Format_ARGB32);
    QImage overlay=input.convertToFormat(QImage::Format_ARGB32);
    QImage confidence(decoded.width,decoded.height,QImage::Format_Grayscale8);
    if(mask.isNull() || overlay.isNull() || confidence.isNull())
    {
        setRunMsg(getLang("XvFuncSystem_NSemanticSegmentation_ImageAllocationFailed",
                          "无法创建ONNX分割显示图像"));
        return EXvFuncRunStatus::Error;
    }
    const double opacity=m_param->overlayOpacity->value();
    for(int y=0;y<decoded.height;++y)
    {
        uchar *confidenceLine=confidence.scanLine(y);
        for(int x=0;x<decoded.width;++x)
        {
            const int index=y*decoded.width+x;
            const int classId=decoded.labels.at(index);
            const int classIndex=decoded.classIds.indexOf(classId);
            if(classIndex<0)
            {
                setRunMsg(getLang("XvFuncSystem_NSemanticSegmentation_ClassMissing",
                                  "分割标签缺少类别信息"));
                return EXvFuncRunStatus::Error;
            }
            const QRgb color=decoded.classColors.at(classIndex);
            mask.setPixel(x,y,color);
            const QColor base=overlay.pixelColor(x,y);
            const double alpha=(qAlpha(color)/255.0)*opacity;
            overlay.setPixelColor(x,y,QColor(
                qRound(base.red()*(1.0-alpha)+qRed(color)*alpha),
                qRound(base.green()*(1.0-alpha)+qGreen(color)*alpha),
                qRound(base.blue()*(1.0-alpha)+qBlue(color)*alpha)));
            confidenceLine[x]=static_cast<uchar>(qBound(
                        0,qRound(decoded.confidences.at(index)*255.0f),255));
        }
    }
    if(!m_result->segmentation->setData(&candidate))
    {
        setRunMsg(getLang("XvFuncSystem_NSemanticSegmentation_CommitFailed",
                          "无法提交ONNX分割结果"));
        return EXvFuncRunStatus::Error;
    }
    m_result->colorMask->setValue(mask);
    m_result->overlayImage->setValue(overlay);
    m_result->confidenceImage->setValue(confidence);
    setRunMsg(getLang("XvFuncSystem_NSemanticSegmentation_RunOk",
                      "ONNX语义分割成功"));
    return EXvFuncRunStatus::Ok;
}

void NSemanticSegmentation::onShowFunc()
{
    if(!m_widget) m_widget=new NOnnxOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
