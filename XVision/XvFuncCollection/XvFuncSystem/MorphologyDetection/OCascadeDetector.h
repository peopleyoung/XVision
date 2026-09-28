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
        cascadePath=new XString("cascadePath",QString(),this,"级联模型路径");
        scaleFactor=new XReal("scaleFactor",1.1,this,"缩放系数");
        minNeighbors=new XInt("minNeighbors",3,this,"最小邻居数");
        flags=new XInt("flags",0,this,"检测标志");
        minWidth=new XInt("minWidth",0,this,"最小宽度");
        minHeight=new XInt("minHeight",0,this,"最小高度");
        maxWidth=new XInt("maxWidth",0,this,"最大宽度");
        maxHeight=new XInt("maxHeight",0,this,"最大高度");
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
        rectangles=new XObjectList("rectangles",XRect2D::type(),this,"矩形集合");
        count=new XInt("count",0,this,"检测数量");
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
