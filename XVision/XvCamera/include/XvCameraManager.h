#ifndef XVCAMERAMANAGER_H
#define XVCAMERAMANAGER_H

#include "XvCameraGlobal.h"
#include "XvCameraTypes.h"

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>

namespace XvCamera
{

class IXvCamera;
class XvCameraProvider;
class XvDirectoryCameraProvider;
class XvCameraManagerPrivate;

class XVCAMERA_EXPORT XvCameraManager : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvCameraManager)
public:
    static XvCameraManager *instance();
    ~XvCameraManager() override;

    bool registerProvider(XvCameraProvider *provider);
    bool unregisterProvider(const QString &providerId);
    QList<XvCameraDeviceInfo> devices() const;
    void refreshDevices();
    QStringList providerDiagnostics() const;
    bool isRefreshing() const;

    EXvCameraError openCamera(const QString &deviceId,IXvCamera **camera=nullptr);
    EXvCameraError closeCamera(const QString &deviceId);
    IXvCamera *camera(const QString &deviceId) const;
    // Retains object lifetime across a concurrent close; native commands may
    // still report NotOpen. Callers must not delete or reparent the camera.
    QSharedPointer<IXvCamera> cameraLease(const QString &deviceId) const;
    void shutdown();

    XvDirectoryCameraProvider *directoryProvider() const;
    QString lastError() const;

signals:
    void devicesChanged();
    void cameraOpened(XvCamera::IXvCamera *camera);
    void cameraClosed(const QString &deviceId);

private:
    explicit XvCameraManager(QObject *parent=nullptr);
    EXvCameraError prepareCamera(const QString &deviceId,IXvCamera **camera);
    void setLastError(const QString &message) const;

    const QScopedPointer<XvCameraManagerPrivate> d_ptr;
};

}

#define XvCameraMgr XvCamera::XvCameraManager::instance()

#endif // XVCAMERAMANAGER_H
