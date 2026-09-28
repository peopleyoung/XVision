#include "NInference.h"
#include "NOnnxOperatorWdg.h"

#include "XLanguage.h"

#include <memory>

using namespace XvCore;

NInference::NInference(QObject *parent)
    :NOnnxBase(parent),m_param(new NInferenceParam()),m_result(new NInferenceResult())
{
    _funcRole="NInference";
    _funcName=getLang("XvFuncSystem_NInference_Name","ONNX通用推理");
    _funcType=EXvFuncType::MachineLearning;
}

NInference::~NInference()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList NInference::persistentPropertyNames() const
{
    return QStringList{"mode"}+commonPersistentPropertyNames();
}

bool NInference::validateConfiguration(QString &error) const
{
    if(m_mode!=Generic && m_mode!=Age)
    {
        error="NInference mode is invalid";
        return false;
    }
    return validateCommonConfiguration(error);
}

bool NInference::appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                                      QString &error) const
{
    return validateConfiguration(error)
            && NOnnxBase::appendPersistentData(doc,functionElement,error);
}

bool NInference::readPersistentData(const QDomElement &dataElement,QString &error)
{
    return validateConfiguration(error)
            && NOnnxBase::readPersistentData(dataElement,error);
}

void NInference::clearResults()
{
    m_result->outputs->clear();
    m_result->outputCount->setValue(0);
    m_result->age->setValue(0.0);
    m_result->ageValid->setValue(false);
}

bool NInference::commitOutputs(const QList<XOnnxTensorData> &outputs,QString &error)
{
    XObjectList candidate("candidateOutputs",XTensor::type());
    for(const XOnnxTensorData &output:outputs)
    {
        auto value=std::make_unique<XTensor>(output.name,output.elementType,
                                              output.dimensions,output.bytes);
        if(value->elementType()!=output.elementType
                || value->dimensions()!=output.dimensions
                || value->bytes()!=output.bytes
                || !candidate.addValue(value.get()))
        {
            error=QString("cannot publish ONNX output tensor '%1'").arg(output.name);
            return false;
        }
        value.release();
    }
    if(!m_result->outputs->setData(&candidate))
    {
        error="cannot publish ONNX output tensor list";
        return false;
    }
    m_result->outputCount->setValue(static_cast<int>(outputs.size()));
    return true;
}

EXvFuncRunStatus NInference::run()
{
    clearResults();
    QString error;
    if(!validateConfiguration(error))
    {
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    QList<XOnnxTensorData> inputs;
    if(m_mode==Generic)
    {
        for(XObject *object:m_param->inputs->values())
        {
            auto tensor=object?dynamic_cast<XTensor*>(object):nullptr;
            if(!tensor)
            {
                setRunMsg(getLang("XvFuncSystem_NInference_InputTypeInvalid",
                                  "ONNX输入张量列表包含无效类型"));
                return EXvFuncRunStatus::Error;
            }
            inputs.append({tensor->objectName(),tensor->elementType(),
                           tensor->dimensions(),tensor->bytes()});
        }
    }
    else
    {
        XOnnxTensorData input;
        XvOnnx::ImageTransform transform;
        if(!prepareImageInput(m_param->inputImage->value(),input,transform,error))
        {
            setRunMsg(error);
            return EXvFuncRunStatus::Error;
        }
        inputs.append(input);
    }

    QList<XOnnxTensorData> outputs;
    if(!executeModel(inputs,outputs,error) || outputs.isEmpty())
    {
        if(error.isEmpty()) error="ONNX model returned no outputs";
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    double ageValue=0.0;
    if(m_mode==Age)
    {
        if(outputs.size()!=1)
        {
            setRunMsg(getLang("XvFuncSystem_NInference_AgeOutputCount",
                              "年龄模型必须只有一个输出张量"));
            return EXvFuncRunStatus::Error;
        }
        if(!XvOnnx::decodeAge(outputs.first(),ageValue,error))
        {
            setRunMsg(error);
            return EXvFuncRunStatus::Error;
        }
    }
    if(!commitOutputs(outputs,error))
    {
        clearResults();
        setRunMsg(error);
        return EXvFuncRunStatus::Error;
    }
    if(m_mode==Age)
    {
        m_result->age->setValue(ageValue);
        m_result->ageValid->setValue(true);
    }
    setRunMsg(getLang("XvFuncSystem_NInference_RunOk","ONNX推理成功"));
    return EXvFuncRunStatus::Ok;
}

void NInference::onShowFunc()
{
    if(!m_widget) m_widget=new NOnnxOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
