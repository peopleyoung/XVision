#ifndef OTEMPLATEMATCH_H
#define OTEMPLATEMATCH_H

#include "OpenCvImageOperatorBase.h"

#include "XObjectList.h"
#include "XRotateRectRoi.h"

#include <QByteArray>
#include <QSize>
#include <XObjectBaseType>

namespace XvCore
{
class OTemplateMatchParam:public OpenCvImageParamBase
{
public:
    OTemplateMatchParam();

    XInt *operation=nullptr;
    XRotateRectRoi *templateRoi=nullptr;
    XBool *useTemplateRoi=nullptr;
    XReal *minScore=nullptr;
    XInt *maxMatches=nullptr;
    XReal *maxOverlap=nullptr;
    XReal *angleStart=nullptr;
    XReal *angleExtent=nullptr;
    XReal *angleStep=nullptr;
    XInt *cannyLower=nullptr;
    XInt *cannyUpper=nullptr;
    XInt *maxFeatures=nullptr;
    XReal *featureRatio=nullptr;
    XInt *minimumFeatureMatches=nullptr;
    XInt *minimumInliers=nullptr;
    XReal *ransacReprojectionThreshold=nullptr;
};

class OTemplateMatchResult:public OpenCvImageResultBase
{
public:
    OTemplateMatchResult();

    XRotateRectRoi *templateRoi=nullptr;
    XInt *matchCount=nullptr;
    XObjectList *matches=nullptr;
};

class XVFUNCSYSTEM_EXPORT OTemplateMatch:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(OTemplateMatch::Mode mode READ mode WRITE setMode)
public:
    enum Mode { Base64=0,Feature=1,Shape=2,Hsv=3 };
    Q_ENUM(Mode)
    enum Operation { CreateTemplate=0,FindTemplate=1 };
    Q_ENUM(Operation)

    Q_INVOKABLE explicit OTemplateMatch(QObject *parent=nullptr);
    ~OTemplateMatch() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    bool hasTemplate() const { return !m_templateAsset.isEmpty(); }
    QByteArray templateAsset() const { return m_templateAsset; }
    QSize templateSize() const { return m_templateSize; }
    QByteArray templateSha256() const { return m_templateSha256; }
    bool configureTemplateImage(const QImage &image,QString &error);
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;
    void clearResultsAfterFailure() override;
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;

private:
    OTemplateMatchParam *m_param=nullptr;
    OTemplateMatchResult *m_result=nullptr;
    Mode m_mode=Base64;

    QByteArray m_templateAsset;
    QSize m_templateSize;
    QByteArray m_templateSha256;

    QByteArray m_candidateTemplateAsset;
    QSize m_candidateTemplateSize;
    QByteArray m_candidateTemplateSha256;
    bool m_replaceTemplate=false;
    XRotateRectRoi m_candidateTemplateRoi;
    XObjectList m_candidateMatches{"candidateMatches",XMatchResult::type()};
};
}

#endif // OTEMPLATEMATCH_H
