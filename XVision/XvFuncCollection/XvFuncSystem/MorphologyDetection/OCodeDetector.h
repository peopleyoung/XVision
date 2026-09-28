#ifndef OCODEDETECTOR_H
#define OCODEDETECTOR_H

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include "XPoint2D.h"
#include <XObjectBaseType>

namespace XvCore
{
class OCodeDetectorParam:public OpenCvImageParamBase
{
};

class OCodeDetectorResult:public OpenCvImageResultBase
{
public:
    OCodeDetectorResult()
    {
        text=new XString("text",QString(),this,"Decoded text");
        corners=new XObjectList("corners",XPoint2D::type(),this,"Code corners");
    }

    XString *text=nullptr;
    XObjectList *corners=nullptr;
};

class XVFUNCSYSTEM_EXPORT OCodeDetector:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(OCodeDetector::Mode mode READ mode WRITE setMode)
public:
    enum Mode { QrCode=0 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OCodeDetector(QObject *parent=nullptr);
    ~OCodeDetector() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override { return {"inputImage"}; }

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    OCodeDetectorParam *m_param=nullptr;
    OCodeDetectorResult *m_result=nullptr;
    Mode m_mode=QrCode;
    QString m_candidateText;
    XObjectList m_candidateCorners{"candidateCorners",XPoint2D::type()};
};
}

#endif // OCODEDETECTOR_H
