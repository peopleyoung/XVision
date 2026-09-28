#ifndef XVCAMERAMANAGER_H
#define XVCAMERAMANAGER_H

#include "XvCameraGlobal.h"
#include "XvCameraTypes.h"

#include <QObject>
#include <QScopedPointer>

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

    EXvCameraError openCamera(const QString &deviceId,IXvCamera **camera=nullptr);
    EXvCameraError closeCamera(const QString &deviceId);
    IXvCamera *camera(const QString &deviceId) const;
    void shutdown();

    XvDirectoryCameraProvider *directoryProvider() const;
    QString lastError() const;

signals:
    void devicesChanged();
    void cameraOpened(XvCamera::IXvCamera *camera);
    void cameraClosed(const QString &deviceId);

private:
    explicit XvCameraManager(QObject *parent=nullptr);
    void setLastError(const QString &message) const;

    const QScopedPointer<XvCameraManagerPrivate> d_ptr;
};

}

#define XvCameraMgr XvCamera::XvCameraManager::instance()

#endif // XVCAMERAMANAGER_H
