#include "ORectification.h"

#include "OpenCvImageUtils.h"
#include "OpenCvTemplateRectificationUtils.h"
#include "XLanguage.h"

#include <QImage>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

namespace
{
constexpr double Pi=3.14159265358979323846;
const std::array<double,9> IdentityTransform={1.0,0.0,0.0,
                                              0.0,1.0,0.0,
                                              0.0,0.0,1.0};
const std::array<double,9> EmptyTransform={0.0,0.0,0.0,
                                           0.0,0.0,0.0,
                                           0.0,0.0,0.0};

bool validateParameters(const ORectificationParam *param,
                        ORectification::Mode mode,QString &error)
{
    if(!param || !param->rotatedRect || !param->foregroundThreshold
            || !param->invertForeground || !param->minimumForegroundArea
            || !param->outputWidth || !param->outputHeight
            || !param->rectificationInterpolation || !param->borderValue)
    {
        error="Rectification parameters are incomplete";
        return false;
    }
    if(mode<ORectification::ForegroundRotatedRect || mode>ORectification::RotatedRect)
    {
        error="Rectification mode is invalid";
        return false;
    }
    if(mode!=ORectification::RotatedRect
            && (param->foregroundThreshold->value()<0
                || param->foregroundThreshold->value()>255
                || !qIsFinite(param->minimumForegroundArea->value())
                || param->minimumForegroundArea->value()<=0.0
                || param->minimumForegroundArea->value()>1.0e12))
    {
        error="Foreground threshold or minimum area is invalid";
        return false;
    }
    if(mode!=ORectification::ForegroundExtract)
    {
        if(param->rectificationInterpolation->value()<0
                || param->rectificationInterpolation->value()>3
                || !qIsFinite(param->borderValue->value())
                || param->borderValue->value()<0.0
                || param->borderValue->value()>255.0)
        {
            error="Rectification interpolation or border is invalid";
            return false;
        }
        const int width=param->outputWidth->value();
        const int height=param->outputHeight->value();
        if((width==0)!=(height==0) || width<0 || height<0
                || width>OpenCvTemplateRectificationUtils::MaxTemplateDimension
                || height>OpenCvTemplateRectificationUtils::MaxTemplateDimension
                || (width>0 && qint64(width)*qint64(height)
                    >OpenCvTemplateRectificationUtils::MaxTemplatePixels))
        {
            error="Output width and height must both be zero or supported positive dimensions";
            return false;
        }
    }
    QSize outputSize;
    if(mode==ORectification::RotatedRect
            && !OpenCvTemplateRectificationUtils::roiOutputSize(
                *param->rotatedRect,param->outputWidth->value(),
                param->outputHeight->value(),outputSize,error)) return false;
    return true;
}

#if defined(XVISION_ENABLE_OPENCV)
bool foregroundInfo(const cv::Mat &input,const ORectificationParam *param,
                    cv::Mat &mask,XRotateRectRoi &roi,QString &error)
{
    cv::Mat gray,thresholded;
    if(!OpenCvImageUtils::toGray(input,gray,error)) return false;
    const int binaryType=param->invertForeground->value()
            ?cv::THRESH_BINARY_INV:cv::THRESH_BINARY;
    const int thresholdType=param->foregroundThreshold->value()==0
            ?binaryType|cv::THRESH_OTSU:binaryType;
    cv::threshold(gray,thresholded,param->foregroundThreshold->value(),255,thresholdType);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholded.clone(),contours,cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);
    int selected=-1;
    double selectedArea=-1.0;
    cv::Rect selectedBounds;
    for(size_t index=0;index<contours.size();++index)
    {
        const double area=qAbs(cv::contourArea(contours[index]));
        if(!qIsFinite(area) || area<param->minimumForegroundArea->value()) continue;
        const cv::Rect bounds=cv::boundingRect(contours[index]);
        if(selected<0 || area>selectedArea
                || (qFuzzyCompare(area+1.0,selectedArea+1.0)
                    && (bounds.y<selectedBounds.y
                        || (bounds.y==selectedBounds.y && bounds.x<selectedBounds.x))))
        {
            selected=int(index);
            selectedArea=area;
            selectedBounds=bounds;
        }
    }
    if(selected<0)
    {
        error="No foreground contour satisfies the minimum area";
        return false;
    }
    mask=cv::Mat::zeros(input.rows,input.cols,CV_8UC1);
    cv::drawContours(mask,contours,selected,cv::Scalar(255),cv::FILLED);
    const cv::RotatedRect rectangle=cv::minAreaRect(contours[size_t(selected)]);
    if(rectangle.size.width<=0.0F || rectangle.size.height<=0.0F
            || !roi.setValue(rectangle.center.x,rectangle.center.y,
                             rectangle.size.width*0.5,rectangle.size.height*0.5,
                             -rectangle.angle*Pi/180.0))
    {
        error="Detected foreground rotated rectangle is invalid";
        return false;
    }
    return true;
}

