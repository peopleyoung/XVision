#include "XLanguage.h"
#include "OImageTransform.h"

#include "OpenCvImageUtils.h"

#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

OImageTransformParam::OImageTransformParam()
{
    flipCode=new XInt("flipCode",1,this,"翻转方向");
    repeatX=new XInt("repeatX",1,this,"水平重复次数");
    repeatY=new XInt("repeatY",1,this,"垂直重复次数");
    outputWidth=new XInt("outputWidth",0,this,"输出宽度");
    outputHeight=new XInt("outputHeight",0,this,"输出高度");
    scaleX=new XReal("scaleX",1.0,this,"水平缩放比例");
    scaleY=new XReal("scaleY",1.0,this,"垂直缩放比例");
    interpolation=new XInt("interpolation",1,this,"插值方式");
    rotateCode=new XInt("rotateCode",0,this,"旋转方向");
    normalizedPoints=new XBool("normalizedPoints",true,this,"归一化特征点");
    const double xDefaults[]={0.0,1.0,1.0,0.0};
    const double yDefaults[]={0.0,0.0,1.0,1.0};
    for(int index=0;index<4;++index)
    {
        sourceX.append(new XReal(QString("sourceX%1").arg(index),xDefaults[index],this,
                                 QString("Source X%1").arg(index)));
        sourceY.append(new XReal(QString("sourceY%1").arg(index),yDefaults[index],this,
                                 QString("Source Y%1").arg(index)));
        destinationX.append(new XReal(QString("destinationX%1").arg(index),xDefaults[index],this,
                                      QString("Destination X%1").arg(index)));
        destinationY.append(new XReal(QString("destinationY%1").arg(index),yDefaults[index],this,
                                      QString("Destination Y%1").arg(index)));
    }
}

OImageTransform::OImageTransform(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageTransformParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageTransform";
    _funcName=getUiText("OpenCV Image Transform");
}

OImageTransform::~OImageTransform()
{
    delete m_param;
    delete m_result;
}

QStringList OImageTransform::pointParameterNames(int count) const
{
    QStringList names={"inputImage","outputWidth","outputHeight","normalizedPoints"};
    for(int index=0;index<count;++index)
        names.append({QString("sourceX%1").arg(index),QString("sourceY%1").arg(index),
                      QString("destinationX%1").arg(index),QString("destinationY%1").arg(index)});
    return names;
}

QStringList OImageTransform::activeParameterNames() const
{
    switch(m_mode)
    {
    case Flip: return {"inputImage","flipCode"};
    case Homography:
    case WarpPerspective: return pointParameterNames(4);
    case Repeat: return {"inputImage","repeatX","repeatY"};
    case Resize:
        return {"inputImage","outputWidth","outputHeight","scaleX","scaleY","interpolation"};
    case Rotate: return {"inputImage","rotateCode"};
    case Transpose: return {"inputImage"};
    case WarpAffine: return pointParameterNames(3);
    }
    return {"inputImage"};
}

