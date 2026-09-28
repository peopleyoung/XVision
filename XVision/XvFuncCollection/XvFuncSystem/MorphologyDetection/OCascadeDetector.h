#ifndef OCASCADEDETECTOR_H
#define OCASCADEDETECTOR_H

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include "XVisionSharedData.h"
#include <XObjectBaseType>

#include <memory>

namespace XvCore
{
class OCascadeDetectorParam:public OpenCvImageParamBase
{
public:
    OCascadeDetectorParam()
    {
        cascadePath=new XString("cascadePath",QString(),this,"Cascade path");
        scaleFactor=new XReal("scaleFactor",1.1,this,"Scale factor");
        minNeighbors=new XInt("minNeighbors",3,this,"Minimum neighbors");
        flags=new XInt("flags",0,this,"Detection flags");
        minWidth=new XInt("minWidth",0,this,"Minimum width");
        minHeight=new XInt("minHeight",0,this,"Minimum height");
        maxWidth=new XInt("maxWidth",0,this,"Maximum width");
        maxHeight=new XInt("maxHeight",0,this,"Maximum height");
    }

    XString *cascadePath=nullptr;
    XReal *scaleFactor=nullptr;
    XInt *minNeighbors=nullptr;
    XInt *flags=nullptr;
    XInt *minWidth=nullptr;
    XInt *minHeight=nullptr;
    XInt *maxWidth=nullptr;
    XInt *maxHeight=nullptr;
};

class OCascadeDetectorResult:public OpenCvImageResultBase
{
public:
    OCascadeDetectorResult()
    {
        rectangles=new XObjectList("rectangles",XRect2D::type(),this,"Rectangles");
        count=new XInt("count",0,this,"Detection count");
    }

    XObjectList *rectangles=nullptr;
    XInt *count=nullptr;
};

class XVFUNCSYSTEM_EXPORT OCascadeDetector:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Haar=0,Lbp=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OCascadeDetector(QObject *parent=nullptr);
    ~OCascadeDetector() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    OCascadeDetectorParam *m_param=nullptr;
    OCascadeDetectorResult *m_result=nullptr;
    Mode m_mode=Haar;
    XObjectList m_candidateRectangles{"candidateRectangles",XRect2D::type()};
    int m_candidateCount=0;
};
}

#endif // OCASCADEDETECTOR_H
