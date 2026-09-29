#include "ImageAcquisition.h"
#include "ImageAcquisitionWdg.h"

#include "ImageAcquisition_p.h"
#include "OpenCvImageUtils.h"

#include "IXvCamera.h"
#include "XvCameraManager.h"

#include <QMetaEnum>
#include <QFile>
#include <QFileInfo>

#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <utility>

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

using namespace XvCore;

namespace XvCore
{
struct SVideoCapture
{
#if defined(XVISION_ENABLE_OPENCV)
    std::unique_ptr<cv::VideoCapture> capture;
#endif
    QString path;
    int startFrame=0;
    int endFrame=-1;
    int frameStep=1;
    qint64 nextFrame=0;
    int frameCount=0;
    double fps=0.0;
    bool loop=false;
    bool endOfStream=false;

    void reset()
    {
#if defined(XVISION_ENABLE_OPENCV)
        capture.reset();
#endif
        path.clear();
        startFrame=0;
        endFrame=-1;
        frameStep=1;
        nextFrame=0;
        frameCount=0;
        fps=0.0;
        loop=false;
        endOfStream=false;
    }

    bool matches(const QString &candidatePath,int candidateStart,int candidateEnd,
                 int candidateStep,bool candidateLoop) const
    {
        return path==candidatePath && startFrame==candidateStart
                && endFrame==candidateEnd && frameStep==candidateStep
                && loop==candidateLoop;
    }
};
}


ImageAcquisition::ImageAcquisition(QObject *parent)
    :XvFunc(parent)
{
    _funcRole="ImageAcquisition";
    _funcName=getLang("XvFuncSystem_ImageAcquisition_Name","图像采集");
    _funcType=EXvFuncType::ImageAcquisition;

    param=new ImageAcquisitionParam();
    result=new ImageAcquisitionResult();

    m_sDirImage=new SDirImage();
    m_sVideoCapture=new SVideoCapture();
}

ImageAcquisition::~ImageAcquisition()
{  
    if(m_frm)
    {
        delete m_frm;
        m_frm=nullptr;
    }
    if(m_sDirImage)
    {
        delete m_sDirImage;
        m_sDirImage=nullptr;
    }
    delete m_sVideoCapture;
    m_sVideoCapture=nullptr;
    delete param;
    param=nullptr;
    delete result;
    result=nullptr;
}

void ImageAcquisition::resetVideoState()
{
    if(m_sVideoCapture) m_sVideoCapture->reset();
    if(result)
    {
        setOutputImage(QImage());
        result->videoFrameIndex->setValue(-1);
        result->videoFrameCount->setValue(0);
        result->videoFps->setValue(0.0);
        result->videoEndOfStream->setValue(false);
    }
}

void ImageAcquisition::setVideoPath(const QString &path)
{
    if(videoPath()==path) return;
    param->videoPath->setValue(path);
    resetVideoState();
    if(result && m_AcqType==AcqType::Video)
        result->acqLocalPath->setValue(path);
}

void ImageAcquisition::setVideoStartFrame(int frame)
{
    if(videoStartFrame()==frame) return;
    param->videoStartFrame->setValue(frame);
    resetVideoState();
}

void ImageAcquisition::setVideoEndFrame(int frame)
{
    if(videoEndFrame()==frame) return;
    param->videoEndFrame->setValue(frame);
    resetVideoState();
}

void ImageAcquisition::setVideoFrameStep(int step)
{
    if(videoFrameStep()==step) return;
    param->videoFrameStep->setValue(step);
    resetVideoState();
}

void ImageAcquisition::setVideoLoop(bool loop)
{
    if(videoLoop()==loop) return;
    param->videoLoop->setValue(loop);
    resetVideoState();
}

void ImageAcquisition::setOutputImage(const QImage &img)
{
    result->outputImage->setValue(img);
    result->outputImageWidth->setValue(img.width());
    result->outputImageHeight->setValue(img.height());
    result->outputImageDepth->setValue(img.depth());
    result->outputImageMsg->setValue(result->outputImage->toString());
}

