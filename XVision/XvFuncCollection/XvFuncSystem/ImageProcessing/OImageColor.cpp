#include "XLanguage.h"
#include "OImageColor.h"

#include "OpenCvImageUtils.h"

#include <vector>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

OImageColor::OImageColor(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageColorParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageColor";
    _funcName=getUiText("OpenCV Image Color");
}

OImageColor::~OImageColor()
{
    delete m_param;
    delete m_result;
}

QStringList OImageColor::activeParameterNames() const
{
    switch(m_mode)
    {
    case Convert:
        return {"inputImage","conversion"};
    case HsvInRange:
        return {"inputImage","hueMin","hueMax","saturationMin","saturationMax",
                "valueMin","valueMax"};
    case Normalize:
        return {"inputImage","normalizeMin","normalizeMax"};
    case SplitBgr:
        return {"inputImage","channel","mergeChannels"};
    }
    return {"inputImage"};
}

bool OImageColor::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    cv::Mat output;
    try
    {
        switch(m_mode)
        {
        case Convert:
            if(m_param->conversion->value()==0)
            {
                if(!OpenCvImageUtils::toGray(input,output,error)) return false;
            }
            else if(m_param->conversion->value()==1)
            {
                if(!OpenCvImageUtils::toBgr(input,output,error)) return false;
            }
            else if(m_param->conversion->value()==2)
            {
                cv::Mat bgr;
                if(!OpenCvImageUtils::toBgr(input,bgr,error)) return false;
                cv::cvtColor(bgr,output,cv::COLOR_BGR2BGRA);
            }
            else
            {
                error="Conversion must be 0 (gray), 1 (BGR), or 2 (BGRA)";
                return false;
            }
            break;
        case HsvInRange:
        {
            const int h0=m_param->hueMin->value(),h1=m_param->hueMax->value();
            const int s0=m_param->saturationMin->value(),s1=m_param->saturationMax->value();
            const int v0=m_param->valueMin->value(),v1=m_param->valueMax->value();
            if(h0<0 || h1>179 || h0>h1 || s0<0 || s1>255 || s0>s1
                    || v0<0 || v1>255 || v0>v1)
            {
                error="HSV ranges are invalid";
                return false;
            }
            cv::Mat bgr,hsv;
            if(!OpenCvImageUtils::toBgr(input,bgr,error)) return false;
            cv::cvtColor(bgr,hsv,cv::COLOR_BGR2HSV);
            cv::inRange(hsv,cv::Scalar(h0,s0,v0),cv::Scalar(h1,s1,v1),output);
            break;
        }
        case Normalize:
            if(!qIsFinite(m_param->normalizeMin->value())
                    || !qIsFinite(m_param->normalizeMax->value())
                    || m_param->normalizeMin->value()>=m_param->normalizeMax->value())
            {
                error="Normalize minimum must be finite and below maximum";
                return false;
            }
            cv::normalize(input,output,m_param->normalizeMin->value(),
                          m_param->normalizeMax->value(),cv::NORM_MINMAX);
            break;
        case SplitBgr:
        {
            cv::Mat bgr;
            if(!OpenCvImageUtils::toBgr(input,bgr,error)) return false;
            if(m_param->channel->value()<0 || m_param->channel->value()>2)
            {
                error="BGR channel must be 0, 1, or 2";
                return false;
            }
            std::vector<cv::Mat> channels;
            cv::split(bgr,channels);
            output=channels[static_cast<size_t>(m_param->channel->value())];
            if(m_param->mergeChannels->value()) cv::cvtColor(output,output,cv::COLOR_GRAY2BGR);
            break;
        }
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV color operation failed: %1").arg(QString::fromUtf8(exception.what()));
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
