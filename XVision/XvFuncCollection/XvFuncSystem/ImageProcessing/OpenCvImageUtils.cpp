#include "OpenCvImageUtils.h"

#include "XvOpenCvImageInterop.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace OpenCvImageUtils
{
bool fromQImage(const QImage &source,cv::Mat &target,QString &error)
{
    if(XvOpenCvImageInterop::toMat(source,target,&error)) return true;
    if(source.isNull()) return false;
    const QImage::Format format=source.hasAlphaChannel()
            ?QImage::Format_ARGB32:QImage::Format_RGB888;
    const QImage normalized=source.convertToFormat(format);
    return XvOpenCvImageInterop::toMat(normalized,target,&error);
}

bool toQImage(const cv::Mat &source,QImage &target,QString &error)
{
    return XvOpenCvImageInterop::toQImage(source,target,&error);
}

bool toGray(const cv::Mat &source,cv::Mat &target,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(source.empty())
    {
        error="Source matrix is empty";
        return false;
    }
    if(source.channels()==1) target=source.clone();
    else if(source.channels()==3) cv::cvtColor(source,target,cv::COLOR_BGR2GRAY);
    else if(source.channels()==4) cv::cvtColor(source,target,cv::COLOR_BGRA2GRAY);
    else
    {
        error="Image must have one, three, or four channels";
        return false;
    }
    error.clear();
    return true;
#else
    Q_UNUSED(source)
    Q_UNUSED(target)
    error="OpenCV backend is disabled";
    return false;
#endif
}

bool toBgr(const cv::Mat &source,cv::Mat &target,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(source.empty())
    {
        error="Source matrix is empty";
        return false;
    }
    if(source.channels()==3) target=source.clone();
    else if(source.channels()==1) cv::cvtColor(source,target,cv::COLOR_GRAY2BGR);
    else if(source.channels()==4) cv::cvtColor(source,target,cv::COLOR_BGRA2BGR);
    else
    {
        error="Image must have one, three, or four channels";
        return false;
    }
    error.clear();
    return true;
#else
    Q_UNUSED(source)
    Q_UNUSED(target)
    error="OpenCV backend is disabled";
    return false;
#endif
}

bool toMask(const cv::Mat &source,cv::Mat &target,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat gray;
    if(!toGray(source,gray,error)) return false;
    cv::compare(gray,0,target,cv::CMP_GT);
    error.clear();
    return true;
#else
    Q_UNUSED(source)
    Q_UNUSED(target)
    error="OpenCV backend is disabled";
    return false;
#endif
}

bool validateOddKernel(int width,int height,QString &error)
{
    if(width<=0 || height<=0 || width%2==0 || height%2==0)
    {
        error="Kernel width and height must be positive odd integers";
        return false;
    }
    error.clear();
    return true;
}
}
