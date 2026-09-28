#include "OTemplateMatch.h"

#include "OpenCvImageUtils.h"
#include "OpenCvTemplateRectificationUtils.h"
#include "XLanguage.h"
#include "XvXmlUtils.h"

#include <QCryptographicHash>
#include <QDomDocument>
#include <QRegularExpression>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <exception>
#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

namespace
{
constexpr double Pi=3.14159265358979323846;
constexpr double FullTurn=2.0*Pi;
constexpr int MaxMatchCount=1000;
constexpr int MaxAngleSamples=721;
constexpr int MaxTemplateBase64Bytes=
        ((OpenCvTemplateRectificationUtils::MaxTemplateAssetBytes+2)/3)*4;

bool hasNonWhitespaceText(const QDomElement &element)
{
    for(QDomNode node=element.firstChild();!node.isNull();node=node.nextSibling())
    {
        if((node.isText() || node.isCDATASection())
                && !node.nodeValue().trimmed().isEmpty()) return true;
    }
    return false;
}

bool validateParameters(const OTemplateMatchParam *param,
                        OTemplateMatch::Mode mode,QString &error)
{
    if(!param || !param->operation || !param->templateRoi
            || !param->useTemplateRoi || !param->minScore
            || !param->maxMatches || !param->maxOverlap)
    {
        error="Template-match parameters are incomplete";
        return false;
    }
    if(mode<OTemplateMatch::Base64 || mode>OTemplateMatch::Hsv
            || param->operation->value()<OTemplateMatch::CreateTemplate
            || param->operation->value()>OTemplateMatch::FindTemplate
            || !qIsFinite(param->minScore->value())
            || param->minScore->value()<0.0 || param->minScore->value()>1.0
            || param->maxMatches->value()<1 || param->maxMatches->value()>MaxMatchCount
            || !qIsFinite(param->maxOverlap->value())
            || param->maxOverlap->value()<0.0 || param->maxOverlap->value()>1.0)
    {
        error="Template-match mode, operation, score, count, or overlap is invalid";
        return false;
    }
    if(mode==OTemplateMatch::Shape)
    {
        const double start=param->angleStart->value();
        const double extent=param->angleExtent->value();
        const double step=param->angleStep->value();
        if(!qIsFinite(start) || !qIsFinite(extent) || !qIsFinite(step)
                || extent<0.0 || extent>FullTurn || step<=0.0 || step>FullTurn
                || int(std::floor(extent/step))+1>MaxAngleSamples
                || param->cannyLower->value()<0 || param->cannyLower->value()>255
                || param->cannyUpper->value()<0 || param->cannyUpper->value()>255
                || param->cannyLower->value()>=param->cannyUpper->value())
        {
            error="Shape-match angle or Canny settings are invalid";
            return false;
        }
    }
    if(mode==OTemplateMatch::Feature)
    {
        if(param->maxFeatures->value()<32 || param->maxFeatures->value()>100000
                || !qIsFinite(param->featureRatio->value())
                || param->featureRatio->value()<=0.0 || param->featureRatio->value()>=1.0
                || param->minimumFeatureMatches->value()<4
                || param->minimumFeatureMatches->value()>param->maxFeatures->value()
                || param->minimumInliers->value()<4
                || param->minimumInliers->value()>param->minimumFeatureMatches->value()
                || !qIsFinite(param->ransacReprojectionThreshold->value())
                || param->ransacReprojectionThreshold->value()<=0.0
                || param->ransacReprojectionThreshold->value()>1000.0)
        {
            error="Feature-match detector, ratio, inlier, or RANSAC settings are invalid";
            return false;
        }
    }
    return true;
}

#if defined(XVISION_ENABLE_OPENCV)
struct MatchCandidate
{
    double x=0.0;
    double y=0.0;
    double angle=0.0;
    double score=0.0;
    double width=1.0;
    double height=1.0;
};

bool rectifyTemplateRoi(const cv::Mat &input,const XRotateRectRoi &roi,
                        cv::Mat &output,QString &error)
{
    QVector<QPointF> corners;
    QSize size;
    if(!OpenCvTemplateRectificationUtils::roiCorners(roi,corners,error)
            || !OpenCvTemplateRectificationUtils::roiOutputSize(roi,0,0,size,error))
        return false;
    if(size.width()<2 || size.height()<2)
    {
        error="Template ROI must produce at least a 2x2 image";
        return false;
    }
    std::vector<cv::Point2f> sourcePoints,destinationPoints;
    for(const QPointF &point:corners)
        sourcePoints.emplace_back(float(point.x()),float(point.y()));
    destinationPoints={{0.0F,0.0F},{float(size.width()-1),0.0F},
                       {float(size.width()-1),float(size.height()-1)},
                       {0.0F,float(size.height()-1)}};
    const cv::Mat transform=cv::getPerspectiveTransform(sourcePoints,destinationPoints);
    if(transform.empty() || !cv::checkRange(transform)
            || qAbs(cv::determinant(transform))<1e-12)
    {
        error="Template ROI produces a degenerate transform";
        return false;
    }
    cv::warpPerspective(input,output,transform,cv::Size(size.width(),size.height()),
                        cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar());
    return !output.empty();
}

cv::Mat rotatedTemplate(const cv::Mat &source,double angle)
{
    if(qAbs(angle)<1e-12) return source.clone();
    const cv::Point2f center((source.cols-1)*0.5F,(source.rows-1)*0.5F);
    cv::Mat matrix=cv::getRotationMatrix2D(center,angle*180.0/Pi,1.0);
    const cv::RotatedRect bounds(
                center,cv::Size2f(float(source.cols),float(source.rows)),
                float(angle*180.0/Pi));
    const cv::Rect2f box=bounds.boundingRect2f();
    matrix.at<double>(0,2)+=box.width*0.5-center.x;
    matrix.at<double>(1,2)+=box.height*0.5-center.y;
    cv::Mat output;
    cv::warpAffine(source,output,matrix,
                   cv::Size(qMax(1,qRound(box.width)),qMax(1,qRound(box.height))),
                   cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar());
    return output;
}

double overlapRatio(const MatchCandidate &left,const MatchCandidate &right)
{
    const double leftX=left.x-left.width*0.5;
    const double leftY=left.y-left.height*0.5;
    const double rightX=right.x-right.width*0.5;
    const double rightY=right.y-right.height*0.5;
    const double intersectionWidth=qMax(0.0,qMin(leftX+left.width,rightX+right.width)
                                        -qMax(leftX,rightX));
    const double intersectionHeight=qMax(0.0,qMin(leftY+left.height,rightY+right.height)
                                         -qMax(leftY,rightY));
    const double intersection=intersectionWidth*intersectionHeight;
    const double total=left.width*left.height+right.width*right.height-intersection;
    return total>0.0?intersection/total:0.0;
}

void collectCorrelation(const cv::Mat &scene,const cv::Mat &model,double relativeAngle,
                        double minScore,int maximum,double maxOverlap,
                        std::vector<MatchCandidate> &candidates)
{
    if(model.empty() || model.cols>scene.cols || model.rows>scene.rows) return;
    cv::Mat response;
    cv::matchTemplate(scene,model,response,cv::TM_CCOEFF_NORMED);
    cv::patchNaNs(response,0.0);
    const int suppressX=qMax(1,qRound(model.cols*(1.0-maxOverlap)));
    const int suppressY=qMax(1,qRound(model.rows*(1.0-maxOverlap)));
    for(int index=0;index<maximum;++index)
    {
        double maximumValue=0.0;
        cv::Point maximumLocation;
        cv::minMaxLoc(response,nullptr,&maximumValue,nullptr,&maximumLocation);
        if(!qIsFinite(maximumValue) || maximumValue<minScore) break;
        candidates.push_back({maximumLocation.x+model.cols*0.5,
                              maximumLocation.y+model.rows*0.5,
                              relativeAngle,qBound(0.0,maximumValue,1.0),
                              double(model.cols),double(model.rows)});
        const cv::Rect suppression(
                    qMax(0,maximumLocation.x-suppressX/2),
                    qMax(0,maximumLocation.y-suppressY/2),
                    qMin(response.cols,maximumLocation.x+suppressX/2+1)
                        -qMax(0,maximumLocation.x-suppressX/2),
                    qMin(response.rows,maximumLocation.y+suppressY/2+1)
                        -qMax(0,maximumLocation.y-suppressY/2));
        response(suppression).setTo(-1.0F);
    }
}

bool correlationMatches(const cv::Mat &scene,const cv::Mat &storedTemplate,
                        const OTemplateMatchParam *param,OTemplateMatch::Mode mode,
                        double baseAngle,std::vector<MatchCandidate> &matches,
                        QString &error)
{
    cv::Mat sceneValue,templateValue;
    if(mode==OTemplateMatch::Hsv)
    {
        cv::Mat sceneBgr,templateBgr;
        if(!OpenCvImageUtils::toBgr(scene,sceneBgr,error)
                || !OpenCvImageUtils::toBgr(storedTemplate,templateBgr,error)) return false;
        cv::cvtColor(sceneBgr,sceneValue,cv::COLOR_BGR2HSV);
        cv::cvtColor(templateBgr,templateValue,cv::COLOR_BGR2HSV);
    }
    else
    {
        if(!OpenCvImageUtils::toGray(scene,sceneValue,error)
                || !OpenCvImageUtils::toGray(storedTemplate,templateValue,error)) return false;
    }

    const int perAngle=qMin(MaxMatchCount,param->maxMatches->value()*4);
    if(mode==OTemplateMatch::Shape)
    {
        cv::Canny(sceneValue,sceneValue,param->cannyLower->value(),
                  param->cannyUpper->value());
        cv::Canny(templateValue,templateValue,param->cannyLower->value(),
                  param->cannyUpper->value());
        const double end=param->angleStart->value()+param->angleExtent->value();
        for(double relative=param->angleStart->value();relative<=end+1e-12;
            relative+=param->angleStep->value())
        {
            const cv::Mat rotated=rotatedTemplate(templateValue,baseAngle+relative);
            collectCorrelation(sceneValue,rotated,relative,param->minScore->value(),
                               perAngle,param->maxOverlap->value(),matches);
        }
    }
    else
    {
        const cv::Mat rotated=rotatedTemplate(templateValue,baseAngle);
        collectCorrelation(sceneValue,rotated,0.0,param->minScore->value(),perAngle,
                           param->maxOverlap->value(),matches);
    }
    std::stable_sort(matches.begin(),matches.end(),[](const MatchCandidate &left,
                                                      const MatchCandidate &right)
    {
        if(left.score!=right.score) return left.score>right.score;
        if(left.y!=right.y) return left.y<right.y;
        if(left.x!=right.x) return left.x<right.x;
        return left.angle<right.angle;
    });
    std::vector<MatchCandidate> selected;
    for(const MatchCandidate &candidate:matches)
    {
        bool overlaps=false;
        for(const MatchCandidate &accepted:selected)
        {
            if(overlapRatio(candidate,accepted)>param->maxOverlap->value())
            {
                overlaps=true;
                break;
            }
        }
        if(!overlaps) selected.push_back(candidate);
        if(int(selected.size())>=param->maxMatches->value()) break;
    }
    matches=std::move(selected);
    return true;
}

bool featureMatch(const cv::Mat &scene,const cv::Mat &storedTemplate,
                  const OTemplateMatchParam *param,double baseAngle,
                  std::vector<MatchCandidate> &matches,QString &error)
{
    cv::Mat sceneGray,templateGray;
    if(!OpenCvImageUtils::toGray(scene,sceneGray,error)
            || !OpenCvImageUtils::toGray(storedTemplate,templateGray,error)) return false;
    auto orb=cv::ORB::create(param->maxFeatures->value());
    std::vector<cv::KeyPoint> templatePoints,scenePoints;
    cv::Mat templateDescriptors,sceneDescriptors;
    orb->detectAndCompute(templateGray,cv::noArray(),templatePoints,templateDescriptors);
    orb->detectAndCompute(sceneGray,cv::noArray(),scenePoints,sceneDescriptors);
    if(templateDescriptors.empty() || sceneDescriptors.empty())
    {
        return true;
    }
    std::vector<std::vector<cv::DMatch>> pairs;
    cv::BFMatcher matcher(cv::NORM_HAMMING,false);
    matcher.knnMatch(templateDescriptors,sceneDescriptors,pairs,2);
    std::vector<cv::Point2f> templateLocations,sceneLocations;
    for(const auto &pair:pairs)
    {
        if(pair.size()<2 || pair[0].distance
                >=float(param->featureRatio->value())*pair[1].distance) continue;
        templateLocations.push_back(templatePoints[size_t(pair[0].queryIdx)].pt);
        sceneLocations.push_back(scenePoints[size_t(pair[0].trainIdx)].pt);
    }
    if(int(templateLocations.size())<param->minimumFeatureMatches->value())
    {
        return true;
    }
    cv::Mat inlierMask;
    const cv::Mat homography=cv::findHomography(
                templateLocations,sceneLocations,cv::RANSAC,
                param->ransacReprojectionThreshold->value(),inlierMask);
    if(homography.empty() || !cv::checkRange(homography))
    {
        return true;
    }
    const int inliers=cv::countNonZero(inlierMask);
    if(inliers<param->minimumInliers->value())
    {
        return true;
    }
    std::vector<cv::Point2f> templateCorners={{0.0F,0.0F},
        {float(storedTemplate.cols-1),0.0F},
        {float(storedTemplate.cols-1),float(storedTemplate.rows-1)},
        {0.0F,float(storedTemplate.rows-1)}};
    std::vector<cv::Point2f> projected;
    cv::perspectiveTransform(templateCorners,projected,homography);
    if(projected.size()!=4 || !cv::isContourConvex(projected)
            || qAbs(cv::contourArea(projected))<1.0)
    {
        error="Feature matching produced invalid projected template corners";
        return false;
    }
    cv::Point2f center;
    for(const cv::Point2f &point:projected)
    {
        if(!qIsFinite(point.x) || !qIsFinite(point.y))
        {
            error="Feature matching produced non-finite coordinates";
            return false;
        }
        center+=point;
    }
    center*=0.25F;
    const cv::Point2f axis=projected[1]-projected[0];
    const double absoluteAngle=-std::atan2(double(axis.y),double(axis.x));
    const double score=double(inliers)/double(templateLocations.size());
    if(score<param->minScore->value()) return true;
    const cv::Rect box=cv::boundingRect(projected);
    matches.push_back({center.x,center.y,absoluteAngle-baseAngle,
                       qBound(0.0,score,1.0),double(box.width),double(box.height)});
    return true;
}
#endif
}

