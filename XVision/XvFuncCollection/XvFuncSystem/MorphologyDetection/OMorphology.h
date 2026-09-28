#ifndef OMORPHOLOGY_H
#define OMORPHOLOGY_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

namespace XvCore
{
class OMorphologyParam:public OpenCvImageParamBase
{
public:
    OMorphologyParam()
    {
        kernelWidth=new XInt("kernelWidth",3,this,"卷积核宽度");
        kernelHeight=new XInt("kernelHeight",3,this,"卷积核高度");
        kernelShape=new XInt("kernelShape",0,this,"卷积核形状");
        useKernel=new XBool("useKernel",false,this,"使用自定义卷积核");
        kernelValues=new XString("kernelValues","1,1,1;1,1,1;1,1,1",this,
                                 "卷积核数值");
        anchorX=new XInt("anchorX",-1,this,"锚点 X");
        anchorY=new XInt("anchorY",-1,this,"锚点 Y");
        iterations=new XInt("iterations",1,this,"迭代次数");
        borderType=new XInt("borderType",0,this,"边界类型");
        borderValue=new XReal("borderValue",0.0,this,"边界填充值");
    }

    XInt *kernelWidth=nullptr;
    XInt *kernelHeight=nullptr;
    XInt *kernelShape=nullptr;
    XBool *useKernel=nullptr;
    XString *kernelValues=nullptr;
    XInt *anchorX=nullptr;
    XInt *anchorY=nullptr;
    XInt *iterations=nullptr;
    XInt *borderType=nullptr;
    XReal *borderValue=nullptr;
};

class OMorphologyResult:public OpenCvImageResultBase
{
};

class XVFUNCSYSTEM_EXPORT OMorphology:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { BlackHat=0,Close=1,Dilate=2,Erode=3,Gradient=4,Open=5,TopHat=6 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OMorphology(QObject *parent=nullptr);
    ~OMorphology() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    OMorphologyParam *m_param=nullptr;
    OMorphologyResult *m_result=nullptr;
    Mode m_mode=BlackHat;
};
}

#endif // OMORPHOLOGY_H
