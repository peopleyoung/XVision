#include "XLanguage.h"
#include "ORegionDetector.h"

#include "OpenCvImageUtils.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

namespace
{
bool validUnitRange(double minimum,double maximum)
{
    return qIsFinite(minimum) && qIsFinite(maximum)
            && minimum>=0.0 && maximum<=1.0 && minimum<=maximum;
}

double normalizedLineAngle(double degrees)
{
    double value=std::fmod(degrees,180.0);
    if(value<0.0) value+=180.0;
    return value;
}

double lineAngleDistance(double left,double right)
{
    const double distance=std::abs(normalizedLineAngle(left)-normalizedLineAngle(right));
    return std::min(distance,180.0-distance);
}
}

ORegionDetectorParam::ORegionDetectorParam()
{
    thresholdStep=new XReal("thresholdStep",10.0,this,"阈值步长");
    minThreshold=new XReal("minThreshold",10.0,this,"阈值下限");
    maxThreshold=new XReal("maxThreshold",220.0,this,"阈值上限");
    minRepeatability=new XInt("minRepeatability",2,this,"最小重复次数");
    minDistBetweenBlobs=new XReal("minDistBetweenBlobs",10.0,this,"最小斑点间距");
    filterByColor=new XBool("filterByColor",false,this,"按颜色筛选");
    blobColor=new XInt("blobColor",0,this,"斑点颜色");
    filterByArea=new XBool("filterByArea",true,this,"按面积筛选");
    minArea=new XReal("minArea",25.0,this,"最小面积");
    maxArea=new XReal("maxArea",5000.0,this,"最大面积");
    filterByCircularity=new XBool("filterByCircularity",false,this,"按圆度筛选");
    minCircularity=new XReal("minCircularity",0.1,this,"最小圆度");
    maxCircularity=new XReal("maxCircularity",1.0,this,"最大圆度");
    filterByInertia=new XBool("filterByInertia",false,this,"按惯性筛选");
    minInertiaRatio=new XReal("minInertiaRatio",0.1,this,"最小惯性比");
    maxInertiaRatio=new XReal("maxInertiaRatio",1.0,this,"最大惯性比");
    filterByConvexity=new XBool("filterByConvexity",false,this,"按凸度筛选");
    minConvexity=new XReal("minConvexity",0.5,this,"最小凸度");
    maxConvexity=new XReal("maxConvexity",1.0,this,"最大凸度");

    binaryThreshold=new XReal("binaryThreshold",127.0,this,"二值化阈值");
    retrievalMode=new XInt("retrievalMode",0,this,"轮廓检索模式");
    approximationMode=new XInt("approximationMode",1,this,"轮廓近似模式");
    offsetX=new XInt("offsetX",0,this,"偏移 X");
    offsetY=new XInt("offsetY",0,this,"偏移 Y");
    minContourArea=new XReal("minContourArea",0.0,this,"最小轮廓面积");

    circleDp=new XReal("circleDp",1.0,this,"累加器分辨率比");
    circleMinDistance=new XReal("circleMinDistance",20.0,this,"最小圆心距");
    circleEdgeThreshold=new XReal("circleEdgeThreshold",100.0,this,"圆边缘阈值");
    circleCenterThreshold=new XReal("circleCenterThreshold",30.0,this,"圆心检测阈值");
    minRadius=new XInt("minRadius",0,this,"最小半径");
    maxRadius=new XInt("maxRadius",0,this,"最大半径");

    connectivity=new XInt("connectivity",8,this,"像素连通性");
    connectedComponentsAlgorithm=new XInt("connectedComponentsAlgorithm",-1,this,
                                          "连通域算法");
    componentMinArea=new XReal("componentMinArea",1.0,this,"最小连通域面积");
    componentMaxArea=new XReal("componentMaxArea",10000000.0,this,"最大连通域面积");
    useRenderBlobs=new XBool("useRenderBlobs",true,this,"绘制斑点");

    rho=new XReal("rho",1.0,this,"距离分辨率");
    theta=new XReal("theta",1.0,this,"角度分辨率（度）");
    houghThreshold=new XInt("houghThreshold",80,this,"霍夫变换阈值");
    srn=new XReal("srn",0.0,this,"多尺度距离分辨率系数");
    stn=new XReal("stn",0.0,this,"多尺度角度分辨率系数");
    minLineLength=new XReal("minLineLength",30.0,this,"最小线段长度");
    maxLineGap=new XReal("maxLineGap",10.0,this,"最大线段间距");
    targetAngle=new XReal("targetAngle",-1.0,this,"目标角度");
    angleTolerance=new XReal("angleTolerance",10.0,this,"角度容差");
}

