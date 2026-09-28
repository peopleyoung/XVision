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
        modelPath=new XString("modelPath",QString(),this,"模型路径");
        algorithm=new XString("algorithm","edsr",this,"算法");
        scale=new XInt("scale",2,this,"缩放比例");
        history=new XInt("history",500,this,"历史帧数");
        mixtures=new XInt("mixtures",5,this,"混合模型数量");
        backgroundRatio=new XReal("backgroundRatio",0.7,this,"背景比例");
        varianceThreshold=new XReal("varianceThreshold",16.0,this,"方差阈值");
        learningRate=new XReal("learningRate",-1.0,this,"学习率");
        outputBackground=new XBool("outputBackground",false,this,"输出背景");
        svmWidth=new XInt("svmWidth",0,this,"SVM 输入宽度");
        svmHeight=new XInt("svmHeight",0,this,"SVM 输入高度");
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
        classId=new XInt("classId",0,this,"类别编号");
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
