#include "XvNotificationCenter.h"

#include <QCoreApplication>
#include <QMetaType>

using namespace XvCore;

XvNotificationCenter::XvNotificationCenter(QObject *parent)
    :QObject(parent)
{
    qRegisterMetaType<XvNotificationKind>("XvCore::XvNotificationKind");
}

XvNotificationCenter *XvNotificationCenter::instance()
{
    static XvNotificationCenter *center=[]()
    {
        auto value=new XvNotificationCenter();
        if(QCoreApplication::instance()
                && value->thread()!=QCoreApplication::instance()->thread())
            value->moveToThread(QCoreApplication::instance()->thread());
        return value;
    }();
    return center;
}

bool XvNotificationCenter::publish(XvNotificationKind kind,const QString &message,
                                   const QString &sourceFunctionId)
{
    const int value=static_cast<int>(kind);
    if(value<static_cast<int>(XvNotificationKind::Ok)
            || value>static_cast<int>(XvNotificationKind::Dialog))
        return false;

    const QString normalized=message.trimmed();
    if(normalized.isEmpty() || normalized.size()>MaximumMessageLength) return false;

    emit notificationPublished(kind,normalized,sourceFunctionId,
                               kind==XvNotificationKind::Dialog);
    return true;
}
