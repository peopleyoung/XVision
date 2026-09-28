#ifndef OIMAGETRANSFORM_H
#define OIMAGETRANSFORM_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

#include <QVector>

namespace XvCore
{
class OImageTransformParam:public OpenCvImageParamBase
{
public:
    OImageTransformParam();

    XInt *flipCode=nullptr;
    XInt *repeatX=nullptr;
    XInt *repeatY=nullptr;
    XInt *outputWidth=nullptr;
    XInt *outputHeight=nullptr;
    XReal *scaleX=nullptr;
    XReal *scaleY=nullptr;
    XInt *interpolation=nullptr;
    XInt *rotateCode=nullptr;
    XBool *normalizedPoints=nullptr;
    QVector<XReal*> sourceX;
    QVector<XReal*> sourceY;
    QVector<XReal*> destinationX;
    QVector<XReal*> destinationY;
};

class XVFUNCSYSTEM_EXPORT OImageTransform:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(OImageTransform::Mode mode READ mode WRITE setMode)
public:
    enum Mode
    {
        Flip=0,Homography=1,Repeat=2,Resize=3,Rotate=4,Transpose=5,
        WarpAffine=6,WarpPerspective=7
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageTransform(QObject *parent=nullptr);
    ~OImageTransform() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    QStringList pointParameterNames(int count) const;

    OImageTransformParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=Flip;
};
}

#endif // OIMAGETRANSFORM_H
