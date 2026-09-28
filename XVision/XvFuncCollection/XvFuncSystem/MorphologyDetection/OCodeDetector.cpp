#include "OCodeDetector.h"

#include "OpenCvImageUtils.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#endif

using namespace XvCore;

OCodeDetector::OCodeDetector(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OCodeDetectorParam()),
      m_result(new OCodeDetectorResult())
{
    _funcRole="OCodeDetector";
    _funcName="OpenCV Code Detector";
}

OCodeDetector::~OCodeDetector()
{
    delete m_param;
    delete m_result;
}

bool OCodeDetector::commitAdditionalResults(QString &error)
{
    if(!m_result->corners->setData(&m_candidateCorners))
    {
        error="QR corner candidate is invalid";
        return false;
    }
    m_result->text->setValue(m_candidateText);
    error.clear();
    return true;
}

bool OCodeDetector::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    if(m_mode!=QrCode)
    {
        error="Code detector mode is invalid";
        return false;
    }
    cv::Mat input,gray,annotated;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)
            || !OpenCvImageUtils::toBgr(input,annotated,error)) return false;
    try
    {
        cv::QRCodeDetector detector;
        cv::Mat points;
        const std::string decoded=detector.detectAndDecode(gray,points);
        XObjectList corners("candidateCorners",XPoint2D::type());
        if(!decoded.empty() && !points.empty())
        {
            std::vector<cv::Point> polygon;
            const size_t count=points.total();
            polygon.reserve(count);
            for(size_t index=0;index<count;++index)
            {
                double x=0.0;
                double y=0.0;
                if(points.type()==CV_32FC2)
                {
                    const cv::Point2f point=points.ptr<cv::Point2f>()[index];
                    x=point.x;
                    y=point.y;
                }
                else if(points.type()==CV_64FC2)
                {
                    const cv::Point2d point=points.ptr<cv::Point2d>()[index];
                    x=point.x;
                    y=point.y;
                }
                else
                {
                    error="QR detector returned unsupported corner coordinates";
                    return false;
                }
                if(!corners.addValue(new XPoint2D(x,y)))
                {
                    error="QR corner conversion failed";
                    return false;
                }
                polygon.emplace_back(cvRound(x),cvRound(y));
            }
            if(polygon.size()>=2)
                cv::polylines(annotated,polygon,true,cv::Scalar(0,255,0),2);
        }
        if(!OpenCvImageUtils::toQImage(annotated,candidate,error)
                || !m_candidateCorners.setData(&corners))
        {
            if(error.isEmpty()) error="QR result candidate preparation failed";
            return false;
        }
        m_candidateText=decoded.empty()?QString()
                                           :QString::fromUtf8(decoded.data(),
                                                             static_cast<int>(decoded.size()));
        return true;
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV QR detection failed: %1").arg(exception.what());
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