OTemplateMatchParam::OTemplateMatchParam()
{
    operation=new XInt("operation",OTemplateMatch::CreateTemplate,this,
                       getLang("XvFuncSystem_OTemplateMatch_Operation","操作"));
    templateRoi=new XRotateRectRoi("templateRoi",0.0,0.0,20.0,20.0,0.0,this,
                                   getLang("XvFuncSystem_OTemplateMatch_Roi","模板区域"));
    useTemplateRoi=new XBool("useTemplateRoi",false,this,
                             getLang("XvFuncSystem_OTemplateMatch_UseRoi","使用模板区域"));
    minScore=new XReal("minScore",0.7,this,
                       getLang("XvFuncSystem_OTemplateMatch_MinScore","最小分数"));
    maxMatches=new XInt("maxMatches",1,this,
                        getLang("XvFuncSystem_OTemplateMatch_MaxMatches","最大结果数"));
    maxOverlap=new XReal("maxOverlap",0.3,this,
                         getLang("XvFuncSystem_OTemplateMatch_MaxOverlap","最大重叠"));
    angleStart=new XReal("angleStart",-0.39,this,
                         getLang("XvFuncSystem_OTemplateMatch_AngleStart","起始角度"));
    angleExtent=new XReal("angleExtent",0.78,this,
                          getLang("XvFuncSystem_OTemplateMatch_AngleExtent","角度范围"));
    angleStep=new XReal("angleStep",0.05,this,
                        getLang("XvFuncSystem_OTemplateMatch_AngleStep","角度步长"));
    cannyLower=new XInt("cannyLower",50,this,"边缘检测下限");
    cannyUpper=new XInt("cannyUpper",150,this,"边缘检测上限");
    maxFeatures=new XInt("maxFeatures",1500,this,
                         getLang("XvFuncSystem_OTemplateMatch_MaxFeatures","最大特征数"));
    featureRatio=new XReal("featureRatio",0.75,this,
                           getLang("XvFuncSystem_OTemplateMatch_FeatureRatio","特征比率"));
    minimumFeatureMatches=new XInt("minimumFeatureMatches",8,this,
                                   getLang("XvFuncSystem_OTemplateMatch_MinFeatureMatches","最少特征匹配"));
    minimumInliers=new XInt("minimumInliers",6,this,
                            getLang("XvFuncSystem_OTemplateMatch_MinInliers","最少内点"));
    ransacReprojectionThreshold=new XReal("ransacReprojectionThreshold",3.0,this,
                                          "RANSAC 重投影阈值");
}

