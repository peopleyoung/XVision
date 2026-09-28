#include "XLanguage.h"
#include "OImageFilter.h"

#include "OpenCvImageUtils.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#endif

using namespace XvCore;

OImageFilter::OImageFilter(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageFilterParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageFilter";
    _funcName=getUiText("OpenCV Image Filter");
}

OImageFilter::~OImageFilter()
{
    delete m_param;
    delete m_result;
}

QStringList OImageFilter::activeParameterNames() const
{
    switch(m_mode)
    {
    case BoxBlur:
        return {"inputImage","kernelWidth","kernelHeight"};
    case GaussianBlur:
        return {"inputImage","kernelWidth","kernelHeight","sigmaX","sigmaY"};
    case DetailEnhance:
    case Stylization:
        return {"inputImage","sigmaS","sigmaR"};
    case EdgePreserving:
        return {"inputImage","method","sigmaS","sigmaR"};
    case PencilSketch:
        return {"inputImage","sigmaS","sigmaR","shadeFactor","outputVariant"};
    }
    return {"inputImage"};
}

bool OImageFilter::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    cv::Mat output;
    try
    {
        if(m_mode==BoxBlur || m_mode==GaussianBlur)
        {
            if(!OpenCvImageUtils::validateOddKernel(m_param->kernelWidth->value(),
                                                    m_param->kernelHeight->value(),error))
                return false;
            const cv::Size kernel(m_param->kernelWidth->value(),m_param->kernelHeight->value());
            if(m_mode==BoxBlur) cv::blur(input,output,kernel);
            else
            {
                if(m_param->sigmaX->value()<0.0 || m_param->sigmaY->value()<0.0)
                {
                    error="Gaussian sigma values must be non-negative";
                    return false;
                }
                cv::GaussianBlur(input,output,kernel,m_param->sigmaX->value(),m_param->sigmaY->value());
            }
        }
        else
        {
            if(m_param->sigmaS->value()<0.0 || m_param->sigmaS->value()>200.0
                    || m_param->sigmaR->value()<0.0 || m_param->sigmaR->value()>1.0)
            {
                error="Filter sigmaS must be in [0, 200] and sigmaR in [0, 1]";
                return false;
            }
            cv::Mat bgr;
            if(!OpenCvImageUtils::toBgr(input,bgr,error)) return false;
            if(m_mode==DetailEnhance)
                cv::detailEnhance(bgr,output,static_cast<float>(m_param->sigmaS->value()),
                                  static_cast<float>(m_param->sigmaR->value()));
            else if(m_mode==EdgePreserving)
            {
                if(m_param->method->value()<0 || m_param->method->value()>1)
                {
                    error="Edge-preserving method must be 0 or 1";
                    return false;
                }
                const int flag=m_param->method->value()==0?cv::RECURS_FILTER:cv::NORMCONV_FILTER;
                cv::edgePreservingFilter(bgr,output,flag,
                                         static_cast<float>(m_param->sigmaS->value()),
                                         static_cast<float>(m_param->sigmaR->value()));
            }
            else if(m_mode==PencilSketch)
            {
                if(m_param->shadeFactor->value()<0.0 || m_param->shadeFactor->value()>0.1
                        || m_param->outputVariant->value()<0 || m_param->outputVariant->value()>1)
                {
                    error="Pencil shade factor or output variant is invalid";
                    return false;
                }
                cv::Mat gray,color;
                cv::pencilSketch(bgr,gray,color,static_cast<float>(m_param->sigmaS->value()),
                                 static_cast<float>(m_param->sigmaR->value()),
                                 static_cast<float>(m_param->shadeFactor->value()));
                output=m_param->outputVariant->value()==0?gray:color;
            }
            else
                cv::stylization(bgr,output,static_cast<float>(m_param->sigmaS->value()),
                                static_cast<float>(m_param->sigmaR->value()));
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV filter failed: %1").arg(QString::fromUtf8(exception.what()));
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