bool ImageAcquisition::readPersistentData(const QDomElement &dataElement,
                                          QString &error)
{
    if(!dataElement.isNull())
    {
        error="ImageAcquisition does not support <PersistentData>";
        return false;
    }
    if(cameraTimeoutMs()<0 || cameraTimeoutMs()>60000)
    {
        error="ImageAcquisition camera timeout must be in [0,60000]";
        return false;
    }
    if(!std::isfinite(cameraExposureUs()) || cameraExposureUs()<0 || cameraExposureUs()>60000000 ||
       !std::isfinite(cameraGain()) || cameraGain()<-1 || cameraGain()>1000000 ||
       cameraTriggerMode()<0 || cameraTriggerMode()>2)
    {
        error="ImageAcquisition camera exposure/gain/trigger configuration is invalid";
        return false;
    }
    if(static_cast<int>(m_AcqType)<static_cast<int>(AcqType::File)
            || static_cast<int>(m_AcqType)>static_cast<int>(AcqType::Video))
    {
        error="ImageAcquisition acquisition type is invalid";
        return false;
    }
    if(videoStartFrame()<0 || videoEndFrame()<-1
            || (videoEndFrame()!=-1 && videoEndFrame()<videoStartFrame())
            || videoFrameStep()<=0 || videoFrameStep()>1000000)
    {
        error="ImageAcquisition video range is invalid";
        return false;
    }
    resetVideoState();
    setAcqType(m_AcqType);
    return true;
}

void ImageAcquisition::onShowFunc()
{
    if(!m_frm)
    {
        m_frm=new ImageAcquisitionWdg(this);
    }
    m_frm->show();
    m_frm->raise();
}

EXvFuncRunStatus ImageAcquisition::run()
{
    result->videoEndOfStream->setValue(false);
    switch (m_AcqType)
    {
    case AcqType::File:
    {
        QImage image;
        if(!image.load(m_strLocalFile))
        {
            this->setRunMsg(getLang("XvFuncSystem_ImageAcquisition_RunError1","读取文件图像失败"));
            return EXvFuncRunStatus::Error;
        }
        image.setText("Name",QFileInfo(m_strLocalFile).fileName());
        QMetaEnum meta = QMetaEnum::fromType<QImage::Format>();
        QString format=meta.valueToKey(image.format());
        image.setText("Format",format);

        setOutputImage(image);
    }
        break;
    case AcqType::Dir:
    {
        if(m_sDirImage->dir!=m_strLocalDir)
        {
           bool bRet= m_sDirImage->update(m_strLocalDir);
           if(!bRet)
           {
               this->setRunMsg(getLang("XvFuncSystem_ImageAcquisition_RunError2","刷新目录图像失败"));
               return EXvFuncRunStatus::Error;
           }
        }
        QImage img;
        bool bRet=m_sDirImage->getCurIdxImage(img);
        if(!bRet)
        {
            this->setRunMsg(getLang("XvFuncSystem_ImageAcquisition_RunError3","获取目录图像失败"));
            return EXvFuncRunStatus::Error;
        }
        setOutputImage(img);
    }
        break;
    case AcqType::Camera:
    {
        setOutputImage(QImage());
        const QString deviceId=cameraDeviceId();
        const int timeoutMs=cameraTimeoutMs();
        if(deviceId.isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_CameraIdEmpty",
                              "未选择相机设备"));
            return EXvFuncRunStatus::Error;
        }
        if(timeoutMs<0 || timeoutMs>60000)
        {
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_CameraTimeoutInvalid",
                              "相机超时必须在0到60000毫秒之间"));
            return EXvFuncRunStatus::Error;
        }

        XvCamera::IXvCamera *camera=XvCameraMgr->camera(deviceId);
        if(camera && camera->status()==XvCamera::EXvCameraStatus::Streaming)
        {
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_CameraStreaming",
                              "相机正在连续采集，无法执行单帧采集"));
            return EXvFuncRunStatus::Error;
        }
        if(camera && camera->status()!=XvCamera::EXvCameraStatus::Open)
        {
            XvCameraMgr->closeCamera(deviceId);
            camera=nullptr;
        }
        if(!camera)
        {
            const XvCamera::EXvCameraError openResult=
                    XvCameraMgr->openCamera(deviceId,&camera);
            if((openResult!=XvCamera::EXvCameraError::None
                    && openResult!=XvCamera::EXvCameraError::AlreadyOpen)
                    || !camera)
            {
                const QString detail=XvCameraMgr->lastError();
                setRunMsg(detail.isEmpty()
                          ?getLang("XvFuncSystem_ImageAcquisition_CameraOpenFailed",
                                   "打开相机失败")
                          :detail);
                return EXvFuncRunStatus::Error;
            }
        }

        if(!std::isfinite(cameraExposureUs()) || cameraExposureUs()<0 || cameraExposureUs()>60000000 ||
           !std::isfinite(cameraGain()) || cameraGain()<-1 || cameraGain()>1000000 ||
           cameraTriggerMode()<0 || cameraTriggerMode()>2)
        {
            setRunMsg(getLang("Camera_InvalidSettings","相机曝光、增益或触发参数无效"));
            return EXvFuncRunStatus::Error;
        }
        const QString provider=camera->deviceInfo().providerId;
        const bool industrial=provider=="hik-mvs" || provider=="daheng-galaxy";
        if(industrial)
        {
            const QStringList modes={"Continuous","Software","Line0"};
            const auto apply=[&](const QString &key,const QVariant &value)
            {
                if(camera->parameter(key)==value) return true;
                if(camera->setParameter(key,value)==XvCamera::EXvCameraError::None) return true;
                setRunMsg(camera->lastError());
                return false;
            };
            if(!apply("TriggerMode",modes.at(cameraTriggerMode())) ||
               (cameraExposureUs()>0 && !apply("ExposureTime",cameraExposureUs())) ||
               (cameraGain()>=0 && !apply("Gain",cameraGain()))) return EXvFuncRunStatus::Error;
        }

        XvCamera::XvCameraFrame frame;
        const XvCamera::EXvCameraError grabResult=
                camera->grabFrame(frame,static_cast<unsigned int>(timeoutMs));
        if(grabResult!=XvCamera::EXvCameraError::None || !frame.isValid())
        {
            const QString detail=camera->lastError();
            setRunMsg(detail.isEmpty()
                      ?getLang("XvFuncSystem_ImageAcquisition_CameraGrabFailed",
                               "相机单帧采集失败")
                      :detail);
            return EXvFuncRunStatus::Error;
        }

        QImage image=frame.image;
        const XvCamera::XvCameraDeviceInfo info=camera->deviceInfo();
        image.setText("Name",info.displayName.isEmpty()?deviceId:info.displayName);
        image.setText("DeviceId",deviceId);
        QMetaEnum meta=QMetaEnum::fromType<QImage::Format>();
        image.setText("Format",meta.valueToKey(image.format()));
        setOutputImage(image);
    }
        break;
    case AcqType::Video:
        return runVideo();
    default:
        setOutputImage(QImage());
        setRunMsg(getLang("XvFuncSystem_ImageAcquisition_TypeInvalid",
                          "图像采集类型无效"));
        return EXvFuncRunStatus::Error;
    }
    this->setRunMsg(getLang("XvFuncSystem_ImageAcquisition_RunOk","获取图像成功"));
    return EXvFuncRunStatus::Ok;
}

