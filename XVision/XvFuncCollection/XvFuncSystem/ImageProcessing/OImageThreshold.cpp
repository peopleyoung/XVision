#include "XLanguage.h"
#include "OImageThreshold.h"

#include "OpenCvImageUtils.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

OImageThreshold::OImageThreshold(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageThresholdParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageThreshold";
    _funcName=getUiText("OpenCV Image Threshold");
}

OImageThreshold::~OImageThreshold()
{
    delete m_param;
    delete m_result;
}

QStringList OImageThreshold::activeParameterNames() const
{
    if(m_mode==Threshold)
        return {"inputImage","threshold","maxValue","thresholdType"};
    return {"inputImage","threshold","comparison"};
}

bool OImageThreshold::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(!qIsFinite(m_param->threshold->value()) || !qIsFinite(m_param->maxValue->value()))
    {
        error="Threshold values must be finite";
        return false;
    }
    cv::Mat input,gray,output;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)) return false;
    try
    {
        if(m_mode==Threshold)
        {
            static const int types[]={cv::THRESH_BINARY,cv::THRESH_BINARY_INV,cv::THRESH_TRUNC,
                                     cv::THRESH_TOZERO,cv::THRESH_TOZERO_INV};
            const int type=m_param->thresholdType->value();
            if(type<0 || type>=static_cast<int>(sizeof(types)/sizeof(types[0])))
            {
                error="Threshold type must be in [0, 4]";
                return false;
            }
            if(m_param->maxValue->value()<0.0 || m_param->maxValue->value()>255.0)
            {
                error="Threshold maximum value must be in [0, 255]";
                return false;
            }
            cv::threshold(gray,output,m_param->threshold->value(),m_param->maxValue->value(),types[type]);
        }
        else
        {
            static const int comparisons[]={cv::CMP_GT,cv::CMP_GE,cv::CMP_EQ,
                                            cv::CMP_LE,cv::CMP_LT,cv::CMP_NE};
            const int comparison=m_param->comparison->value();
            if(comparison<0 || comparison>=static_cast<int>(sizeof(comparisons)/sizeof(comparisons[0])))
            {
                error="Pixel comparison must be in [0, 5]";
                return false;
            }
            cv::compare(gray,m_param->threshold->value(),output,comparisons[comparison]);
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV threshold failed: %1").arg(QString::fromUtf8(exception.what()));
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
