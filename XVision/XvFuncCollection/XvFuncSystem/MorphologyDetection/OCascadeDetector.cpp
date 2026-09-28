#include "OCascadeDetector.h"

#include "OpenCvImageUtils.h"

#include <QFileInfo>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#endif

using namespace XvCore;

class OCascadeDetector::Impl
{
public:
#if defined(XVISION_ENABLE_OPENCV)
    std::unique_ptr<cv::CascadeClassifier> classifier;
    std::unique_ptr<cv::CascadeClassifier> pendingClassifier;
#endif
    QString path;
    QString pendingPath;
};

OCascadeDetector::OCascadeDetector(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_impl(std::make_unique<Impl>()),
      m_param(new OCascadeDetectorParam()),m_result(new OCascadeDetectorResult())
{
    _funcRole="OCascadeDetector";
    _funcName="OpenCV Cascade Detector";
}

OCascadeDetector::~OCascadeDetector()
{
    delete m_param;
    delete m_result;
}

QStringList OCascadeDetector::activeParameterNames() const
{
    return {"inputImage","cascadePath","scaleFactor","minNeighbors","flags",
            "minWidth","minHeight","maxWidth","maxHeight"};
}

bool OCascadeDetector::commitAdditionalResults(QString &error)
{
    if(!m_result->rectangles->setData(&m_candidateRectangles))
    {
        error="Cascade rectangle candidate is invalid";
        return false;
    }
    m_result->count->setValue(m_candidateCount);
#if defined(XVISION_ENABLE_OPENCV)
    if(m_impl->pendingClassifier)
    {
        m_impl->classifier=std::move(m_impl->pendingClassifier);
        m_impl->path=m_impl->pendingPath;
        m_impl->pendingPath.clear();
    }
#endif
    error.clear();
    return true;
}

bool OCascadeDetector::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    m_impl->pendingClassifier.reset();
    m_impl->pendingPath.clear();
    if(m_mode!=Haar && m_mode!=Lbp)
    {
        error="Cascade detector mode is invalid";
        return false;
    }
    const QFileInfo asset(m_param->cascadePath->value());
    if(!asset.isAbsolute() || !asset.isFile() || !asset.isReadable())
    {
        error="Cascade path must be an absolute readable file";
        return false;
    }
    if(m_param->scaleFactor->value()<=1.0 || m_param->scaleFactor->value()>1.5
            || m_param->minNeighbors->value()<0 || m_param->flags->value()<0
            || m_param->minWidth->value()<0 || m_param->minHeight->value()<0
            || m_param->maxWidth->value()<0 || m_param->maxHeight->value()<0
            || ((m_param->minWidth->value()>0 || m_param->minHeight->value()>0)
                && (m_param->minWidth->value()==0 || m_param->minHeight->value()==0))
            || ((m_param->maxWidth->value()>0 || m_param->maxHeight->value()>0)
                && (m_param->maxWidth->value()==0 || m_param->maxHeight->value()==0))
            || (m_param->maxWidth->value()>0
                && (m_param->minWidth->value()>m_param->maxWidth->value()
                    || m_param->minHeight->value()>m_param->maxHeight->value())))
    {
        error="Cascade scale, neighbors, flags, or size range is invalid";
        return false;
    }

    cv::Mat input,gray,annotated;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)
            || !OpenCvImageUtils::toBgr(input,annotated,error)) return false;
    try
    {
        const QString path=asset.canonicalFilePath().isEmpty()
                ?asset.absoluteFilePath():asset.canonicalFilePath();
        cv::CascadeClassifier *classifier=nullptr;
        if(m_impl->classifier && m_impl->path==path)
        {
            classifier=m_impl->classifier.get();
        }
        else
        {
            auto prepared=std::make_unique<cv::CascadeClassifier>();
            if(!prepared->load(path.toStdString()) || prepared->empty())
            {
                error="Cascade asset cannot be loaded by OpenCV";
                return false;
            }
            classifier=prepared.get();
            m_impl->pendingClassifier=std::move(prepared);
            m_impl->pendingPath=path;
        }

        std::vector<cv::Rect> detected;
        classifier->detectMultiScale(gray,detected,m_param->scaleFactor->value(),
                                     m_param->minNeighbors->value(),m_param->flags->value(),
                                     cv::Size(m_param->minWidth->value(),
                                              m_param->minHeight->value()),
                                     cv::Size(m_param->maxWidth->value(),
                                              m_param->maxHeight->value()));
        XObjectList rectangles("candidateRectangles",XRect2D::type());
        for(const cv::Rect &rectangle:detected)
        {
            if(rectangle.width<=0 || rectangle.height<=0
                    || !rectangles.addValue(new XRect2D(rectangle.x,rectangle.y,
                                                        rectangle.width,rectangle.height)))
            {
                error="Cascade rectangle conversion failed";
                return false;
            }
            cv::rectangle(annotated,rectangle,cv::Scalar(0,255,0),2);
        }
        if(!OpenCvImageUtils::toQImage(annotated,candidate,error)
                || !m_candidateRectangles.setData(&rectangles))
        {
            if(error.isEmpty()) error="Cascade result candidate preparation failed";
            return false;
        }
        m_candidateCount=static_cast<int>(detected.size());
        return true;
    }
    catch(const cv::Exception &exception)
    {
        m_impl->pendingClassifier.reset();
        m_impl->pendingPath.clear();
        error=QString("OpenCV cascade detection failed: %1").arg(exception.what());
        return false;
    }
#else
    Q_UNUSED(source)
    Q_UNUSED(candidate)
    error="OpenCV backend is disabled";
    return false;
#endif
}
