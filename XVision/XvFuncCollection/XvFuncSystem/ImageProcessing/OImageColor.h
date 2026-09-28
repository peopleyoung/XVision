#ifndef OIMAGECOLOR_H
#define OIMAGECOLOR_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

namespace XvCore
{
class OImageColorParam:public OpenCvImageParamBase
{
public:
    OImageColorParam()
    {
        conversion=new XInt("conversion",0,this,"Conversion");
        hueMin=new XInt("hueMin",0,this,"Hue minimum");
        hueMax=new XInt("hueMax",179,this,"Hue maximum");
        saturationMin=new XInt("saturationMin",0,this,"Saturation minimum");
        saturationMax=new XInt("saturationMax",255,this,"Saturation maximum");
        valueMin=new XInt("valueMin",0,this,"Value minimum");
        valueMax=new XInt("valueMax",255,this,"Value maximum");
        normalizeMin=new XReal("normalizeMin",0.0,this,"Normalize minimum");
        normalizeMax=new XReal("normalizeMax",255.0,this,"Normalize maximum");
        channel=new XInt("channel",0,this,"BGR channel");
        mergeChannels=new XBool("mergeChannels",false,this,"Merge selected channel");
    }

    XInt *conversion=nullptr;
    XInt *hueMin=nullptr;
    XInt *hueMax=nullptr;
    XInt *saturationMin=nullptr;
    XInt *saturationMax=nullptr;
    XInt *valueMin=nullptr;
    XInt *valueMax=nullptr;
    XReal *normalizeMin=nullptr;
    XReal *normalizeMax=nullptr;
    XInt *channel=nullptr;
    XBool *mergeChannels=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageColor:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(OImageColor::Mode mode READ mode WRITE setMode)
public:
    enum Mode { Convert=0,HsvInRange=1,Normalize=2,SplitBgr=3 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageColor(QObject *parent=nullptr);
    ~OImageColor() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    OImageColorParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=Convert;
};
}

#endif // OIMAGECOLOR_H
