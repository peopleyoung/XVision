#ifndef IMAGEACQUISITION_H
#define IMAGEACQUISITION_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"
#include "XLanguage.h"

class ImageAcquisitionWdg;
namespace XvCore
{
class ImageAcquisitionParam:public XvBaseParam
{
public:
    ImageAcquisitionParam()
    {
        cameraDeviceId=new XString("cameraDeviceId","",this,
                getLang("XvFuncSystem_ImageAcquisition_CameraDeviceId","相机设备"));
        cameraTimeoutMs=new XInt("cameraTimeoutMs",1000,this,
                getLang("XvFuncSystem_ImageAcquisition_CameraTimeout","相机超时(ms)"));
        videoPath=new XString("videoPath","",this,
                getLang("XvFuncSystem_ImageAcquisition_VideoPath","视频文件"));
        videoStartFrame=new XInt("videoStartFrame",0,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoStartFrame","起始帧"));
        videoEndFrame=new XInt("videoEndFrame",-1,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoEndFrame","结束帧"));
        videoFrameStep=new XInt("videoFrameStep",1,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoFrameStep","帧步长"));
        videoLoop=new XBool("videoLoop",false,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoLoop","循环播放"));
    }
public:
    XString *cameraDeviceId=nullptr;
    XInt *cameraTimeoutMs=nullptr;
    XString *videoPath=nullptr;
    XInt *videoStartFrame=nullptr;
    XInt *videoEndFrame=nullptr;
    XInt *videoFrameStep=nullptr;
    XBool *videoLoop=nullptr;
};



class ImageAcquisitionResult:public XvBaseResult
{
public:
    ImageAcquisitionResult()
    {
        acqType=new XInt("acqType",0,this,getLang("XvFuncSystem_ImageAcquisition_AcqType","采集类型"));
        acqLocalPath=new XString("acqLocalPath","",this,getLang("XvFuncSystem_ImageAcquisition_AcqLocalPath","本地路径"));
        outputImage=new XImage("outputImage",QImage(),this,getLang("XvFuncSystem_ImageAcquisition_OutputImage","输出图像"));
        outputImageMsg=new XString("outputImageMsg","",this,getLang("XvFuncSystem_ImageAcquisition_OutputImageMsg","输出图像信息"));
        outputImageWidth=new XInt("outputImageWidth",0,this,getLang("XvFuncSystem_ImageAcquisition_OutputImageWidth","输出图像宽度"));
        outputImageHeight=new XInt("outputImageHeight",0,this,getLang("XvFuncSystem_ImageAcquisition_OutputImageHeight","输出图像高度"));
        outputImageDepth=new XInt("outputImageDepth",0,this,getLang("XvFuncSystem_ImageAcquisition_OutputImageDepth","输出图像深度"));
        videoFrameIndex=new XInt("videoFrameIndex",-1,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoFrameIndex","视频帧索引"));
        videoFrameCount=new XInt("videoFrameCount",0,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoFrameCount","视频总帧数"));
        videoFps=new XReal("videoFps",0.0,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoFps","视频帧率"));
        videoEndOfStream=new XBool("videoEndOfStream",false,this,
                getLang("XvFuncSystem_ImageAcquisition_VideoEndOfStream","视频流结束"));
    }
public:
    XInt *acqType=nullptr;
    XString *acqLocalPath=nullptr;

    XImage *outputImage=nullptr;
    XString *outputImageMsg=nullptr;
    XInt   *outputImageWidth=nullptr;
    XInt   *outputImageHeight=nullptr;
    XInt   *outputImageDepth=nullptr;
    XInt *videoFrameIndex=nullptr;
    XInt *videoFrameCount=nullptr;
    XReal *videoFps=nullptr;
    XBool *videoEndOfStream=nullptr;

};
struct SDirImage;
struct SVideoCapture;
class XVFUNCSYSTEM_EXPORT ImageAcquisition:public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(AcqType acqType READ acqType WRITE setAcqType)
    Q_PROPERTY(QString localFile READ localFile WRITE setLocalFile)
    Q_PROPERTY(QString localDir READ localDir WRITE setLocalDir)
    Q_PROPERTY(QString cameraDeviceId READ cameraDeviceId WRITE setCameraDeviceId)
    Q_PROPERTY(int cameraTimeoutMs READ cameraTimeoutMs WRITE setCameraTimeoutMs)
    friend class ::ImageAcquisitionWdg;
public:
    Q_INVOKABLE explicit ImageAcquisition(QObject *parent = nullptr);
    ~ImageAcquisition();
public:
    //采集类型
    enum AcqType
    {
        File=0,
        Dir=1,
        Camera=2,
        Video=3,
    };
    Q_ENUM(AcqType);
    AcqType acqType() const {return m_AcqType;}
    void setAcqType(AcqType type)
    {
        if(m_AcqType!=type) resetVideoState();
        m_AcqType=type;
        result->acqType->setValue(type);
        switch (m_AcqType)
        {
        case AcqType::File:
            result->acqLocalPath->setValue(m_strLocalFile);
            break;
        case AcqType::Dir:
            result->acqLocalPath->setValue(m_strLocalDir);
            break;
        case AcqType::Camera:
            result->acqLocalPath->setValue(cameraDeviceId());
            break;
        case AcqType::Video:
            result->acqLocalPath->setValue(videoPath());
            break;
        }
    }
public:
    ///本地路径
    QString localFile() const { return m_strLocalFile; }
    QString localFlie() const { return localFile(); }
    void setLocalFile(const QString &path)
    {
        m_strLocalFile=path;
        if(m_AcqType==AcqType::File)
            result->acqLocalPath->setValue(m_strLocalFile);
    }
    ///本地目录
    QString localDir() const { return m_strLocalDir; }
    void setLocalDir(const QString &dir)
    {
        m_strLocalDir=dir;
        if(m_AcqType==AcqType::Dir)
            result->acqLocalPath->setValue(m_strLocalDir);
    }

    QString cameraDeviceId() const
    {
        return param&&param->cameraDeviceId?param->cameraDeviceId->value():QString();
    }
    void setCameraDeviceId(const QString &deviceId)
    {
        if(param&&param->cameraDeviceId) param->cameraDeviceId->setValue(deviceId);
        if(result && m_AcqType==AcqType::Camera)
            result->acqLocalPath->setValue(deviceId);
    }
    int cameraTimeoutMs() const
    {
        return param&&param->cameraTimeoutMs?param->cameraTimeoutMs->value():1000;
    }
    void setCameraTimeoutMs(int timeoutMs)
    {
        if(timeoutMs>=0 && timeoutMs<=60000 && param&&param->cameraTimeoutMs)
            param->cameraTimeoutMs->setValue(timeoutMs);
    }

    QString videoPath() const
    {
        return param&&param->videoPath?param->videoPath->value():QString();
    }
    void setVideoPath(const QString &path);
    int videoStartFrame() const
    {
        return param&&param->videoStartFrame?param->videoStartFrame->value():0;
    }
    void setVideoStartFrame(int frame);
    int videoEndFrame() const
    {
        return param&&param->videoEndFrame?param->videoEndFrame->value():-1;
    }
    void setVideoEndFrame(int frame);
    int videoFrameStep() const
    {
        return param&&param->videoFrameStep?param->videoFrameStep->value():1;
    }
    void setVideoFrameStep(int step);
    bool videoLoop() const
    {
        return param&&param->videoLoop?param->videoLoop->value():false;
    }
    void setVideoLoop(bool loop);

    QStringList persistentPropertyNames() const override
    {
        return {"acqType","localFile","localDir"};
    }
    QStringList optionalPersistentParameterNames() const override
    {
        return {"cameraDeviceId","cameraTimeoutMs","videoPath",
                "videoStartFrame","videoEndFrame","videoFrameStep","videoLoop"};
    }

protected:
    ///设置输出的图像
    void setOutputImage(const QImage &img);
public slots:
    void onShowFunc() override;
protected:

    EXvFuncRunStatus run() override;
    XvExecutionDirective executionDirective() const override;
    bool readPersistentData(const QDomElement &dataElement,
                            QString &error) override;
    XvBaseParam *getParam() const override { return param;};
    XvBaseResult *getResult() const override { return result;};
    void resetVideoState();
    EXvFuncRunStatus runVideo();
    EXvFuncRunStatus finishVideoEndOfStream();
protected:
    ImageAcquisitionParam *param=nullptr;
    ImageAcquisitionResult *result=nullptr;
    ImageAcquisitionWdg* m_frm=nullptr;
    AcqType             m_AcqType=AcqType::File;
    QString             m_strLocalFile="";//本地路径
    QString             m_strLocalDir="";//本地目录
    SDirImage*          m_sDirImage=nullptr;
    SVideoCapture*      m_sVideoCapture=nullptr;
};
}

#endif // IMAGEACQUISITION_H