EXvFuncRunStatus ImageAcquisition::finishVideoEndOfStream()
{
    m_sVideoCapture->endOfStream=true;
    setOutputImage(QImage());
    result->videoEndOfStream->setValue(true);
    setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoEnd",
                      "视频流已结束"));
    return EXvFuncRunStatus::Ok;
}

EXvFuncRunStatus ImageAcquisition::runVideo()
{
    setOutputImage(QImage());
    result->videoEndOfStream->setValue(false);

    const QString path=videoPath().trimmed();
    const int startFrame=videoStartFrame();
    const int endFrame=videoEndFrame();
    const int frameStep=videoFrameStep();
    const bool loop=videoLoop();
    result->acqLocalPath->setValue(path);
    const QFileInfo fileInfo(path);
    if(path.isEmpty() || !fileInfo.isAbsolute() || !fileInfo.isFile()
            || !fileInfo.isReadable())
    {
        resetVideoState();
        setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoPathInvalid",
                          "视频路径必须是可读的绝对文件路径"));
        return EXvFuncRunStatus::Error;
    }
    if(startFrame<0 || endFrame<-1 || (endFrame!=-1 && endFrame<startFrame)
            || frameStep<=0 || frameStep>1000000)
    {
        resetVideoState();
        setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoRangeInvalid",
                          "视频帧范围无效"));
        return EXvFuncRunStatus::Error;
    }

#if !defined(XVISION_ENABLE_OPENCV)
    resetVideoState();
    setRunMsg(getLang("XvFuncSystem_ImageAcquisition_OpenCvDisabled",
                      "视频采集需要使用XVISION_ENABLE_OPENCV=ON启用OpenCV后端"));
    return EXvFuncRunStatus::Error;