bool OImageTransform::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input,output;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    try
    {
        if(m_mode==Flip)
        {
            if(m_param->flipCode->value()<-1 || m_param->flipCode->value()>1)
            {
                error="Flip code must be -1, 0, or 1";
                return false;
            }
            cv::flip(input,output,m_param->flipCode->value());
        }
        else if(m_mode==Repeat)
        {
            if(m_param->repeatX->value()<1 || m_param->repeatY->value()<1
                    || m_param->repeatX->value()>100 || m_param->repeatY->value()>100)
            {
                error="Repeat counts must be in [1, 100]";
                return false;
            }
            cv::repeat(input,m_param->repeatY->value(),m_param->repeatX->value(),output);
        }
        else if(m_mode==Resize)
        {
            static const int interpolations[]={cv::INTER_NEAREST,cv::INTER_LINEAR,cv::INTER_CUBIC,
                                                cv::INTER_AREA,cv::INTER_LANCZOS4};
            const int interpolation=m_param->interpolation->value();
            if(interpolation<0 || interpolation>=static_cast<int>(sizeof(interpolations)/sizeof(interpolations[0])))
            {
                error="Resize interpolation must be in [0, 4]";
                return false;
            }
            const int width=m_param->outputWidth->value(),height=m_param->outputHeight->value();
            if((width>0)!=(height>0) || width<0 || height<0)
            {
                error="Resize width and height must both be positive or both be zero";
                return false;
            }
            if(width>0) cv::resize(input,output,cv::Size(width,height),0.0,0.0,interpolations[interpolation]);
            else
            {
                if(m_param->scaleX->value()<=0.0 || m_param->scaleY->value()<=0.0)
                {
                    error="Resize scale factors must be positive";
                    return false;
                }
                cv::resize(input,output,cv::Size(),m_param->scaleX->value(),m_param->scaleY->value(),
                           interpolations[interpolation]);
            }
        }
        else if(m_mode==Rotate)
        {
            static const int rotations[]={cv::ROTATE_90_CLOCKWISE,cv::ROTATE_180,
                                          cv::ROTATE_90_COUNTERCLOCKWISE};
            const int rotation=m_param->rotateCode->value();
            if(rotation<0 || rotation>=static_cast<int>(sizeof(rotations)/sizeof(rotations[0])))
            {
                error="Rotate code must be in [0, 2]";
                return false;
            }
            cv::rotate(input,output,rotations[rotation]);
        }
        else if(m_mode==Transpose)
            cv::transpose(input,output);
        else
        {
            const int count=m_mode==WarpAffine?3:4;
            std::vector<cv::Point2f> sourcePoints,destinationPoints;
            const double xScale=input.cols>1?input.cols-1:1;
            const double yScale=input.rows>1?input.rows-1:1;
            for(int index=0;index<count;++index)
            {
                double sx=m_param->sourceX[index]->value(),sy=m_param->sourceY[index]->value();
                double dx=m_param->destinationX[index]->value(),dy=m_param->destinationY[index]->value();
                if(!qIsFinite(sx) || !qIsFinite(sy) || !qIsFinite(dx) || !qIsFinite(dy))
                {
                    error="Transform points must be finite";
                    return false;
                }
                if(m_param->normalizedPoints->value())
                {
                    sx*=xScale; sy*=yScale; dx*=xScale; dy*=yScale;
                }
                sourcePoints.emplace_back(static_cast<float>(sx),static_cast<float>(sy));
                destinationPoints.emplace_back(static_cast<float>(dx),static_cast<float>(dy));
            }
            const int width=m_param->outputWidth->value()==0?input.cols:m_param->outputWidth->value();
            const int height=m_param->outputHeight->value()==0?input.rows:m_param->outputHeight->value();
            if(width<=0 || height<=0)
            {
                error="Transform output size must be positive";
                return false;
            }
            if(m_mode==WarpAffine)
            {
                const cv::Mat matrix=cv::getAffineTransform(sourcePoints,destinationPoints);
                const double determinant=matrix.at<double>(0,0)*matrix.at<double>(1,1)
                        -matrix.at<double>(0,1)*matrix.at<double>(1,0);
                if(qAbs(determinant)<1e-12)
                {
                    error="Transform points produce a degenerate matrix";
                    return false;
                }
                cv::warpAffine(input,output,matrix,cv::Size(width,height));
            }
            else
            {
                cv::Mat matrix;
                if(m_mode==Homography) matrix=cv::findHomography(sourcePoints,destinationPoints,0);
                else matrix=cv::getPerspectiveTransform(sourcePoints,destinationPoints);
                if(matrix.empty() || qAbs(cv::determinant(matrix))<1e-12)
                {
                    error="Transform points produce a degenerate matrix";
                    return false;
                }
                cv::warpPerspective(input,output,matrix,cv::Size(width,height));
            }
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV transform failed: %1").arg(QString::fromUtf8(exception.what()));
        return false;
    }
    return OpenCvImageUtils::toQImage(output,candidate,error);
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