OTemplateMatchResult::OTemplateMatchResult()
{
    templateRoi=new XRotateRectRoi("templateRoi",0.0,0.0,20.0,20.0,0.0,this,
                                   getLang("XvFuncSystem_OTemplateMatch_ResultRoi","模板区域"));
    matchCount=new XInt("matchCount",0,this,
                        getLang("XvFuncSystem_OTemplateMatch_MatchCount","匹配数量"));
    matches=new XObjectList("matches",XMatchResult::type(),this,
                            getLang("XvFuncSystem_OTemplateMatch_Matches","匹配结果"));
}

OTemplateMatch::OTemplateMatch(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OTemplateMatchParam()),
      m_result(new OTemplateMatchResult())
{
    _funcRole="OTemplateMatch";
    _funcName=getLang("XvFuncSystem_OTemplateMatch_Name","OpenCV模板匹配");
    _funcType=EXvFuncType::Location;
}

OTemplateMatch::~OTemplateMatch()
{
    delete m_param;
    delete m_result;
}

QStringList OTemplateMatch::activeParameterNames() const
{
    QStringList names={"inputImage","operation","useTemplateRoi","templateRoi",
                       "minScore","maxMatches","maxOverlap"};
    if(m_mode==Shape)
        names << "angleStart" << "angleExtent" << "angleStep"
              << "cannyLower" << "cannyUpper";
    if(m_mode==Feature)
        names << "maxFeatures" << "featureRatio" << "minimumFeatureMatches"
              << "minimumInliers" << "ransacReprojectionThreshold";
    return names;
}

