#ifndef HSEMANTICSEGMENTATION_H
#define HSEMANTICSEGMENTATION_H

#include "XVFuncSystemGlobal.h"
#include "XLanguage.h"
#include "XSegmentationResult.h"
#include "XvFunc.h"

#include <QScopedPointer>

class HSemanticSegmentationWdg;

namespace XvCore
{
class HSemanticSegmentationPrivate;

class HSemanticSegmentationParam : public XvBaseParam
{
public:
    HSemanticSegmentationParam()
    {
        inputImage=new XImage(
                    "inputImage",QImage(),this,
                    getLang("XvFuncSystem_HSemanticSegmentation_InputImage",
                            "输入图像"));
    }

    XImage *inputImage=nullptr;
};

class HSemanticSegmentationResult : public XvBaseResult
{
public:
    HSemanticSegmentationResult()
    {
        segmentation=new XSegmentationResult(
                    "segmentation",this,
                    getLang("XvFuncSystem_HSemanticSegmentation_Result",
                            "分割结果"));
        colorMask=new XImage(
                    "colorMask",QImage(),this,
                    getLang("XvFuncSystem_HSemanticSegmentation_ColorMask",
                            "彩色掩膜"));
        overlayImage=new XImage(
                    "overlayImage",QImage(),this,
                    getLang("XvFuncSystem_HSemanticSegmentation_OverlayImage",
                            "分割叠加图"));
        confidenceImage=new XImage(
                    "confidenceImage",QImage(),this,
                    getLang("XvFuncSystem_HSemanticSegmentation_ConfidenceImage",
                            "置信度图"));
    }

    XSegmentationResult *segmentation=nullptr;
    XImage *colorMask=nullptr;
    XImage *overlayImage=nullptr;
    XImage *confidenceImage=nullptr;
};

class XVFUNCSYSTEM_EXPORT HSemanticSegmentation : public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath)
    Q_PROPERTY(QString preprocessPath READ preprocessPath WRITE setPreprocessPath)
    Q_PROPERTY(HSemanticSegmentation::Runtime runtime READ runtime WRITE setRuntime)
    Q_PROPERTY(double overlayOpacity READ overlayOpacity WRITE setOverlayOpacity)
    friend class ::HSemanticSegmentationWdg;

public:
    enum Runtime
    {
        Cpu=0,
        Gpu=1
    };
    Q_ENUM(Runtime)

    Q_INVOKABLE explicit HSemanticSegmentation(QObject *parent=nullptr);
    ~HSemanticSegmentation() override;

    QString modelPath() const { return m_modelPath; }
    void setModelPath(const QString &path);
    QString preprocessPath() const { return m_preprocessPath; }
    void setPreprocessPath(const QString &path);
    Runtime runtime() const { return m_runtime; }
    void setRuntime(Runtime runtime);
    double overlayOpacity() const { return m_overlayOpacity; }
    void setOverlayOpacity(double opacity) { m_overlayOpacity=opacity; }

    bool configureAssets(const QString &modelPath,const QString &preprocessPath,
                         QString &error);
    bool hasConfiguredAssets() const;
    QString assetSummary() const;
    QStringList classSummary() const;

    QStringList persistentPropertyNames() const override
    {
        return {"modelPath","preprocessPath","runtime","overlayOpacity"};
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

    HSemanticSegmentationParam *param=nullptr;
    HSemanticSegmentationResult *result=nullptr;
    HSemanticSegmentationWdg *m_frm=nullptr;
    QString m_modelPath;
    QString m_preprocessPath;
    Runtime m_runtime=Cpu;
    double m_overlayOpacity=0.45;
    QScopedPointer<HSemanticSegmentationPrivate> d;
};
}

#endif // HSEMANTICSEGMENTATION_H
