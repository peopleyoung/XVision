#ifndef NSEMANTICSEGMENTATION_H
#define NSEMANTICSEGMENTATION_H

#include "NOnnxBase.h"
#include "XSegmentationResult.h"

class NOnnxOperatorWdg;

namespace XvCore
{
class NSemanticSegmentationParam:public XvBaseParam
{
public:
    NSemanticSegmentationParam()
    {
        inputImage=new XImage("inputImage",QImage(),this,"输入图像");
        binaryThreshold=new XReal("binaryThreshold",0.5,this,"二值化阈值");
        overlayOpacity=new XReal("overlayOpacity",0.5,this,"叠加不透明度");
    }
    XImage *inputImage=nullptr;
    XReal *binaryThreshold=nullptr;
    XReal *overlayOpacity=nullptr;
};

class NSemanticSegmentationResult:public XvBaseResult
{
public:
    NSemanticSegmentationResult()
    {
        segmentation=new XSegmentationResult("segmentation",this,"分割结果");
        colorMask=new XImage("colorMask",QImage(),this,"颜色掩膜");
        overlayImage=new XImage("overlayImage",QImage(),this,"叠加图像");
        confidenceImage=new XImage("confidenceImage",QImage(),this,"置信度图像");
    }
    XSegmentationResult *segmentation=nullptr;
    XImage *colorMask=nullptr;
    XImage *overlayImage=nullptr;
    XImage *confidenceImage=nullptr;
};

class XVFUNCSYSTEM_EXPORT NSemanticSegmentation:public NOnnxBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(XvCore::NOnnxBase::InputLayout outputLayout READ outputLayout WRITE setOutputLayout)
    Q_PROPERTY(QString classNames READ classNames WRITE setClassNames)
    friend class ::NOnnxOperatorWdg;
public:
    enum Mode
    {
        Generic=0,
        Human=1
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit NSemanticSegmentation(QObject *parent=nullptr);
    ~NSemanticSegmentation() override;
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    InputLayout outputLayout() const { return m_outputLayout; }
    void setOutputLayout(InputLayout layout) { m_outputLayout=layout; }
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

    NSemanticSegmentationParam *m_param=nullptr;
    NSemanticSegmentationResult *m_result=nullptr;
    NOnnxOperatorWdg *m_widget=nullptr;
    Mode m_mode=Generic;
    InputLayout m_outputLayout=Auto;
    QString m_classNames;
};
}

#endif // NSEMANTICSEGMENTATION_H