bool OTemplateMatch::configureTemplateImage(const QImage &image,QString &error)
{
    QByteArray asset,digest;
    QSize size;
    if(!OpenCvTemplateRectificationUtils::encodeTemplatePng(
                image,asset,size,digest,error)) return false;
    m_templateAsset=asset;
    m_templateSize=size;
    m_templateSha256=digest;
    return true;
}

bool OTemplateMatch::processImage(const QImage &source,QImage &candidate,QString &error)
{
    m_replaceTemplate=false;
    m_candidateTemplateAsset.clear();
    m_candidateTemplateSize=QSize();
    m_candidateTemplateSha256.clear();
    XObjectList empty("candidateMatches",XMatchResult::type());
    if(!m_candidateMatches.setData(&empty))
    {
        error="Template-match candidate list could not be reset";
        return false;
    }
    if(!validateParameters(m_param,m_mode,error)) return false;

#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat scene;
    if(!OpenCvImageUtils::fromQImage(source,scene,error)) return false;
    try
    {
        if(m_param->operation->value()==CreateTemplate)
        {
            cv::Mat templateMat;
            XRotateRectRoi effectiveRoi;
            if(m_param->useTemplateRoi->value())
            {
                if(!rectifyTemplateRoi(scene,*m_param->templateRoi,templateMat,error))
                    return false;
                if(!effectiveRoi.setData(m_param->templateRoi))
                {
                    error="Template ROI could not be copied";
                    return false;
                }
            }
            else
            {
                if(scene.cols<2 || scene.rows<2)
                {
                    error="Full-image template must be at least 2x2 pixels";
                    return false;
                }
                templateMat=scene.clone();
                if(!effectiveRoi.setValue((scene.cols-1)*0.5,(scene.rows-1)*0.5,
                                          scene.cols*0.5,scene.rows*0.5,0.0))
                {
                    error="Full-image template geometry is invalid";
                    return false;
                }
            }
            QImage templateImage;
            if(!OpenCvImageUtils::toQImage(templateMat,templateImage,error)
                    || !OpenCvTemplateRectificationUtils::encodeTemplatePng(
                        templateImage,m_candidateTemplateAsset,m_candidateTemplateSize,
                        m_candidateTemplateSha256,error)) return false;
            if(!m_candidateTemplateRoi.setData(&effectiveRoi))
            {
                error="Template ROI candidate could not be prepared";
                return false;
            }
            m_replaceTemplate=true;
            candidate=source.copy();
            return !candidate.isNull();
        }

        if(m_templateAsset.isEmpty())
        {
            error="No template has been created";
            return false;
        }
        QImage templateImage;
        QSize decodedSize;
        QByteArray digest;
        if(!OpenCvTemplateRectificationUtils::decodeTemplatePng(
                    m_templateAsset,templateImage,decodedSize,digest,error)) return false;
        if(decodedSize!=m_templateSize || digest!=m_templateSha256)
        {
            error="Stored template metadata no longer matches its PNG bytes";
            return false;
        }
        cv::Mat templateMat;
        if(!OpenCvImageUtils::fromQImage(templateImage,templateMat,error)) return false;
        const double baseAngle=m_param->useTemplateRoi->value()
                ?m_param->templateRoi->angle():0.0;
        std::vector<MatchCandidate> matches;
        const bool ok=m_mode==Feature
                ?featureMatch(scene,templateMat,m_param,baseAngle,matches,error)
                :correlationMatches(scene,templateMat,m_param,m_mode,baseAngle,matches,error);
        if(!ok) return false;

        XObjectList candidateMatches("candidateMatches",XMatchResult::type());
        for(size_t index=0;index<matches.size();++index)
        {
            const MatchCandidate &value=matches[index];
            auto match=new XMatchResult(QString("match%1").arg(index),value.x,value.y,
                                        value.angle,value.score);
            if(!match->setValue(value.x,value.y,value.angle,value.score)
                    || !candidateMatches.addValue(match))
            {
                delete match;
                error="Template-match result conversion failed";
                return false;
            }
        }
        if(!m_candidateMatches.setData(&candidateMatches)
                || !m_candidateTemplateRoi.setValue(
                    0.0,0.0,m_templateSize.width()*0.5,m_templateSize.height()*0.5,
                    baseAngle))
        {
            error="Template-match candidates could not be prepared";
            return false;
        }
        candidate=source.copy();
        return !candidate.isNull();
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV template matching failed: %1").arg(exception.what());
    }
    catch(const std::exception &exception)
    {
        error=QString("Template matching failed: %1").arg(exception.what());
    }
    catch(...)
    {
        error="Template matching failed with an unknown error";
    }
    return false;
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}