ORegionDetectorResult::ORegionDetectorResult()
{
    keyPoints=new XObjectList("keyPoints",XKeyPoint::type(),this,"关键点");
    contours=new XObjectList("contours",XContour::type(),this,"轮廓集合");
    circles=new XObjectList("circles",XCircle2D::type(),this,"圆集合");
    regions=new XObjectList("regions",XRegion::type(),this,"区域集合");
    rectangles=new XObjectList("rectangles",XRect2D::type(),this,"矩形集合");
    lines=new XObjectList("lines",XLine2D::type(),this,"直线集合");
    count=new XInt("count",0,this,"检测数量");
}

ORegionDetector::ORegionDetector(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new ORegionDetectorParam()),
      m_result(new ORegionDetectorResult())
{
    _funcRole="ORegionDetector";
    _funcType=EXvFuncType::DefectDetection;
    _funcName=getUiText("OpenCV Region Detector");
}

ORegionDetector::~ORegionDetector()
{
    delete m_param;
    delete m_result;
}

QStringList ORegionDetector::activeParameterNames() const
{
    switch(m_mode)
    {
    case Blob:
        return {"inputImage","thresholdStep","minThreshold","maxThreshold",
                "minRepeatability","minDistBetweenBlobs","filterByColor","blobColor",
                "filterByArea","minArea","maxArea","filterByCircularity",
                "minCircularity","maxCircularity","filterByInertia","minInertiaRatio",
                "maxInertiaRatio","filterByConvexity","minConvexity","maxConvexity"};
    case Contours:
        return {"inputImage","binaryThreshold","retrievalMode","approximationMode",
                "offsetX","offsetY","minContourArea"};
    case HoughCircles:
        return {"inputImage","circleDp","circleMinDistance","circleEdgeThreshold",
                "circleCenterThreshold","minRadius","maxRadius"};
    case RenderBlobs:
        return {"inputImage","binaryThreshold","connectivity",
                "connectedComponentsAlgorithm","componentMinArea","componentMaxArea",
                "useRenderBlobs"};
    case HoughLines:
        return {"inputImage","binaryThreshold","rho","theta","houghThreshold","srn","stn"};
    case HoughLinesP:
        return {"inputImage","binaryThreshold","rho","theta","houghThreshold",
                "minLineLength","maxLineGap","targetAngle","angleTolerance"};
    }
    return {"inputImage"};
}

bool ORegionDetector::commitCandidateLists(QString &error)
{
    if(!m_result->keyPoints->setData(&m_candidateKeyPoints)
            || !m_result->contours->setData(&m_candidateContours)
            || !m_result->circles->setData(&m_candidateCircles)
            || !m_result->regions->setData(&m_candidateRegions)
            || !m_result->rectangles->setData(&m_candidateRectangles)
            || !m_result->lines->setData(&m_candidateLines))
    {
        error="Region detector result list candidate is invalid";
        return false;
    }
    m_result->count->setValue(m_candidateCount);
    error.clear();
    return true;
}

bool ORegionDetector::commitAdditionalResults(QString &error)
{
    return commitCandidateLists(error);
}