#else
    try
    {
    if(!m_sVideoCapture->matches(path,startFrame,endFrame,frameStep,loop)
            || !m_sVideoCapture->capture
            || !m_sVideoCapture->capture->isOpened())
    {
        SVideoCapture candidate;
        candidate.capture=std::make_unique<cv::VideoCapture>();
        const QByteArray nativePath=QFile::encodeName(path);
        if(!candidate.capture->open(nativePath.constData()))
        {
            resetVideoState();
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoOpenFailed",
                              "打开视频文件失败"));
            return EXvFuncRunStatus::Error;
        }
        candidate.path=path;
        candidate.startFrame=startFrame;
        candidate.endFrame=endFrame;
        candidate.frameStep=frameStep;
        candidate.nextFrame=startFrame;
        candidate.loop=loop;
        const double rawCount=candidate.capture->get(cv::CAP_PROP_FRAME_COUNT);
        if(std::isfinite(rawCount) && rawCount>0.0)
            candidate.frameCount=static_cast<int>(qMin(
                    rawCount,static_cast<double>(std::numeric_limits<int>::max())));
        const double rawFps=candidate.capture->get(cv::CAP_PROP_FPS);
        if(std::isfinite(rawFps) && rawFps>0.0) candidate.fps=rawFps;
        if(candidate.frameCount>0 && startFrame>=candidate.frameCount)
        {
            resetVideoState();
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoStartOutside",
                              "视频起始帧超出文件范围"));
            return EXvFuncRunStatus::Error;
        }
        if(!candidate.capture->set(cv::CAP_PROP_POS_FRAMES,
                                   static_cast<double>(startFrame)))
        {
            resetVideoState();
            setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoSeekFailed",
                              "定位视频帧失败"));
            return EXvFuncRunStatus::Error;
        }
        *m_sVideoCapture=std::move(candidate);
    }

    const qint64 effectiveEnd=m_sVideoCapture->frameCount>0
            ?(endFrame==-1?m_sVideoCapture->frameCount-1
                          :qMin(endFrame,m_sVideoCapture->frameCount-1))
            :endFrame;
    auto outsideRange=[&]()
    {
        return effectiveEnd!=-1 && m_sVideoCapture->nextFrame>effectiveEnd;
    };
    if(m_sVideoCapture->endOfStream || outsideRange())
    {
        if(!loop) return finishVideoEndOfStream();
        m_sVideoCapture->nextFrame=startFrame;
        m_sVideoCapture->endOfStream=false;
    }

    const qint64 frameIndex=m_sVideoCapture->nextFrame;
    if(!m_sVideoCapture->capture->set(cv::CAP_PROP_POS_FRAMES,
                                      static_cast<double>(frameIndex)))
    {
        setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoSeekFailed",
                          "定位视频帧失败"));
        return EXvFuncRunStatus::Error;
    }
    cv::Mat frame;
    if(!m_sVideoCapture->capture->read(frame) || frame.empty())
    {
        const bool naturalEnd=endFrame==-1
                && (m_sVideoCapture->frameCount<=0
                    || frameIndex>=m_sVideoCapture->frameCount);
        if(naturalEnd && !loop) return finishVideoEndOfStream();
        if(naturalEnd && loop && frameIndex!=startFrame)
        {
            m_sVideoCapture->nextFrame=startFrame;
            return runVideo();
        }
        setRunMsg(getLang("XvFuncSystem_ImageAcquisition_VideoDecodeFailed",
                          "解码视频帧失败"));
        return EXvFuncRunStatus::Error;
    }

    QImage image;
    QString conversionError;
    if(!OpenCvImageUtils::toQImage(frame,image,conversionError) || image.isNull())
    {
        setRunMsg(conversionError.isEmpty()
                  ?getLang("XvFuncSystem_ImageAcquisition_VideoConvertFailed",
                           "转换视频帧失败")
                  :conversionError);
        return EXvFuncRunStatus::Error;
    }
    image.setText("Name",fileInfo.fileName());
    image.setText("FrameIndex",QString::number(frameIndex));
    QMetaEnum meta=QMetaEnum::fromType<QImage::Format>();
    image.setText("Format",meta.valueToKey(image.format()));

    setOutputImage(image);
    result->videoFrameIndex->setValue(static_cast<int>(frameIndex));
    result->videoFrameCount->setValue(m_sVideoCapture->frameCount);
    result->videoFps->setValue(m_sVideoCapture->fps);
    result->videoEndOfStream->setValue(false);
    m_sVideoCapture->nextFrame=frameIndex+frameStep;
    setRunMsg(getLang("XvFuncSystem_ImageAcquisition_RunOk","获取图像成功"));
    return EXvFuncRunStatus::Ok;
    }
    catch(const cv::Exception &exception)
    {
        resetVideoState();
        setRunMsg(QString("OpenCV video error: %1")
                  .arg(QString::fromUtf8(exception.what())));
        return EXvFuncRunStatus::Error;
    }
    catch(const std::exception &exception)
    {
        resetVideoState();
        setRunMsg(QString("Video acquisition error: %1")
                  .arg(QString::fromUtf8(exception.what())));
        return EXvFuncRunStatus::Error;
    }
#endif
}

XvExecutionDirective ImageAcquisition::executionDirective() const
{
    XvExecutionDirective directive;
    if(m_AcqType==AcqType::Video && result && result->videoEndOfStream->value())
        directive.kind=XvExecutionDirective::SelectPorts;
    return directive;
}
