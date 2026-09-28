#ifndef HMODELMATCH_H
#define HMODELMATCH_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"
#include "XLanguage.h"
#include <QByteArray>

class HModelMatchWdg;
namespace XvCore
{
class HModelMatchParam:public XvBaseParam
{
public:
    HModelMatchParam()
    {
        inputImage=new XImage("inputImage",QImage(),this,getLang("XvFuncSystem_HModelMatch_InputImage","输入图像"));
        templateRoi=new XRotateRectRoi("templateRoi",0.0,0.0,20.0,20.0,0.0,this,getLang("XvFuncSystem_HModelMatch_TemplateRoi","模板区域"));
        useTemplateRoi=new XBool("useTemplateRoi",false,this,getLang("XvFuncSystem_HModelMatch_UseTemplateRoi","使用模板区域"));
        mode=new XInt("mode",0,this,getLang("XvFuncSystem_HModelMatch_Mode","运行模式"));
        minScore=new XReal("minScore",0.5,this,getLang("XvFuncSystem_HModelMatch_MinScore","最小分数"));
        angleStart=new XReal("angleStart",-0.39,this,getLang("XvFuncSystem_HModelMatch_AngleStart","起始角度"));
        angleExtent=new XReal("angleExtent",0.78,this,getLang("XvFuncSystem_HModelMatch_AngleExtent","角度范围"));
        numMatches=new XInt("numMatches",1,this,getLang("XvFuncSystem_HModelMatch_NumMatches","最大结果数"));
        maxOverlap=new XReal("maxOverlap",0.5,this,getLang("XvFuncSystem_HModelMatch_MaxOverlap","最大重叠"));
        greediness=new XReal("greediness",0.9,this,getLang("XvFuncSystem_HModelMatch_Greediness","贪婪度"));
    }
public:
    XImage *inputImage=nullptr;
    XRotateRectRoi *templateRoi=nullptr;
    XBool *useTemplateRoi=nullptr;
    XInt *mode=nullptr;
    XReal *minScore=nullptr;
    XReal *angleStart=nullptr;
    XReal *angleExtent=nullptr;
    XInt *numMatches=nullptr;
    XReal *maxOverlap=nullptr;
    XReal *greediness=nullptr;

};

class HModelMatchResult:public XvBaseResult
{
public:
    HModelMatchResult()
    {
        outputImage=new XImage("outputImage",QImage(),this,getLang("XvFuncSystem_HModelMatch_OutputImage","匹配图像"));
        templateRoi=new XRotateRectRoi("templateRoi",0.0,0.0,20.0,20.0,0.0,this,getLang("XvFuncSystem_HModelMatch_TemplateRoiResult","模板区域"));
        matchCount=new XInt("matchCount",0,this,getLang("XvFuncSystem_HModelMatch_MatchCount","匹配数量"));
        matches=new XObjectList("matches",XMatchResult::type(),this,getLang("XvFuncSystem_HModelMatch_Matches","匹配结果"));
    }
public:
    XImage *outputImage=nullptr;
    XRotateRectRoi *templateRoi=nullptr;
    XInt *matchCount=nullptr;
    XObjectList *matches=nullptr;

};

class XVFUNCSYSTEM_EXPORT HModelMatch:public XvFunc
{
    Q_OBJECT
    friend class ::HModelMatchWdg;
public:
    enum RunMode
    {
        CreateTemplate=0,
        FindTemplate=1
    };
    Q_ENUM(RunMode)

    Q_INVOKABLE explicit HModelMatch(QObject *parent = nullptr);
    ~HModelMatch();
    bool hasTemplateModel() const { return !m_modelAsset.isEmpty(); }
    QByteArray templateModelAsset() const { return m_modelAsset; }
public slots:
    void onShowFunc() override;
protected:
    QPixmap funcIcon() override { return QPixmap(":/images/HModelMatch.svg");}
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return param;};
    XvBaseResult *getResult() const override { return result;};
    QStringList optionalPersistentParameterNames() const override
    {
        return {"templateRoi","useTemplateRoi","mode","minScore","angleStart",
                "angleExtent","numMatches","maxOverlap","greediness"};
    }
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;
protected:
    HModelMatchParam *param=nullptr;
    HModelMatchResult *result=nullptr;
    HModelMatchWdg* m_frm=nullptr;
    QByteArray m_modelAsset;




};
}



#endif // HMODELMATCH_H
