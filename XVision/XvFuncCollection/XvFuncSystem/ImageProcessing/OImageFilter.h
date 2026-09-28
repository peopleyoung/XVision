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
        kernelWidth=new XInt("kernelWidth",3,this,"Kernel width");
        kernelHeight=new XInt("kernelHeight",3,this,"Kernel height");
        sigmaX=new XReal("sigmaX",0.0,this,"Sigma X");
        sigmaY=new XReal("sigmaY",0.0,this,"Sigma Y");
        sigmaS=new XReal("sigmaS",60.0,this,"Sigma spatial");
        sigmaR=new XReal("sigmaR",0.4,this,"Sigma range");
        shadeFactor=new XReal("shadeFactor",0.02,this,"Shade factor");
        method=new XInt("method",0,this,"Method");
        outputVariant=new XInt("outputVariant",0,this,"Output variant");
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
    Q_PROPERTY(OImageFilter::Mode mode READ mode WRITE setMode)
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
