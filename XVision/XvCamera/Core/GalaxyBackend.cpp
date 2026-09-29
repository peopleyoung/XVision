#include "VendorLibrary.h"
#include <mutex>
#include <vector>

namespace XvCamera { namespace Hardware {
namespace {
Result galaxyResult(int32_t code,const QString &action)
{
    if(!code) return {};
    const auto error=code==-14?EXvCameraError::Timeout:code==-4?EXvCameraError::Disconnected:
        code==-3?EXvCameraError::NotFound:code==-12?EXvCameraError::Unsupported:EXvCameraError::IoError;
    return {error,action+QStringLiteral("，大恒错误码 %1").arg(code)};
}
struct Api : VendorLibrary
{
    std::mutex enumeration;
    bool ready=false,initialized=false;
    int32_t (XV_CAMERA_CALL *initialize)()=nullptr;
    int32_t (XV_CAMERA_CALL *finalize)()=nullptr;
    int32_t (XV_CAMERA_CALL *update)(uint32_t*,uint32_t)=nullptr;
    int32_t (XV_CAMERA_CALL *enumerate)(Abi::GalaxyDevice*,size_t*)=nullptr;
    int32_t (XV_CAMERA_CALL *open)(Abi::GalaxyOpen*,void**)=nullptr;
    int32_t (XV_CAMERA_CALL *close)(void*)=nullptr;
    int32_t (XV_CAMERA_CALL *getInt)(void*,int32_t,int64_t*)=nullptr;
    int32_t (XV_CAMERA_CALL *getEnum)(void*,int32_t,int64_t*)=nullptr;
    int32_t (XV_CAMERA_CALL *setEnum)(void*,int32_t,int64_t)=nullptr;
    int32_t (XV_CAMERA_CALL *command)(void*,int32_t)=nullptr;
    int32_t (XV_CAMERA_CALL *get)(void*,Abi::GalaxyFrame*,uint32_t)=nullptr;
    int32_t (XV_CAMERA_CALL *clear)(void*)=nullptr;
    int32_t (XV_CAMERA_CALL *getFloat)(void*,int32_t,double*)=nullptr;
    int32_t (XV_CAMERA_CALL *floatRange)(void*,int32_t,Abi::GalaxyFloatRange*)=nullptr;
    int32_t (XV_CAMERA_CALL *setFloat)(void*,int32_t,double)=nullptr;
    ~Api() { if(initialized && finalize) finalize(); }
    Result ensure()
    {
        if(ready) return {};
        QStringList paths;
        const QString genicam=qEnvironmentVariable("GALAXY_GENICAM_ROOT");
        if(!genicam.isEmpty()) paths << genicam << QDir(genicam).filePath("bin/Win64_x64");
#ifdef Q_OS_WIN
        for(const auto &root:{qEnvironmentVariable("ProgramFiles"),qEnvironmentVariable("ProgramFiles(x86)")})
            if(!root.isEmpty()) paths << QDir(root).filePath("Daheng Imaging/GalaxySDK/GenICam/bin/Win64_x64")
                                     << QDir(root).filePath("Daheng Imaging/GalaxySDK/APIDll/Win64");
        for(const auto &root:{qEnvironmentVariable("CommonProgramFiles"),qEnvironmentVariable("CommonProgramFiles(x86)")})
            if(!root.isEmpty()) paths << QDir(root).filePath("Daheng Imaging/GalaxySDK/APIDll/Win64")
                                     << QDir(root).filePath("Daheng Imaging/GalaxySDK/GenICam/bin/Win64_x64");
        const QString base=QStringLiteral("GxIAPI.dll");
#else
        paths << QStringLiteral("/usr/lib") << QStringLiteral("/usr/local/lib")
              << QStringLiteral("/opt/Galaxy_camera/lib/x86_64");
        const QString base=QStringLiteral("libgxiapi.so");
#endif
        const Result loaded=load("XVISION_GALAXY_LIBRARY",base,paths);
        if(!loaded) return loaded;
#define GX_REQUIRED(member,name) if(!resolve(member,name)) return {EXvCameraError::Unsupported,QStringLiteral("Galaxy SDK 缺少接口：")+QString::fromLatin1(name)};
        GX_REQUIRED(initialize,"GXInitLib") GX_REQUIRED(finalize,"GXCloseLib")
        GX_REQUIRED(update,"GXUpdateDeviceList") GX_REQUIRED(enumerate,"GXGetAllDeviceBaseInfo")
        GX_REQUIRED(open,"GXOpenDevice") GX_REQUIRED(close,"GXCloseDevice")
        GX_REQUIRED(getInt,"GXGetInt") GX_REQUIRED(getEnum,"GXGetEnum") GX_REQUIRED(setEnum,"GXSetEnum")
        GX_REQUIRED(command,"GXSendCommand") GX_REQUIRED(get,"GXGetImage") GX_REQUIRED(clear,"GXFlushQueue")
        GX_REQUIRED(getFloat,"GXGetFloat") GX_REQUIRED(floatRange,"GXGetFloatRange") GX_REQUIRED(setFloat,"GXSetFloat")
#undef GX_REQUIRED
        const Result result=galaxyResult(initialize(),QStringLiteral("初始化 Galaxy 失败"));
        if(!result) return result;
        initialized=true; ready=true; return {};
    }
};
class GalaxyBackend final : public Backend
{
public:
    GalaxyBackend(Device device,std::shared_ptr<Api> api):m_device(std::move(device)),m_api(std::move(api)) {}
    ~GalaxyBackend() override { close(); }
    Result open() override
    {
        m_requested.clear();
        {
            std::lock_guard<std::mutex> lock(m_api->enumeration);
            Result result=m_api->ensure(); if(!result) return result;
            uint32_t count=0;
            result=galaxyResult(m_api->update(&count,500),QStringLiteral("刷新大恒设备失败"));
            if(!result) return result;
            QByteArray serial=m_device.locator.toUtf8();
            Abi::GalaxyOpen options{serial.data(),0,4};
            result=galaxyResult(m_api->open(&options,&m_handle),QStringLiteral("打开大恒相机失败，请检查驱动、网段及占用"));
            if(!result) return result;
        }
        Result result=galaxyResult(m_api->setEnum(m_handle,Abi::GxTriggerMode,0),QStringLiteral("设置大恒连续触发失败"));
        if(!result) return result;
        result=galaxyResult(m_api->setEnum(m_handle,Abi::GxAcquisitionMode,2),QStringLiteral("设置大恒采集模式失败"));
        if(!result) return result;
        int64_t format=0;
        result=galaxyResult(m_api->getEnum(m_handle,Abi::GxPixelFormat,&format),QStringLiteral("读取大恒像素格式失败"));
        if(!result) return result;
        const bool supported=format==Abi::Mono8 || format==Abi::Rgb8 || format==Abi::Bgr8 || (format>=0x01080008 && format<=0x0108000b);
        if(!supported)
            return {EXvCameraError::Unsupported,QStringLiteral("大恒当前像素格式为 0x%1，请在 Galaxy 软件中设为 Mono8、Bayer8 或 RGB8 后重连")
                .arg(quint64(format),8,16,QLatin1Char('0'))};
        int64_t payload=0;
        result=galaxyResult(m_api->getInt(m_handle,Abi::GxPayload,&payload),QStringLiteral("读取大恒帧长度失败"));
        if(!result) return result;
        if(payload<=0 || payload>512*1024*1024) return {EXvCameraError::IoError,QStringLiteral("大恒图像长度无效或超过 512 MB")};
        m_pixels.resize(int(payload));
        result=galaxyResult(m_api->command(m_handle,Abi::GxStart),QStringLiteral("启动大恒采集失败"));
        m_started=bool(result); m_trigger=QStringLiteral("Continuous");
        return result;
    }
    void close() override
    {
        if(!m_handle) return;
        if(m_started) m_api->command(m_handle,Abi::GxStop);
        m_api->close(m_handle); m_handle=nullptr; m_started=false; m_pixels.clear();
    }
    Result beginFrame() override
    {
        if(!m_started) return {EXvCameraError::NotOpen,QStringLiteral("大恒采集尚未启动")};
        Result result=galaxyResult(m_api->clear(m_handle),QStringLiteral("清空大恒旧帧失败"));
        if(result && m_trigger==QStringLiteral("Software")) result=galaxyResult(m_api->command(m_handle,Abi::GxTrigger),QStringLiteral("大恒软件触发失败"));
        return result;
    }
    Result read(QImage &image,unsigned int timeoutMs) override
    {
        Abi::GalaxyFrame frame{};
        frame.data=m_pixels.data(); frame.length=m_pixels.size();
        Result result=galaxyResult(m_api->get(m_handle,&frame,timeoutMs),QStringLiteral("读取大恒图像失败"));
        if(!result) return result;
        if(frame.status!=0 || frame.data!=m_pixels.data() || frame.length<=0 || frame.length>m_pixels.size())
            return {EXvCameraError::IoError,QStringLiteral("大恒图像不完整或缓冲区无效")};
        return copyPackedImage(frame.data,frame.width,frame.height,frame.pixelType,quint64(frame.length),image);
    }
    QList<XvCameraParameterDescriptor> parameters() const override
    {
        QList<XvCameraParameterDescriptor> result;
        if(!m_handle) return result;
        for(const auto &key:{QStringLiteral("ExposureTime"),QStringLiteral("Gain")})
        {
            const int32_t feature=key==QStringLiteral("Gain")?Abi::GxGain:Abi::GxExposure;
            Abi::GalaxyFloatRange range{}; double value=0;
            if(!m_api->floatRange(m_handle,feature,&range) && !m_api->getFloat(m_handle,feature,&value))
                result.append(realParameter(key,key==QStringLiteral("Gain")?QStringLiteral("增益"):QStringLiteral("曝光时间（微秒）"),range.minimum,range.maximum,value));
        }
        XvCameraParameterDescriptor trigger;
        trigger.key=QStringLiteral("TriggerMode"); trigger.displayName=QStringLiteral("触发模式");
        trigger.type=EXvCameraParameterType::Enumeration;
        trigger.enumValues=QStringList{QStringLiteral("Continuous"),QStringLiteral("Software"),QStringLiteral("Line0")};
        trigger.defaultValue=m_trigger; result.append(trigger);
        return result;
    }
    QVariant parameter(const QString &key) const override
    {
        if(key==QStringLiteral("TriggerMode")) return m_trigger;
        double value=0;
        if(m_handle && (key==QStringLiteral("Gain") || key==QStringLiteral("ExposureTime")) &&
            !m_api->getFloat(m_handle,key==QStringLiteral("Gain")?Abi::GxGain:Abi::GxExposure,&value)) return value;
        return {};
    }
    Result setParameter(const QString &key,const QVariant &value) override
    {
        if(key==QStringLiteral("TriggerMode"))
        {
            const QString mode=value.toString();
            if(value.userType()!=QMetaType::QString || (mode!=QStringLiteral("Continuous") && mode!=QStringLiteral("Software") && mode!=QStringLiteral("Line0")))
                return {EXvCameraError::InvalidArgument,QStringLiteral("无效的触发模式")};
            if(mode==m_trigger) return {};
            Result result=galaxyResult(m_api->command(m_handle,Abi::GxStop),QStringLiteral("停止大恒采集失败"));
            if(!result) return result;
            m_started=false;
            int32_t code=m_api->setEnum(m_handle,Abi::GxTriggerMode,mode==QStringLiteral("Continuous")?0:1);
            if(!code && mode!=QStringLiteral("Continuous")) code=m_api->setEnum(m_handle,Abi::GxTriggerSource,mode==QStringLiteral("Software")?0:1);
            if(code)
            {
                m_api->setEnum(m_handle,Abi::GxTriggerMode,m_trigger==QStringLiteral("Continuous")?0:1);
                if(m_trigger!=QStringLiteral("Continuous")) m_api->setEnum(m_handle,Abi::GxTriggerSource,m_trigger==QStringLiteral("Software")?0:1);
            }
            else m_trigger=mode;
            const int32_t started=m_api->command(m_handle,Abi::GxStart); m_started=!started;
            return galaxyResult(started?started:code,QStringLiteral("设置大恒触发模式失败，设备可能不支持此触发源"));
        }
        if(key!=QStringLiteral("ExposureTime") && key!=QStringLiteral("Gain")) return Backend::setParameter(key,value);
        double number=0;
        if(!numericParameter(value,number)) return {EXvCameraError::InvalidArgument,QStringLiteral("相机参数必须为有限数值")};
        if(m_requested.contains(key) && m_requested.value(key)==number) return {};
        const int32_t feature=key==QStringLiteral("Gain")?Abi::GxGain:Abi::GxExposure;
        Abi::GalaxyFloatRange range{};
        Result result=galaxyResult(m_api->floatRange(m_handle,feature,&range),QStringLiteral("读取大恒参数范围失败"));
        if(!result) return result;
        if(!numericParameter(value,number) || number<range.minimum || number>range.maximum)
            return {EXvCameraError::InvalidArgument,QStringLiteral("参数超出相机允许范围")};
        result=galaxyResult(m_api->setEnum(m_handle,key==QStringLiteral("Gain")?Abi::GxGainAuto:Abi::GxExposureAuto,0),QStringLiteral("关闭大恒自动调节失败"));
        if(!result) return result;
        result=galaxyResult(m_api->setFloat(m_handle,feature,number),QStringLiteral("设置大恒参数失败"));
        if(result) m_requested.insert(key,number);
        return result;
    }
private:
    QHash<QString,double> m_requested;
    Device m_device; std::shared_ptr<Api> m_api; void *m_handle=nullptr;
    bool m_started=false; QByteArray m_pixels; QString m_trigger=QStringLiteral("Continuous");
};
class GalaxySystem final : public System
{
public:
    QString id() const override { return QStringLiteral("daheng-galaxy"); }
    QString name() const override { return QStringLiteral("大恒 Galaxy"); }
    Result scan(QList<Device> &devices) override
    {
        std::lock_guard<std::mutex> lock(m_api->enumeration);
        Result result=m_api->ensure(); if(!result) return result;
        uint32_t count=0; result=galaxyResult(m_api->update(&count,500),QStringLiteral("扫描大恒设备失败"));
        if(!result || !count) return result;
        if(count>256) return {EXvCameraError::IoError,QStringLiteral("大恒设备数量超过安全上限")};
        std::vector<Abi::GalaxyDevice> list(count);
        size_t size=list.size()*sizeof(Abi::GalaxyDevice);
        result=galaxyResult(m_api->enumerate(list.data(),&size),QStringLiteral("读取大恒设备列表失败"));
        if(!result) return result;
        if(size>list.size()*sizeof(Abi::GalaxyDevice) || size%sizeof(Abi::GalaxyDevice))
            return {EXvCameraError::IoError,QStringLiteral("大恒设备列表长度无效")};
        for(size_t i=0;i<size/sizeof(Abi::GalaxyDevice);++i)
        {
            const auto &entry=list[i];
            Device device;
            device.locator=sdkText(entry.serial);
            if(device.locator.isEmpty()) continue;
            device.info.providerId=id(); device.info.deviceId=id()+QLatin1Char(':')+device.locator;
            device.info.vendor=sdkText(entry.vendor); device.info.model=sdkText(entry.model); device.info.serialNumber=device.locator;
            device.info.transport=entry.type==2?QStringLiteral("GigE Vision"):QStringLiteral("USB Vision");
            device.info.displayName=QStringLiteral("大恒 · %1 [%2]").arg(device.info.model,device.locator);
            devices.append(device);
        }
        return {};
    }
    std::unique_ptr<Backend> create(const Device &device) override { return std::make_unique<GalaxyBackend>(device,m_api); }
private:
    std::shared_ptr<Api> m_api=std::make_shared<Api>();
};
}
std::shared_ptr<System> makeGalaxySystem() { return std::make_shared<GalaxySystem>(); }
} }
