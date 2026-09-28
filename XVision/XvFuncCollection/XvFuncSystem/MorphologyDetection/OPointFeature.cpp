#include "XLanguage.h"
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
    maxFeatures=new XInt("maxFeatures",500,this,"最大特征数量");
    qualityLevel=new XReal("qualityLevel",0.01,this,"质量等级");
    minDistance=new XReal("minDistance",5.0,this,"最小距离");
    blockSize=new XInt("blockSize",3,this,"邻域大小");
    harrisK=new XReal("harrisK",0.04,this,"Harris 响应系数");
    subpixelWindow=new XInt("subpixelWindow",5,this,"亚像素窗口");
    subpixelMaxIterations=new XInt("subpixelMaxIterations",40,this,"亚像素迭代次数");
    subpixelEpsilon=new XReal("subpixelEpsilon",0.001,this,"亚像素精度");

    akazeDescriptorType=new XInt("akazeDescriptorType",2,this,"AKAZE 描述子类型");
    descriptorSize=new XInt("descriptorSize",0,this,"描述子大小");
    descriptorChannels=new XInt("descriptorChannels",3,this,"描述子通道数");
    featureThreshold=new XReal("featureThreshold",0.001,this,"特征阈值");
    octaves=new XInt("octaves",4,this,"金字塔组数");
    octaveLayers=new XInt("octaveLayers",4,this,"组内层数");
    diffusivity=new XInt("diffusivity",1,this,"扩散函数");
    extended=new XBool("extended",false,this,"扩展描述子");
    upright=new XBool("upright",false,this,"直立描述子");

    briskThreshold=new XInt("briskThreshold",30,this,"BRISK 阈值");
    briskOctaves=new XInt("briskOctaves",3,this,"BRISK 组数");
    briskPatternScale=new XReal("briskPatternScale",1.0,this,"BRISK 模式缩放");
    fastThreshold=new XInt("fastThreshold",20,this,"FAST 阈值");
    nonmaxSuppression=new XBool("nonmaxSuppression",true,this,"非极大值抑制");

    orbScaleFactor=new XReal("orbScaleFactor",1.2,this,"ORB 缩放系数");
    orbLevels=new XInt("orbLevels",8,this,"ORB 金字塔层数");
    orbEdgeThreshold=new XInt("orbEdgeThreshold",31,this,"ORB 边缘阈值");
    orbFirstLevel=new XInt("orbFirstLevel",0,this,"ORB 起始层");
    orbWtaK=new XInt("orbWtaK",2,this,"ORB 采样点数");
    orbScoreType=new XInt("orbScoreType",0,this,"ORB 评分方式");
    orbPatchSize=new XInt("orbPatchSize",31,this,"ORB 邻域尺寸");
    orientationNormalized=new XBool("orientationNormalized",true,this,"方向归一化");
    scaleNormalized=new XBool("scaleNormalized",true,this,"尺度归一化");
    freakPatternScale=new XReal("freakPatternScale",22.0,this,"FREAK 模式缩放");
    freakOctaves=new XInt("freakOctaves",4,this,"FREAK 组数");

    mserDelta=new XInt("mserDelta",5,this,"MSER 灰度步长");
    mserMinArea=new XInt("mserMinArea",60,this,"MSER 最小面积");
    mserMaxArea=new XInt("mserMaxArea",14400,this,"MSER 最大面积");
    mserMaxVariation=new XReal("mserMaxVariation",0.25,this,"MSER 最大变化率");
    mserMinDiversity=new XReal("mserMinDiversity",0.2,this,"MSER 最小差异度");
    mserMaxEvolution=new XInt("mserMaxEvolution",200,this,"MSER 最大演化次数");
    mserAreaThreshold=new XReal("mserAreaThreshold",1.01,this,"MSER 面积阈值");
    mserMinMargin=new XReal("mserMinMargin",0.003,this,"MSER 最小边距");
    mserEdgeBlurSize=new XInt("mserEdgeBlurSize",5,this,"MSER 边缘模糊尺寸");

    starMaxSize=new XInt("starMaxSize",45,this,"STAR 最大尺寸");
    starResponseThreshold=new XInt("starResponseThreshold",30,this,"STAR 响应阈值");
    starLineThresholdProjected=new XInt("starLineThresholdProjected",10,this,
                                        "STAR 投影线阈值");
    starLineThresholdBinarized=new XInt("starLineThresholdBinarized",8,this,
                                        "STAR 二值线阈值");
    starSuppressNonmaxSize=new XInt("starSuppressNonmaxSize",5,this,
                                    "STAR 抑制邻域");
}

OPointFeature::OPointFeature(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OPointFeatureParam()),
      m_result(new OPointFeatureResult())
{
    _funcRole="OPointFeature";
    _funcType=EXvFuncType::Location;
    _funcName=getUiText("OpenCV Point Feature");
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
