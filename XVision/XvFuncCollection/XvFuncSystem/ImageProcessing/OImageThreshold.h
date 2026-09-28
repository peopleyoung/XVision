#ifndef OIMAGETHRESHOLD_H
#define OIMAGETHRESHOLD_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

namespace XvCore
{
class OImageThresholdParam:public OpenCvImageParamBase
{
public:
    OImageThresholdParam()
    {
        threshold=new XReal("threshold",127.0,this,"Threshold");
        maxValue=new XReal("maxValue",255.0,this,"Maximum value");
        thresholdType=new XInt("thresholdType",0,this,"Threshold type");
        comparison=new XInt("comparison",0,this,"Comparison");
    }

    XReal *threshold=nullptr;
    XReal *maxValue=nullptr;
    XInt *thresholdType=nullptr;
    XInt *comparison=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageThreshold:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Threshold=0,PixelCondition=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageThreshold(QObject *parent=nullptr);
    ~OImageThreshold() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    OImageThresholdParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=Threshold;
};
}

#endif // OIMAGETHRESHOLD_H
