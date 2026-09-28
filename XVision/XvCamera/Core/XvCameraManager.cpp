#include "XvCameraManager.h"

#include "IXvCamera.h"
#include "XvCameraProvider.h"
#include "XvDirectoryCamera.h"

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

EXvCameraError XvCameraManager::openCamera(const QString &deviceId,
                                           IXvCamera **cameraOut)
{
    if(QThread::currentThread()!=thread())
    {
        EXvCameraError result=EXvCameraError::Internal;
        IXvCamera *resolvedCamera=nullptr;
        const bool invoked=QMetaObject::invokeMethod(this,[this,deviceId,
                                                   &result,&resolvedCamera]()
        {
            result=openCamera(deviceId,&resolvedCamera);
        },Qt::BlockingQueuedConnection);
        if(!invoked)
        {
            setLastError("Unable to dispatch camera open to the manager thread");
            if(cameraOut) *cameraOut=nullptr;
            return EXvCameraError::Internal;
        }
        if(cameraOut) *cameraOut=resolvedCamera;
        return result;
    }

    Q_D(XvCameraManager);
    if(cameraOut) *cameraOut=nullptr;
    if(deviceId.isEmpty())
    {
        setLastError("Camera device ID is empty");
        return EXvCameraError::InvalidArgument;
    }
    {
        QMutexLocker locker(&d->mutex);
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
    const EXvCameraError openResult=candidate->open();
    if(openResult!=EXvCameraError::None)
    {
        setLastError(candidate->lastError());
        candidate->deleteLater();
        return openResult;
    }

    {
        QMutexLocker locker(&d->mutex);
        if(d->cameras.value(deviceId))
        {
            locker.unlock();
            candidate->close();
            candidate->deleteLater();
            setLastError(QString("Camera [%1] was opened concurrently").arg(deviceId));
            return EXvCameraError::AlreadyOpen;
        }
        d->cameras.insert(deviceId,candidate);
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
    emit cameraOpened(candidate);
    return EXvCameraError::None;
}

EXvCameraError XvCameraManager::closeCamera(const QString &deviceId)
{
    if(QThread::currentThread()!=thread())
    {
        EXvCameraError result=EXvCameraError::Internal;
        const bool invoked=QMetaObject::invokeMethod(this,[this,deviceId,&result]()
        {
            result=closeCamera(deviceId);
        },Qt::BlockingQueuedConnection);
        if(!invoked)
        {
            setLastError("Unable to dispatch camera close to the manager thread");
            return EXvCameraError::Internal;
        }
        return result;
    }

    Q_D(XvCameraManager);
    QPointer<IXvCamera> camera;
    {
        QMutexLocker locker(&d->mutex);
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
    return d->cameras.value(deviceId);
}

void XvCameraManager::shutdown()
{
    Q_D(XvCameraManager);
    QList<QPointer<IXvCamera>> cameras;
    {
        QMutexLocker locker(&d->mutex);
        cameras=d->cameras.values();
        d->cameras.clear();
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
