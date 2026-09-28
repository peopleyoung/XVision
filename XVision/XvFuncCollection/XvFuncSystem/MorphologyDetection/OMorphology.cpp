#include "XLanguage.h"
#include "OMorphology.h"

#include "OpenCvImageUtils.h"

#include <QRegularExpression>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

using namespace XvCore;

OMorphology::OMorphology(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OMorphologyParam()),
      m_result(new OMorphologyResult())
{
    _funcRole="OMorphology";
    _funcName=getUiText("OpenCV Morphology");
}

OMorphology::~OMorphology()
{
    delete m_param;
    delete m_result;
}

QStringList OMorphology::activeParameterNames() const
{
    QStringList names={"inputImage","kernelWidth","kernelHeight","useKernel"};
    names.append(m_param->useKernel->value()?"kernelValues":"kernelShape");
    names << "anchorX" << "anchorY" << "iterations" << "borderType" << "borderValue";
    return names;
}

bool OMorphology::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    const int width=m_param->kernelWidth->value();
    const int height=m_param->kernelHeight->value();
    if(!OpenCvImageUtils::validateOddKernel(width,height,error)) return false;
    if(!m_param->useKernel->value()
            && (m_param->kernelShape->value()<0 || m_param->kernelShape->value()>2))
    {
        error="Kernel shape must be rectangle, cross, or ellipse";
        return false;
    }
    const int anchorX=m_param->anchorX->value();
    const int anchorY=m_param->anchorY->value();
    if(!((anchorX==-1 && anchorY==-1)
         || (anchorX>=0 && anchorX<width && anchorY>=0 && anchorY<height)))
    {
        error="Morphology anchor must be (-1,-1) or lie inside the kernel";
        return false;
    }
    if(m_param->iterations->value()<=0)
    {
        error="Morphology iterations must be positive";
        return false;
    }
    if(m_param->borderType->value()<0 || m_param->borderType->value()>3)
    {
        error="Morphology border type is invalid";
        return false;
    }
    if(static_cast<int>(m_mode)<BlackHat || static_cast<int>(m_mode)>TopHat)
    {
        error="Morphology mode is invalid";
        return false;
    }

    cv::Mat input;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    try
    {
        cv::Mat kernel;
        if(m_param->useKernel->value())
        {
            const QStringList values=m_param->kernelValues->value().split(
                        QRegularExpression("[,;\\s]+"),Qt::SkipEmptyParts);
            if(values.size()!=width*height)
            {
                error=QString("Custom kernel requires exactly %1 values").arg(width*height);
                return false;
            }
            kernel=cv::Mat(height,width,CV_8U);
            for(int index=0;index<values.size();++index)
            {
                bool ok=false;
                const int value=values.at(index).toInt(&ok);
                if(!ok)
                {
                    error=QString("Custom kernel value %1 is not an integer").arg(index);
                    return false;
                }
                kernel.at<uchar>(index/width,index%width)=value==0?0:1;
            }
            if(cv::countNonZero(kernel)==0)
            {
                error="Custom morphology kernel must contain a non-zero value";
                return false;
            }
        }
        else
        {
            const int shapes[]={cv::MORPH_RECT,cv::MORPH_CROSS,cv::MORPH_ELLIPSE};
            kernel=cv::getStructuringElement(shapes[m_param->kernelShape->value()],
                                             cv::Size(width,height));
        }
        const int operations[]={cv::MORPH_BLACKHAT,cv::MORPH_CLOSE,cv::MORPH_DILATE,
                                cv::MORPH_ERODE,cv::MORPH_GRADIENT,cv::MORPH_OPEN,
                                cv::MORPH_TOPHAT};
        const int borders[]={cv::BORDER_CONSTANT,cv::BORDER_REPLICATE,
                             cv::BORDER_REFLECT,cv::BORDER_REFLECT_101};
        cv::Mat output;
        cv::morphologyEx(input,output,operations[static_cast<int>(m_mode)],kernel,
                         cv::Point(anchorX,anchorY),m_param->iterations->value(),
                         borders[m_param->borderType->value()],
                         cv::Scalar::all(m_param->borderValue->value()));
        return OpenCvImageUtils::toQImage(output,candidate,error);
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV morphology failed: %1").arg(exception.what());
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
