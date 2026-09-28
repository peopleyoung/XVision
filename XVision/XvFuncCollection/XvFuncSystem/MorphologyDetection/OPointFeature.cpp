#include "OPointFeature.h"

#include "OpenCvImageUtils.h"

#include <algorithm>
#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/xfeatures2d.hpp>
#endif

using namespace XvCore;

OPointFeatureParam::OPointFeatureParam()
{
    maxFeatures=new XInt("maxFeatures",500,this,"Maximum features");
    qualityLevel=new XReal("qualityLevel",0.01,this,"Quality level");
    minDistance=new XReal("minDistance",5.0,this,"Minimum distance");
    blockSize=new XInt("blockSize",3,this,"Block size");
    harrisK=new XReal("harrisK",0.04,this,"Harris K");
    subpixelWindow=new XInt("subpixelWindow",5,this,"Subpixel window");
    subpixelMaxIterations=new XInt("subpixelMaxIterations",40,this,"Subpixel iterations");
    subpixelEpsilon=new XReal("subpixelEpsilon",0.001,this,"Subpixel epsilon");

    akazeDescriptorType=new XInt("akazeDescriptorType",2,this,"AKAZE descriptor type");
    descriptorSize=new XInt("descriptorSize",0,this,"Descriptor size");
    descriptorChannels=new XInt("descriptorChannels",3,this,"Descriptor channels");
    featureThreshold=new XReal("featureThreshold",0.001,this,"Feature threshold");
    octaves=new XInt("octaves",4,this,"Octaves");
    octaveLayers=new XInt("octaveLayers",4,this,"Octave layers");
    diffusivity=new XInt("diffusivity",1,this,"Diffusivity");
    extended=new XBool("extended",false,this,"Extended descriptor");
    upright=new XBool("upright",false,this,"Upright descriptor");

    briskThreshold=new XInt("briskThreshold",30,this,"BRISK threshold");
    briskOctaves=new XInt("briskOctaves",3,this,"BRISK octaves");
    briskPatternScale=new XReal("briskPatternScale",1.0,this,"BRISK pattern scale");
    fastThreshold=new XInt("fastThreshold",20,this,"FAST threshold");
    nonmaxSuppression=new XBool("nonmaxSuppression",true,this,"Non-maximum suppression");

    orbScaleFactor=new XReal("orbScaleFactor",1.2,this,"ORB scale factor");
    orbLevels=new XInt("orbLevels",8,this,"ORB levels");
    orbEdgeThreshold=new XInt("orbEdgeThreshold",31,this,"ORB edge threshold");
    orbFirstLevel=new XInt("orbFirstLevel",0,this,"ORB first level");
    orbWtaK=new XInt("orbWtaK",2,this,"ORB WTA K");
    orbScoreType=new XInt("orbScoreType",0,this,"ORB score type");
    orbPatchSize=new XInt("orbPatchSize",31,this,"ORB patch size");
    orientationNormalized=new XBool("orientationNormalized",true,this,"Normalize orientation");
    scaleNormalized=new XBool("scaleNormalized",true,this,"Normalize scale");
    freakPatternScale=new XReal("freakPatternScale",22.0,this,"FREAK pattern scale");
    freakOctaves=new XInt("freakOctaves",4,this,"FREAK octaves");

    mserDelta=new XInt("mserDelta",5,this,"MSER delta");
    mserMinArea=new XInt("mserMinArea",60,this,"MSER minimum area");
    mserMaxArea=new XInt("mserMaxArea",14400,this,"MSER maximum area");
    mserMaxVariation=new XReal("mserMaxVariation",0.25,this,"MSER maximum variation");
    mserMinDiversity=new XReal("mserMinDiversity",0.2,this,"MSER minimum diversity");
    mserMaxEvolution=new XInt("mserMaxEvolution",200,this,"MSER maximum evolution");
    mserAreaThreshold=new XReal("mserAreaThreshold",1.01,this,"MSER area threshold");
    mserMinMargin=new XReal("mserMinMargin",0.003,this,"MSER minimum margin");
    mserEdgeBlurSize=new XInt("mserEdgeBlurSize",5,this,"MSER edge blur size");

    starMaxSize=new XInt("starMaxSize",45,this,"STAR maximum size");
    starResponseThreshold=new XInt("starResponseThreshold",30,this,"STAR response threshold");
    starLineThresholdProjected=new XInt("starLineThresholdProjected",10,this,
                                        "STAR projected line threshold");
    starLineThresholdBinarized=new XInt("starLineThresholdBinarized",8,this,
                                        "STAR binarized line threshold");
    starSuppressNonmaxSize=new XInt("starSuppressNonmaxSize",5,this,
                                    "STAR suppression size");
}

