#include "XvCameraManager.h"

#include "IXvCamera.h"
#include "XvCameraProvider.h"
#include "XvDirectoryCamera.h"
#include "HardwareBackend.h"

#include <QCoreApplication>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QThread>

namespace XvCamera
{

class XvCameraManagerPrivate
{
    Q_DISABLE_COPY(XvCameraManagerPrivate)
    Q_DECLARE_PUBLIC(XvCameraManager)
public:
    explicit XvCameraManagerPrivate(XvCameraManager *q):q_ptr(q) {}

    XvCameraManager *const q_ptr;
    mutable QMutex mutex;
    QMap<QString,QPointer<XvCameraProvider>> providers;
    QMap<QString,QPointer<IXvCamera>> cameras;
    QPointer<XvDirectoryCameraProvider> directoryProvider;
    mutable QString lastError;
    QSet<QString> opening;
    QSet<QString> closeAfterOpen;
};

XvCameraManager *XvCameraManager::instance()
{
    static XvCameraManager manager;
    return &manager;
}

XvCameraManager::XvCameraManager(QObject *parent)
    :QObject(parent),d_ptr(new XvCameraManagerPrivate(this))
{
    qRegisterMetaType<EXvCameraStatus>("XvCamera::EXvCameraStatus");
    qRegisterMetaType<EXvCameraError>("XvCamera::EXvCameraError");
    qRegisterMetaType<XvCameraDeviceInfo>("XvCamera::XvCameraDeviceInfo");
    qRegisterMetaType<XvCameraFrame>("XvCamera::XvCameraFrame");

    auto provider=new XvDirectoryCameraProvider(this);
    if(registerProvider(provider))
    {
        Q_D(XvCameraManager);
        d->directoryProvider=provider;
    }
    else
    {
        delete provider;
    }
    for(const auto &system:{Hardware::makeUvcSystem(),Hardware::makeMvsSystem(),Hardware::makeGalaxySystem()})
        registerProvider(new Hardware::Provider(system,this));
    if(QCoreApplication::instance()
            && QCoreApplication::instance()->thread()!=thread())
    {
        moveToThread(QCoreApplication::instance()->thread());
    }
}

XvCameraManager::~XvCameraManager()
{
    Q_D(XvCameraManager);
    {
        QMutexLocker locker(&d->mutex);
        d->cameras.clear();
        d->providers.clear();
        d->directoryProvider.clear();
    }
    const QList<IXvCamera*> cameras=
            findChildren<IXvCamera*>(QString(),Qt::FindDirectChildrenOnly);
    for(IXvCamera *camera:cameras)
    {
        if(!camera) continue;
        disconnect(camera,nullptr,this,nullptr);
        camera->close();
        delete camera;
    }
    const QList<XvCameraProvider*> providers=
            findChildren<XvCameraProvider*>(QString(),Qt::FindDirectChildrenOnly);
    for(XvCameraProvider *provider:providers)
    {
        if(!provider) continue;
        disconnect(provider,nullptr,this,nullptr);
        delete provider;
    }
}

void XvCameraManager::setLastError(const QString &message) const
{
    Q_D(const XvCameraManager);
    QMutexLocker locker(&d->mutex);
    d->lastError=message;
}

QString XvCameraManager::lastError() const
{
    Q_D(const XvCameraManager);
    QMutexLocker locker(&d->mutex);
    return d->lastError;
}

bool XvCameraManager::registerProvider(XvCameraProvider *provider)
{
    Q_D(XvCameraManager);
    if(!provider || provider->providerId().isEmpty())
    {
        setLastError("Camera provider or provider ID is invalid");
        return false;
    }
    if(provider->parent() && provider->parent()!=this)
    {
        setLastError("Camera provider is already owned by another object");
        return false;
    }

    const QString id=provider->providerId();
    {
        QMutexLocker locker(&d->mutex);
        if(d->providers.contains(id) && d->providers.value(id))
        {
            d->lastError=QString("Duplicate camera provider ID [%1]").arg(id);
            return false;
        }
        if(!provider->parent()) provider->setParent(this);
        d->providers.insert(id,provider);
        d->lastError.clear();
    }
    XvCameraProvider *providerIdentity=provider;
    connect(provider,&QObject::destroyed,this,[this,id,providerIdentity]()
    {
        Q_D(XvCameraManager);
        {
            QMutexLocker locker(&d->mutex);
            if(d->providers.value(id)==providerIdentity)
                d->providers.remove(id);
            if(d->directoryProvider==providerIdentity)
                d->directoryProvider.clear();
        }
        emit devicesChanged();
    });
    connect(provider,&XvCameraProvider::devicesChanged,
            this,&XvCameraManager::devicesChanged);
    emit devicesChanged();
    return true;
}

bool XvCameraManager::unregisterProvider(const QString &providerId)
{
    Q_D(XvCameraManager);
    QPointer<XvCameraProvider> provider;
    {
        QMutexLocker locker(&d->mutex);
        provider=d->providers.value(providerId);
        if(!provider)
        {
            d->lastError=QString("Camera provider [%1] was not found").arg(providerId);
            return false;
        }
        for(const QPointer<IXvCamera> &camera:d->cameras)
        {
            if(camera && camera->deviceInfo().providerId==providerId)
            {
                d->lastError=QString("Camera provider [%1] still has open devices")
                        .arg(providerId);
                return false;
            }
        }
        d->providers.remove(providerId);
        if(d->directoryProvider==provider) d->directoryProvider.clear();
        d->lastError.clear();
    }
    provider->deleteLater();
    emit devicesChanged();
    return true;
}

QList<XvCameraDeviceInfo> XvCameraManager::devices() const
{
    Q_D(const XvCameraManager);
    QList<QPointer<XvCameraProvider>> providers;
    {
        QMutexLocker locker(&d->mutex);
        providers=d->providers.values();
    }

    QMap<QString,XvCameraDeviceInfo> uniqueDevices;
    QSet<QString> duplicateIds;
    QString validationError;
    for(const QPointer<XvCameraProvider> &provider:providers)
    {
        if(!provider) continue;
        for(const XvCameraDeviceInfo &device:provider->devices())
        {
            if(!device.isValid() || device.providerId!=provider->providerId())
            {
                validationError=QString("Provider [%1] returned an invalid device")
                        .arg(provider->providerId());
                continue;
            }
            if(uniqueDevices.contains(device.deviceId)
                    || duplicateIds.contains(device.deviceId))
            {
                validationError=QString("Duplicate camera device ID [%1]")
                        .arg(device.deviceId);
                uniqueDevices.remove(device.deviceId);
                duplicateIds.insert(device.deviceId);
                continue;
            }
            uniqueDevices.insert(device.deviceId,device);
        }
    }
    setLastError(validationError);
    return uniqueDevices.values();
}

void XvCameraManager::refreshDevices()
{
    if(QThread::currentThread()!=thread())
    {
        QMetaObject::invokeMethod(this,[this]() { refreshDevices(); },Qt::QueuedConnection);
        return;
    }
    Q_D(XvCameraManager);
    QList<QPointer<XvCameraProvider>> providers;
    { QMutexLocker lock(&d->mutex); providers=d->providers.values(); }
    for(const auto &provider:providers) if(provider) provider->refreshDevices();
}
QStringList XvCameraManager::providerDiagnostics() const
{
    Q_D(const XvCameraManager);
    QList<QPointer<XvCameraProvider>> providers;
    { QMutexLocker lock(&d->mutex); providers=d->providers.values(); }
    QStringList result;
    for(const auto &provider:providers)
        if(provider && !provider->diagnostic().isEmpty()) result.append(provider->diagnostic());
    return result;
}
bool XvCameraManager::isRefreshing() const
{
    Q_D(const XvCameraManager);
    QMutexLocker lock(&d->mutex);
    for(const auto &provider:d->providers) if(provider && provider->isRefreshing()) return true;
    return false;
}
EXvCameraError XvCameraManager::openCamera(const QString &deviceId,IXvCamera **cameraOut)
{
    Q_D(XvCameraManager);
    if(cameraOut) *cameraOut=nullptr;
    IXvCamera *candidate=nullptr;
    EXvCameraError prepared=EXvCameraError::Internal;
    const auto prepare=[&]() { prepared=prepareCamera(deviceId,&candidate); };
    if(QThread::currentThread()==thread()) prepare();
    else if(!QMetaObject::invokeMethod(this,prepare,Qt::BlockingQueuedConnection))
    { setLastError(QStringLiteral("无法创建相机对象")); return EXvCameraError::Internal; }
    if(prepared!=EXvCameraError::None)
    {
        if(cameraOut && prepared==EXvCameraError::AlreadyOpen) *cameraOut=candidate;
        return prepared;
    }
    // Only QObject creation is dispatched to its owning thread. Native open runs
    // on the requesting flow/connection worker, so the GUI keeps processing events.
    EXvCameraError result=candidate->open();
    bool cancelled=false;
    {
        QMutexLocker lock(&d->mutex);
        d->opening.remove(deviceId);
        cancelled=d->closeAfterOpen.remove(deviceId);
        if(result!=EXvCameraError::None || cancelled) d->cameras.remove(deviceId);
        d->lastError=cancelled?QStringLiteral("相机打开已取消"):candidate->lastError();
    }
    if(result!=EXvCameraError::None || cancelled)
    {
        candidate->close(); candidate->deleteLater();
        return cancelled?EXvCameraError::NotOpen:result;
    }
    if(cameraOut) *cameraOut=candidate;
    emit cameraOpened(candidate);
    return EXvCameraError::None;
}
EXvCameraError XvCameraManager::prepareCamera(const QString &deviceId,IXvCamera **cameraOut)
{
    Q_D(XvCameraManager);
    if(cameraOut) *cameraOut=nullptr;
    if(deviceId.isEmpty())
    {
        setLastError("Camera device ID is empty");
        return EXvCameraError::InvalidArgument;
    }
    {
        QMutexLocker locker(&d->mutex);
        if(d->opening.contains(deviceId))
        { d->lastError=QStringLiteral("相机正在打开"); return EXvCameraError::Busy; }
        IXvCamera *existing=d->cameras.value(deviceId);
        if(existing)
        {
            if(cameraOut) *cameraOut=existing;
            d->lastError=QString("Camera [%1] is already open").arg(deviceId);
            return EXvCameraError::AlreadyOpen;
        }
    }

    QList<QPointer<XvCameraProvider>> providers;
    {
        QMutexLocker locker(&d->mutex);
        providers=d->providers.values();
    }
    QPointer<XvCameraProvider> matchedProvider;
    int matchCount=0;
    for(const QPointer<XvCameraProvider> &provider:providers)
    {
        if(!provider) continue;
        for(const XvCameraDeviceInfo &device:provider->devices())
        {
            if(device.deviceId==deviceId)
            {
                matchedProvider=provider;
                ++matchCount;
            }
        }
    }
    if(matchCount==0 || !matchedProvider)
    {
        setLastError(QString("Camera device [%1] was not found").arg(deviceId));
        return EXvCameraError::NotFound;
    }
    if(matchCount!=1)
    {
        setLastError(QString("Camera device ID [%1] is not unique").arg(deviceId));
        return EXvCameraError::InvalidArgument;
    }

    IXvCamera *candidate=matchedProvider->createCamera(deviceId,this);
    if(!candidate)
    {
        setLastError(QString("Provider could not create camera [%1]").arg(deviceId));
        return EXvCameraError::Internal;
    }
    {
        QMutexLocker locker(&d->mutex);
        d->cameras.insert(deviceId,candidate);
        d->opening.insert(deviceId);
        d->lastError.clear();
    }
    IXvCamera *cameraIdentity=candidate;
    connect(candidate,&QObject::destroyed,this,[this,deviceId,cameraIdentity]()
    {
        Q_D(XvCameraManager);
        QMutexLocker locker(&d->mutex);
        if(d->cameras.value(deviceId)==cameraIdentity)
            d->cameras.remove(deviceId);
    });
    if(cameraOut) *cameraOut=candidate;
    return EXvCameraError::None;
}

EXvCameraError XvCameraManager::closeCamera(const QString &deviceId)
{
    Q_D(XvCameraManager);
    QPointer<IXvCamera> camera;
    {
        QMutexLocker locker(&d->mutex);
        if(d->opening.contains(deviceId))
        { d->closeAfterOpen.insert(deviceId); return EXvCameraError::Busy; }
        camera=d->cameras.take(deviceId);
        if(!camera)
        {
            d->lastError=QString("Camera [%1] is not open").arg(deviceId);
            return EXvCameraError::NotOpen;
        }
    }
    const EXvCameraError result=camera->close();
    if(result!=EXvCameraError::None) setLastError(camera->lastError());
    else setLastError(QString());
    camera->deleteLater();
    emit cameraClosed(deviceId);
    return result;
}

IXvCamera *XvCameraManager::camera(const QString &deviceId) const
{
    Q_D(const XvCameraManager);
    QMutexLocker locker(&d->mutex);
    return d->opening.contains(deviceId)?nullptr:d->cameras.value(deviceId);
}

void XvCameraManager::shutdown()
{
    Q_D(XvCameraManager);
    QList<QPointer<IXvCamera>> cameras;
    {
        QMutexLocker locker(&d->mutex);
        for(auto it=d->cameras.begin();it!=d->cameras.end();)
        {
            if(d->opening.contains(it.key())) { d->closeAfterOpen.insert(it.key()); ++it; }
            else { cameras.append(it.value()); it=d->cameras.erase(it); }
        }
    }
    for(const QPointer<IXvCamera> &camera:cameras)
    {
        if(!camera) continue;
        const QString id=camera->deviceInfo().deviceId;
        camera->close();
        camera->deleteLater();
        emit cameraClosed(id);
    }
}

XvDirectoryCameraProvider *XvCameraManager::directoryProvider() const
{
    Q_D(const XvCameraManager);
    QMutexLocker locker(&d->mutex);
    return d->directoryProvider;
}

}
