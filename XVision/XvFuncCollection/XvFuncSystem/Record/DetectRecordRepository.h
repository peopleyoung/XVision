#ifndef DETECTRECORDREPOSITORY_H
#define DETECTRECORDREPOSITORY_H

#include "XVFuncSystemGlobal.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace XvCore
{
class XVFUNCSYSTEM_EXPORT DetectRecordRepository
{
public:
    struct Record
    {
        QString recordId;
        QString category;
        QString objectId;
        qint64 timestampMs=0;
        QString outcome;
        QJsonObject details;
    };

    static int schemaVersion() { return 1; }

    bool append(const QString &databasePath,int busyTimeoutMs,
                const Record &record,QString &error) const;
    bool queryByCategory(const QString &databasePath,int busyTimeoutMs,
                         const QString &category,int limit,
                         QList<Record> &records,QString &error) const;
    bool queryByObject(const QString &databasePath,int busyTimeoutMs,
                       const QString &objectId,int limit,
                       QList<Record> &records,QString &error) const;
    bool exists(const QString &databasePath,int busyTimeoutMs,
                const QString &category,const QString &objectId,
                bool &found,QString &error) const;
};
}

#endif // DETECTRECORDREPOSITORY_H
