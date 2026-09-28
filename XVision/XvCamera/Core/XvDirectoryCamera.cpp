#include "XvDirectoryCamera.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <QWaitCondition>
#include <algorithm>

namespace XvCamera
{
namespace
{
const unsigned int MaximumFrameIntervalMs=60000;

bool isImageFile(const QFileInfo &file)
{
    static const QStringList extensions={"bmp","gif","jpeg","jpg",
                                         "png","tif","tiff"};
    return extensions.contains(file.suffix().toLower());
}

bool isIntegerVariant(const QVariant &value)
{
    const int type=value.userType();
    return type==QMetaType::Int || type==QMetaType::UInt
            || type==QMetaType::LongLong || type==QMetaType::ULongLong;
}
}

struct XvDirectoryCameraConfig
{
    XvCameraDeviceInfo info;
    QString directory;
    bool loop=false;
    unsigned int frameIntervalMs=0;
};

class XvDirectoryCameraProviderPrivate
{
    Q_DISABLE_COPY(XvDirectoryCameraProviderPrivate)
    Q_DECLARE_PUBLIC(XvDirectoryCameraProvider)
public:
    explicit XvDirectoryCameraProviderPrivate(XvDirectoryCameraProvider *q):q_ptr(q) {}

    XvDirectoryCameraProvider *const q_ptr;
    mutable QMutex mutex;
    QMap<QString,XvDirectoryCameraConfig> devices;
};

class XvDirectoryCameraPrivate
{
    Q_DISABLE_COPY(XvDirectoryCameraPrivate)
    Q_DECLARE_PUBLIC(XvDirectoryCamera)
public:
    explicit XvDirectoryCameraPrivate(XvDirectoryCamera *q):q_ptr(q) {}

    XvDirectoryCamera *const q_ptr;
    XvCameraDeviceInfo info;
    QString directory;
    bool loop=false;
    unsigned int frameIntervalMs=0;

