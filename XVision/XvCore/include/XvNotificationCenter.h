#ifndef XVNOTIFICATIONCENTER_H
#define XVNOTIFICATIONCENTER_H

#include "XvCoreGlobal.h"

#include <QObject>
#include <QMetaType>
#include <QString>

namespace XvCore
{
enum class XvNotificationKind
{
    Ok=0,
    Ng=1,
    Info=2,
    Success=3,
    Warning=4,
    Error=5,
    Fatal=6,
    Dialog=7
};

class XVCORE_EXPORT XvNotificationCenter:public QObject
{
    Q_OBJECT
public:
    static constexpr int MaximumMessageLength=4096;

    static XvNotificationCenter *instance();

    bool publish(XvNotificationKind kind,const QString &message,
                 const QString &sourceFunctionId=QString());

signals:
    void notificationPublished(XvCore::XvNotificationKind kind,
                               const QString &message,
                               const QString &sourceFunctionId,
                               bool modal);

private:
    explicit XvNotificationCenter(QObject *parent=nullptr);
    Q_DISABLE_COPY(XvNotificationCenter)
};
}

Q_DECLARE_METATYPE(XvCore::XvNotificationKind)

#define XvNotificationMgr XvCore::XvNotificationCenter::instance()

#endif // XVNOTIFICATIONCENTER_H
