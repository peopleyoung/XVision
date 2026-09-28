#include "NClassification.h"
#include "NOnnxOperatorWdg.h"

#include "XLanguage.h"

#include <memory>

using namespace XvCore;

NClassification::NClassification(QObject *parent)
    :NOnnxBase(parent),m_param(new NClassificationParam()),
      m_result(new NClassificationResult())
{
    _funcRole="NClassification";
    _funcName=getLang("XvFuncSystem_NClassification_Name","ONNX图像分类");
    _funcType=EXvFuncType::Other;
}

NClassification::~NClassification()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

void NClassification::setMode(Mode mode)
{
    if(m_mode==mode) return;
    m_mode=mode;
    if(mode==Gender && m_classNames.trimmed().isEmpty())
        m_classNames="Female\nMale";
}

QStringList NClassification::persistentPropertyNames() const
{
    return QStringList{"mode","applySoftmax","classNames"}
            +commonPersistentPropertyNames();
}

bool NClassification::validateConfiguration(QString &error) const
{
    if(m_mode!=Generic && m_mode!=Gender)
    {
        error="NClassification mode is invalid";
        return false;
    }
    if(m_param->topK->value()<=0 || m_param->topK->value()>100000
            || m_param->minScore->value()<0.0 || m_param->minScore->value()>1.0)
    {
        error="classification topK or minimum score is invalid";
        return false;
    }
    return validateCommonConfiguration(error);
}

bool NClassification::appendPersistentData(QDomDocument &doc,
                                            QDomElement &functionElement,
                                            QString &error) const
{
    return validateConfiguration(error)
            && NOnnxBase::appendPersistentData(doc,functionElement,error);
}

bool NClassification::readPersistentData(const QDomElement &dataElement,
                                          QString &error)
{
    return validateConfiguration(error)
            && NOnnxBase::readPersistentData(dataElement,error);
}

void NClassification::clearResults()
{
    m_result->outputImage->setValue(QImage());
    m_result->classificationCount->setValue(0);
    m_result->classifications->clear();
}

EXvFuncRunStatus NClassification::run()
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
        if(error.isEmpty()) error="classification model must return exactly one output";
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    QVector<XvOnnx::ClassificationValue> decoded;
    if(!XvOnnx::decodeClassification(outputs.first(),m_applySoftmax,
                                     m_param->topK->value(),m_param->minScore->value(),
                                     XvOnnx::parseClassNames(m_classNames),decoded,error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    XObjectList candidate("candidateClassifications",XClassificationResult::type());
    for(const XvOnnx::ClassificationValue &item:decoded)
    {
        auto value=std::make_unique<XClassificationResult>(
                    item.classId,item.className,item.score);
        if(!candidate.addValue(value.get()))
        {
            setRunMsg(getLang("XvFuncSystem_NClassification_PublishFailed",
                              "无法发布ONNX分类结果"));
            return EXvFuncRunStatus::Error;
        }
        value.release();
    }
    if(!m_result->classifications->setData(&candidate))
    {
        setRunMsg(getLang("XvFuncSystem_NClassification_ListFailed",
                          "无法发布ONNX分类结果列表"));
        return EXvFuncRunStatus::Error;
    }
    m_result->outputImage->setValue(input.copy());
    m_result->classificationCount->setValue(static_cast<int>(decoded.size()));
    setRunMsg(getLang("XvFuncSystem_NClassification_RunOk","ONNX分类成功"));
    return EXvFuncRunStatus::Ok;
}

void NClassification::onShowFunc()
{
    if(!m_widget) m_widget=new NOnnxOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
