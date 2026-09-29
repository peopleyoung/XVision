#include "VendorLibrary.h"
#include <mutex>

namespace XvCamera { namespace Hardware {
namespace {
Result mvsResult(int code,const QString &action)
{
    if(!code) return {};
    const auto value=static_cast<uint32_t>(code);
    const auto error=value==0x80000007?EXvCameraError::Timeout:
        value==0x80000206?EXvCameraError::Disconnected:EXvCameraError::IoError;
    return {error,action+QStringLiteral("，海康错误码 0x%1").arg(value,8,16,QLatin1Char('0'))};
}
struct Api : VendorLibrary
{
    std::mutex enumeration;
    bool ready=false,initialized=false;
    int (XV_CAMERA_CALL *initialize)()=nullptr;
    int (XV_CAMERA_CALL *finalize)()=nullptr;
    int (XV_CAMERA_CALL *enumerate)(unsigned int,Abi::MvsDevices*)=nullptr;
    int (XV_CAMERA_CALL *create)(void**,const void*)=nullptr;
    int (XV_CAMERA_CALL *destroy)(void*)=nullptr;
    int (XV_CAMERA_CALL *open)(void*,unsigned int,unsigned short)=nullptr;
    int (XV_CAMERA_CALL *close)(void*)=nullptr;
    int (XV_CAMERA_CALL *start)(void*)=nullptr;
    int (XV_CAMERA_CALL *stop)(void*)=nullptr;
    int (XV_CAMERA_CALL *get)(void*,Abi::MvsFrame*,unsigned int)=nullptr;
    int (XV_CAMERA_CALL *release)(void*,Abi::MvsFrame*)=nullptr;
    int (XV_CAMERA_CALL *convert)(void*,Abi::MvsConvert*)=nullptr;
    int (XV_CAMERA_CALL *setEnum)(void*,const char*,const char*)=nullptr;
    int (XV_CAMERA_CALL *getFloat)(void*,const char*,Abi::MvsFloat*)=nullptr;
    int (XV_CAMERA_CALL *setFloat)(void*,const char*,float)=nullptr;
    int (XV_CAMERA_CALL *command)(void*,const char*)=nullptr;
    int (XV_CAMERA_CALL *clear)(void*)=nullptr;
    int (XV_CAMERA_CALL *packetSize)(void*)=nullptr;
    int (XV_CAMERA_CALL *setInt)(void*,const char*,int64_t)=nullptr;
    ~Api() { if(initialized && finalize) finalize(); }
    Result ensure()
    {
        if(ready) return {};
        QStringList paths;
        const QString runtime=qEnvironmentVariable("MVCAM_COMMON_RUNENV");
        if(!runtime.isEmpty()) paths << runtime << QDir(runtime).filePath("Win64_x64") << QDir(runtime).filePath("64");
#ifdef Q_OS_WIN
        for(const auto &root:{qEnvironmentVariable("CommonProgramFiles"),qEnvironmentVariable("CommonProgramFiles(x86)")})
            if(!root.isEmpty()) paths << QDir(root).filePath("MVS/Runtime/Win64_x64");
        const QString base=QStringLiteral("MvCameraControl.dll");
#else
        paths << QStringLiteral("/opt/MVS/lib/64");
        const QString base=QStringLiteral("libMvCameraControl.so");
#endif
        const Result loaded=load("XVISION_MVS_LIBRARY",base,paths);
        if(!loaded) return loaded;
#define MVS_REQUIRED(member,name) if(!resolve(member,name)) return {EXvCameraError::Unsupported,QStringLiteral("MVS SDK 缺少接口：")+QString::fromLatin1(name)};
        MVS_REQUIRED(enumerate,"MV_CC_EnumDevices")
        MVS_REQUIRED(create,"MV_CC_CreateHandle") MVS_REQUIRED(destroy,"MV_CC_DestroyHandle")
        MVS_REQUIRED(open,"MV_CC_OpenDevice") MVS_REQUIRED(close,"MV_CC_CloseDevice")
        MVS_REQUIRED(start,"MV_CC_StartGrabbing") MVS_REQUIRED(stop,"MV_CC_StopGrabbing")
        MVS_REQUIRED(get,"MV_CC_GetImageBuffer") MVS_REQUIRED(release,"MV_CC_FreeImageBuffer")
        MVS_REQUIRED(convert,"MV_CC_ConvertPixelTypeEx") MVS_REQUIRED(setEnum,"MV_CC_SetEnumValueByString")
        MVS_REQUIRED(getFloat,"MV_CC_GetFloatValue") MVS_REQUIRED(setFloat,"MV_CC_SetFloatValue")
        MVS_REQUIRED(command,"MV_CC_SetCommandValue") MVS_REQUIRED(clear,"MV_CC_ClearImageBuffer")
#undef MVS_REQUIRED
        resolve(packetSize,"MV_CC_GetOptimalPacketSize"); resolve(setInt,"MV_CC_SetIntValueEx");
        resolve(initialize,"MV_CC_Initialize"); resolve(finalize,"MV_CC_Finalize");
        if(initialize && !initialized)
        {
            if(!finalize) return {EXvCameraError::Unsupported,QStringLiteral("MVS SDK 缺少退出接口")};
            const Result result=mvsResult(initialize(),QStringLiteral("初始化 MVS 失败"));
            if(!result) return result;
            initialized=true;
        }
        ready=true; return {};
    }
};
Device describe(const void *raw)
{
    const auto &entry=*static_cast<const Abi::MvsDevicePrefix*>(raw);
    Device device;
    if(entry.transport==1)
    {
        device.info.vendor=sdkText(entry.special.gige.vendor);
        device.info.model=sdkText(entry.special.gige.model);
        device.info.serialNumber=sdkText(entry.special.gige.serial);
        device.info.transport=QStringLiteral("GigE Vision");
        device.locator=device.info.serialNumber.isEmpty()?
            QStringLiteral("mac-%1-%2").arg(entry.macHigh,0,16).arg(entry.macLow,0,16):device.info.serialNumber;
    }
    else if(entry.transport==4)
    {
        device.info.vendor=sdkText(entry.special.usb.vendor);
        device.info.model=sdkText(entry.special.usb.model);
        device.info.serialNumber=sdkText(entry.special.usb.serial);
        device.info.transport=QStringLiteral("USB3 Vision");
        device.locator=device.info.serialNumber.isEmpty()?sdkText(entry.special.usb.guid):device.info.serialNumber;
    }
    else return {};
    if(device.locator.isEmpty()) return {};
    device.info.providerId=QStringLiteral("hik-mvs");
    device.info.deviceId=device.info.providerId+QLatin1Char(':')+device.locator;
    device.info.displayName=QStringLiteral("海康 · %1 [%2]").arg(device.info.model,device.locator);
    return device;
}
class MvsBackend final : public Backend
{
public:
    MvsBackend(Device device,std::shared_ptr<Api> api):m_device(std::move(device)),m_api(std::move(api)) {}
    ~MvsBackend() override { close(); }
    Result open() override
    {
        m_requested.clear();
        {
            std::lock_guard<std::mutex> lock(m_api->enumeration);
            Result result=m_api->ensure(); if(!result) return result;
            Abi::MvsDevices devices{};
            result=mvsResult(m_api->enumerate(1|4,&devices),QStringLiteral("重新枚举海康相机失败"));
            if(!result) return result;
            if(devices.count>256) return {EXvCameraError::IoError,QStringLiteral("海康设备数量无效")};
            for(uint32_t i=0;i<devices.count;++i)
                if(devices.devices[i] && describe(devices.devices[i]).info.deviceId==m_device.info.deviceId)
                { result=mvsResult(m_api->create(&m_handle,devices.devices[i]),QStringLiteral("创建海康句柄失败")); break; }
            if(!result) return result;
            if(!m_handle) return {EXvCameraError::NotFound,QStringLiteral("海康相机已不在线，请刷新列表")};
        }
        Result result=mvsResult(m_api->open(m_handle,1,0),QStringLiteral("打开海康相机失败，请检查网段及设备占用"));
        if(!result) return result;
        m_open=true;
        if(m_device.info.transport==QStringLiteral("GigE Vision") && m_api->packetSize && m_api->setInt)
        {
            const int packet=m_api->packetSize(m_handle);
            if(packet>0) m_api->setInt(m_handle,"GevSCPSPacketSize",packet);
        }
        result=mvsResult(m_api->setEnum(m_handle,"TriggerMode","Off"),QStringLiteral("设置连续触发失败"));
        if(!result) return result;
        result=mvsResult(m_api->setEnum(m_handle,"AcquisitionMode","Continuous"),QStringLiteral("设置采集模式失败"));
        if(!result) return result;
        result=mvsResult(m_api->start(m_handle),QStringLiteral("启动海康采集失败"));
        m_started=bool(result); m_trigger=QStringLiteral("Continuous");
        return result;
    }
    void close() override
    {
        if(!m_handle) return;
        if(m_started) m_api->stop(m_handle);
        if(m_open) m_api->close(m_handle);
        m_api->destroy(m_handle); m_handle=nullptr; m_open=false; m_started=false;
    }
    Result beginFrame() override
    {
        if(!m_started) return {EXvCameraError::NotOpen,QStringLiteral("海康采集尚未启动")};
        Result result=mvsResult(m_api->clear(m_handle),QStringLiteral("清空海康旧帧失败"));
        if(result && m_trigger==QStringLiteral("Software")) result=mvsResult(m_api->command(m_handle,"TriggerSoftware"),QStringLiteral("海康软件触发失败"));
        return result;
    }
    Result read(QImage &image,unsigned int timeoutMs) override
    {
        Abi::MvsFrame frame{};
        Result result=mvsResult(m_api->get(m_handle,&frame,timeoutMs),QStringLiteral("读取海康图像失败"));
        if(!result) return result;
        struct Release { Api &api; void *handle; Abi::MvsFrame &frame; ~Release() { api.release(handle,&frame); } } release{*m_api,m_handle,frame};
        const int width=int(frame.info.width?frame.info.width:frame.info.extendedWidth);
        const int height=int(frame.info.height?frame.info.height:frame.info.extendedHeight);
        const quint64 length=frame.info.length?frame.info.length:frame.info.extendedLength;
        const unsigned int bits=(quint32(frame.info.pixelType)>>16)&0xff;
        const quint64 minimumLength=(quint64(width)*quint64(height)*bits+7)/8;
        if(!frame.data || !validImageSize(width,height,1,512*1024*1024) ||
           !bits || !length || length<minimumLength || length>512*1024*1024)
            return {EXvCameraError::IoError,QStringLiteral("海康图像缓冲区不完整")};
        if(frame.info.pixelType==Abi::Mono8 || frame.info.pixelType==Abi::Rgb8 || frame.info.pixelType==Abi::Bgr8)
            return copyPackedImage(frame.data,width,height,frame.info.pixelType,length,image);
        if(!validImageSize(width,height,3,512*1024*1024))
            return {EXvCameraError::IoError,QStringLiteral("海康图像超过内存上限")};
        QByteArray pixels(width*height*3,Qt::Uninitialized);
        Abi::MvsConvert conversion{};
        conversion.width=uint32_t(width); conversion.height=uint32_t(height);
        conversion.sourceType=frame.info.pixelType; conversion.source=frame.data; conversion.sourceLength=uint32_t(length);
        conversion.destinationType=static_cast<Abi::MvsPixel>(Abi::Rgb8); conversion.destination=reinterpret_cast<unsigned char*>(pixels.data());
        conversion.destinationCapacity=uint32_t(pixels.size());
        result=mvsResult(m_api->convert(m_handle,&conversion),QStringLiteral("海康像素转换失败"));
        if(!result) return result;
        if(conversion.destinationLength>conversion.destinationCapacity)
            return {EXvCameraError::IoError,QStringLiteral("海康像素转换返回无效长度")};
        return copyPackedImage(pixels.constData(),width,height,Abi::Rgb8,conversion.destinationLength,image);
    }
    QList<XvCameraParameterDescriptor> parameters() const override
    {
        QList<XvCameraParameterDescriptor> result;
        if(!m_open) return result;
        for(const auto &key:{QStringLiteral("ExposureTime"),QStringLiteral("Gain")})
        {
            Abi::MvsFloat value{};
            if(!m_api->getFloat(m_handle,key.toLatin1().constData(),&value))
                result.append(realParameter(key,key==QStringLiteral("Gain")?QStringLiteral("增益"):QStringLiteral("曝光时间（微秒）"),value.minimum,value.maximum,value.value));
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
        Abi::MvsFloat value{};
        if(m_open && (key==QStringLiteral("ExposureTime") || key==QStringLiteral("Gain")) && !m_api->getFloat(m_handle,key.toLatin1().constData(),&value)) return double(value.value);
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
            Result result=mvsResult(m_api->stop(m_handle),QStringLiteral("停止海康采集失败"));
            if(!result) return result;
            m_started=false;
            int code=m_api->setEnum(m_handle,"TriggerMode",mode==QStringLiteral("Continuous")?"Off":"On");
            if(!code && mode!=QStringLiteral("Continuous")) code=m_api->setEnum(m_handle,"TriggerSource",mode.toLatin1().constData());
            if(code)
            {
                m_api->setEnum(m_handle,"TriggerMode",m_trigger==QStringLiteral("Continuous")?"Off":"On");
                if(m_trigger!=QStringLiteral("Continuous")) m_api->setEnum(m_handle,"TriggerSource",m_trigger.toLatin1().constData());
            }
            else m_trigger=mode;
            const int started=m_api->start(m_handle); m_started=!started;
            return mvsResult(started?started:code,QStringLiteral("设置海康触发模式失败，设备可能不支持此触发源"));
        }
        if(key!=QStringLiteral("ExposureTime") && key!=QStringLiteral("Gain")) return Backend::setParameter(key,value);
        double number=0;
        if(!numericParameter(value,number)) return {EXvCameraError::InvalidArgument,QStringLiteral("相机参数必须为有限数值")};
        if(m_requested.contains(key) && m_requested.value(key)==number) return {};
        Abi::MvsFloat range{};
        Result result=mvsResult(m_api->getFloat(m_handle,key.toLatin1().constData(),&range),QStringLiteral("读取海康参数范围失败"));
        if(!result) return result;
        if(!numericParameter(value,number) || number<range.minimum || number>range.maximum)
            return {EXvCameraError::InvalidArgument,QStringLiteral("参数超出相机允许范围")};
        result=mvsResult(m_api->setEnum(m_handle,key==QStringLiteral("Gain")?"GainAuto":"ExposureAuto","Off"),QStringLiteral("关闭相机自动调节失败"));
        if(!result) return result;
        result=mvsResult(m_api->setFloat(m_handle,key.toLatin1().constData(),float(number)),QStringLiteral("设置海康参数失败"));
        if(result) m_requested.insert(key,number);
        return result;
    }
private:
    QHash<QString,double> m_requested;
    Device m_device; std::shared_ptr<Api> m_api; void *m_handle=nullptr;
    bool m_open=false,m_started=false; QString m_trigger=QStringLiteral("Continuous");
};
class MvsSystem final : public System
{
public:
    QString id() const override { return QStringLiteral("hik-mvs"); }
    QString name() const override { return QStringLiteral("海康 MVS"); }
    Result scan(QList<Device> &devices) override
    {
        std::lock_guard<std::mutex> lock(m_api->enumeration);
        Result result=m_api->ensure(); if(!result) return result;
        Abi::MvsDevices list{}; result=mvsResult(m_api->enumerate(1|4,&list),QStringLiteral("扫描海康设备失败"));
        if(!result) return result;
        if(list.count>256) return {EXvCameraError::IoError,QStringLiteral("海康设备数量无效")};
        for(uint32_t i=0;i<list.count;++i)
            if(list.devices[i]) { const auto device=describe(list.devices[i]); if(device.info.isValid()) devices.append(device); }
        return {};
    }
    std::unique_ptr<Backend> create(const Device &device) override { return std::make_unique<MvsBackend>(device,m_api); }
private:
    std::shared_ptr<Api> m_api=std::make_shared<Api>();
};
}
std::shared_ptr<System> makeMvsSystem() { return std::make_shared<MvsSystem>(); }
} }
