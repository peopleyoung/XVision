#ifndef OPOINTFEATURE_H
#define OPOINTFEATURE_H

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include "XVisionSharedData.h"
#include <XObjectBaseType>

namespace XvCore
{
class OPointFeatureParam:public OpenCvImageParamBase
{
public:
    OPointFeatureParam();

    XInt *maxFeatures=nullptr;
    XReal *qualityLevel=nullptr;
    XReal *minDistance=nullptr;
    XInt *blockSize=nullptr;
    XReal *harrisK=nullptr;
    XInt *subpixelWindow=nullptr;
    XInt *subpixelMaxIterations=nullptr;
    XReal *subpixelEpsilon=nullptr;

    XInt *akazeDescriptorType=nullptr;
    XInt *descriptorSize=nullptr;
    XInt *descriptorChannels=nullptr;
    XReal *featureThreshold=nullptr;
    XInt *octaves=nullptr;
    XInt *octaveLayers=nullptr;
    XInt *diffusivity=nullptr;
    XBool *extended=nullptr;
    XBool *upright=nullptr;

    XInt *briskThreshold=nullptr;
    XInt *briskOctaves=nullptr;
    XReal *briskPatternScale=nullptr;
    XInt *fastThreshold=nullptr;
    XBool *nonmaxSuppression=nullptr;

    XReal *orbScaleFactor=nullptr;
    XInt *orbLevels=nullptr;
    XInt *orbEdgeThreshold=nullptr;
    XInt *orbFirstLevel=nullptr;
    XInt *orbWtaK=nullptr;
    XInt *orbScoreType=nullptr;
    XInt *orbPatchSize=nullptr;
    XBool *orientationNormalized=nullptr;
    XBool *scaleNormalized=nullptr;
    XReal *freakPatternScale=nullptr;
    XInt *freakOctaves=nullptr;

    XInt *mserDelta=nullptr;
    XInt *mserMinArea=nullptr;
    XInt *mserMaxArea=nullptr;
    XReal *mserMaxVariation=nullptr;
    XReal *mserMinDiversity=nullptr;
    XInt *mserMaxEvolution=nullptr;
    XReal *mserAreaThreshold=nullptr;
    XReal *mserMinMargin=nullptr;
    XInt *mserEdgeBlurSize=nullptr;

    XInt *starMaxSize=nullptr;
    XInt *starResponseThreshold=nullptr;
    XInt *starLineThresholdProjected=nullptr;
    XInt *starLineThresholdBinarized=nullptr;
    XInt *starSuppressNonmaxSize=nullptr;
};

class OPointFeatureResult:public OpenCvImageResultBase
{
public:
    OPointFeatureResult()
    {
        keyPoints=new XObjectList("keyPoints",XKeyPoint::type(),this,"Key points");
        count=new XInt("count",0,this,"Feature count");
    }

    XObjectList *keyPoints=nullptr;
    XInt *count=nullptr;
};

class XVFUNCSYSTEM_EXPORT OPointFeature:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Harris=0,Subpixel=1,Akaze=2,Brisk=3,Fast=4,Freak=5,Kaze=6,Mser=7,Star=8 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OPointFeature(QObject *parent=nullptr);
    ~OPointFeature() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    OPointFeatureParam *m_param=nullptr;
    OPointFeatureResult *m_result=nullptr;
    Mode m_mode=Harris;
    XObjectList m_candidateKeyPoints{"candidateKeyPoints",XKeyPoint::type()};
    int m_candidateCount=0;
};
}

#endif // OPOINTFEATURE_H
