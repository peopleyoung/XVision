#ifndef NCLASSIFICATION_H
#define NCLASSIFICATION_H

#include "NOnnxBase.h"
#include "XObjectList.h"
#include "XVisionSharedData.h"

class NOnnxOperatorWdg;

namespace XvCore
{
class NClassificationParam:public XvBaseParam
{
public:
    NClassificationParam()
    {
        inputImage=new XImage("inputImage",QImage(),this,"输入图像");
        topK=new XInt("topK",5,this,"前 K 个结果");
        minScore=new XReal("minScore",0.0,this,"最低匹配分数");
    }
    XImage *inputImage=nullptr;
    XInt *topK=nullptr;
    XReal *minScore=nullptr;
};

class NClassificationResult:public XvBaseResult
{
public:
    NClassificationResult()
    {
        outputImage=new XImage("outputImage",QImage(),this,"输出图像");
        classificationCount=new XInt("classificationCount",0,this,"分类数量");
        classifications=new XObjectList("classifications",XClassificationResult::type(),
                                        this,"分类结果");
    }
    XImage *outputImage=nullptr;
    XInt *classificationCount=nullptr;
    XObjectList *classifications=nullptr;
};

class XVFUNCSYSTEM_EXPORT NClassification:public NOnnxBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(bool applySoftmax READ applySoftmax WRITE setApplySoftmax)
    Q_PROPERTY(QString classNames READ classNames WRITE setClassNames)
    friend class ::NOnnxOperatorWdg;
public:
    enum Mode
    {
        Generic=0,
        Gender=1
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit NClassification(QObject *parent=nullptr);
    ~NClassification() override;
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool applySoftmax() const { return m_applySoftmax; }
    void setApplySoftmax(bool enabled) { m_applySoftmax=enabled; }
    QString classNames() const { return m_classNames; }
    void setClassNames(const QString &names) { m_classNames=names; }
    QStringList persistentPropertyNames() const override;

public slots:
    void onShowFunc() override;

protected:

    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;

private:
    bool validateConfiguration(QString &error) const;
    void clearResults();

    NClassificationParam *m_param=nullptr;
    NClassificationResult *m_result=nullptr;
    NOnnxOperatorWdg *m_widget=nullptr;
    Mode m_mode=Generic;
    bool m_applySoftmax=true;
    QString m_classNames;
};
}

#endif // NCLASSIFICATION_H