bool ORegionDetector::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(static_cast<int>(m_mode)<Blob || static_cast<int>(m_mode)>HoughLinesP)
    {
        error="Region detector mode is invalid";
        return false;
    }
    cv::Mat input,gray,annotated;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)
            || !OpenCvImageUtils::toBgr(input,annotated,error)) return false;

    XObjectList keyPoints("candidateKeyPoints",XKeyPoint::type());
    XObjectList contours("candidateContours",XContour::type());
    XObjectList circles("candidateCircles",XCircle2D::type());
    XObjectList regions("candidateRegions",XRegion::type());
    XObjectList rectangles("candidateRectangles",XRect2D::type());
    XObjectList lines("candidateLines",XLine2D::type());
    int count=0;
    try
    {
        if(m_mode==Blob)
        {
            const double minThreshold=m_param->minThreshold->value();
            const double maxThreshold=m_param->maxThreshold->value();
            if(m_param->thresholdStep->value()<=0.0 || minThreshold<0.0
                    || maxThreshold>255.0 || minThreshold>=maxThreshold
                    || m_param->minRepeatability->value()<1
                    || m_param->minDistBetweenBlobs->value()<0.0
                    || (m_param->blobColor->value()!=0 && m_param->blobColor->value()!=255)
                    || (m_param->filterByArea->value()
                        && (m_param->minArea->value()<=0.0
                            || m_param->minArea->value()>m_param->maxArea->value()))
                    || (m_param->filterByCircularity->value()
                        && !validUnitRange(m_param->minCircularity->value(),
                                           m_param->maxCircularity->value()))
                    || (m_param->filterByInertia->value()
                        && !validUnitRange(m_param->minInertiaRatio->value(),
                                           m_param->maxInertiaRatio->value()))
                    || (m_param->filterByConvexity->value()
                        && !validUnitRange(m_param->minConvexity->value(),
                                           m_param->maxConvexity->value())))
            {
                error="Blob detector thresholds, filters, or ranges are invalid";
                return false;
            }
            cv::SimpleBlobDetector::Params parameters;
            parameters.thresholdStep=static_cast<float>(m_param->thresholdStep->value());
            parameters.minThreshold=static_cast<float>(minThreshold);
            parameters.maxThreshold=static_cast<float>(maxThreshold);
            parameters.minRepeatability=static_cast<size_t>(m_param->minRepeatability->value());
            parameters.minDistBetweenBlobs=static_cast<float>(m_param->minDistBetweenBlobs->value());
            parameters.filterByColor=m_param->filterByColor->value();
            parameters.blobColor=static_cast<uchar>(m_param->blobColor->value());
            parameters.filterByArea=m_param->filterByArea->value();
            parameters.minArea=static_cast<float>(m_param->minArea->value());
            parameters.maxArea=static_cast<float>(m_param->maxArea->value());
            parameters.filterByCircularity=m_param->filterByCircularity->value();
            parameters.minCircularity=static_cast<float>(m_param->minCircularity->value());
            parameters.maxCircularity=static_cast<float>(m_param->maxCircularity->value());
            parameters.filterByInertia=m_param->filterByInertia->value();
            parameters.minInertiaRatio=static_cast<float>(m_param->minInertiaRatio->value());
            parameters.maxInertiaRatio=static_cast<float>(m_param->maxInertiaRatio->value());
            parameters.filterByConvexity=m_param->filterByConvexity->value();
            parameters.minConvexity=static_cast<float>(m_param->minConvexity->value());
            parameters.maxConvexity=static_cast<float>(m_param->maxConvexity->value());
            std::vector<cv::KeyPoint> detected;
            cv::SimpleBlobDetector::create(parameters)->detect(gray,detected);
            cv::drawKeypoints(annotated,detected,annotated,cv::Scalar(0,0,255),
                              cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
            for(const cv::KeyPoint &point:detected)
            {
                if(!keyPoints.addValue(new XKeyPoint(point.pt.x,point.pt.y,
                                                     std::max(1.0F,point.size),point.angle,
                                                     point.response,std::max(0,point.octave),
                                                     std::max(-1,point.class_id))))
                {
                    error="Blob key-point conversion failed";
                    return false;
                }
            }
            count=static_cast<int>(detected.size());
        }
        else if(m_mode==Contours)
        {
            if(m_param->binaryThreshold->value()<0.0 || m_param->binaryThreshold->value()>255.0
                    || m_param->retrievalMode->value()<0 || m_param->retrievalMode->value()>3
                    || m_param->approximationMode->value()<0
                    || m_param->approximationMode->value()>3
                    || m_param->minContourArea->value()<0.0)
            {
                error="Contour threshold, mode, or minimum area is invalid";
                return false;
            }
            cv::Mat binary;
            cv::threshold(gray,binary,m_param->binaryThreshold->value(),255.0,cv::THRESH_BINARY);
            const int retrievals[]={cv::RETR_EXTERNAL,cv::RETR_LIST,cv::RETR_CCOMP,cv::RETR_TREE};
            const int approximations[]={cv::CHAIN_APPROX_NONE,cv::CHAIN_APPROX_SIMPLE,
                                        cv::CHAIN_APPROX_TC89_L1,cv::CHAIN_APPROX_TC89_KCOS};
            std::vector<std::vector<cv::Point>> detected;
            cv::findContours(binary,detected,retrievals[m_param->retrievalMode->value()],
                             approximations[m_param->approximationMode->value()],
                             cv::Point(m_param->offsetX->value(),m_param->offsetY->value()));
            std::vector<std::vector<cv::Point>> accepted;
            for(const std::vector<cv::Point> &contour:detected)
            {
                if(contour.size()<2
                        || std::abs(cv::contourArea(contour))<m_param->minContourArea->value()) continue;
                QVector<QPointF> points;
                points.reserve(static_cast<int>(contour.size()));
                for(const cv::Point &point:contour) points.append(QPointF(point.x,point.y));
                if(!contours.addValue(new XContour(points)))
                {
                    error="Contour result conversion failed";
                    return false;
                }
                accepted.push_back(contour);
            }
            cv::drawContours(annotated,accepted,-1,cv::Scalar(0,255,0),2);
            count=static_cast<int>(accepted.size());
        }
        else if(m_mode==HoughCircles)
        {
            if(m_param->circleDp->value()<1.0 || m_param->circleMinDistance->value()<=0.0
                    || m_param->circleEdgeThreshold->value()<=0.0
                    || m_param->circleCenterThreshold->value()<=0.0
                    || m_param->minRadius->value()<0 || m_param->maxRadius->value()<0
                    || (m_param->maxRadius->value()>0
                        && m_param->minRadius->value()>m_param->maxRadius->value()))
            {
                error="Hough circle parameters are invalid";
                return false;
            }
            std::vector<cv::Vec3f> detected;
            cv::HoughCircles(gray,detected,cv::HOUGH_GRADIENT,m_param->circleDp->value(),
                             m_param->circleMinDistance->value(),
                             m_param->circleEdgeThreshold->value(),
                             m_param->circleCenterThreshold->value(),
                             m_param->minRadius->value(),m_param->maxRadius->value());
            for(const cv::Vec3f &circle:detected)
            {
                if(circle[2]<=0.0F || !circles.addValue(
                            new XCircle2D(circle[0],circle[1],circle[2])))
                {
                    error="Circle result conversion failed";
                    return false;
                }
                cv::circle(annotated,cv::Point(cvRound(circle[0]),cvRound(circle[1])),
                           cvRound(circle[2]),cv::Scalar(0,255,0),2);
            }
            count=static_cast<int>(detected.size());
        }
        else if(m_mode==RenderBlobs)
        {
            if(m_param->binaryThreshold->value()<0.0 || m_param->binaryThreshold->value()>255.0
                    || (m_param->connectivity->value()!=4 && m_param->connectivity->value()!=8)
                    || m_param->connectedComponentsAlgorithm->value()<-1
                    || m_param->connectedComponentsAlgorithm->value()>5
                    || m_param->componentMinArea->value()<0.0
                    || m_param->componentMinArea->value()>m_param->componentMaxArea->value())
            {
                error="Connected-component threshold, connectivity, algorithm, or area is invalid";
                return false;
            }
            cv::Mat binary,labels,stats,centroids;
            cv::threshold(gray,binary,m_param->binaryThreshold->value(),255.0,cv::THRESH_BINARY);
            const int labelCount=cv::connectedComponentsWithStats(
                        binary,labels,stats,centroids,m_param->connectivity->value(),CV_32S,
                        m_param->connectedComponentsAlgorithm->value());
            for(int label=1;label<labelCount;++label)
            {
                const int area=stats.at<int>(label,cv::CC_STAT_AREA);
                if(area<m_param->componentMinArea->value()
                        || area>m_param->componentMaxArea->value()) continue;
                const int x=stats.at<int>(label,cv::CC_STAT_LEFT);
                const int y=stats.at<int>(label,cv::CC_STAT_TOP);
                const int width=stats.at<int>(label,cv::CC_STAT_WIDTH);
                const int height=stats.at<int>(label,cv::CC_STAT_HEIGHT);
                cv::Mat mask;
                cv::compare(labels,label,mask,cv::CMP_EQ);
                QImage regionImage;
                if(!OpenCvImageUtils::toQImage(mask,regionImage,error)
                        || !regions.addValue(new XRegion(regionImage))
                        || !rectangles.addValue(new XRect2D(x,y,width,height)))
                {
                    if(error.isEmpty()) error="Connected-component result conversion failed";
                    return false;
                }
                if(m_param->useRenderBlobs->value())
                    cv::rectangle(annotated,cv::Rect(x,y,width,height),cv::Scalar(0,255,0),2);
                ++count;
            }
        }
        else
        {
            if(m_param->binaryThreshold->value()<0.0 || m_param->binaryThreshold->value()>255.0
                    || m_param->rho->value()<=0.0 || m_param->theta->value()<=0.0
                    || m_param->theta->value()>360.0 || m_param->houghThreshold->value()<=0)
            {
                error="Hough line threshold or resolution is invalid";
                return false;
            }
            cv::Mat binary;
            cv::threshold(gray,binary,m_param->binaryThreshold->value(),255.0,cv::THRESH_BINARY);
            if(m_mode==HoughLines)
            {
                if(m_param->srn->value()<0.0 || m_param->stn->value()<0.0)
                {
                    error="Hough line srn and stn must be non-negative";
                    return false;
                }
                std::vector<cv::Vec2f> detected;
                cv::HoughLines(binary,detected,m_param->rho->value(),
                               qDegreesToRadians(m_param->theta->value()),
                               m_param->houghThreshold->value(),m_param->srn->value(),
                               m_param->stn->value());
                for(const cv::Vec2f &line:detected)
                {
                    const double a=std::cos(line[1]);
                    const double b=std::sin(line[1]);
                    const QPointF start(a*line[0]+10000.0*(-b),b*line[0]+10000.0*a);
                    const QPointF end(a*line[0]-10000.0*(-b),b*line[0]-10000.0*a);
                    if(!lines.addValue(new XLine2D(start,end)))
                    {
                        error="Hough line result conversion failed";
                        return false;
                    }
                    cv::line(annotated,cv::Point(cvRound(start.x()),cvRound(start.y())),
                             cv::Point(cvRound(end.x()),cvRound(end.y())),cv::Scalar(0,0,255),2);
                }
                count=static_cast<int>(detected.size());
            }
            else
            {
                if(m_param->minLineLength->value()<0.0 || m_param->maxLineGap->value()<0.0
                        || !qIsFinite(m_param->targetAngle->value())
                        || !qIsFinite(m_param->angleTolerance->value())
                        || (m_param->targetAngle->value()!=-1.0
                            && (m_param->targetAngle->value()<0.0
                                || m_param->targetAngle->value()>360.0
                                || m_param->angleTolerance->value()<=0.0
                                || m_param->angleTolerance->value()>360.0)))
                {
                    error="Probabilistic Hough line length, gap, or angle is invalid";
                    return false;
                }
                std::vector<cv::Vec4i> detected;
                cv::HoughLinesP(binary,detected,m_param->rho->value(),
                                qDegreesToRadians(m_param->theta->value()),
                                m_param->houghThreshold->value(),
                                m_param->minLineLength->value(),m_param->maxLineGap->value());
                for(const cv::Vec4i &line:detected)
                {
                    const double angle=qRadiansToDegrees(std::atan2(
                                      static_cast<double>(line[3]-line[1]),
                                      static_cast<double>(line[2]-line[0])));
                    if(m_param->targetAngle->value()!=-1.0
                            && lineAngleDistance(angle,m_param->targetAngle->value())
                               >m_param->angleTolerance->value()) continue;
                    const QPointF start(line[0],line[1]);
                    const QPointF end(line[2],line[3]);
                    if(start==end) continue;
                    if(!lines.addValue(new XLine2D(start,end)))
                    {
                        error="Probabilistic Hough line result conversion failed";
                        return false;
                    }
                    cv::line(annotated,cv::Point(line[0],line[1]),cv::Point(line[2],line[3]),
                             cv::Scalar(0,0,255),2);
                    ++count;
                }
            }
        }

        if(!OpenCvImageUtils::toQImage(annotated,candidate,error)
                || !m_candidateKeyPoints.setData(&keyPoints)
                || !m_candidateContours.setData(&contours)
                || !m_candidateCircles.setData(&circles)
                || !m_candidateRegions.setData(&regions)
                || !m_candidateRectangles.setData(&rectangles)
                || !m_candidateLines.setData(&lines))
        {
            if(error.isEmpty()) error="Region detector candidate preparation failed";
            return false;
        }
        m_candidateCount=count;
        return true;
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV region detection failed: %1").arg(exception.what());
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
