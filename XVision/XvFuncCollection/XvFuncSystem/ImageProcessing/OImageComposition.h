#ifndef OIMAGECOMPOSITION_H
#define OIMAGECOMPOSITION_H

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include <XObjectBaseType>

namespace XvCore
{
class OImageCompositionParam:public OpenCvImageParamBase
{
public:
    OImageCompositionParam()
    {
        backgroundImage=new XImage("backgroundImage",QImage(),this,"Background image");
        maskImage=new XImage("maskImage",QImage(),this,"Mask image");
        images=new XObjectList("images",XImage::type(),this,"Additional images");
        backgroundPath=new XString("backgroundPath",QString(),this,"Background path");
        centerX=new XInt("centerX",-1,this,"Center X");
        centerY=new XInt("centerY",-1,this,"Center Y");
        blurBackground=new XBool("blurBackground",false,this,"Blur background");
        stitchMode=new XInt("stitchMode",0,this,"Stitch mode");
    }

    XImage *backgroundImage=nullptr;
    XImage *maskImage=nullptr;
    XObjectList *images=nullptr;
    XString *backgroundPath=nullptr;
    XInt *centerX=nullptr;
    XInt *centerY=nullptr;
    XBool *blurBackground=nullptr;
    XInt *stitchMode=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageComposition:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Background=0,SeamlessClone=1,Stitching=2 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageComposition(QObject *parent=nullptr);
    ~OImageComposition() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    bool backgroundImage(QImage &image,QString &error) const;

    OImageCompositionParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=Background;
};
}

#endif // OIMAGECOMPOSITION_H
