#ifndef NOBJECTDETECTION_H
#define NOBJECTDETECTION_H

#include "NOnnxBase.h"
#include "XDetectionResult.h"
#include "XObjectList.h"

class NOnnxOperatorWdg;

namespace XvCore
{
class NObjectDetectionParam:public XvBaseParam
{
public:
    NObjectDetectionParam()
    {
        inputImage=new XImage("inputImage",QImage(),this,"Input image");
        confidenceThreshold=new XReal("confidenceThreshold",0.25,this,
                                      "Confidence threshold");
        iouThreshold=new XReal("iouThreshold",0.45,this,"IoU threshold");
        classAgnosticNms=new XBool("classAgnosticNms",false,this,
                                   "Class agnostic NMS");
        maximumDetections=new XInt("maximumDetections",300,this,
                                   "Maximum detections");
    }
    XImage *inputImage=nullptr;
    XReal *confidenceThreshold=nullptr;
    XReal *iouThreshold=nullptr;
    XBool *classAgnosticNms=nullptr;
    XInt *maximumDetections=nullptr;
};

class NObjectDetectionResult:public XvBaseResult
{
public:
    NObjectDetectionResult()
    {
        outputImage=new XImage("outputImage",QImage(),this,"Output image");
        detectionCount=new XInt("detectionCount",0,this,"Detection count");
        detections=new XObjectList("detections",XDetectionResult::type(),
                                   this,"Detections");
    }
    XImage *outputImage=nullptr;
    XInt *detectionCount=nullptr;
    XObjectList *detections=nullptr;
};

class XVFUNCSYSTEM_EXPORT NObjectDetection:public NOnnxBase
{
    Q_OBJECT
    Q_PROPERTY(NObjectDetection::Mode mode READ mode WRITE setMode)
    Q_PROPERTY(bool normalizedCoordinates READ normalizedCoordinates WRITE setNormalizedCoordinates)
    Q_PROPERTY(QString classNames READ classNames WRITE setClassNames)
    friend class ::NOnnxOperatorWdg;
public:
    enum Mode
    {
        Generic=0,
        Yolov3=1,
        Yolov5=2,
        Yolov5Face=3
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit NObjectDetection(QObject *parent=nullptr);
    ~NObjectDetection() override;
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool normalizedCoordinates() const { return m_normalizedCoordinates; }
    void setNormalizedCoordinates(bool normalized) { m_normalizedCoordinates=normalized; }
    QString classNames() const { return m_classNames; }
    void setClassNames(const QString &names) { m_classNames=names; }
    QStringList persistentPropertyNames() const override;

public slots:
    void onShowFunc() override;

protected:
    QPixmap funcIcon() override { return QPixmap(":/images/HObjectDetection.svg"); }
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;

private:
    bool validateConfiguration(QString &error) const;
    void clearResults();

    NObjectDetectionParam *m_param=nullptr;
    NObjectDetectionResult *m_result=nullptr;
    NOnnxOperatorWdg *m_widget=nullptr;
    Mode m_mode=Generic;
    bool m_normalizedCoordinates=false;
    QString m_classNames;
};
}

#endif // NOBJECTDETECTION_H
