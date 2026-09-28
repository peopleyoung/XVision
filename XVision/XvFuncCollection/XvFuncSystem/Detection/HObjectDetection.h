#ifndef HOBJECTDETECTION_H
#define HOBJECTDETECTION_H

#include "XVFuncSystemGlobal.h"
#include "XDetectionResult.h"
#include "XLanguage.h"
#include "XvFunc.h"

#include <QScopedPointer>

class HObjectDetectionWdg;

namespace XvCore
{
class HObjectDetectionPrivate;

class HObjectDetectionParam : public XvBaseParam
{
public:
    HObjectDetectionParam()
    {
        inputImage=new XImage(
                    "inputImage",QImage(),this,
                    getLang("XvFuncSystem_HObjectDetection_InputImage",
                            "输入图像"));
        minConfidence=new XReal(
                    "minConfidence",0.5,this,
                    getLang("XvFuncSystem_HObjectDetection_MinConfidence",
                            "最小置信度"));
        maxOverlap=new XReal(
                    "maxOverlap",0.5,this,
                    getLang("XvFuncSystem_HObjectDetection_MaxOverlap",
                            "最大重叠"));
        maxOverlapClassAgnostic=new XBool(
                    "maxOverlapClassAgnostic",false,this,
                    getLang("XvFuncSystem_HObjectDetection_ClassAgnostic",
                            "跨类别抑制"));
        maxNumDetections=new XInt(
                    "maxNumDetections",100,this,
                    getLang("XvFuncSystem_HObjectDetection_MaxDetections",
                            "最大检测数"));
    }

    XImage *inputImage=nullptr;
    XReal *minConfidence=nullptr;
    XReal *maxOverlap=nullptr;
    XBool *maxOverlapClassAgnostic=nullptr;
    XInt *maxNumDetections=nullptr;
};

class HObjectDetectionResult : public XvBaseResult
{
public:
    HObjectDetectionResult()
    {
        outputImage=new XImage(
                    "outputImage",QImage(),this,
                    getLang("XvFuncSystem_HObjectDetection_OutputImage",
                            "检测图像"));
        detectionCount=new XInt(
                    "detectionCount",0,this,
                    getLang("XvFuncSystem_HObjectDetection_DetectionCount",
                            "检测数量"));
        detections=new XObjectList(
                    "detections",XDetectionResult::type(),this,
                    getLang("XvFuncSystem_HObjectDetection_Detections",
                            "检测结果"));
    }

    XImage *outputImage=nullptr;
    XInt *detectionCount=nullptr;
    XObjectList *detections=nullptr;
};

class XVFUNCSYSTEM_EXPORT HObjectDetection : public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath)
    Q_PROPERTY(QString preprocessPath READ preprocessPath WRITE setPreprocessPath)
    Q_PROPERTY(Runtime runtime READ runtime WRITE setRuntime)
    friend class ::HObjectDetectionWdg;

public:
    enum Runtime
    {
        Cpu=0,
        Gpu=1
    };
    Q_ENUM(Runtime)

    Q_INVOKABLE explicit HObjectDetection(QObject *parent=nullptr);
    ~HObjectDetection() override;

    QString modelPath() const { return m_modelPath; }
    void setModelPath(const QString &path);
    QString preprocessPath() const { return m_preprocessPath; }
    void setPreprocessPath(const QString &path);
    Runtime runtime() const { return m_runtime; }
    void setRuntime(Runtime runtime);

    bool configureAssets(const QString &modelPath,const QString &preprocessPath,
                         QString &error);
    bool hasConfiguredAssets() const;
    QString assetSummary() const;
    QStringList classSummary() const;

    QStringList persistentPropertyNames() const override
    {
        return {"modelPath","preprocessPath","runtime"};
    }

public slots:
    void onShowFunc() override;

protected:
    QPixmap funcIcon() override;
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return param; }
    XvBaseResult *getResult() const override { return result; }
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,
                            QString &error) override;

private:
    void clearResults();

    HObjectDetectionParam *param=nullptr;
    HObjectDetectionResult *result=nullptr;
    HObjectDetectionWdg *m_frm=nullptr;
    QString m_modelPath;
    QString m_preprocessPath;
    Runtime m_runtime=Cpu;
    QScopedPointer<HObjectDetectionPrivate> d;
};
}

#endif // HOBJECTDETECTION_H
