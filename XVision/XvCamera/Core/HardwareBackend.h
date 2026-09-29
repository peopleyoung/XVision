#pragma once

#include "XvCameraTypes.h"
#include "XvCameraProvider.h"
#include "IXvCamera.h"
#include <QMutex>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace XvCamera { namespace Hardware {
struct Result
{
    EXvCameraError error=EXvCameraError::None;
    QString message;
    explicit operator bool() const { return error==EXvCameraError::None; }
};
struct Device { XvCameraDeviceInfo info; QString locator; };
class Backend
{
public:
    virtual ~Backend()=default;
    virtual Result open()=0;
    virtual void close()=0;
    virtual Result beginFrame() { return {}; }
    // Must finish within timeoutMs plus bounded driver overhead. Own the pixels.
    virtual Result read(QImage &image,unsigned int timeoutMs)=0;
    virtual QList<XvCameraParameterDescriptor> parameters() const { return {}; }
    virtual QVariant parameter(const QString &) const { return {}; }
    virtual Result setParameter(const QString &,const QVariant &)
    { return {EXvCameraError::Unsupported,QStringLiteral("相机不支持此参数")}; }
};
class System
{
public:
    virtual ~System()=default;
    virtual QString id() const=0;
    virtual QString name() const=0;
    virtual Result scan(QList<Device> &devices)=0;
    virtual std::unique_ptr<Backend> create(const Device &device)=0;
};

// Private implementation seam exported solely for the camera runtime's tests.
class XVCAMERA_EXPORT Camera final : public IXvCamera
{
public:
    Camera(const Device &device,std::unique_ptr<Backend> backend,QObject *parent=nullptr);
    ~Camera() override;
    XvCameraDeviceInfo deviceInfo() const override { return m_device.info; }
    EXvCameraStatus status() const override { return m_status.load(); }
    QString lastError() const override;
    EXvCameraError open() override;
    EXvCameraError close() override;
    EXvCameraError grabFrame(XvCameraFrame &,unsigned int timeoutMs) override;
    EXvCameraError grabFrameWithParameters(XvCameraFrame &,unsigned int,const QVariantMap &) override;
    EXvCameraError startContinuous() override;
    EXvCameraError stopContinuous() override;
    QList<XvCameraParameterDescriptor> parameters() const override;
    QVariant parameter(const QString &key) const override;
    EXvCameraError setParameter(const QString &key,const QVariant &value) override;
private:
    EXvCameraError finish(const Result &result);
    EXvCameraError acquire(XvCameraFrame &,unsigned int,bool streaming,const QVariantMap &values={});
    void postFrame(const XvCameraFrame &,quint64 generation);
    Device m_device;
    std::unique_ptr<Backend> m_backend;
    mutable QMutex m_io;
    mutable QMutex m_errorMutex;
    QString m_error;
    std::atomic<EXvCameraStatus> m_status{EXvCameraStatus::Closed};
    std::atomic_bool m_stop{false};
    std::atomic_bool m_closing{false};
    std::mutex m_streamMutex;
    std::thread m_stream;
    quint64 m_sequence=0;
    std::atomic<quint64> m_generation{0};
    QMutex m_deliveryMutex;
    XvCameraFrame m_latest;
    bool m_deliveryPending=false;
};

class XVCAMERA_EXPORT Provider final : public XvCameraProvider
{
public:
    explicit Provider(std::shared_ptr<System> system,QObject *parent=nullptr);
    ~Provider() override;
    QString providerId() const override { return m_system->id(); }
    QList<XvCameraDeviceInfo> devices() const override;
    IXvCamera *createCamera(const QString &id,QObject *parent=nullptr) override;
    void refreshDevices() override;
    QString diagnostic() const override;
    bool isRefreshing() const override { return m_scanning.load(); }
private:
    std::shared_ptr<System> m_system;
    mutable QMutex m_mutex;
    QList<Device> m_devices;
    QString m_diagnostic;
    std::atomic_bool m_scanning{false};
    std::thread m_scan;
};
XVCAMERA_EXPORT std::shared_ptr<System> makeUvcSystem();
XVCAMERA_EXPORT std::shared_ptr<System> makeMvsSystem();
XVCAMERA_EXPORT std::shared_ptr<System> makeGalaxySystem();
} }