bool rectify(const cv::Mat &input,const XRotateRectRoi &roi,
             const ORectificationParam *param,cv::Mat &output,
             std::array<double,9> &transformValues,QString &error)
{
    QVector<QPointF> corners;
    QSize size;
    if(!OpenCvTemplateRectificationUtils::roiCorners(roi,corners,error)
            || !OpenCvTemplateRectificationUtils::roiOutputSize(
                roi,param->outputWidth->value(),param->outputHeight->value(),
                size,error)) return false;
    if(size.width()<2 || size.height()<2)
    {
        error="Rectification output must be at least 2x2 pixels";
        return false;
    }
    std::vector<cv::Point2f> sourcePoints,destinationPoints;
    for(const QPointF &point:corners)
        sourcePoints.emplace_back(float(point.x()),float(point.y()));
    destinationPoints={{0.0F,0.0F},{float(size.width()-1),0.0F},
                       {float(size.width()-1),float(size.height()-1)},
                       {0.0F,float(size.height()-1)}};
    cv::Mat transform=cv::getPerspectiveTransform(sourcePoints,destinationPoints);
    if(transform.empty() || !cv::checkRange(transform)
            || qAbs(cv::determinant(transform))<1e-12)
    {
        error="Rectification transform is degenerate";
        return false;
    }
    static const int interpolations[]={cv::INTER_NEAREST,cv::INTER_LINEAR,
                                       cv::INTER_CUBIC,cv::INTER_LANCZOS4};
    cv::warpPerspective(input,output,transform,cv::Size(size.width(),size.height()),
                        interpolations[param->rectificationInterpolation->value()],
                        cv::BORDER_CONSTANT,cv::Scalar::all(param->borderValue->value()));
    if(output.empty())
    {
        error="Rectification produced an empty image";
        return false;
    }
    for(int row=0;row<3;++row)
        for(int column=0;column<3;++column)
            transformValues[size_t(row*3+column)]=transform.at<double>(row,column);
    return true;
}
#endif
}

ORectificationParam::ORectificationParam()
{
    rotatedRect=new XRotateRectRoi("rotatedRect",0.0,0.0,20.0,20.0,0.0,this,
                                   getLang("XvFuncSystem_ORectification_Roi","旋转矩形"));
    foregroundThreshold=new XInt("foregroundThreshold",0,this,
                                 getLang("XvFuncSystem_ORectification_Threshold","前景阈值(0为Otsu)"));
    invertForeground=new XBool("invertForeground",false,this,
                               getLang("XvFuncSystem_ORectification_Invert","反相前景"));
    minimumForegroundArea=new XReal("minimumForegroundArea",25.0,this,
                                    getLang("XvFuncSystem_ORectification_MinArea","最小前景面积"));
    outputWidth=new XInt("outputWidth",0,this,
                         getLang("XvFuncSystem_ORectification_Width","输出宽度"));
    outputHeight=new XInt("outputHeight",0,this,
                          getLang("XvFuncSystem_ORectification_Height","输出高度"));
    rectificationInterpolation=new XInt("rectificationInterpolation",1,this,
                                         getLang("XvFuncSystem_ORectification_Interpolation","插值方式"));
    borderValue=new XReal("borderValue",0.0,this,
                          getLang("XvFuncSystem_ORectification_Border","边界填充值"));
}

ORectificationResult::ORectificationResult()
{
    foregroundMask=new XImage("foregroundMask",QImage(),this,
                              getLang("XvFuncSystem_ORectification_Mask","前景掩膜"));
    foregroundRegion=new XRegion("foregroundRegion",QImage(),this,
                                 getLang("XvFuncSystem_ORectification_Region","前景区域"));
    rectifiedRoi=new XRotateRectRoi("rectifiedRoi",0.0,0.0,1.0,1.0,0.0,this,
                                    getLang("XvFuncSystem_ORectification_ResultRoi","矫正区域"));
    QByteArray bytes(int(sizeof(double)*IdentityTransform.size()),'\0');
    std::memcpy(bytes.data(),IdentityTransform.data(),size_t(bytes.size()));
    transform=new XTensor("transform","float64",{3,3},bytes,this,
                          getLang("XvFuncSystem_ORectification_Transform","变换矩阵"));
}

