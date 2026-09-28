#ifndef OIMAGEMODEL_H
#define OIMAGEMODEL_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

#include <memory>

namespace XvCore
{
class OImageModelParam:public OpenCvImageParamBase
{
public:
    OImageModelParam()
    {
        modelPath=new XString("modelPath",QString(),this,"Model path");
        algorithm=new XString("algorithm","edsr",this,"Algorithm");
        scale=new XInt("scale",2,this,"Scale");
        history=new XInt("history",500,this,"History");
        mixtures=new XInt("mixtures",5,this,"Mixtures");
        backgroundRatio=new XReal("backgroundRatio",0.7,this,"Background ratio");
        varianceThreshold=new XReal("varianceThreshold",16.0,this,"Variance threshold");
        learningRate=new XReal("learningRate",-1.0,this,"Learning rate");
        outputBackground=new XBool("outputBackground",false,this,"Output background");
        svmWidth=new XInt("svmWidth",0,this,"SVM input width");
        svmHeight=new XInt("svmHeight",0,this,"SVM input height");
    }

    XString *modelPath=nullptr;
    XString *algorithm=nullptr;
    XInt *scale=nullptr;
    XInt *history=nullptr;
    XInt *mixtures=nullptr;
    XReal *backgroundRatio=nullptr;
    XReal *varianceThreshold=nullptr;
    XReal *learningRate=nullptr;
    XBool *outputBackground=nullptr;
    XInt *svmWidth=nullptr;
    XInt *svmHeight=nullptr;
};

class OImageModelResult:public OpenCvImageResultBase
{
public:
    OImageModelResult()
    {
        classId=new XInt("classId",0,this,"Class ID");
    }

    XInt *classId=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageModel:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { SuperResolution=0,BackgroundSubtraction=1,Svm=2 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageModel(QObject *parent=nullptr);
    ~OImageModel() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    OImageModelParam *m_param=nullptr;
    OImageModelResult *m_result=nullptr;
    Mode m_mode=SuperResolution;
    int m_candidateClassId=0;
};
}

#endif // OIMAGEMODEL_H