    mutable QMutex mutex;
    QMutex acquisitionMutex;
    QWaitCondition waitCondition;
    QStringList files;
    int frameIndex=0;
    quint64 sequence=0;
    EXvCameraStatus status=EXvCameraStatus::Closed;
    QString lastError;
    bool stopRequested=false;
    quint64 streamGeneration=0;
    QPointer<QThread> streamThread;
};

QString XvDirectoryCameraProvider::staticProviderId()
{
    return "sim-directory";
}

QString XvDirectoryCameraProvider::deviceIdForLocalId(const QString &localId)
{
    return staticProviderId()+":"+localId;
}

XvDirectoryCameraProvider::XvDirectoryCameraProvider(QObject *parent)
    :XvCameraProvider(parent),d_ptr(new XvDirectoryCameraProviderPrivate(this))
{
}

XvDirectoryCameraProvider::~XvDirectoryCameraProvider()=default;

QString XvDirectoryCameraProvider::providerId() const
{
    return staticProviderId();
}

QList<XvCameraDeviceInfo> XvDirectoryCameraProvider::devices() const
{
    Q_D(const XvDirectoryCameraProvider);
    QMutexLocker locker(&d->mutex);
    QList<XvCameraDeviceInfo> result;
    for(const XvDirectoryCameraConfig &config:d->devices)
        result.append(config.info);
    return result;
}

IXvCamera *XvDirectoryCameraProvider::createCamera(const QString &deviceId,
                                                   QObject *parent)
{
    Q_D(XvDirectoryCameraProvider);
    XvDirectoryCameraConfig config;
    {
        QMutexLocker locker(&d->mutex);
        if(!d->devices.contains(deviceId)) return nullptr;
        config=d->devices.value(deviceId);
    }
    return new XvDirectoryCamera(config.info,config.directory,config.loop,
                                 config.frameIntervalMs,parent);
}

bool XvDirectoryCameraProvider::addDevice(const QString &localId,
                                          const QString &directory,
                                          const QString &displayName,bool loop,
                                          unsigned int frameIntervalMs)
{
    Q_D(XvDirectoryCameraProvider);
    static const QRegularExpression validId("^[A-Za-z0-9_.-]+$");
    const QString cleanLocalId=localId.trimmed();
    const QDir dir(directory);
    if(!validId.match(cleanLocalId).hasMatch() || !dir.exists()
            || frameIntervalMs>MaximumFrameIntervalMs)
        return false;

    XvDirectoryCameraConfig config;
    config.info.deviceId=deviceIdForLocalId(cleanLocalId);
    config.info.providerId=staticProviderId();
    config.info.displayName=displayName.isEmpty()?cleanLocalId:displayName;
    config.info.vendor="XVision";
    config.info.model="Directory Camera";
    config.info.serialNumber=cleanLocalId;
    config.info.transport="Directory";
    config.info.simulated=true;
    config.directory=QFileInfo(dir.absolutePath()).absoluteFilePath();
    config.loop=loop;
    config.frameIntervalMs=frameIntervalMs;

    {
        QMutexLocker locker(&d->mutex);
        if(d->devices.contains(config.info.deviceId)) return false;
        d->devices.insert(config.info.deviceId,config);
    }
    emit devicesChanged();
    return true;
}

bool XvDirectoryCameraProvider::removeDevice(const QString &deviceId)
{
    Q_D(XvDirectoryCameraProvider);
    bool removed=false;
    {
        QMutexLocker locker(&d->mutex);
        removed=d->devices.remove(deviceId)>0;
    }
    if(removed) emit devicesChanged();
    return removed;
}

void XvDirectoryCameraProvider::clearDevices()
{
    Q_D(XvDirectoryCameraProvider);
    bool changed=false;
    {
        QMutexLocker locker(&d->mutex);
        changed=!d->devices.isEmpty();
        d->devices.clear();
    }
    if(changed) emit devicesChanged();
}

XvDirectoryCamera::XvDirectoryCamera(const XvCameraDeviceInfo &info,
                                     const QString &directory,bool loop,
                                     unsigned int frameIntervalMs,QObject *parent)
    :IXvCamera(parent),d_ptr(new XvDirectoryCameraPrivate(this))
{
    Q_D(XvDirectoryCamera);
    d->info=info;
    d->directory=directory;
    d->loop=loop;
    d->frameIntervalMs=frameIntervalMs;
}

XvDirectoryCamera::~XvDirectoryCamera()
{
    close();
}

void XvDirectoryCamera::notifyStatus(EXvCameraStatus expectedStatus)
{
    if(QThread::currentThread()==thread())
    {
        if(status()==expectedStatus) emit statusChanged(expectedStatus);
        return;
    }
    QMetaObject::invokeMethod(this,[this,expectedStatus]()
    {
        if(status()==expectedStatus) emit statusChanged(expectedStatus);
    },Qt::QueuedConnection);
}

void XvDirectoryCamera::notifyFrame(const XvCameraFrame &frame,
                                    quint64 streamGeneration)
{
    QMetaObject::invokeMethod(this,[this,frame,streamGeneration]()
    {
        Q_D(XvDirectoryCamera);
        {
            QMutexLocker locker(&d->mutex);
            if(d->streamGeneration!=streamGeneration) return;
        }
        emit frameReady(frame);
    },Qt::QueuedConnection);
}

XvCameraDeviceInfo XvDirectoryCamera::deviceInfo() const
{
    Q_D(const XvDirectoryCamera);
    return d->info;
}

EXvCameraStatus XvDirectoryCamera::status() const
{
    Q_D(const XvDirectoryCamera);
    QMutexLocker locker(&d->mutex);
    return d->status;
}

QString XvDirectoryCamera::lastError() const
{
    Q_D(const XvDirectoryCamera);
    QMutexLocker locker(&d->mutex);
    return d->lastError;
}

EXvCameraError XvDirectoryCamera::open()
{
    Q_D(XvDirectoryCamera);
    {
        QMutexLocker locker(&d->mutex);
        if(d->status==EXvCameraStatus::Open
                || d->status==EXvCameraStatus::Streaming)
        {
            d->lastError="Directory camera is already open";
            return EXvCameraError::AlreadyOpen;
        }
    }

    QDir directory(d->directory);
    QStringList files;
    if(directory.exists())
    {
        const QFileInfoList entries=directory.entryInfoList(
                    QDir::Files|QDir::Readable|QDir::NoDotAndDotDot,QDir::NoSort);
        for(const QFileInfo &entry:entries)
        {
            if(isImageFile(entry)) files.append(entry.absoluteFilePath());
        }
        std::sort(files.begin(),files.end());
    }

    EXvCameraStatus newStatus=EXvCameraStatus::Open;
    EXvCameraError result=EXvCameraError::None;
    QString error;
    if(!directory.exists())
    {
        newStatus=EXvCameraStatus::Error;
        result=EXvCameraError::NotFound;
        error=QString("Directory [%1] does not exist").arg(d->directory);
    }
    else if(files.isEmpty())
    {
        newStatus=EXvCameraStatus::Error;
        result=EXvCameraError::IoError;
        error=QString("Directory [%1] contains no supported images")
                .arg(d->directory);
    }
    {
        QMutexLocker locker(&d->mutex);
        d->files=files;
        d->frameIndex=0;
        d->sequence=0;
        d->stopRequested=false;
        d->status=newStatus;
        d->lastError=error;
    }
    notifyStatus(newStatus);
    if(result!=EXvCameraError::None) emit errorOccurred(result,error);
    return result;
}

EXvCameraError XvDirectoryCamera::close()
{
    Q_D(XvDirectoryCamera);
    const EXvCameraError stopResult=stopContinuous();
    if(stopResult==EXvCameraError::Busy) return stopResult;

    bool changed=false;
    {
        QMutexLocker locker(&d->mutex);
        changed=d->status!=EXvCameraStatus::Closed;
        d->stopRequested=true;
        d->waitCondition.wakeAll();
        d->files.clear();
        d->frameIndex=0;
        d->status=EXvCameraStatus::Closed;
        d->lastError.clear();
    }
    if(changed) notifyStatus(EXvCameraStatus::Closed);
    return EXvCameraError::None;
}

EXvCameraError XvDirectoryCamera::grabFrame(XvCameraFrame &frame,
                                            unsigned int timeoutMs)
{
    return grabFrameInternal(frame,timeoutMs,false);
}

EXvCameraError XvDirectoryCamera::grabFrameInternal(XvCameraFrame &frame,
                                                    unsigned int timeoutMs,
                                                    bool streaming)
{
    Q_D(XvDirectoryCamera);
    QMutexLocker acquisitionLocker(&d->acquisitionMutex);
    frame=XvCameraFrame();

    QString filePath;
    {
        QMutexLocker locker(&d->mutex);
        const EXvCameraStatus expected=streaming
                ?EXvCameraStatus::Streaming:EXvCameraStatus::Open;
        if(!streaming && d->status==EXvCameraStatus::Streaming)
        {
            d->lastError="Single-frame acquisition is unavailable while streaming";
            return EXvCameraError::Busy;
        }
        if(d->status==EXvCameraStatus::Disconnected)
            return EXvCameraError::Disconnected;
        if(d->status==EXvCameraStatus::Error)
            return EXvCameraError::IoError;
        if(d->status!=expected)
        {
            d->lastError="Directory camera is not open";
            return EXvCameraError::NotOpen;
        }

        if(d->frameIntervalMs>timeoutMs)
        {
            if(timeoutMs>0) d->waitCondition.wait(&d->mutex,timeoutMs);
            if(d->status!=expected || (streaming && d->stopRequested))
                return d->status==EXvCameraStatus::Disconnected
                        ?EXvCameraError::Disconnected:EXvCameraError::NotOpen;
            const QString message=QString("Camera frame timed out after %1 ms")
                    .arg(timeoutMs);
            d->lastError=message;
            locker.unlock();
            acquisitionLocker.unlock();
            emit errorOccurred(EXvCameraError::Timeout,message);
            return EXvCameraError::Timeout;
        }
        if(d->frameIntervalMs>0)
            d->waitCondition.wait(&d->mutex,d->frameIntervalMs);
        if(d->status!=expected || (streaming && d->stopRequested))
            return d->status==EXvCameraStatus::Disconnected
                    ?EXvCameraError::Disconnected:EXvCameraError::NotOpen;

        if(d->frameIndex>=d->files.count())
        {
            if(d->loop)
            {
                d->frameIndex=0;
            }
            else
            {
                const QString message="Directory camera reached the end of its frame sequence";
                d->status=EXvCameraStatus::Disconnected;
                d->lastError=message;
                d->stopRequested=true;
                locker.unlock();
                acquisitionLocker.unlock();
                notifyStatus(EXvCameraStatus::Disconnected);
                emit disconnected(message);
                emit errorOccurred(EXvCameraError::Disconnected,message);
                return EXvCameraError::Disconnected;
            }
        }
        filePath=d->files.at(d->frameIndex);
    }

    QImage image;
    if(!image.load(filePath))
    {
        const QString message=QString("Unable to read camera frame [%1]").arg(filePath);
        {
            QMutexLocker locker(&d->mutex);
            d->status=EXvCameraStatus::Error;
            d->lastError=message;
            d->stopRequested=true;
            d->waitCondition.wakeAll();
        }
        acquisitionLocker.unlock();
        notifyStatus(EXvCameraStatus::Error);
        emit errorOccurred(EXvCameraError::IoError,message);
        return EXvCameraError::IoError;
    }

    {
        QMutexLocker locker(&d->mutex);
        const EXvCameraStatus expected=streaming
                ?EXvCameraStatus::Streaming:EXvCameraStatus::Open;
        if(d->status!=expected || (streaming && d->stopRequested))
            return d->status==EXvCameraStatus::Disconnected
                    ?EXvCameraError::Disconnected:EXvCameraError::NotOpen;
        ++d->frameIndex;
        ++d->sequence;
        d->lastError.clear();
        frame.image=image;
        frame.deviceId=d->info.deviceId;
        frame.sequence=d->sequence;
        frame.capturedAtUtc=QDateTime::currentDateTimeUtc();
    }
    return EXvCameraError::None;
}

EXvCameraError XvDirectoryCamera::startContinuous()
{
    Q_D(XvDirectoryCamera);
    QThread *thread=nullptr;
    {
        QMutexLocker locker(&d->mutex);
        if(d->status==EXvCameraStatus::Streaming)
        {
            d->lastError="Directory camera is already streaming";
            return EXvCameraError::Busy;
        }
        if(d->status!=EXvCameraStatus::Open)
        {
            d->lastError="Directory camera is not open";
            return d->status==EXvCameraStatus::Disconnected
                    ?EXvCameraError::Disconnected:EXvCameraError::NotOpen;
        }
        d->stopRequested=false;
        ++d->streamGeneration;
        thread=QThread::create([this]() { streamLoop(); });
        if(!thread)
        {
            d->lastError="Unable to create the camera streaming thread";
            return EXvCameraError::Internal;
        }
        thread->setObjectName("XvDirectoryCamera_"+d->info.deviceId);
        thread->setParent(this);
        d->streamThread=thread;
        d->status=EXvCameraStatus::Streaming;
        d->lastError.clear();
    }
    connect(thread,&QThread::finished,thread,&QObject::deleteLater);
    notifyStatus(EXvCameraStatus::Streaming);
    thread->start(QThread::HighPriority);
    return EXvCameraError::None;
}

void XvDirectoryCamera::streamLoop()
{
    Q_D(XvDirectoryCamera);
    while(true)
    {
        unsigned int timeout=0;
        quint64 streamGeneration=0;
        {
            QMutexLocker locker(&d->mutex);
            if(d->stopRequested || d->status!=EXvCameraStatus::Streaming) break;
            timeout=d->frameIntervalMs;
            streamGeneration=d->streamGeneration;
        }
        if(timeout==0) QThread::msleep(1);
        XvCameraFrame frame;
        const EXvCameraError result=grabFrameInternal(frame,timeout,true);
        if(result==EXvCameraError::None)
        {
            notifyFrame(frame,streamGeneration);
            continue;
        }
        if(result==EXvCameraError::Timeout) continue;
        break;
    }

    bool statusChangedToOpen=false;
    {
        QMutexLocker locker(&d->mutex);
        if(d->status==EXvCameraStatus::Streaming)
        {
            d->status=EXvCameraStatus::Open;
            statusChangedToOpen=true;
        }
    }
    if(statusChangedToOpen) notifyStatus(EXvCameraStatus::Open);
}

EXvCameraError XvDirectoryCamera::stopContinuous()
{
    Q_D(XvDirectoryCamera);
    QPointer<QThread> thread;
    {
        QMutexLocker locker(&d->mutex);
        thread=d->streamThread;
        if(!thread) return EXvCameraError::None;
        d->stopRequested=true;
        ++d->streamGeneration;
        d->waitCondition.wakeAll();
    }
    if(QThread::currentThread()==thread.data()) return EXvCameraError::None;
    thread->wait();
    return EXvCameraError::None;
}

QList<XvCameraParameterDescriptor> XvDirectoryCamera::parameters() const
{
    XvCameraParameterDescriptor directory;
    directory.key="directory";
    directory.displayName="Directory";
    directory.type=EXvCameraParameterType::String;
    directory.readOnly=true;

    XvCameraParameterDescriptor loop;
    loop.key="loop";
    loop.displayName="Loop";
    loop.type=EXvCameraParameterType::Boolean;
    loop.defaultValue=false;

    XvCameraParameterDescriptor interval;
    interval.key="frameIntervalMs";
    interval.displayName="Frame interval (ms)";
    interval.type=EXvCameraParameterType::Integer;
    interval.minimum=0;
    interval.maximum=MaximumFrameIntervalMs;
    interval.step=1;
    interval.defaultValue=0;
    return {directory,loop,interval};
}

QVariant XvDirectoryCamera::parameter(const QString &key) const
{
    Q_D(const XvDirectoryCamera);
    QMutexLocker locker(&d->mutex);
    if(key=="directory") return d->directory;
    if(key=="loop") return d->loop;
    if(key=="frameIntervalMs") return d->frameIntervalMs;
    return QVariant();
}

EXvCameraError XvDirectoryCamera::setParameter(const QString &key,
                                               const QVariant &value)
{
    Q_D(XvDirectoryCamera);
    QMutexLocker locker(&d->mutex);
    if(d->status==EXvCameraStatus::Streaming)
    {
        d->lastError="Camera parameters cannot be changed while streaming";
        return EXvCameraError::Busy;
    }
    if(key=="directory")
    {
        d->lastError="The directory parameter is read-only";
        return EXvCameraError::Unsupported;
    }
    if(key=="loop")
    {
        if(value.userType()!=QMetaType::Bool)
        {
            d->lastError="The loop parameter requires a Boolean value";
            return EXvCameraError::InvalidArgument;
        }
        d->loop=value.toBool();
        d->lastError.clear();
        return EXvCameraError::None;
    }
    if(key=="frameIntervalMs")
    {
        if(!isIntegerVariant(value))
        {
            d->lastError="The frame interval requires an integer value";
            return EXvCameraError::InvalidArgument;
        }
        bool ok=false;
        const qlonglong interval=value.toLongLong(&ok);
        if(!ok || interval<0 || interval>MaximumFrameIntervalMs)
        {
            d->lastError=QString("Frame interval must be in [0,%1]")
                    .arg(MaximumFrameIntervalMs);
            return EXvCameraError::InvalidArgument;
        }
        d->frameIntervalMs=static_cast<unsigned int>(interval);
        d->lastError.clear();
        return EXvCameraError::None;
    }
    d->lastError=QString("Camera parameter [%1] was not found").arg(key);
    return EXvCameraError::NotFound;
}

EXvCameraError XvDirectoryCamera::simulateDisconnect(const QString &reason)
{
    Q_D(XvDirectoryCamera);
    QString message=reason.isEmpty()?"Directory camera was disconnected":reason;
    {
        QMutexLocker locker(&d->mutex);
        if(d->status==EXvCameraStatus::Closed)
        {
            d->lastError="Directory camera is not open";
            return EXvCameraError::NotOpen;
        }
        d->status=EXvCameraStatus::Disconnected;
        d->lastError=message;
        d->stopRequested=true;
        ++d->streamGeneration;
        d->waitCondition.wakeAll();
    }
    notifyStatus(EXvCameraStatus::Disconnected);
    emit disconnected(message);
    emit errorOccurred(EXvCameraError::Disconnected,message);
    return EXvCameraError::None;
}

}
