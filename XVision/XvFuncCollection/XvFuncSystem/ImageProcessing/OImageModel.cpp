#include "OImageModel.h"

#include "OpenCvImageUtils.h"

#include <QFileInfo>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/dnn_superres.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ml.hpp>
#include <opencv2/video/background_segm.hpp>
#endif

using namespace XvCore;

struct OImageModel::Impl
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Ptr<cv::BackgroundSubtractorMOG2> subtractor;
    int history=0;
    int mixtures=0;
    double backgroundRatio=0.0;
    double varianceThreshold=0.0;
    std::unique_ptr<cv::dnn_superres::DnnSuperResImpl> superResolution;
    QString superResolutionPath;
    QString superResolutionAlgorithm;
    int superResolutionScale=0;
    cv::Ptr<cv::ml::SVM> svm;
    QString svmPath;
#endif
};

namespace
{
bool validateAssetPath(const QString &path,QString &absolutePath,QString &error)
{
    const QFileInfo info(path);
    if(path.trimmed().isEmpty() || !info.isAbsolute() || !info.exists()
            || !info.isFile() || !info.isReadable())
    {
        error="Model path must name an absolute readable file";
        return false;
    }
    absolutePath=info.canonicalFilePath();
    if(absolutePath.isEmpty()) absolutePath=info.absoluteFilePath();
    return true;
}
}

OImageModel::OImageModel(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_impl(std::make_unique<Impl>()),
      m_param(new OImageModelParam()),m_result(new OImageModelResult())
{
    _funcRole="OImageModel";
    _funcName="OpenCV Image Model";
}

OImageModel::~OImageModel()
{
    delete m_param;
    delete m_result;
}

QStringList OImageModel::activeParameterNames() const
{
    switch(m_mode)
    {
    case SuperResolution:
        return {"inputImage","modelPath","algorithm","scale"};
    case BackgroundSubtraction:
        return {"inputImage","history","mixtures","backgroundRatio","varianceThreshold",
                "learningRate","outputBackground"};
    case Svm:
        return {"inputImage","modelPath","svmWidth","svmHeight"};
    }
    return {"inputImage"};
}

bool OImageModel::commitAdditionalResults(QString &error)
{
    m_result->classId->setValue(m_candidateClassId);
    error.clear();
    return true;
}

bool OImageModel::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input,output;
    if(!OpenCvImageUtils::fromQImage(source,input,error)) return false;
    try
    {
        if(m_mode==SuperResolution)
        {
            QString path;
            if(!validateAssetPath(m_param->modelPath->value(),path,error)) return false;
            const QString algorithm=m_param->algorithm->value().trimmed().toLower();
            const QStringList algorithms={"edsr","espcn","fsrcnn","lapsrn"};
            if(!algorithms.contains(algorithm) || m_param->scale->value()<2 || m_param->scale->value()>8)
            {
                error="Super-resolution algorithm or scale is invalid";
                return false;
            }
            if(!m_impl->superResolution || m_impl->superResolutionPath!=path
                    || m_impl->superResolutionAlgorithm!=algorithm
                    || m_impl->superResolutionScale!=m_param->scale->value())
            {
                auto prepared=std::make_unique<cv::dnn_superres::DnnSuperResImpl>();
                prepared->readModel(path.toStdString());
                prepared->setModel(algorithm.toStdString(),m_param->scale->value());
                prepared->upsample(input,output);
                m_impl->superResolution=std::move(prepared);
                m_impl->superResolutionPath=path;
                m_impl->superResolutionAlgorithm=algorithm;
                m_impl->superResolutionScale=m_param->scale->value();
            }
            else m_impl->superResolution->upsample(input,output);
            m_candidateClassId=0;
        }
        else if(m_mode==BackgroundSubtraction)
        {
            const int history=m_param->history->value(),mixtures=m_param->mixtures->value();
            const double ratio=m_param->backgroundRatio->value();
            const double variance=m_param->varianceThreshold->value();
            const double learning=m_param->learningRate->value();
            if(history<1 || mixtures<1 || mixtures>64 || ratio<0.0 || ratio>1.0
                    || variance<=0.0 || learning< -1.0 || learning>1.0)
            {
                error="Background subtraction parameters are invalid";
                return false;
            }
            const bool replaceSubtractor=!m_impl->subtractor
                    || m_impl->history!=history
                    || m_impl->mixtures!=mixtures
                    || m_impl->backgroundRatio!=ratio
                    || m_impl->varianceThreshold!=variance;
            cv::Ptr<cv::BackgroundSubtractorMOG2> subtractor=m_impl->subtractor;
            if(replaceSubtractor)
            {
                subtractor=cv::createBackgroundSubtractorMOG2(history,variance,false);
                subtractor->setNMixtures(mixtures);
                subtractor->setBackgroundRatio(ratio);
            }
            cv::Mat mask;
            subtractor->apply(input,mask,learning);
            if(m_param->outputBackground->value())
            {
                subtractor->getBackgroundImage(output);
                if(output.empty()) output=input.clone();
            }
            else output=mask;
            if(replaceSubtractor)
            {
                m_impl->subtractor=subtractor;
                m_impl->history=history;
                m_impl->mixtures=mixtures;
                m_impl->backgroundRatio=ratio;
                m_impl->varianceThreshold=variance;
            }
            m_candidateClassId=0;
        }
        else
        {
            QString path;
            if(!validateAssetPath(m_param->modelPath->value(),path,error)) return false;
            cv::Ptr<cv::ml::SVM> prepared=m_impl->svm;
            if(prepared.empty() || m_impl->svmPath!=path)
            {
                prepared=cv::Algorithm::load<cv::ml::SVM>(path.toStdString());
                if(prepared.empty() || !prepared->isTrained())
                {
                    error="SVM model is empty or not trained";
                    return false;
                }
            }
            cv::Mat gray;
            if(!OpenCvImageUtils::toGray(input,gray,error)) return false;
            const int width=m_param->svmWidth->value(),height=m_param->svmHeight->value();
            if((width>0)!=(height>0) || width<0 || height<0)
            {
                error="SVM width and height must both be positive or both be zero";
                return false;
            }
            if(width>0) cv::resize(gray,gray,cv::Size(width,height));
            cv::Mat sample;
            gray.convertTo(sample,CV_32F,1.0/255.0);
            sample=sample.reshape(1,1);
            if(prepared->getVarCount()!=sample.cols)
            {
                error=QString("SVM expects %1 values but image provides %2")
                        .arg(prepared->getVarCount()).arg(sample.cols);
                return false;
            }
            m_candidateClassId=qRound(prepared->predict(sample));
            output=input.clone();
            if(m_impl->svmPath!=path)
            {
                m_impl->svm=prepared;
                m_impl->svmPath=path;
            }
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV model operation failed: %1").arg(QString::fromUtf8(exception.what()));
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
