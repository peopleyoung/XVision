#ifndef IXVCAMERA_H
#define IXVCAMERA_H

#include "XvCameraGlobal.h"
#include "XvCameraTypes.h"

#include <QObject>

namespace XvCamera
{

class XVCAMERA_EXPORT IXvCamera : public QObject
{
    Q_OBJECT
public:
    explicit IXvCamera(QObject *parent=nullptr):QObject(parent) {}
    ~IXvCamera() override=default;

    virtual XvCameraDeviceInfo deviceInfo() const=0;
    virtual EXvCameraStatus status() const=0;
    virtual QString lastError() const=0;

    virtual EXvCameraError open()=0;
    virtual EXvCameraError close()=0;
    virtual EXvCameraError grabFrame(XvCameraFrame &frame,
                                     unsigned int timeoutMs=1000)=0;
    virtual EXvCameraError startContinuous()=0;
    virtual EXvCameraError stopContinuous()=0;

    virtual QList<XvCameraParameterDescriptor> parameters() const=0;
    virtual QVariant parameter(const QString &key) const=0;
    virtual EXvCameraError setParameter(const QString &key,
                                        const QVariant &value)=0;

signals:
    void statusChanged(XvCamera::EXvCameraStatus status);
    void frameReady(const XvCamera::XvCameraFrame &frame);
    void disconnected(const QString &reason);
    void errorOccurred(XvCamera::EXvCameraError error,const QString &message);
};

}

#endif // IXVCAMERA_H