bool OTemplateMatch::commitAdditionalResults(QString &error)
{
    if(!m_result->matches->setData(&m_candidateMatches)
            || !m_result->templateRoi->setData(&m_candidateTemplateRoi))
    {
        error="Template-match results could not be committed";
        return false;
    }
    m_result->matchCount->setValue(int(m_candidateMatches.count()));
    if(m_replaceTemplate)
    {
        m_templateAsset=m_candidateTemplateAsset;
        m_templateSize=m_candidateTemplateSize;
        m_templateSha256=m_candidateTemplateSha256;
    }
    error.clear();
    return true;
}

void OTemplateMatch::clearResultsAfterFailure()
{
    m_result->outputImage->setValue(QImage());
    XObjectList empty("matches",XMatchResult::type());
    m_result->matches->setData(&empty);
    m_result->matchCount->setValue(0);
    m_result->templateRoi->setValue(0.0,0.0,1.0,1.0,0.0);
}

bool OTemplateMatch::appendPersistentData(QDomDocument &doc,
                                          QDomElement &functionElement,
                                          QString &error) const
{
    if(m_templateAsset.isEmpty()) return true;
    QImage image;
    QSize size;
    QByteArray digest;
    if(!OpenCvTemplateRectificationUtils::decodeTemplatePng(
                m_templateAsset,image,size,digest,error)) return false;
    if(size!=m_templateSize || digest!=m_templateSha256)
    {
        error="Template PNG metadata is inconsistent";
        return false;
    }
    QDomElement dataElement=doc.createElement("PersistentData");
    QDomElement templateElement=doc.createElement("OpenCvTemplate");
    templateElement.setAttribute("format","png");
    templateElement.setAttribute("version","1");
    templateElement.setAttribute("encoding","base64");
    templateElement.setAttribute("length",QString::number(m_templateAsset.size()));
    templateElement.setAttribute("sha256",QString::fromLatin1(digest));
    templateElement.appendChild(doc.createTextNode(
                                    QString::fromLatin1(m_templateAsset.toBase64())));
    dataElement.appendChild(templateElement);
    functionElement.appendChild(dataElement);
    return true;
}

