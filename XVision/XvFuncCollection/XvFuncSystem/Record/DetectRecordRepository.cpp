#include "DetectRecordRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include <atomic>
#include <functional>

using namespace XvCore;

namespace
{
constexpr int MaxBusyTimeoutMs=60000;
constexpr int MaxQueryLimit=10000;
constexpr int MaxRecordIdLength=512;
constexpr int MaxCategoryLength=256;
constexpr int MaxObjectIdLength=512;
constexpr int MaxOutcomeLength=256;
constexpr int MaxDetailsBytes=1024*1024;

std::atomic<quint64> ConnectionSequence{0};

QString sqlError(const QString &context,const QSqlError &error)
{
    const QString detail=error.text().trimmed();
    return detail.isEmpty()?context:QString("%1: %2").arg(context,detail);
}

bool validateText(QString &value,int maximum,const QString &label,QString &error)
{
    value=value.trimmed();
    if(value.isEmpty())
    {
        error=label+" must be nonempty";
        return false;
    }
    if(value.size()>maximum)
    {
        error=QString("%1 exceeds the supported length of %2").arg(label).arg(maximum);
        return false;
    }
    return true;
}

bool normalizedRecord(const DetectRecordRepository::Record &source,
                      DetectRecordRepository::Record &record,
                      QByteArray &detailsJson,QString &error)
{
    record=source;
    if(!validateText(record.recordId,MaxRecordIdLength,"Record ID",error)
            || !validateText(record.category,MaxCategoryLength,"Category",error)
            || !validateText(record.objectId,MaxObjectIdLength,"Object ID",error)
            || !validateText(record.outcome,MaxOutcomeLength,"Outcome",error)) return false;
    if(record.timestampMs<0)
    {
        error="Record timestamp must be nonnegative";
        return false;
    }
    detailsJson=QJsonDocument(record.details).toJson(QJsonDocument::Compact);
    if(detailsJson.size()>MaxDetailsBytes)
    {
        error=QString("Record details exceed the supported size of %1 bytes")
                .arg(MaxDetailsBytes);
        return false;
    }
    return true;
}

bool validateDatabasePath(const QString &candidate,QString &absolutePath,QString &error)
{
    const QString trimmed=candidate.trimmed();
    if(trimmed.isEmpty())
    {
        error="Database path must be nonempty";
        return false;
    }
    const QFileInfo destination(trimmed);
    absolutePath=destination.absoluteFilePath();
    const QFileInfo absoluteInfo(absolutePath);
    if(absoluteInfo.exists() && !absoluteInfo.isFile())
    {
        error="Database path does not identify a file";
        return false;
    }
    const QFileInfo directoryInfo(absoluteInfo.absolutePath());
    if(!directoryInfo.exists() || !directoryInfo.isDir())
    {
        error="Database parent directory does not exist";
        return false;
    }
    return true;
}

bool validateBusyTimeout(int busyTimeoutMs,QString &error)
{
    if(busyTimeoutMs<0 || busyTimeoutMs>MaxBusyTimeoutMs)
    {
        error=QString("Database busy timeout must be between 0 and %1 ms")
                .arg(MaxBusyTimeoutMs);
        return false;
    }
    return true;
}

bool execStatement(QSqlDatabase &database,const QString &statement,
                   const QString &context,QString &error)
{
    QSqlQuery query(database);
    if(query.exec(statement)) return true;
    error=sqlError(context,query.lastError());
    return false;
}

bool readSchemaVersion(QSqlDatabase &database,int &version,QString &error)
{
    QSqlQuery query(database);
    if(!query.exec("PRAGMA user_version") || !query.next())
    {
        error=sqlError("Could not read detect-record schema version",query.lastError());
        return false;
    }
    bool ok=false;
    version=query.value(0).toInt(&ok);
    if(!ok || version<0)
    {
        error="Detect-record schema version is invalid";
        return false;
    }
    return true;
}

struct ColumnDefinition
{
    QString type;
    bool notNull=false;
    int primaryKey=0;
};

bool validateTable(QSqlDatabase &database,QString &error)
{
    const QMap<QString,ColumnDefinition> expected={
        {"id",{"INTEGER",false,1}},
        {"record_id",{"TEXT",true,0}},
        {"category",{"TEXT",true,0}},
        {"object_id",{"TEXT",true,0}},
        {"timestamp_ms",{"INTEGER",true,0}},
        {"outcome",{"TEXT",true,0}},
        {"details_json",{"TEXT",true,0}}
    };
    QMap<QString,ColumnDefinition> actual;
    QSqlQuery query(database);
    if(!query.exec("PRAGMA table_info(detect_records)"))
    {
        error=sqlError("Could not inspect detect-record table",query.lastError());
        return false;
    }
    while(query.next())
    {
        const QString name=query.value(1).toString();
        if(name.isEmpty() || actual.contains(name))
        {
            error="Detect-record table contains an invalid or duplicate column";
            return false;
        }
        actual.insert(name,{query.value(2).toString().trimmed().toUpper(),
                            query.value(3).toInt()!=0,query.value(5).toInt()});
    }
    if(actual.size()!=expected.size())
    {
        error="Detect-record table columns do not match schema version 1";
        return false;
    }
    for(auto iterator=expected.cbegin();iterator!=expected.cend();++iterator)
    {
        if(!actual.contains(iterator.key()))
        {
            error=QString("Detect-record table is missing column '%1'").arg(iterator.key());
            return false;
        }
        const ColumnDefinition candidate=actual.value(iterator.key());
        if(candidate.type!=iterator.value().type
                || candidate.notNull!=iterator.value().notNull
                || candidate.primaryKey!=iterator.value().primaryKey)
        {
            error=QString("Detect-record column '%1' does not match schema version 1")
                    .arg(iterator.key());
            return false;
        }
    }
    return true;
}

bool validateIndex(QSqlDatabase &database,const QString &name,bool unique,
                   const QStringList &columns,QString &error)
{
    QSqlQuery listQuery(database);
    if(!listQuery.exec("PRAGMA index_list(detect_records)"))
    {
        error=sqlError("Could not inspect detect-record indexes",listQuery.lastError());
        return false;
    }
    bool found=false;
    bool actualUnique=false;
    while(listQuery.next())
    {
        if(listQuery.value(1).toString()==name)
        {
            found=true;
            actualUnique=listQuery.value(2).toInt()!=0;
            break;
        }
    }
    if(!found || actualUnique!=unique)
    {
        error=QString("Detect-record index '%1' does not match schema version 1").arg(name);
        return false;
    }
    QSqlQuery infoQuery(database);
    if(!infoQuery.exec(QString("PRAGMA index_info(%1)").arg(name)))
    {
        error=sqlError(QString("Could not inspect detect-record index '%1'").arg(name),
                       infoQuery.lastError());
        return false;
    }
    QStringList actualColumns;
    while(infoQuery.next()) actualColumns.append(infoQuery.value(2).toString());
    if(actualColumns!=columns)
    {
        error=QString("Detect-record index '%1' columns do not match schema version 1")
                .arg(name);
        return false;
    }
    return true;
}

bool validateSchema(QSqlDatabase &database,QString &error)
{
    return validateTable(database,error)
            && validateIndex(database,"uq_detect_records_record_id",true,{"record_id"},error)
            && validateIndex(database,"ix_detect_records_category_time",false,
                             {"category","timestamp_ms","id"},error)
            && validateIndex(database,"ix_detect_records_object_time",false,
                             {"object_id","timestamp_ms","id"},error);
}

bool initializeSchema(QSqlDatabase &database,QString &error)
{
    if(!database.transaction())
    {
        error=sqlError("Could not begin detect-record schema transaction",
                       database.lastError());
        return false;
    }
    const QStringList statements={
        "CREATE TABLE IF NOT EXISTS detect_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "record_id TEXT NOT NULL,"
        "category TEXT NOT NULL,"
        "object_id TEXT NOT NULL,"
        "timestamp_ms INTEGER NOT NULL CHECK(timestamp_ms >= 0),"
        "outcome TEXT NOT NULL,"
        "details_json TEXT NOT NULL)",
        "CREATE UNIQUE INDEX IF NOT EXISTS uq_detect_records_record_id "
        "ON detect_records(record_id)",
        "CREATE INDEX IF NOT EXISTS ix_detect_records_category_time "
        "ON detect_records(category, timestamp_ms DESC, id DESC)",
        "CREATE INDEX IF NOT EXISTS ix_detect_records_object_time "
        "ON detect_records(object_id, timestamp_ms DESC, id DESC)"
    };
    for(const QString &statement:statements)
    {
        if(execStatement(database,statement,"Could not initialize detect-record schema",error))
            continue;
        database.rollback();
        return false;
    }
    if(!validateSchema(database,error)
            || !execStatement(database,"PRAGMA user_version = 1",
                              "Could not write detect-record schema version",error))
    {
        database.rollback();
        return false;
    }
    if(!database.commit())
    {
        error=sqlError("Could not commit detect-record schema",database.lastError());
        database.rollback();
        return false;
    }
    return true;
}

bool ensureSchema(QSqlDatabase &database,QString &error)
{
    int version=0;
    if(!readSchemaVersion(database,version,error)) return false;
    if(version==0) return initializeSchema(database,error);
    if(version!=DetectRecordRepository::schemaVersion())
    {
        error=QString("Unsupported detect-record schema version %1; expected %2")
                .arg(version).arg(DetectRecordRepository::schemaVersion());
        return false;
    }
    return validateSchema(database,error);
}

using DatabaseOperation=std::function<bool(QSqlDatabase &,QString &)>;

bool withDatabase(const QString &databasePath,int busyTimeoutMs,
                  const DatabaseOperation &operation,QString &error)
{
    error.clear();
    QString absolutePath;
    if(!validateDatabasePath(databasePath,absolutePath,error)
            || !validateBusyTimeout(busyTimeoutMs,error)) return false;
    if(!QSqlDatabase::isDriverAvailable("QSQLITE"))
    {
        error="Qt SQLite driver QSQLITE is unavailable";
        return false;
    }
    const quint64 sequence=ConnectionSequence.fetch_add(1,std::memory_order_relaxed);
    const QString connectionName=QString("xvision_detect_record_%1_%2_%3")
            .arg(QCoreApplication::applicationPid())
            .arg(reinterpret_cast<quintptr>(QThread::currentThread()),0,16)
            .arg(sequence);
    bool success=false;
    {
        QSqlDatabase database=QSqlDatabase::addDatabase("QSQLITE",connectionName);
        if(!database.isValid())
        {
            error="Could not create a QSQLITE database connection";
        }
        else
        {
            database.setDatabaseName(absolutePath);
            database.setConnectOptions(QString("QSQLITE_BUSY_TIMEOUT=%1").arg(busyTimeoutMs));
            if(!database.open())
            {
                error=sqlError(QString("Could not open detect-record database '%1'")
                               .arg(QDir::toNativeSeparators(absolutePath)),
                               database.lastError());
            }
            else if(execStatement(database,QString("PRAGMA busy_timeout = %1")
                                  .arg(busyTimeoutMs),
                                  "Could not configure detect-record busy timeout",error)
                    && ensureSchema(database,error))
            {
                success=operation(database,error);
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    if(!success && error.trimmed().isEmpty()) error="Detect-record database operation failed";
    return success;
}

bool decodeRecord(QSqlQuery &query,DetectRecordRepository::Record &record,QString &error)
{
    record.recordId=query.value(0).toString();
    record.category=query.value(1).toString();
    record.objectId=query.value(2).toString();
    bool timestampOk=false;
    record.timestampMs=query.value(3).toLongLong(&timestampOk);
    record.outcome=query.value(4).toString();
    const QByteArray detailsJson=query.value(5).toString().toUtf8();
    if(!timestampOk || detailsJson.size()>MaxDetailsBytes)
    {
        error="Stored detect record contains an invalid timestamp or oversized details";
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument details=QJsonDocument::fromJson(detailsJson,&parseError);
    if(parseError.error!=QJsonParseError::NoError || !details.isObject())
    {
        error="Stored detect record details are not a valid JSON object";
        return false;
    }
    record.details=details.object();
    DetectRecordRepository::Record normalized;
    QByteArray normalizedJson;
    if(!normalizedRecord(record,normalized,normalizedJson,error)) return false;
    record=normalized;
    return true;
}

bool queryRecords(const QString &databasePath,int busyTimeoutMs,
                  const QString &column,const QString &filter,int limit,
                  QList<DetectRecordRepository::Record> &records,QString &error)
{
    QString normalizedFilter=filter;
    const int maximum=column=="category"?MaxCategoryLength:MaxObjectIdLength;
    const QString label=column=="category"?"Category":"Object ID";
    if(!validateText(normalizedFilter,maximum,label,error)) return false;
    if(limit<1 || limit>MaxQueryLimit)
    {
        error=QString("Record query limit must be between 1 and %1").arg(MaxQueryLimit);
        return false;
    }
    QList<DetectRecordRepository::Record> candidate;
    const QString statement=QString(
                "SELECT record_id, category, object_id, timestamp_ms, outcome, details_json "
                "FROM detect_records WHERE %1 = :filter "
                "ORDER BY timestamp_ms DESC, id DESC LIMIT :limit").arg(column);
    if(!withDatabase(databasePath,busyTimeoutMs,
        [&](QSqlDatabase &database,QString &operationError)
        {
            QSqlQuery query(database);
            if(!query.prepare(statement))
            {
                operationError=sqlError("Could not prepare detect-record query",query.lastError());
                return false;
            }
            query.bindValue(":filter",normalizedFilter);
            query.bindValue(":limit",limit);
            if(!query.exec())
            {
                operationError=sqlError("Could not execute detect-record query",query.lastError());
                return false;
            }
            while(query.next())
            {
                DetectRecordRepository::Record record;
                if(!decodeRecord(query,record,operationError)) return false;
                candidate.append(record);
            }
            return true;
        },error)) return false;
    records=candidate;
    return true;
}
}

bool DetectRecordRepository::append(const QString &databasePath,int busyTimeoutMs,
                                    const Record &source,QString &error) const
{
    Record record;
    QByteArray detailsJson;
    if(!normalizedRecord(source,record,detailsJson,error)) return false;
    return withDatabase(databasePath,busyTimeoutMs,
        [&](QSqlDatabase &database,QString &operationError)
        {
            if(!database.transaction())
            {
                operationError=sqlError("Could not begin detect-record append transaction",
                                        database.lastError());
                return false;
            }
            QSqlQuery query(database);
            if(!query.prepare(
                    "INSERT INTO detect_records "
                    "(record_id, category, object_id, timestamp_ms, outcome, details_json) "
                    "VALUES (:record_id, :category, :object_id, :timestamp_ms, :outcome, :details_json)"))
            {
                operationError=sqlError("Could not prepare detect-record append",query.lastError());
                database.rollback();
                return false;
            }
            query.bindValue(":record_id",record.recordId);
            query.bindValue(":category",record.category);
            query.bindValue(":object_id",record.objectId);
            query.bindValue(":timestamp_ms",record.timestampMs);
            query.bindValue(":outcome",record.outcome);
            query.bindValue(":details_json",QString::fromUtf8(detailsJson));
            if(!query.exec())
            {
                operationError=sqlError("Could not append detect record",query.lastError());
                database.rollback();
                return false;
            }
            if(!database.commit())
            {
                operationError=sqlError("Could not commit detect-record append",
                                        database.lastError());
                database.rollback();
                return false;
            }
            return true;
        },error);
}

bool DetectRecordRepository::queryByCategory(const QString &databasePath,int busyTimeoutMs,
                                             const QString &category,int limit,
                                             QList<Record> &records,QString &error) const
{
    return queryRecords(databasePath,busyTimeoutMs,"category",category,limit,records,error);
}

bool DetectRecordRepository::queryByObject(const QString &databasePath,int busyTimeoutMs,
                                           const QString &objectId,int limit,
                                           QList<Record> &records,QString &error) const
{
    return queryRecords(databasePath,busyTimeoutMs,"object_id",objectId,limit,records,error);
}

bool DetectRecordRepository::exists(const QString &databasePath,int busyTimeoutMs,
                                    const QString &categoryValue,
                                    const QString &objectIdValue,
                                    bool &found,QString &error) const
{
    QString category=categoryValue.trimmed();
    QString objectId=objectIdValue.trimmed();
    if(category.isEmpty() && objectId.isEmpty())
    {
        error="Record existence query requires a category or object ID";
        return false;
    }
    if((!category.isEmpty()
        && !validateText(category,MaxCategoryLength,"Category",error))
            || (!objectId.isEmpty()
                && !validateText(objectId,MaxObjectIdLength,"Object ID",error))) return false;
    bool candidate=false;
    if(!withDatabase(databasePath,busyTimeoutMs,
        [&](QSqlDatabase &database,QString &operationError)
        {
            QString statement;
            if(!category.isEmpty() && !objectId.isEmpty())
                statement="SELECT EXISTS(SELECT 1 FROM detect_records "
                          "WHERE category = :category AND object_id = :object_id LIMIT 1)";
            else if(!category.isEmpty())
                statement="SELECT EXISTS(SELECT 1 FROM detect_records "
                          "WHERE category = :category LIMIT 1)";
            else
                statement="SELECT EXISTS(SELECT 1 FROM detect_records "
                          "WHERE object_id = :object_id LIMIT 1)";
            QSqlQuery query(database);
            if(!query.prepare(statement))
            {
                operationError=sqlError("Could not prepare detect-record existence query",
                                        query.lastError());
                return false;
            }
            if(!category.isEmpty()) query.bindValue(":category",category);
            if(!objectId.isEmpty()) query.bindValue(":object_id",objectId);
            if(!query.exec() || !query.next())
            {
                operationError=sqlError("Could not execute detect-record existence query",
                                        query.lastError());
                return false;
            }
            candidate=query.value(0).toInt()!=0;
            return true;
        },error)) return false;
    found=candidate;
    return true;
}
