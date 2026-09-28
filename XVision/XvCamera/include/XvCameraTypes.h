#ifndef XVCAMERATYPES_H
#define XVCAMERATYPES_H

#include "XvCameraGlobal.h"

#include <QDateTime>
#include <QImage>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariant>

namespace XvCamera
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
Q_NAMESPACE_EXPORT(XVCAMERA_EXPORT)
#else
Q_NAMESPACE
#endif

enum class EXvCameraStatus
{
    Closed=0,
    Open=1,
    Streaming=2,
    Disconnected=3,
    Error=4
};
Q_ENUM_NS(EXvCameraStatus)

enum class EXvCameraError
{
    None=0,
    InvalidArgument=1,
    NotFound=2,
    AlreadyOpen=3,
    NotOpen=4,
    Busy=5,
    Timeout=6,
    Disconnected=7,
    IoError=8,
    Unsupported=9,
    Internal=10
};
Q_ENUM_NS(EXvCameraError)

enum class EXvCameraParameterType
{
    Boolean=0,
    Integer=1,
    Real=2,
    String=3,
    Enumeration=4
};
Q_ENUM_NS(EXvCameraParameterType)

struct XVCAMERA_EXPORT XvCameraDeviceInfo
{
    bool isValid() const { return !deviceId.isEmpty() && !providerId.isEmpty(); }

    QString deviceId;
    QString providerId;
    QString displayName;
    QString vendor;
    QString model;
    QString serialNumber;
    QString transport;
    bool simulated=false;
};

struct XVCAMERA_EXPORT XvCameraParameterDescriptor
{
    QString key;
    QString displayName;
    EXvCameraParameterType type=EXvCameraParameterType::String;
    QVariant minimum;
    QVariant maximum;
    QVariant step;
    QVariant defaultValue;
    QStringList enumValues;
    bool readOnly=false;
};

struct XVCAMERA_EXPORT XvCameraFrame
{
    bool isValid() const { return !image.isNull() && !deviceId.isEmpty(); }

    QImage image;
    QString deviceId;
    quint64 sequence=0;
    QDateTime capturedAtUtc;
};

}

Q_DECLARE_METATYPE(XvCamera::EXvCameraStatus)
Q_DECLARE_METATYPE(XvCamera::EXvCameraError)
Q_DECLARE_METATYPE(XvCamera::XvCameraDeviceInfo)
Q_DECLARE_METATYPE(XvCamera::XvCameraFrame)

#endif // XVCAMERATYPES_H
