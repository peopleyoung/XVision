#include "HardwareBackend.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QFileInfo>
#include <QSet>
#include <QElapsedTimer>
#include <algorithm>
#include <cstring>
#include <vector>

#ifdef Q_OS_LINUX
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <climits>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <condition_variable>
#include <mutex>
#endif

namespace XvCamera { namespace Hardware {
namespace {
QString stableId(const QString &locator)
{ return QStringLiteral("uvc:")+QString::fromLatin1(QCryptographicHash::hash(locator.toUtf8(),QCryptographicHash::Sha256).toHex()); }
#ifdef Q_OS_LINUX
int control(int fd,unsigned long request,void *data)
{
    int result;
    do { result=::ioctl(fd,request,data); } while(result<0 && errno==EINTR);
    return result;
}
Result linuxError(const QString &action)
{
    return {errno==ENODEV?EXvCameraError::Disconnected:EXvCameraError::IoError,
            action+QStringLiteral("：")+QString::fromLocal8Bit(std::strerror(errno))};
}
class UvcBackend final : public Backend
{
public:
    explicit UvcBackend(QString path):m_path(std::move(path)) {}
    ~UvcBackend() override { close(); }
    Result open() override
    {
        m_fd=::open(m_path.toLocal8Bit().constData(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
        if(m_fd<0) return linuxError(QStringLiteral("无法打开 USB 相机，请检查权限和占用"));
        v4l2_capability cap{};
        if(control(m_fd,VIDIOC_QUERYCAP,&cap)<0) return linuxError(QStringLiteral("无法查询 USB 相机"));
        const auto capabilities=(cap.capabilities&V4L2_CAP_DEVICE_CAPS)?cap.device_caps:cap.capabilities;
        if(!(capabilities&V4L2_CAP_VIDEO_CAPTURE) || !(capabilities&V4L2_CAP_STREAMING))
            return {EXvCameraError::Unsupported,QStringLiteral("设备不支持 V4L2 单平面视频采集")};
        bool selected=false;
        // Prefer uncompressed YUYV for predictable CPU work, then camera JPEG.
        for(const auto pixel:{V4L2_PIX_FMT_YUYV,V4L2_PIX_FMT_MJPEG,V4L2_PIX_FMT_GREY,V4L2_PIX_FMT_RGB24})
        {
            v4l2_format format{};
            format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format.fmt.pix.width=640; format.fmt.pix.height=480;
            format.fmt.pix.pixelformat=pixel; format.fmt.pix.field=V4L2_FIELD_ANY;
            if(control(m_fd,VIDIOC_S_FMT,&format)==0 && format.fmt.pix.pixelformat==pixel)
            { m_format=format.fmt.pix; selected=true; break; }
        }
        if(!selected) return {EXvCameraError::Unsupported,QStringLiteral("USB 相机没有可用的 YUYV/MJPEG/Mono8/RGB24 格式")};
        if(!m_format.width || !m_format.height || m_format.width>16384 || m_format.height>16384)
            return {EXvCameraError::Unsupported,QStringLiteral("USB 相机返回无效分辨率")};
        v4l2_requestbuffers request{};
        request.count=3; request.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; request.memory=V4L2_MEMORY_MMAP;
        if(control(m_fd,VIDIOC_REQBUFS,&request)<0) return linuxError(QStringLiteral("分配相机缓冲区失败"));
        if(!request.count || request.count>32) return {EXvCameraError::IoError,QStringLiteral("相机缓冲区数量无效")};
        for(unsigned int i=0;i<request.count;++i)
        {
            v4l2_buffer buffer{};
            buffer.type=request.type; buffer.memory=request.memory; buffer.index=i;
            if(control(m_fd,VIDIOC_QUERYBUF,&buffer)<0) return linuxError(QStringLiteral("查询相机缓冲区失败"));
            void *memory=::mmap(nullptr,buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,m_fd,buffer.m.offset);
            if(memory==MAP_FAILED) return linuxError(QStringLiteral("映射相机缓冲区失败"));
            m_buffers.push_back({memory,buffer.length});
            if(control(m_fd,VIDIOC_QBUF,&buffer)<0) return linuxError(QStringLiteral("提交相机缓冲区失败"));
        }
        auto type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(control(m_fd,VIDIOC_STREAMON,&type)<0) return linuxError(QStringLiteral("启动 USB 相机失败"));
        m_streaming=true;
        return {};
    }
    void close() override
    {
        if(m_fd<0) return;
        if(m_streaming) { auto type=V4L2_BUF_TYPE_VIDEO_CAPTURE; control(m_fd,VIDIOC_STREAMOFF,&type); }
        for(const auto &buffer:m_buffers) ::munmap(buffer.memory,buffer.size);
        m_buffers.clear(); ::close(m_fd); m_fd=-1; m_streaming=false;
    }
    Result beginFrame() override
    {
        // Bound draining by the allocated queue depth so a fast camera cannot
        // keep us in this loop; the next read waits for a newly completed frame.
        for(size_t i=0;i<m_buffers.size();++i)
        {
            v4l2_buffer buffer{};
            buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; buffer.memory=V4L2_MEMORY_MMAP;
            if(control(m_fd,VIDIOC_DQBUF,&buffer)<0)
                return errno==EAGAIN?Result{}:linuxError(QStringLiteral("清理 USB 旧帧失败"));
            if(control(m_fd,VIDIOC_QBUF,&buffer)<0) return linuxError(QStringLiteral("归还 USB 旧帧失败"));
        }
        return {};
    }
    Result read(QImage &image,unsigned int timeoutMs) override
    {
        pollfd descriptor{m_fd,POLLIN,0};
        const int ready=::poll(&descriptor,1,static_cast<int>(timeoutMs));
        if(!ready || (ready<0 && errno==EINTR)) return {EXvCameraError::Timeout,QStringLiteral("USB 相机等待图像超时")};
        if(ready<0 || (descriptor.revents&(POLLHUP|POLLERR|POLLNVAL)))
            return {EXvCameraError::Disconnected,QStringLiteral("USB 相机已断开或采集失败")};
        v4l2_buffer buffer{};
        buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; buffer.memory=V4L2_MEMORY_MMAP;
        if(control(m_fd,VIDIOC_DQBUF,&buffer)<0)
            return errno==EAGAIN?Result{EXvCameraError::Timeout,QStringLiteral("暂无 USB 图像")}:linuxError(QStringLiteral("读取 USB 图像失败"));
        Result result;
        if(buffer.index>=m_buffers.size() || buffer.bytesused>m_buffers[buffer.index].size || (buffer.flags&V4L2_BUF_FLAG_ERROR))
            result={EXvCameraError::IoError,QStringLiteral("USB 图像缓冲区无效或不完整")};
        else
        {
            const auto *data=static_cast<const uchar*>(m_buffers[buffer.index].memory);
            const int width=int(m_format.width),height=int(m_format.height);
            const int stride=int(m_format.bytesperline);
            if(m_format.pixelformat==V4L2_PIX_FMT_MJPEG)
                image=QImage::fromData(data,int(buffer.bytesused),"JPG");
            else if(m_format.pixelformat==V4L2_PIX_FMT_YUYV && width%2==0 && stride>=width*2 && quint64(stride)*height<=buffer.bytesused)
            {
                image=QImage(width,height,QImage::Format_RGB32);
                for(int y=0;y<height && !image.isNull();++y)
                {
                    auto *dst=reinterpret_cast<QRgb*>(image.scanLine(y));
                    const uchar *src=data+y*stride;
                    for(int x=0;x<width;x+=2,src+=4)
                    {
                        const int u=int(src[1])-128,v=int(src[3])-128;
                        for(int k=0;k<2;++k)
                        {
                            const int l=std::max(0,int(src[k*2])-16)*298;
                            dst[x+k]=qRgb(std::clamp((l+409*v+128)>>8,0,255),
                                std::clamp((l-100*u-208*v+128)>>8,0,255),std::clamp((l+516*u+128)>>8,0,255));
                        }
                    }
                }
            }
            else
            {
                const bool gray=m_format.pixelformat==V4L2_PIX_FMT_GREY;
                if((gray || m_format.pixelformat==V4L2_PIX_FMT_RGB24) && stride>=width*(gray?1:3) && quint64(stride)*height<=buffer.bytesused)
                    image=QImage(data,width,height,stride,gray?QImage::Format_Grayscale8:QImage::Format_RGB888).copy();
            }
            if(image.isNull()) result={EXvCameraError::IoError,QStringLiteral("USB 图像长度、步长或像素格式无效")};
        }
        if(control(m_fd,VIDIOC_QBUF,&buffer)<0) return linuxError(QStringLiteral("归还 USB 缓冲区失败"));
        return result;
    }
private:
    struct Buffer { void *memory; size_t size; };
    QString m_path;
    int m_fd=-1;
    bool m_streaming=false;
    v4l2_pix_format m_format{};
    std::vector<Buffer> m_buffers;
};
#endif
#ifdef Q_OS_WIN
using Microsoft::WRL::ComPtr;
struct ComScope
{
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComScope() { if(SUCCEEDED(result)) CoUninitialize(); }
};
struct MfRuntime
{
    HRESULT result=MFStartup(MF_VERSION,MFSTARTUP_FULL);
    ~MfRuntime() { if(SUCCEEDED(result)) MFShutdown(); }
};
Result mfError(HRESULT code,const QString &action)
{
    return {code==MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED?EXvCameraError::Disconnected:EXvCameraError::IoError,
        action+QStringLiteral("（0x%1）；请检查相机隐私权限、驱动和占用").arg(quint32(code),8,16,QLatin1Char('0'))};
}
// The callback owns only its mailbox, never a Camera or widget pointer.
class ReaderCallback final : public IMFSourceReaderCallback
{
public:
    STDMETHODIMP QueryInterface(REFIID id,void **object) override
    {
        if(!object) return E_POINTER;
        if(id==__uuidof(IUnknown) || id==__uuidof(IMFSourceReaderCallback))
        { *object=static_cast<IMFSourceReaderCallback*>(this); AddRef(); return S_OK; }
        *object=nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs; }
    STDMETHODIMP_(ULONG) Release() override
    { const ULONG count=--refs; if(!count) delete this; return count; }
    STDMETHODIMP OnEvent(DWORD,IMFMediaEvent *) override { return S_OK; }
    STDMETHODIMP OnFlush(DWORD) override
    { std::lock_guard<std::mutex> lock(mutex); pending=false; condition.notify_all(); return S_OK; }
    STDMETHODIMP OnReadSample(HRESULT status,DWORD,DWORD flags,LONGLONG,IMFSample *sample) override
    {
        QImage captured;
        if(SUCCEEDED(status) && (flags&MF_SOURCE_READERF_ENDOFSTREAM)) status=MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED;
        if(SUCCEEDED(status) && (flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) status=MF_E_INVALIDMEDIATYPE;
        if(SUCCEEDED(status) && sample)
        {
            ComPtr<IMFMediaBuffer> buffer;
            status=sample->ConvertToContiguousBuffer(&buffer);
            BYTE *data=nullptr; DWORD length=0;
            if(SUCCEEDED(status)) status=buffer->Lock(&data,nullptr,&length);
            if(SUCCEEDED(status))
            {
                const quint64 required=quint64(std::abs(stride))*(height-1)+quint64(width)*4;
                if(width && height && std::abs(stride)>=int(width)*4 && required<=length)
                {
                    captured=QImage(int(width),int(height),QImage::Format_RGB32);
                    for(UINT32 y=0;y<height && !captured.isNull();++y)
                    {
                        const UINT32 row=stride<0?height-1-y:y;
                        std::memcpy(captured.scanLine(int(y)),data+size_t(row)*size_t(std::abs(stride)),size_t(width)*4);
                    }
                }
                else status=MF_E_BUFFERTOOSMALL;
                buffer->Unlock();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            image=std::move(captured); result=status; pending=false; completed=true;
        }
        condition.notify_all();
        return S_OK;
    }
    std::atomic<ULONG> refs{1};
    std::mutex mutex;
    std::condition_variable condition;
    bool pending=false,completed=false;
    HRESULT result=S_OK;
    QImage image;
    UINT32 width=0,height=0;
    LONG stride=0;
};
class UvcBackend final : public Backend
{
public:
    UvcBackend(QString path,std::shared_ptr<MfRuntime> runtime):m_path(std::move(path)),m_runtime(std::move(runtime)) {}
    ~UvcBackend() override { close(); }
    Result open() override
    {
        ComScope com;
        if(FAILED(m_runtime->result)) return mfError(m_runtime->result,QStringLiteral("初始化系统视频组件失败"));
        ComPtr<IMFAttributes> attributes;
        HRESULT code=MFCreateAttributes(&attributes,2);
        if(SUCCEEDED(code)) code=attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if(SUCCEEDED(code)) code=attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,reinterpret_cast<LPCWSTR>(m_path.utf16()));
        if(SUCCEEDED(code)) code=MFCreateDeviceSource(attributes.Get(),&m_source);
        if(FAILED(code)) return mfError(code,QStringLiteral("打开 USB 相机失败"));
        m_callback.Attach(new ReaderCallback);
        attributes.Reset();
        code=MFCreateAttributes(&attributes,2);
        if(SUCCEEDED(code)) code=attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK,m_callback.Get());
        if(SUCCEEDED(code)) code=attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE);
        if(SUCCEEDED(code)) code=MFCreateSourceReaderFromMediaSource(m_source.Get(),attributes.Get(),&m_reader);
        ComPtr<IMFMediaType> format;
        if(SUCCEEDED(code)) code=MFCreateMediaType(&format);
        if(SUCCEEDED(code)) code=format->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);
        if(SUCCEEDED(code)) code=format->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32);
        if(SUCCEEDED(code)) code=m_reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,format.Get());
        format.Reset();
        if(SUCCEEDED(code)) code=m_reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&format);
        if(SUCCEEDED(code)) code=MFGetAttributeSize(format.Get(),MF_MT_FRAME_SIZE,&m_callback->width,&m_callback->height);
        UINT32 stride=0;
        if(SUCCEEDED(code))
        {
            if(FAILED(format->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride)))
                code=MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1,m_callback->width,&m_callback->stride);
            else m_callback->stride=static_cast<LONG>(stride);
        }
        if(FAILED(code)) return mfError(code,QStringLiteral("配置 USB 相机 RGB 图像失败"));
        if(!m_callback->width || !m_callback->height || m_callback->width>16384 || m_callback->height>16384 ||
           quint64(m_callback->width)*m_callback->height*4>512*1024*1024 ||
           m_callback->stride==LONG_MIN)
            return {EXvCameraError::Unsupported,QStringLiteral("USB 相机返回无效分辨率")};
        return {};
    }
    void close() override
    {
        ComScope com;
        if(m_reader) m_reader->Flush(MF_SOURCE_READER_ALL_STREAMS);
        m_reader.Reset();
        if(m_source) m_source->Shutdown();
        m_source.Reset(); m_callback.Reset();
    }
    Result read(QImage &image,unsigned int timeoutMs) override
    {
        ComScope com;
        auto *callback=m_callback.Get();
        std::unique_lock<std::mutex> lock(callback->mutex);
        if(!callback->pending && !callback->completed)
        {
            callback->pending=true;
            lock.unlock();
            const HRESULT code=m_reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,nullptr,nullptr,nullptr);
            lock.lock();
            if(FAILED(code)) { callback->pending=false; return mfError(code,QStringLiteral("请求 USB 图像失败")); }
        }
        if(!callback->condition.wait_for(lock,std::chrono::milliseconds(timeoutMs),[callback]() { return callback->completed; }))
            return {EXvCameraError::Timeout,QStringLiteral("USB 相机等待图像超时")};
        callback->completed=false;
        if(FAILED(callback->result)) return mfError(callback->result,QStringLiteral("读取 USB 图像失败"));
        image=std::move(callback->image);
        if(image.isNull()) return {EXvCameraError::Timeout,QStringLiteral("USB 相机尚未输出图像")};
        return {};
    }
