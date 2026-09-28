#include "XLanguage.h"
#include "OImageAnalysis.h"

#include "OpenCvImageUtils.h"

#include <cstring>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#endif

using namespace XvCore;

OImageAnalysis::OImageAnalysis(QObject *parent)
    :OpenCvImageOperatorBase(parent),m_param(new OImageAnalysisParam()),
      m_result(new OImageAnalysisResult())
{
    _funcRole="OImageAnalysis";
    _funcName=getUiText("OpenCV Image Analysis");
}

OImageAnalysis::~OImageAnalysis()
{
    delete m_param;
    delete m_result;
}

QStringList OImageAnalysis::activeParameterNames() const
{
    switch(m_mode)
    {
    case Canny:
        return {"inputImage","threshold1","threshold2","apertureSize","l2Gradient"};
    case Histogram:
        return {"inputImage","histogramBins"};
    case Hog:
        return {"inputImage","hogWidth","hogHeight","hogBins"};
    case Subdiv2d:
        return {"inputImage","subdivisionSpacing","subdivisionOutput"};
    }
    return {"inputImage"};
}

void OImageAnalysis::setCandidateDescriptor(const std::vector<float> &values,int featureCount)
{
    const std::vector<float> stored=values.empty()?std::vector<float>{0.0F}:values;
    m_candidateDescriptor=QByteArray(reinterpret_cast<const char*>(stored.data()),
                                     static_cast<int>(stored.size()*sizeof(float)));
    m_candidateDimensions={static_cast<qint64>(stored.size())};
    m_candidateFeatureCount=featureCount;
}

bool OImageAnalysis::commitAdditionalResults(QString &error)
{
    if(!m_result->descriptor->setValue("float32",m_candidateDimensions,m_candidateDescriptor))
    {
        error="Analysis descriptor candidate is invalid";
        return false;
    }
    m_result->featureCount->setValue(m_candidateFeatureCount);
    error.clear();
    return true;
}

bool OImageAnalysis::processImage(const QImage &source,QImage &candidate,QString &error)
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat input,gray,output;
    if(!OpenCvImageUtils::fromQImage(source,input,error)
            || !OpenCvImageUtils::toGray(input,gray,error)) return false;
    try
    {
        if(m_mode==Canny)
        {
            const int aperture=m_param->apertureSize->value();
            if(m_param->threshold1->value()<0.0 || m_param->threshold2->value()<0.0
                    || m_param->threshold1->value()>m_param->threshold2->value()
                    || (aperture!=3 && aperture!=5 && aperture!=7))
            {
                error="Canny thresholds or aperture size are invalid";
                return false;
            }
            cv::Canny(gray,output,m_param->threshold1->value(),m_param->threshold2->value(),
                      aperture,m_param->l2Gradient->value());
            setCandidateDescriptor({static_cast<float>(cv::countNonZero(output))},
                                   cv::countNonZero(output));
        }
        else if(m_mode==Histogram)
        {
            const int bins=m_param->histogramBins->value();
            if(bins<2 || bins>4096)
            {
                error="Histogram bins must be in [2, 4096]";
                return false;
            }
            const float rangeValues[]={0.0F,256.0F};
            const float *ranges[]={rangeValues};
            cv::Mat histogram;
            cv::calcHist(&gray,1,nullptr,cv::Mat(),histogram,1,&bins,ranges,true,false);
            std::vector<float> values(static_cast<size_t>(bins));
            std::memcpy(values.data(),histogram.ptr<float>(),values.size()*sizeof(float));
            setCandidateDescriptor(values,bins);

            cv::Mat normalized;
            cv::normalize(histogram,normalized,0.0,119.0,cv::NORM_MINMAX);
            output=cv::Mat(128,qMin(bins,1024),CV_8UC3,cv::Scalar(255,255,255));
            for(int x=1;x<output.cols;++x)
            {
                const int left=(x-1)*bins/output.cols;
                const int right=x*bins/output.cols;
                cv::line(output,cv::Point(x-1,127-cvRound(normalized.at<float>(left))),
                         cv::Point(x,127-cvRound(normalized.at<float>(right))),cv::Scalar(0,0,0),1);
            }
        }
        else if(m_mode==Hog)
        {
            const int width=m_param->hogWidth->value(),height=m_param->hogHeight->value();
            const int bins=m_param->hogBins->value();
            if(width<16 || height<16 || width%8!=0 || height%8!=0 || bins<1 || bins>32)
            {
                error="HOG size must be at least 16 and divisible by 8; bins must be in [1, 32]";
                return false;
            }
            cv::resize(gray,output,cv::Size(width,height));
            cv::HOGDescriptor hog(cv::Size(width,height),cv::Size(16,16),cv::Size(8,8),
                                  cv::Size(8,8),bins);
            std::vector<float> descriptor;
            hog.compute(output,descriptor);
            setCandidateDescriptor(descriptor,static_cast<int>(descriptor.size()));
        }
        else
        {
            const int spacing=m_param->subdivisionSpacing->value();
            if(spacing<2 || spacing>qMax(input.cols,input.rows)
                    || m_param->subdivisionOutput->value()<0
                    || m_param->subdivisionOutput->value()>1)
            {
                error="Subdivision spacing or output mode is invalid";
                return false;
            }
            if(!OpenCvImageUtils::toBgr(input,output,error)) return false;
            cv::Subdiv2D subdivision(cv::Rect(0,0,input.cols,input.rows));
            for(int y=spacing/2;y<input.rows;y+=spacing)
                for(int x=spacing/2;x<input.cols;x+=spacing)
                    subdivision.insert(cv::Point2f(static_cast<float>(x),static_cast<float>(y)));
            std::vector<cv::Vec6f> triangles;
            subdivision.getTriangleList(triangles);
            std::vector<float> descriptor;
            int count=0;
            const cv::Rect bounds(0,0,input.cols,input.rows);
            for(const cv::Vec6f &triangle:triangles)
            {
                const cv::Point a(cvRound(triangle[0]),cvRound(triangle[1]));
                const cv::Point b(cvRound(triangle[2]),cvRound(triangle[3]));
                const cv::Point c(cvRound(triangle[4]),cvRound(triangle[5]));
                if(!bounds.contains(a) || !bounds.contains(b) || !bounds.contains(c)) continue;
                descriptor.insert(descriptor.end(),triangle.val,triangle.val+6);
                const cv::Scalar color=m_param->subdivisionOutput->value()==0
                        ?cv::Scalar(0,200,0):cv::Scalar(255,120,0);
                cv::line(output,a,b,color,1);
                cv::line(output,b,c,color,1);
                cv::line(output,c,a,color,1);
                ++count;
            }
            setCandidateDescriptor(descriptor,count);
        }
    }
    catch(const cv::Exception &exception)
    {
        error=QString("OpenCV analysis failed: %1").arg(QString::fromUtf8(exception.what()));
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
