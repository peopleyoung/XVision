#include "HardwareBackend.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>
#include <algorithm>
#include <exception>

namespace XvCamera { namespace Hardware {
Camera::Camera(const Device &device,std::unique_ptr<Backend> backend,QObject *parent)
    :IXvCamera(parent),m_device(device),m_backend(std::move(backend)) {}
Camera::~Camera() { close(); }
QString Camera::lastError() const
{ QMutexLocker lock(&m_errorMutex); return m_error; }
EXvCameraError Camera::finish(const Result &result)
{
    { QMutexLocker lock(&m_errorMutex); m_error=result.message; }
    if(result.error==EXvCameraError::Disconnected &&
       (status()==EXvCameraStatus::Open || status()==EXvCameraStatus::Streaming))
    {
        m_status=EXvCameraStatus::Disconnected;
        QMetaObject::invokeMethod(this,[this,result]()
        { emit statusChanged(status()); emit disconnected(result.message); },Qt::QueuedConnection);
    }
    // Queue notifications: no user callback runs while a native I/O lock is held.
    if(!result && result.error!=EXvCameraError::Timeout)
        QMetaObject::invokeMethod(this,[this,result]()
        { emit errorOccurred(result.error,result.message); },Qt::QueuedConnection);
    return result.error;
}
EXvCameraError Camera::open()
{
    QMutexLocker lock(&m_io);
    if(status()!=EXvCameraStatus::Closed)
        return finish({EXvCameraError::AlreadyOpen,QStringLiteral("相机已打开")});
    m_closing=false;
    if(!m_backend) return finish({EXvCameraError::Unsupported,QStringLiteral("当前平台没有可用的相机适配器")});
    const Result result=m_backend->open();
    if(!result) { m_backend->close(); return finish(result); }
    m_stop=false;
    m_sequence=0;
    m_status=EXvCameraStatus::Open;
    QMetaObject::invokeMethod(this,[this]() { emit statusChanged(status()); },Qt::QueuedConnection);
    return finish({});
}
EXvCameraError Camera::close()
{
    m_closing=true;
    stopContinuous();
    QMutexLocker lock(&m_io);
    if(m_backend) m_backend->close();
    m_status=EXvCameraStatus::Closed;
    ++m_generation;
    QMetaObject::invokeMethod(this,[this]() { emit statusChanged(status()); },Qt::QueuedConnection);
    return finish({});
}
EXvCameraError Camera::acquire(XvCameraFrame &frame,unsigned int timeoutMs,bool streaming,const QVariantMap &values)
{
    frame={};
    if(!m_io.tryLock()) return finish({EXvCameraError::Busy,QStringLiteral("相机正在执行其他操作")});
    struct Unlock { QMutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{m_io};
    const auto state=status();
    if((streaming && state!=EXvCameraStatus::Streaming) ||
       (!streaming && state!=EXvCameraStatus::Open))
        return finish({state==EXvCameraStatus::Streaming?EXvCameraError::Busy:EXvCameraError::NotOpen,
                       QStringLiteral("相机未就绪或正在连续采集")});
    for(auto it=values.cbegin();it!=values.cend();++it)
    {
        const Result configured=m_backend->setParameter(it.key(),it.value());
        if(!configured) return finish(configured);
    }
    QElapsedTimer timer;
    timer.start();
    Result result=m_backend->beginFrame();
    if(!result) return finish(result);
    do
    {
        if(m_closing || (streaming && m_stop))
            return finish({EXvCameraError::NotOpen,QStringLiteral("相机采集已停止")});
        const auto remaining=std::max<qint64>(0,qint64(timeoutMs)-timer.elapsed());
        QImage image;
        result=m_backend->read(image,static_cast<unsigned int>(std::min<qint64>(remaining,100)));
        if(result)
        {
            if(image.isNull()) return finish({EXvCameraError::IoError,QStringLiteral("相机返回空图像")});
            frame.image=std::move(image);
            frame.deviceId=m_device.info.deviceId;
            frame.sequence=++m_sequence;
            frame.capturedAtUtc=QDateTime::currentDateTimeUtc();
            return finish({});
        }
        if(result.error!=EXvCameraError::Timeout) break;
        // A driver may return a timeout immediately; do not spin at 100% CPU.
        if(remaining>0) QThread::msleep(1);
    } while(timer.elapsed()<qint64(timeoutMs));
    return finish(result);
}
EXvCameraError Camera::grabFrame(XvCameraFrame &frame,unsigned int timeoutMs)
{ return acquire(frame,timeoutMs,false); }
EXvCameraError Camera::grabFrameWithParameters(XvCameraFrame &frame,unsigned int timeoutMs,const QVariantMap &values)
{ return acquire(frame,timeoutMs,false,values); }
void Camera::postFrame(const XvCameraFrame &frame,quint64 generation)
{
    QMutexLocker lock(&m_deliveryMutex);
    m_latest=frame;
    if(m_deliveryPending) return;
    m_deliveryPending=true;
    QMetaObject::invokeMethod(this,[this,generation]()
    {
        XvCameraFrame frame;
        { QMutexLocker lock(&m_deliveryMutex); frame=m_latest; m_deliveryPending=false; }
        if(m_generation==generation && status()==EXvCameraStatus::Streaming) emit frameReady(frame);
    },Qt::QueuedConnection);
}
EXvCameraError Camera::startContinuous()
{
    std::lock_guard<std::mutex> streamLock(m_streamMutex);
    if(status()==EXvCameraStatus::Streaming)
        return finish({EXvCameraError::Busy,QStringLiteral("相机已在连续采集")});
    if(m_stream.joinable()) m_stream.join();
    QMutexLocker lock(&m_io);
    if(status()!=EXvCameraStatus::Open || m_closing)
        return finish({EXvCameraError::NotOpen,QStringLiteral("请先打开相机")});
    m_stop=false;
    m_status=EXvCameraStatus::Streaming;
    const quint64 generation=++m_generation;
    m_stream=std::thread([this,generation]()
    {
        while(!m_stop && !m_closing)
        {
            XvCameraFrame frame;
            const auto result=acquire(frame,60000,true);
            if(result==EXvCameraError::None) postFrame(frame,generation);
            else if(result!=EXvCameraError::Timeout && result!=EXvCameraError::Busy) break;
            QThread::msleep(1);
        }
        auto expected=EXvCameraStatus::Streaming;
        m_status.compare_exchange_strong(expected,EXvCameraStatus::Open);
        QMetaObject::invokeMethod(this,[this]() { emit statusChanged(status()); },Qt::QueuedConnection);
    });
    QMetaObject::invokeMethod(this,[this]() { emit statusChanged(status()); },Qt::QueuedConnection);
    return finish({});
}
EXvCameraError Camera::stopContinuous()
{
    std::lock_guard<std::mutex> streamLock(m_streamMutex);
    m_stop=true;
    ++m_generation;
    if(m_stream.joinable()) m_stream.join();
    return EXvCameraError::None;
}
QList<XvCameraParameterDescriptor> Camera::parameters() const
{ QMutexLocker lock(&m_io); return m_backend->parameters(); }
QVariant Camera::parameter(const QString &key) const
{ QMutexLocker lock(&m_io); return m_backend->parameter(key); }
EXvCameraError Camera::setParameter(const QString &key,const QVariant &value)
{
    if(!m_io.tryLock()) return finish({EXvCameraError::Busy,QStringLiteral("相机忙，请稍后设置参数")});
    struct Unlock { QMutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{m_io};
    if(status()!=EXvCameraStatus::Open)
        return finish({EXvCameraError::Busy,QStringLiteral("请在停止采集后设置相机参数")});
    return finish(m_backend->setParameter(key,value));
}

Provider::Provider(std::shared_ptr<System> system,QObject *parent)
    :XvCameraProvider(parent),m_system(std::move(system)),m_diagnostic(m_system->name()+QStringLiteral("：尚未扫描")) {}
Provider::~Provider() { if(m_scan.joinable()) m_scan.join(); }
QList<XvCameraDeviceInfo> Provider::devices() const
{
    QMutexLocker lock(&m_mutex);
    QList<XvCameraDeviceInfo> result;
    for(const auto &device:m_devices) result.append(device.info);
    return result;
}
QString Provider::diagnostic() const
{ QMutexLocker lock(&m_mutex); return m_diagnostic; }
IXvCamera *Provider::createCamera(const QString &id,QObject *parent)
{
    QMutexLocker lock(&m_mutex);
    for(const auto &device:m_devices)
        if(device.info.deviceId==id)
        {
            auto backend=m_system->create(device);
            return backend?new Camera(device,std::move(backend),parent):nullptr;
        }
    return nullptr;
}
void Provider::refreshDevices()
{
    if(m_scanning.exchange(true)) return;
    if(m_scan.joinable()) m_scan.join();
    { QMutexLocker lock(&m_mutex); m_diagnostic=m_system->name()+QStringLiteral("：正在扫描…"); }
    emit devicesChanged();
    m_scan=std::thread([this]()
    {
        QList<Device> devices;
        Result result;
        try { result=m_system->scan(devices); }
        catch(const std::exception &) { result={EXvCameraError::Internal,QStringLiteral("相机扫描失败")}; }
        {
            QMutexLocker lock(&m_mutex);
            m_devices=result?devices:QList<Device>{};
            m_diagnostic=m_system->name()+QStringLiteral("：")+(result?
                QStringLiteral("发现 %1 台设备").arg(m_devices.size()):result.message);
        }
        m_scanning=false;
        QMetaObject::invokeMethod(this,[this]() { emit devicesChanged(); },Qt::QueuedConnection);
    });
}
} }