OPointFeature::OPointFeature(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OPointFeatureParam()),
      m_result(new OPointFeatureResult())
{
    _funcRole="OPointFeature";
    _funcName="OpenCV Point Feature";
}

OPointFeature::~OPointFeature()
{
    delete m_param;
    delete m_result;
}

QStringList OPointFeature::activeParameterNames() const
{
    switch(m_mode)
    {
    case Harris:
        return {"inputImage","maxFeatures","qualityLevel","minDistance","blockSize","harrisK"};
    case Subpixel:
        return {"inputImage","maxFeatures","qualityLevel","minDistance","blockSize",
                "subpixelWindow","subpixelMaxIterations","subpixelEpsilon"};
    case Akaze:
        return {"inputImage","akazeDescriptorType","descriptorSize","descriptorChannels",
                "featureThreshold","octaves","octaveLayers","diffusivity"};
    case Brisk:
        return {"inputImage","briskThreshold","briskOctaves","briskPatternScale"};
    case Fast:
        return {"inputImage","fastThreshold","nonmaxSuppression"};
    case Freak:
        return {"inputImage","maxFeatures","orbScaleFactor","orbLevels","orbEdgeThreshold",
                "orbFirstLevel","orbWtaK","orbScoreType","orbPatchSize","fastThreshold",
                "orientationNormalized","scaleNormalized","freakPatternScale","freakOctaves"};
    case Kaze:
        return {"inputImage","extended","upright","featureThreshold","octaves",
                "octaveLayers","diffusivity"};
    case Mser:
        return {"inputImage","mserDelta","mserMinArea","mserMaxArea","mserMaxVariation",
                "mserMinDiversity","mserMaxEvolution","mserAreaThreshold","mserMinMargin",
                "mserEdgeBlurSize"};
    case Star:
        return {"inputImage","starMaxSize","starResponseThreshold",
                "starLineThresholdProjected","starLineThresholdBinarized",
                "starSuppressNonmaxSize"};
    }
    return {"inputImage"};
}

bool OPointFeature::commitAdditionalResults(QString &error)
{
    if(!m_result->keyPoints->setData(&m_candidateKeyPoints))
    {
        error="Point-feature candidate list is invalid";
        return false;
    }
    m_result->count->setValue(m_candidateCount);
    error.clear();
    return true;
}

