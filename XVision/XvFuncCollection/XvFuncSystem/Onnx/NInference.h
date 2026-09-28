#ifndef NINFERENCE_H
#define NINFERENCE_H

#include "NOnnxBase.h"
#include "XObjectList.h"
#include "XVisionRuntimeData.h"

class NOnnxOperatorWdg;

namespace XvCore
{
class NInferenceParam:public XvBaseParam
{
public:
    NInferenceParam()
    {
        inputs=new XObjectList("inputs",XTensor::type(),this,"输入张量");
        inputImage=new XImage("inputImage",QImage(),this,"输入图像");
    }
    XObjectList *inputs=nullptr;
    XImage *inputImage=nullptr;
};

class NInferenceResult:public XvBaseResult
{
public:
    NInferenceResult()
    {
        outputs=new XObjectList("outputs",XTensor::type(),this,"输出张量");
        outputCount=new XInt("outputCount",0,this,"输出数量");
        age=new XReal("age",0.0,this,"年龄估计");
        ageValid=new XBool("ageValid",false,this,"年龄有效");
    }
    XObjectList *outputs=nullptr;
    XInt *outputCount=nullptr;
    XReal *age=nullptr;
    XBool *ageValid=nullptr;
};

class XVFUNCSYSTEM_EXPORT NInference:public NOnnxBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    friend class ::NOnnxOperatorWdg;
public:
    enum Mode
    {
        Generic=0,
        Age=1
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit NInference(QObject *parent=nullptr);
    ~NInference() override;
    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList persistentPropertyNames() const override;

public slots:
    void onShowFunc() override;

protected:
    QPixmap funcIcon() override { return QPixmap(":/images/HObjectDetection.svg"); }
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;

private:
    bool validateConfiguration(QString &error) const;
    void clearResults();
    bool commitOutputs(const QList<XOnnxTensorData> &outputs,QString &error);

    NInferenceParam *m_param=nullptr;
    NInferenceResult *m_result=nullptr;
    NOnnxOperatorWdg *m_widget=nullptr;
    Mode m_mode=Generic;
};
}

#endif // NINFERENCE_H