ORectification::ORectification(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new ORectificationParam()),
      m_result(new ORectificationResult())
{
    _funcRole="ORectification";
    _funcName=getLang("XvFuncSystem_ORectification_Name","OpenCV图像矫正");
    _funcType=EXvFuncType::ImageProcessing;
    QString error;
    OpenCvTemplateRectificationUtils::setTransformTensor(
                IdentityTransform,m_candidateTransform,error);
}

ORectification::~ORectification()
{
    delete m_param;
    delete m_result;
}

QStringList ORectification::activeParameterNames() const
{
    QStringList names={"inputImage"};
    if(m_mode==RotatedRect) names << "rotatedRect";
    else names << "foregroundThreshold" << "invertForeground"
               << "minimumForegroundArea";
    if(m_mode!=ForegroundExtract)
        names << "outputWidth" << "outputHeight" << "rectificationInterpolation"
              << "borderValue";
    return names;
}

bool ORectification::processImage(const QImage &source,QImage &candidate,QString &error)
{
    m_candidateMask=QImage();
    m_candidateRegion.setValue(QImage());
    m_candidateRoi.setValue(0.0,0.0,1.0,1.0,0.0);
    if(!OpenCvTemplateRectificationUtils::setTransformTensor(
                EmptyTransform,m_candidateTransform,error)) return false;
    if(!validateParameters(m_param,m_mode,error)) return false;

#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    try
    {
        cv::Mat mask,output;
        XRotateRectRoi roi;
        std::array<double,9> transformValues=EmptyTransform;
        if(m_mode==RotatedRect)
        {
            if(!roi.setData(m_param->rotatedRect)
                    || !rectify(input,roi,m_param,output,transformValues,error)) return false;
        }
        else
        {
            if(!foregroundInfo(input,m_param,mask,roi,error)) return false;
            if(!OpenCvImageUtils::toQImage(mask,m_candidateMask,error)
                    || m_candidateMask.format()!=QImage::Format_Grayscale8
                    || !m_candidateRegion.setValue(m_candidateMask))
            {
                if(error.isEmpty()) error="Foreground mask conversion failed";
                return false;
            }
            if(m_mode==ForegroundExtract)
            {
                output=cv::Mat::zeros(input.size(),input.type());
                input.copyTo(output,mask);
                transformValues=IdentityTransform;
            }
            else if(!rectify(input,roi,m_param,output,transformValues,error)) return false;
        }
        if(!m_candidateRoi.setData(&roi)
                || !OpenCvTemplateRectificationUtils::setTransformTensor(
                    transformValues,m_candidateTransform,error)
                || !OpenCvImageUtils::toQImage(output,candidate,error))
        {
            if(error.isEmpty()) error="Rectification candidates could not be prepared";
            return false;
        }
        return !candidate.isNull();
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV rectification failed: %1").arg(exception.what());
    }
    catch(const std::exception &exception)
    {
        error=QString("Rectification failed: %1").arg(exception.what());
    }
    catch(...)
    {
        error="Rectification failed with an unknown error";
    }
    return false;
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}

bool ORectification::commitAdditionalResults(QString &error)
{
    if(!m_result->foregroundRegion->setData(&m_candidateRegion)
            || !m_result->rectifiedRoi->setData(&m_candidateRoi)
            || !m_result->transform->setData(&m_candidateTransform))
    {
        error="Rectification results could not be committed";
        return false;
    }
    m_result->foregroundMask->setValue(m_candidateMask);
    error.clear();
    return true;
}

void ORectification::clearResultsAfterFailure()
{
    m_result->outputImage->setValue(QImage());
    m_result->foregroundMask->setValue(QImage());
    m_result->foregroundRegion->setValue(QImage());
    m_result->rectifiedRoi->setValue(0.0,0.0,1.0,1.0,0.0);
    QString error;
    OpenCvTemplateRectificationUtils::setTransformTensor(
                EmptyTransform,*m_result->transform,error);
}
