#ifndef OIMAGEFILTER_H
#define OIMAGEFILTER_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

namespace XvCore
{
class OImageFilterParam:public OpenCvImageParamBase
{
public:
    OImageFilterParam()
    {
        kernelWidth=new XInt("kernelWidth",3,this,"卷积核宽度");
        kernelHeight=new XInt("kernelHeight",3,this,"卷积核高度");
        sigmaX=new XReal("sigmaX",0.0,this,"水平标准差");
        sigmaY=new XReal("sigmaY",0.0,this,"垂直标准差");
        sigmaS=new XReal("sigmaS",60.0,this,"空间标准差");
        sigmaR=new XReal("sigmaR",0.4,this,"值域标准差");
        shadeFactor=new XReal("shadeFactor",0.02,this,"阴影系数");
        method=new XInt("method",0,this,"方法");
        outputVariant=new XInt("outputVariant",0,this,"输出类型");
    }

    XInt *kernelWidth=nullptr;
    XInt *kernelHeight=nullptr;
    XReal *sigmaX=nullptr;
    XReal *sigmaY=nullptr;
    XReal *sigmaS=nullptr;
    XReal *sigmaR=nullptr;
    XReal *shadeFactor=nullptr;
    XInt *method=nullptr;
    XInt *outputVariant=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageFilter:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode
    {
        BoxBlur=0,GaussianBlur=1,DetailEnhance=2,EdgePreserving=3,
        PencilSketch=4,Stylization=5
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageFilter(QObject *parent=nullptr);
    ~OImageFilter() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    OImageFilterParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=BoxBlur;
};
}

#endif // OIMAGEFILTER_H