bool OPointFeature::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(static_cast<int>(m_mode)<Harris || static_cast<int>(m_mode)>Star)
    {
        error="Point-feature mode is invalid";
        return false;
    }
    cv::Mat input,gray,annotated;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)
            || !OpenCvImageUtils::toBgr(input,annotated,error)) return false;
    std::vector<cv::KeyPoint> detected;
    try
    {
        if(m_mode==Harris || m_mode==Subpixel)
        {
            if(m_param->maxFeatures->value()<=0 || m_param->qualityLevel->value()<=0.0
                    || m_param->qualityLevel->value()>1.0 || m_param->minDistance->value()<0.0
                    || m_param->blockSize->value()<2
                    || (m_mode==Harris
                        && (m_param->harrisK->value()<=0.0 || m_param->harrisK->value()>=1.0)))
            {
                error="Corner feature count, quality, distance, block size, or Harris K is invalid";
                return false;
            }
            std::vector<cv::Point2f> corners;
            cv::goodFeaturesToTrack(gray,corners,m_param->maxFeatures->value(),
                                    m_param->qualityLevel->value(),m_param->minDistance->value(),
                                    cv::Mat(),m_param->blockSize->value(),m_mode==Harris,
                                    m_param->harrisK->value());
            if(m_mode==Subpixel && !corners.empty())
            {
                const int window=m_param->subpixelWindow->value();
                if(window<=0 || m_param->subpixelMaxIterations->value()<=0
                        || m_param->subpixelEpsilon->value()<=0.0
                        || window*2+1>gray.cols || window*2+1>gray.rows)
                {
                    error="Subpixel window, iterations, epsilon, or image size is invalid";
                    return false;
                }
                cv::cornerSubPix(gray,corners,cv::Size(window,window),cv::Size(-1,-1),
                                 cv::TermCriteria(cv::TermCriteria::EPS|cv::TermCriteria::MAX_ITER,
                                                  m_param->subpixelMaxIterations->value(),
                                                  m_param->subpixelEpsilon->value()));
            }
            for(const cv::Point2f &point:corners)
                detected.emplace_back(point,static_cast<float>(m_param->blockSize->value()),
                                      -1.0F,0.0F,0,-1);
        }
        else if(m_mode==Akaze || m_mode==Kaze)
        {
            if(m_param->featureThreshold->value()<=0.0 || m_param->octaves->value()<=0
                    || m_param->octaveLayers->value()<=0 || m_param->diffusivity->value()<0
                    || m_param->diffusivity->value()>3)
            {
                error="KAZE threshold, octaves, layers, or diffusivity is invalid";
                return false;
            }
            if(m_mode==Akaze)
            {
                if(m_param->akazeDescriptorType->value()<0
                        || m_param->akazeDescriptorType->value()>3
                        || m_param->descriptorSize->value()<0
                        || m_param->descriptorChannels->value()<1
                        || m_param->descriptorChannels->value()>3)
                {
                    error="AKAZE descriptor type, size, or channels is invalid";
                    return false;
                }
                const cv::AKAZE::DescriptorType descriptors[]={
                    cv::AKAZE::DESCRIPTOR_KAZE,cv::AKAZE::DESCRIPTOR_KAZE_UPRIGHT,
                    cv::AKAZE::DESCRIPTOR_MLDB,cv::AKAZE::DESCRIPTOR_MLDB_UPRIGHT};
                cv::AKAZE::create(descriptors[m_param->akazeDescriptorType->value()],
                                  m_param->descriptorSize->value(),
                                  m_param->descriptorChannels->value(),
                                  static_cast<float>(m_param->featureThreshold->value()),
                                  m_param->octaves->value(),m_param->octaveLayers->value(),
                                  static_cast<cv::KAZE::DiffusivityType>(
                                      m_param->diffusivity->value()))->detect(gray,detected);
            }
            else
            {
                cv::KAZE::create(m_param->extended->value(),m_param->upright->value(),
                                 static_cast<float>(m_param->featureThreshold->value()),
                                 m_param->octaves->value(),m_param->octaveLayers->value(),
                                 static_cast<cv::KAZE::DiffusivityType>(
                                     m_param->diffusivity->value()))->detect(gray,detected);
            }
        }
        else if(m_mode==Brisk)
        {
            if(m_param->briskThreshold->value()<0 || m_param->briskOctaves->value()<0
                    || m_param->briskPatternScale->value()<=0.0)
            {
                error="BRISK threshold, octaves, or pattern scale is invalid";
                return false;
            }
            cv::BRISK::create(m_param->briskThreshold->value(),m_param->briskOctaves->value(),
                              static_cast<float>(m_param->briskPatternScale->value()))
                    ->detect(gray,detected);
        }
        else if(m_mode==Fast)
        {
            if(m_param->fastThreshold->value()<0)
            {
                error="FAST threshold must be non-negative";
                return false;
            }
            cv::FastFeatureDetector::create(m_param->fastThreshold->value(),
                                             m_param->nonmaxSuppression->value())
                    ->detect(gray,detected);
        }
        else if(m_mode==Freak)
        {
            if(m_param->maxFeatures->value()<=0 || m_param->orbScaleFactor->value()<=1.0
                    || m_param->orbLevels->value()<=0 || m_param->orbEdgeThreshold->value()<0
                    || m_param->orbFirstLevel->value()<0
                    || (m_param->orbWtaK->value()!=2 && m_param->orbWtaK->value()!=3
                        && m_param->orbWtaK->value()!=4)
                    || m_param->orbScoreType->value()<0 || m_param->orbScoreType->value()>1
                    || m_param->orbPatchSize->value()<=0 || m_param->fastThreshold->value()<0
                    || m_param->freakPatternScale->value()<=0.0
                    || m_param->freakOctaves->value()<=0)
            {
                error="ORB/FREAK detector parameters are invalid";
                return false;
            }
            const cv::ORB::ScoreType score=m_param->orbScoreType->value()==0
                    ?cv::ORB::HARRIS_SCORE:cv::ORB::FAST_SCORE;
            cv::ORB::create(m_param->maxFeatures->value(),
                            static_cast<float>(m_param->orbScaleFactor->value()),
                            m_param->orbLevels->value(),m_param->orbEdgeThreshold->value(),
                            m_param->orbFirstLevel->value(),m_param->orbWtaK->value(),score,
                            m_param->orbPatchSize->value(),m_param->fastThreshold->value())
                    ->detect(gray,detected);
            cv::Mat descriptors;
            cv::xfeatures2d::FREAK::create(m_param->orientationNormalized->value(),
                                           m_param->scaleNormalized->value(),
                                           static_cast<float>(m_param->freakPatternScale->value()),
                                           m_param->freakOctaves->value())
                    ->compute(gray,detected,descriptors);
        }
        else if(m_mode==Mser)
        {
            if(m_param->mserDelta->value()<=0 || m_param->mserMinArea->value()<=0
                    || m_param->mserMinArea->value()>m_param->mserMaxArea->value()
                    || m_param->mserMaxVariation->value()<0.0
                    || m_param->mserMinDiversity->value()<0.0
                    || m_param->mserMaxEvolution->value()<=0
                    || m_param->mserAreaThreshold->value()<=0.0
                    || m_param->mserMinMargin->value()<0.0
                    || m_param->mserEdgeBlurSize->value()<0)
            {
                error="MSER area, variation, diversity, evolution, or blur parameters are invalid";
                return false;
            }
            cv::MSER::create(m_param->mserDelta->value(),m_param->mserMinArea->value(),
                             m_param->mserMaxArea->value(),m_param->mserMaxVariation->value(),
                             m_param->mserMinDiversity->value(),m_param->mserMaxEvolution->value(),
                             m_param->mserAreaThreshold->value(),m_param->mserMinMargin->value(),
                             m_param->mserEdgeBlurSize->value())->detect(gray,detected);
        }
        else
        {
            if(m_param->starMaxSize->value()<=0 || m_param->starResponseThreshold->value()<0
                    || m_param->starLineThresholdProjected->value()<0
                    || m_param->starLineThresholdBinarized->value()<0
                    || m_param->starSuppressNonmaxSize->value()<=0)
            {
                error="STAR size or threshold parameters are invalid";
                return false;
            }
            cv::xfeatures2d::StarDetector::create(
                        m_param->starMaxSize->value(),m_param->starResponseThreshold->value(),
                        m_param->starLineThresholdProjected->value(),
                        m_param->starLineThresholdBinarized->value(),
                        m_param->starSuppressNonmaxSize->value())->detect(gray,detected);
        }

        XObjectList keyPoints("candidateKeyPoints",XKeyPoint::type());
        for(const cv::KeyPoint &point:detected)
        {
            if(!keyPoints.addValue(new XKeyPoint(point.pt.x,point.pt.y,
                                                 std::max(1.0F,point.size),point.angle,
                                                 point.response,std::max(0,point.octave),
                                                 std::max(-1,point.class_id))))
            {
                error="Point-feature result conversion failed";
                return false;
            }
        }
        cv::drawKeypoints(annotated,detected,annotated,cv::Scalar(0,255,0),
                          cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        if(!OpenCvImageUtils::toQImage(annotated,candidate,error)
                || !m_candidateKeyPoints.setData(&keyPoints))
        {
            if(error.isEmpty()) error="Point-feature candidate preparation failed";
            return false;
        }
        m_candidateCount=static_cast<int>(detected.size());
        return true;
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV point-feature detection failed: %1").arg(exception.what());
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
