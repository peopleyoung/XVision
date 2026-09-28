#include "OImageArithmetic.h"

#include "OpenCvImageUtils.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#endif

using namespace XvCore;

OImageArithmetic::OImageArithmetic(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageArithmeticParam()),
      m_result(new OpenCvImageResultBase())
{
    _funcRole="OImageArithmetic";
    _funcName="OpenCV Image Arithmetic";
}

OImageArithmetic::~OImageArithmetic()
{
    delete m_param;
    delete m_result;
}

QStringList OImageArithmetic::activeParameterNames() const
{
    if(m_mode==BitwiseNot) return {"inputImage"};
    if(m_mode==AddSubtract) return {"inputImage","value","useAbsolute"};
    return {"inputImage","value"};
}

bool OImageArithmetic::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(!qIsFinite(m_param->value->value()))
    {
        error="Arithmetic value must be finite";
        return false;
    }
    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    cv::Mat output;
    const double value=m_param->value->value();
    try
    {
        switch(m_mode)
        {
        case AddSubtract:
            if(m_param->useAbsolute->value())
                cv::absdiff(input,cv::Scalar::all(qAbs(value)),output);
            else
                input.convertTo(output,input.type(),1.0,value);
            break;
        case BitwiseNot:
            cv::bitwise_not(input,output);
            break;
        case MultiplyDivide:
            if(value<0.0)
            {
                error="Multiply/divide factor must be non-negative";
                return false;
            }
            input.convertTo(output,input.type(),value);
            break;
        case Pow:
        {
            if(value<0.0)
            {
                error="Power must be non-negative";
                return false;
            }
            cv::Mat floating;
            input.convertTo(floating,CV_MAKETYPE(CV_32F,input.channels()));
            cv::pow(floating,value,floating);
            floating.convertTo(output,input.type());
            break;
        }
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV arithmetic failed: %1").arg(QString::fromUtf8(exception.what()));
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
