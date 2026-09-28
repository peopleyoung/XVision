#ifndef XVCAMERAPROVIDER_H
#define XVCAMERAPROVIDER_H

#include "XvCameraGlobal.h"
#include "XvCameraTypes.h"

#include <QObject>

namespace XvCamera
{

class IXvCamera;

class XVCAMERA_EXPORT XvCameraProvider : public QObject
{
    Q_OBJECT
public:
    explicit XvCameraProvider(QObject *parent=nullptr):QObject(parent) {}
    ~XvCameraProvider() override=default;

    virtual QString providerId() const=0;
    virtual QList<XvCameraDeviceInfo> devices() const=0;
    virtual IXvCamera *createCamera(const QString &deviceId,
                                    QObject *parent=nullptr)=0;

signals:
    void devicesChanged();
};

}

#endif // XVCAMERAPROVIDER_H