bool OTemplateMatch::readPersistentData(const QDomElement &dataElement,QString &error)
{
    if(dataElement.isNull())
    {
        m_templateAsset.clear();
        m_templateSize=QSize();
        m_templateSha256.clear();
        return true;
    }
    if(!XvXml::validateAttributes(dataElement,{}, {},error)
            || !XvXml::validateChildren(dataElement,{"OpenCvTemplate"},
                                        {"OpenCvTemplate"},error)
            || hasNonWhitespaceText(dataElement))
    {
        if(error.isEmpty()) error="<PersistentData> contains unsupported text";
        return false;
    }
    const QDomElement templateElement=XvXml::singleChild(dataElement,"OpenCvTemplate");
    if(!XvXml::validateAttributes(
                templateElement,{"format","version","encoding","length","sha256"},
                {"format","version","encoding","length","sha256"},error)
            || !XvXml::validateChildren(templateElement,{}, {},error)) return false;
    if(templateElement.attribute("format")!="png"
            || templateElement.attribute("version")!="1"
            || templateElement.attribute("encoding")!="base64")
    {
        error="Unsupported OpenCV template format, version, or encoding";
        return false;
    }
    unsigned int expectedLength=0;
    if(!XvXml::unsignedAttribute(templateElement,"length",expectedLength,error)
            || expectedLength==0
            || expectedLength>OpenCvTemplateRectificationUtils::MaxTemplateAssetBytes)
    {
        if(error.isEmpty()) error="Template PNG length is outside the supported range";
        return false;
    }
    const QString digestText=templateElement.attribute("sha256");
    if(!QRegularExpression("^[0-9a-f]{64}$").match(digestText).hasMatch())
    {
        error="Template PNG SHA-256 must be 64 lowercase hexadecimal characters";
        return false;
    }
    const QString encodedText=templateElement.text();
    if(encodedText.isEmpty() || encodedText.size()>MaxTemplateBase64Bytes
            || encodedText.size()%4!=0)
    {
        error="Template PNG is not canonical Base64";
        return false;
    }
    const QByteArray encoded=encodedText.toLatin1();
    if(QString::fromLatin1(encoded)!=encodedText || encoded.contains(' ')
            || encoded.contains('\n') || encoded.contains('\r') || encoded.contains('\t'))
    {
        error="Template PNG is not canonical Base64";
        return false;
    }
    const QByteArray decoded=QByteArray::fromBase64(encoded);
    if(decoded.size()!=int(expectedLength) || decoded.toBase64()!=encoded)
    {
        error="Template PNG Base64 or length is invalid";
        return false;
    }
    QImage image;
    QSize size;
    QByteArray digest;
    if(!OpenCvTemplateRectificationUtils::decodeTemplatePng(
                decoded,image,size,digest,error)) return false;
    if(QString::fromLatin1(digest)!=digestText)
    {
        error="Template PNG SHA-256 does not match its bytes";
        return false;
    }
    m_templateAsset=decoded;
    m_templateSize=size;
    m_templateSha256=digest;
    return true;
}
