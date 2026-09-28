#include "XvOpenCvImageInterop.h"

#include <QImage>

#include <utility>

namespace
{
void setError(QString *error,const QString &message)
{
    if(error) *error=message;
}
}

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace XvOpenCvImageInterop
{
bool toMat(const QImage &source,cv::Mat &target,QString *error)
{
    try
    {
        if(source.isNull())
        {
            setError(error,"source image is empty");
            return false;
        }

        cv::Mat candidate;
        switch(source.format())
        {
        case QImage::Format_Grayscale8:
        {
            const cv::Mat view(source.height(),source.width(),CV_8UC1,
                               const_cast<uchar*>(source.constBits()),source.bytesPerLine());
            candidate=view.clone();
            break;
        }
        case QImage::Format_RGB888:
        {
            const cv::Mat view(source.height(),source.width(),CV_8UC3,
                               const_cast<uchar*>(source.constBits()),source.bytesPerLine());
            cv::cvtColor(view,candidate,cv::COLOR_RGB2BGR);
            break;
        }
        case QImage::Format_RGBA8888:
        {
            const cv::Mat view(source.height(),source.width(),CV_8UC4,
                               const_cast<uchar*>(source.constBits()),source.bytesPerLine());
            cv::cvtColor(view,candidate,cv::COLOR_RGBA2BGRA);
            break;
        }
        case QImage::Format_ARGB32:
        {
            const QImage normalized=source.convertToFormat(QImage::Format_RGBA8888);
            const cv::Mat view(normalized.height(),normalized.width(),CV_8UC4,
                               const_cast<uchar*>(normalized.constBits()),normalized.bytesPerLine());
            cv::cvtColor(view,candidate,cv::COLOR_RGBA2BGRA);
            break;
        }
        default:
            setError(error,"unsupported QImage format; expected Grayscale8, RGB888, RGBA8888 or ARGB32");
            return false;
        }
        target=std::move(candidate);
        setError(error,QString());
        return true;
    }
    catch(const cv::Exception &exception)
    {
        setError(error,QString("OpenCV image conversion failed: %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }
}

bool toQImage(const cv::Mat &source,QImage &target,QString *error)
{
    try
    {
        if(source.empty() || source.depth()!=CV_8U
                || (source.channels()!=1 && source.channels()!=3 && source.channels()!=4))
        {
            setError(error,"OpenCV image must be a non-empty 8-bit one, three or four channel matrix");
            return false;
        }

        QImage candidate;
        if(source.channels()==1)
        {
            candidate=QImage(source.data,source.cols,source.rows,static_cast<int>(source.step),
                             QImage::Format_Grayscale8).copy();
        }
        else
        {
            cv::Mat rgb;
            if(source.channels()==3) cv::cvtColor(source,rgb,cv::COLOR_BGR2RGB);
            else cv::cvtColor(source,rgb,cv::COLOR_BGRA2RGBA);
            candidate=QImage(rgb.data,rgb.cols,rgb.rows,static_cast<int>(rgb.step),
                             source.channels()==3?QImage::Format_RGB888:QImage::Format_RGBA8888).copy();
        }
        if(candidate.isNull())
        {
            setError(error,"OpenCV image conversion produced an empty QImage");
            return false;
        }
        target=std::move(candidate);
        setError(error,QString());
        return true;
    }
    catch(const cv::Exception &exception)
    {
        setError(error,QString("OpenCV image conversion failed: %1").arg(QString::fromUtf8(exception.what())));
        return false;
    }
}
}
#else
namespace XvOpenCvImageInterop
{
bool toMat(const QImage &,cv::Mat &,QString *error)
{
    setError(error,"OpenCV backend is disabled; configure with XVISION_ENABLE_OPENCV=ON");
    return false;
}

bool toQImage(const cv::Mat &,QImage &,QString *error)
{
    setError(error,"OpenCV backend is disabled; configure with XVISION_ENABLE_OPENCV=ON");
    return false;
}
}
#endif