private:
    QString m_path;
    std::shared_ptr<MfRuntime> m_runtime;
    ComPtr<IMFMediaSource> m_source;
    ComPtr<IMFSourceReader> m_reader;
    ComPtr<ReaderCallback> m_callback;
};
#endif
class UvcSystem final : public System
{
public:
    QString id() const override { return QStringLiteral("uvc"); }
    QString name() const override { return QStringLiteral("USB/UVC 系统相机"); }
    Result scan(QList<Device> &devices) override
    {
#ifdef Q_OS_LINUX
        QMap<QString,QString> paths;
        const QDir byId(QStringLiteral("/dev/v4l/by-id"));
        for(const auto &entry:byId.entryInfoList(QDir::Files|QDir::System|QDir::NoDotAndDotDot))
            paths.insert(entry.canonicalFilePath(),entry.absoluteFilePath());
        for(const auto &entry:QDir(QStringLiteral("/dev")).entryInfoList({QStringLiteral("video*")},QDir::System|QDir::Files))
            if(!paths.contains(entry.canonicalFilePath())) paths.insert(entry.canonicalFilePath(),entry.absoluteFilePath());
        QString denied;
        for(auto entry=paths.cbegin();entry!=paths.cend();++entry)
        {
            const int fd=::open(entry.value().toLocal8Bit().constData(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
            if(fd<0) { denied=QStringLiteral("部分视频设备无法访问，请检查 video 组权限或设备占用"); continue; }
            v4l2_capability cap{};
            const int result=control(fd,VIDIOC_QUERYCAP,&cap); ::close(fd);
            const auto capabilities=(cap.capabilities&V4L2_CAP_DEVICE_CAPS)?cap.device_caps:cap.capabilities;
            if(result<0 || !(capabilities&V4L2_CAP_VIDEO_CAPTURE)) continue;
            QString identity=entry.value();
            // Use the sysfs endpoint path, never a reassignable /dev/video index.
            if(!entry.value().startsWith(QStringLiteral("/dev/v4l/by-id/")))
            {
                const QString sys=QFileInfo(QStringLiteral("/sys/class/video4linux/")+QFileInfo(entry.key()).fileName()+QStringLiteral("/device")).canonicalFilePath();
                QFile index(QStringLiteral("/sys/class/video4linux/")+QFileInfo(entry.key()).fileName()+QStringLiteral("/index"));
                if(!index.open(QIODevice::ReadOnly) || sys.isEmpty()) continue;
                identity=sys+QLatin1Char(':')+QString::fromLatin1(index.readAll().trimmed());
            }
            Device device;
            device.locator=entry.value(); device.info.deviceId=stableId(identity); device.info.providerId=id();
            device.info.displayName=QString::fromLocal8Bit(reinterpret_cast<const char*>(cap.card),int(strnlen(reinterpret_cast<const char*>(cap.card),sizeof(cap.card))));
            device.info.model=device.info.displayName; device.info.transport=QStringLiteral("USB / V4L2");
            devices.append(device);
        }
        if(devices.isEmpty() && !denied.isEmpty()) return {EXvCameraError::IoError,denied};
        return {};
#elif defined(Q_OS_WIN)
        ComScope com;
        if(FAILED(m_runtime->result)) return mfError(m_runtime->result,QStringLiteral("系统视频组件不可用"));
        ComPtr<IMFAttributes> attributes;
        HRESULT code=MFCreateAttributes(&attributes,1);
        if(SUCCEEDED(code)) code=attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        IMFActivate **sources=nullptr; UINT32 count=0;
        if(SUCCEEDED(code)) code=MFEnumDeviceSources(attributes.Get(),&sources,&count);
        if(FAILED(code)) return mfError(code,QStringLiteral("扫描 USB 相机失败"));
        for(UINT32 i=0;i<count;++i)
        {
            WCHAR *name=nullptr,*link=nullptr; UINT32 length=0;
            sources[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,&name,&length);
            sources[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,&link,&length);
            if(link)
            {
                Device device;
                device.locator=QString::fromWCharArray(link); device.info.deviceId=stableId(device.locator.toLower());
                device.info.providerId=id(); device.info.displayName=name?QString::fromWCharArray(name):QStringLiteral("USB 相机");
                device.info.model=device.info.displayName; device.info.transport=QStringLiteral("USB / Media Foundation");
                devices.append(device);
            }
            CoTaskMemFree(name); CoTaskMemFree(link); sources[i]->Release();
        }
        CoTaskMemFree(sources);
        return {};
#else
        Q_UNUSED(devices)
        return {EXvCameraError::Unsupported,QStringLiteral("当前平台尚未实现系统相机后端")};
#endif
    }
    std::unique_ptr<Backend> create(const Device &device) override
    {
#ifdef Q_OS_LINUX
        return std::make_unique<UvcBackend>(device.locator);
#elif defined(Q_OS_WIN)
        return std::make_unique<UvcBackend>(device.locator,m_runtime);
#else
        Q_UNUSED(device)
        return {};
#endif
    }
#ifdef Q_OS_WIN
    std::shared_ptr<MfRuntime> m_runtime=std::make_shared<MfRuntime>();
#endif
};
}
std::shared_ptr<System> makeUvcSystem() { return std::make_shared<UvcSystem>(); }
} }
