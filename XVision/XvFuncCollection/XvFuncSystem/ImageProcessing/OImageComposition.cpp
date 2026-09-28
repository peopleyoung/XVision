#include "OImageComposition.h"

#include "OpenCvImageUtils.h"

#include <QFileInfo>

#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/stitching.hpp>
#endif

using namespace XvCore;

OImageComposition::OImageComposition(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageCompositionParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageComposition";
    _funcName="OpenCV Image Composition";
}

OImageComposition::~OImageComposition()
{
    delete m_param;
    delete m_result;
}

QStringList OImageComposition::activeParameterNames() const
{
    if(m_mode==Background)
        return {"inputImage","backgroundImage","backgroundPath","maskImage","blurBackground"};
    if(m_mode==SeamlessClone)
        return {"inputImage","backgroundImage","backgroundPath","maskImage","centerX","centerY"};
    return {"inputImage","backgroundImage","images","stitchMode"};
}

bool OImageComposition::backgroundImage(QImage &image,QString &error) const
{
    image=m_param->backgroundImage->value();
    if(!image.isNull()) return true;
    const QString path=m_param->backgroundPath->value().trimmed();
    if(path.isEmpty())
    {
        error="Background image or background path is required";
        return false;
    }
    const QFileInfo info(path);
    if(!info.isAbsolute() || !info.exists() || !info.isFile() || !info.isReadable()
            || !image.load(path))
    {
        error="Background path must name an absolute readable image";
        return false;
    }
    return true;
}

bool OImageComposition::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    try
    {
        cv::Mat output;
        if(m_mode==Background || m_mode==SeamlessClone)
        {
            QImage backgroundValue;
            if(!backgroundImage(backgroundValue,error)) return false;
            cv::Mat background;
            if(!OpenCvImageUtils::fromQImage(backgroundValue,background,error)) return false;
            cv::Mat foregroundBgr,backgroundBgr;
            if(!OpenCvImageUtils::toBgr(input,foregroundBgr,error)
                    || !OpenCvImageUtils::toBgr(background,backgroundBgr,error)) return false;
            cv::Mat mask;
            if(!m_param->maskImage->value().isNull())
            {
                cv::Mat sourceMask;
                if(!OpenCvImageUtils::fromQImage(m_param->maskImage->value(),sourceMask,error)
                        || !OpenCvImageUtils::toMask(sourceMask,mask,error)) return false;
            }
            else if(!OpenCvImageUtils::toMask(input,mask,error)) return false;

            if(m_mode==Background)
            {
                cv::resize(backgroundBgr,output,foregroundBgr.size());
                if(mask.size()!=foregroundBgr.size()) cv::resize(mask,mask,foregroundBgr.size(),0,0,cv::INTER_NEAREST);
                if(m_param->blurBackground->value()) cv::GaussianBlur(output,output,cv::Size(15,15),0.0);
                foregroundBgr.copyTo(output,mask);
            }
            else
            {
                if(mask.size()!=foregroundBgr.size())
                {
                    error="Seamless-clone mask size must match the source image";
                    return false;
                }
                int centerX=m_param->centerX->value(),centerY=m_param->centerY->value();
                if(centerX<0) centerX=backgroundBgr.cols/2;
                if(centerY<0) centerY=backgroundBgr.rows/2;
                if(centerX<0 || centerY<0 || centerX>=backgroundBgr.cols || centerY>=backgroundBgr.rows)
                {
                    error="Seamless-clone center lies outside the background image";
                    return false;
                }
                cv::seamlessClone(foregroundBgr,backgroundBgr,mask,cv::Point(centerX,centerY),
                                  output,cv::NORMAL_CLONE);
            }
        }
        else
        {
            if(m_param->stitchMode->value()<0 || m_param->stitchMode->value()>1)
            {
                error="Stitch mode must be 0 (panorama) or 1 (scans)";
                return false;
            }
            std::vector<cv::Mat> images;
            cv::Mat bgr;
            if(!OpenCvImageUtils::toBgr(input,bgr,error)) return false;
            images.push_back(bgr);
            if(!m_param->backgroundImage->value().isNull())
            {
                cv::Mat additional,additionalBgr;
                if(!OpenCvImageUtils::fromQImage(m_param->backgroundImage->value(),additional,error)
                        || !OpenCvImageUtils::toBgr(additional,additionalBgr,error)) return false;
                images.push_back(additionalBgr);
            }
            for(XObject *object:m_param->images->values())
            {
                auto image=dynamic_cast<XImage*>(object);
                if(!image || image->value().isNull())
                {
                    error="Stitching image list contains an invalid image";
                    return false;
                }
                cv::Mat additional,additionalBgr;
                if(!OpenCvImageUtils::fromQImage(image->value(),additional,error)
                        || !OpenCvImageUtils::toBgr(additional,additionalBgr,error)) return false;
                images.push_back(additionalBgr);
            }
            if(images.size()<2)
            {
                error="Stitching requires at least two images";
                return false;
            }
            const cv::Stitcher::Mode mode=m_param->stitchMode->value()==0
                    ?cv::Stitcher::PANORAMA:cv::Stitcher::SCANS;
            const cv::Ptr<cv::Stitcher> stitcher=cv::Stitcher::create(mode);
            const cv::Stitcher::Status status=stitcher->stitch(images,output);
            if(status!=cv::Stitcher::OK)
            {
                error=QString("OpenCV stitching failed with status %1").arg(static_cast<int>(status));
                return false;
            }
        }
        return OpenCvImageUtils::toQImage(output,candidate,error);
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV composition failed: %1").arg(QString::fromUtf8(exception.what()));
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
