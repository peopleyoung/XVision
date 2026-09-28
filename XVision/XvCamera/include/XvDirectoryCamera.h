#ifndef XVDIRECTORYCAMERA_H
#define XVDIRECTORYCAMERA_H

#include "IXvCamera.h"
#include "XvCameraProvider.h"

#include <QScopedPointer>

namespace XvCamera
{

class XvDirectoryCameraPrivate;
class XvDirectoryCameraProviderPrivate;

class XVCAMERA_EXPORT XvDirectoryCamera : public IXvCamera
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvDirectoryCamera)
public:
    ~XvDirectoryCamera() override;

    XvCameraDeviceInfo deviceInfo() const override;
    EXvCameraStatus status() const override;
    QString lastError() const override;

    EXvCameraError open() override;
    EXvCameraError close() override;
    EXvCameraError grabFrame(XvCameraFrame &frame,
                             unsigned int timeoutMs=1000) override;
    EXvCameraError startContinuous() override;
    EXvCameraError stopContinuous() override;

    QList<XvCameraParameterDescriptor> parameters() const override;
    QVariant parameter(const QString &key) const override;
    EXvCameraError setParameter(const QString &key,
                                const QVariant &value) override;

    EXvCameraError simulateDisconnect(const QString &reason=QString());

protected:
    friend class XvDirectoryCameraProvider;
    explicit XvDirectoryCamera(const XvCameraDeviceInfo &info,
                               const QString &directory,bool loop,
                               unsigned int frameIntervalMs,
                               QObject *parent=nullptr);
    EXvCameraError grabFrameInternal(XvCameraFrame &frame,
                                     unsigned int timeoutMs,bool streaming);
    void streamLoop();
    void notifyStatus(EXvCameraStatus expectedStatus);
    void notifyFrame(const XvCameraFrame &frame,quint64 streamGeneration);

private:
    const QScopedPointer<XvDirectoryCameraPrivate> d_ptr;
};

class XVCAMERA_EXPORT XvDirectoryCameraProvider : public XvCameraProvider
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvDirectoryCameraProvider)
public:
    static QString staticProviderId();
    static QString deviceIdForLocalId(const QString &localId);

    explicit XvDirectoryCameraProvider(QObject *parent=nullptr);
    ~XvDirectoryCameraProvider() override;

    QString providerId() const override;
    QList<XvCameraDeviceInfo> devices() const override;
    IXvCamera *createCamera(const QString &deviceId,
                            QObject *parent=nullptr) override;

    bool addDevice(const QString &localId,const QString &directory,
                   const QString &displayName=QString(),bool loop=false,
                   unsigned int frameIntervalMs=0);
    bool removeDevice(const QString &deviceId);
    void clearDevices();

private:
    const QScopedPointer<XvDirectoryCameraProviderPrivate> d_ptr;
};

}

#endif // XVDIRECTORYCAMERA_H
