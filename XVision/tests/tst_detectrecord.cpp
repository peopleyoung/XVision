#include <QtTest>

#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include <functional>

#include "DetectRecord.h"
#include "XBool.h"
#include "XInt.h"
#include "XObjectList.h"
#include "XReal.h"
#include "XString.h"
#include "XVisionRuntimeData.h"

using namespace XvCore;

namespace
{
template<typename T>
T *parameter(DetectRecord &function,const QString &name)
{
    return dynamic_cast<T*>(function.getParamsByName(name));
}

template<typename T>
T *result(DetectRecord &function,const QString &name)
{
    return dynamic_cast<T*>(function.getResultsByName(name));
}

void configureDirectRecord(DetectRecord &function,const QString &databasePath,
                           const QString &recordId,qint64 timestampMs,
                           const QString &category,const QString &objectId,
                           const QString &outcome="ok",
                           const QString &detailsJson="{}")
{
    parameter<XString>(function,"databasePath")->setValue(databasePath);
    parameter<XString>(function,"recordId")->setValue(recordId);
    parameter<XReal>(function,"timestampMs")->setValue(double(timestampMs));
    parameter<XString>(function,"category")->setValue(category);
    parameter<XString>(function,"objectId")->setValue(objectId);
    parameter<XString>(function,"outcome")->setValue(outcome);
    parameter<XString>(function,"detailsJson")->setValue(detailsJson);
    parameter<XBool>(function,"useRecordInput")->setValue(false);
}

XDetectRecord *recordAt(DetectRecord &function,int index)
{
    XObjectList *records=result<XObjectList>(function,"records");
    return records?dynamic_cast<XDetectRecord*>(records->value(index)):nullptr;
}

bool runRawDatabase(const QString &path,
                    const std::function<bool(QSqlDatabase &)> &operation)
{
    const QString name="detect_record_test_"+QUuid::createUuid().toString(QUuid::Id128);
    bool success=false;
    {
        QSqlDatabase database=QSqlDatabase::addDatabase("QSQLITE",name);
        database.setDatabaseName(path);
        if(database.open()) success=operation(database);
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
    return success;
}

bool hasOperatorConnection()
{
    for(const QString &name:QSqlDatabase::connectionNames())
        if(name.startsWith("xvision_detect_record_")) return true;
    return false;
}
}

class DetectRecordTest:public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(QSqlDatabase::isDriverAvailable("QSQLITE"),
                 "Qt QSQLITE driver is required for detect-record tests");
    }

    void appendQueryAndExistence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("records.sqlite");

        DetectRecord writer;
        configureDirectRecord(writer,path,"r1",100,"class-a","object-a","ok",
                              "{\"score\":0.8}");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(writer,"recordCount")->value(),1);
        QVERIFY(result<XBool>(writer,"exists")->value());
        QCOMPARE(result<XString>(writer,"insertedRecordId")->value(),QString("r1"));
        QCOMPARE(result<XObjectList>(writer,"records")->count(),qsizetype(1));
        QCOMPARE(recordAt(writer,0)->recordId(),QString("r1"));
        QCOMPARE(recordAt(writer,0)->details().value("score").toDouble(),0.8);
        QCOMPARE(result<XDetectRecord>(writer,"latestRecord")->recordId(),QString("r1"));

        configureDirectRecord(writer,path,"r2",200,"class-a","object-b");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);
        configureDirectRecord(writer,path,"r3",200,"class-a","object-a","ng");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);

        DetectRecord classQuery;
        classQuery.setMode(DetectRecord::ClassRecord);
        parameter<XString>(classQuery,"databasePath")->setValue(path);
        parameter<XString>(classQuery,"category")->setValue("class-a");
        parameter<XInt>(classQuery,"limit")->setValue(10);
        QCOMPARE(classQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(classQuery,"recordCount")->value(),3);
        QVERIFY(result<XBool>(classQuery,"exists")->value());
        QCOMPARE(recordAt(classQuery,0)->recordId(),QString("r3"));
        QCOMPARE(recordAt(classQuery,1)->recordId(),QString("r2"));
        QCOMPARE(recordAt(classQuery,2)->recordId(),QString("r1"));

        parameter<XInt>(classQuery,"limit")->setValue(2);
        QCOMPARE(classQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XObjectList>(classQuery,"records")->count(),qsizetype(2));
        QCOMPARE(recordAt(classQuery,0)->recordId(),QString("r3"));
        QCOMPARE(recordAt(classQuery,1)->recordId(),QString("r2"));

        parameter<XString>(classQuery,"category")->setValue("missing-class");
        QCOMPARE(classQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(classQuery,"recordCount")->value(),0);
        QVERIFY(!result<XBool>(classQuery,"exists")->value());
        QCOMPARE(result<XObjectList>(classQuery,"records")->count(),qsizetype(0));
        QCOMPARE(result<XDetectRecord>(classQuery,"latestRecord")->recordId(),
                 QString("record"));
        QCOMPARE(result<XString>(classQuery,"insertedRecordId")->value(),QString());

        DetectRecord objectQuery;
        objectQuery.setMode(DetectRecord::ObjectRecord);
        parameter<XString>(objectQuery,"databasePath")->setValue(path);
        parameter<XString>(objectQuery,"objectId")->setValue("object-a");
        QCOMPARE(objectQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(objectQuery,"recordCount")->value(),2);
        QCOMPARE(recordAt(objectQuery,0)->recordId(),QString("r3"));
        QCOMPARE(recordAt(objectQuery,1)->recordId(),QString("r1"));

        DetectRecord hasQuery;
        hasQuery.setMode(DetectRecord::HasRecord);
        parameter<XString>(hasQuery,"databasePath")->setValue(path);
        parameter<XString>(hasQuery,"category")->setValue("class-a");
        parameter<XString>(hasQuery,"objectId")->setValue("object-b");
        QCOMPARE(hasQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY(result<XBool>(hasQuery,"exists")->value());
        QCOMPARE(result<XInt>(hasQuery,"recordCount")->value(),1);
        QCOMPARE(result<XObjectList>(hasQuery,"records")->count(),qsizetype(0));

        parameter<XString>(hasQuery,"objectId")->setValue("missing");
        QCOMPARE(hasQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY(!result<XBool>(hasQuery,"exists")->value());
        QCOMPARE(result<XInt>(hasQuery,"recordCount")->value(),0);

        parameter<XString>(hasQuery,"category")->setValue("");
        parameter<XString>(hasQuery,"objectId")->setValue("object-a");
        QCOMPARE(hasQuery.runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY(result<XBool>(hasQuery,"exists")->value());

        QVERIFY(runRawDatabase(path,[](QSqlDatabase &database)
        {
            QSqlQuery query(database);
            if(!query.exec("PRAGMA user_version") || !query.next()) return false;
            if(query.value(0).toInt()!=1) return false;
            if(!query.exec("SELECT COUNT(*) FROM detect_records") || !query.next())
                return false;
            return query.value(0).toInt()==3;
        }));

        const QString lockConnection="detect_record_lock_"
                +QUuid::createUuid().toString(QUuid::Id128);
        {
            QSqlDatabase database=QSqlDatabase::addDatabase("QSQLITE",lockConnection);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery lock(database);
            QVERIFY(lock.exec("BEGIN EXCLUSIVE"));
            configureDirectRecord(writer,path,"locked",2000,"class-a","object-a");
            parameter<XInt>(writer,"busyTimeoutMs")->setValue(0);
            QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Error);
            QVERIFY(!writer.getXvFuncRunMsg().isEmpty());
            QCOMPARE(result<XString>(writer,"insertedRecordId")->value(),
                     QString("r3"));
            QCOMPARE(recordAt(writer,0)->recordId(),QString("r3"));
            QVERIFY(lock.exec("ROLLBACK"));
            database.close();
        }
        QSqlDatabase::removeDatabase(lockConnection);
        QVERIFY(!hasOperatorConnection());
    }

    void subscribedRecordAndDuplicateRollback()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("typed.sqlite");
        DetectRecord writer;
        parameter<XString>(writer,"databasePath")->setValue(path);
        parameter<XString>(writer,"category")->setValue("typed");
        parameter<XString>(writer,"objectId")->setValue("camera-1");
        parameter<XBool>(writer,"useRecordInput")->setValue(true);
        QJsonObject details;
        details.insert("confidence",0.95);
        QVERIFY(parameter<XDetectRecord>(writer,"record")
                ->setValue("typed-1",1234,"ok",details));
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XString>(writer,"insertedRecordId")->value(),QString("typed-1"));
        QCOMPARE(recordAt(writer,0)->outcome(),QString("ok"));

        QVERIFY(parameter<XDetectRecord>(writer,"record")
                ->setValue("typed-1",9999,"changed",QJsonObject()));
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!writer.getXvFuncRunMsg().isEmpty());
        QCOMPARE(result<XString>(writer,"insertedRecordId")->value(),QString("typed-1"));
        QCOMPARE(result<XInt>(writer,"recordCount")->value(),1);
        QCOMPARE(recordAt(writer,0)->timestampMs(),qint64(1234));
        QCOMPARE(recordAt(writer,0)->outcome(),QString("ok"));

        QVERIFY(runRawDatabase(path,[](QSqlDatabase &database)
        {
            QSqlQuery query(database);
            return query.exec("SELECT COUNT(*) FROM detect_records")
                    && query.next() && query.value(0).toInt()==1;
        }));
        QVERIFY(!hasOperatorConnection());
    }

    void invalidInputSchemaAndStoredJsonPreserveResults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("valid.sqlite");
        DetectRecord writer;
        configureDirectRecord(writer,path,"good",10,"class-a","object-a");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);

        DetectRecord query;
        query.setMode(DetectRecord::ClassRecord);
        parameter<XString>(query,"databasePath")->setValue(path);
        parameter<XString>(query,"category")->setValue("class-a");
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(recordAt(query,0)->recordId(),QString("good"));

        parameter<XInt>(query,"limit")->setValue(0);
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!query.getXvFuncRunMsg().isEmpty());
        QCOMPARE(result<XInt>(query,"recordCount")->value(),1);
        QCOMPARE(recordAt(query,0)->recordId(),QString("good"));
        parameter<XInt>(query,"limit")->setValue(100);

        parameter<XInt>(query,"busyTimeoutMs")->setValue(60001);
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XInt>(query,"recordCount")->value(),1);
        parameter<XInt>(query,"busyTimeoutMs")->setValue(5000);

        configureDirectRecord(writer,path,"bad-json",11,"class-a","object-a",
                              "ok","[]");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XString>(writer,"insertedRecordId")->value(),QString("good"));
        QCOMPARE(recordAt(writer,0)->recordId(),QString("good"));

        configureDirectRecord(writer,path,"fractional-time",11,"class-a","object-a");
        parameter<XReal>(writer,"timestampMs")->setValue(11.5);
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(recordAt(writer,0)->recordId(),QString("good"));

        QVERIFY(runRawDatabase(path,[](QSqlDatabase &database)
        {
            QSqlQuery update(database);
            return update.exec("UPDATE detect_records SET details_json = 'not-json' "
                               "WHERE record_id = 'good'");
        }));
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XInt>(query,"recordCount")->value(),1);
        QCOMPARE(recordAt(query,0)->recordId(),QString("good"));

        DetectRecord hasQuery;
        hasQuery.setMode(DetectRecord::HasRecord);
        parameter<XString>(hasQuery,"databasePath")->setValue(path);
        parameter<XString>(hasQuery,"category")->setValue("");
        parameter<XString>(hasQuery,"objectId")->setValue("");
        QCOMPARE(hasQuery.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!hasQuery.getXvFuncRunMsg().isEmpty());

        DetectRecord invalidPath;
        invalidPath.setMode(DetectRecord::ClassRecord);
        parameter<XString>(invalidPath,"databasePath")
                ->setValue(directory.filePath("missing/records.sqlite"));
        parameter<XString>(invalidPath,"category")->setValue("class-a");
        QCOMPARE(invalidPath.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!invalidPath.getXvFuncRunMsg().isEmpty());

        const QString malformedPath=directory.filePath("malformed.sqlite");
        QVERIFY(runRawDatabase(malformedPath,[](QSqlDatabase &database)
        {
            QSqlQuery query(database);
            return query.exec("CREATE TABLE detect_records (id INTEGER PRIMARY KEY)")
                    && query.exec("PRAGMA user_version = 1");
        }));
        DetectRecord malformed;
        malformed.setMode(DetectRecord::ClassRecord);
        parameter<XString>(malformed,"databasePath")->setValue(malformedPath);
        parameter<XString>(malformed,"category")->setValue("class-a");
        QCOMPARE(malformed.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!malformed.getXvFuncRunMsg().isEmpty());

        const QString futurePath=directory.filePath("future.sqlite");
        QVERIFY(runRawDatabase(futurePath,[](QSqlDatabase &database)
        {
            QSqlQuery query(database);
            return query.exec("PRAGMA user_version = 2");
        }));
        DetectRecord future;
        future.setMode(DetectRecord::ClassRecord);
        parameter<XString>(future,"databasePath")->setValue(futurePath);
        parameter<XString>(future,"category")->setValue("class-a");
        QCOMPARE(future.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!future.getXvFuncRunMsg().isEmpty());
        QVERIFY(!hasOperatorConnection());
    }

    void databasePathsAreIsolated()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath=directory.filePath("first.sqlite");
        const QString secondPath=directory.filePath("second.sqlite");
        DetectRecord writer;
        configureDirectRecord(writer,firstPath,"first",1,"shared","object-a");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);
        configureDirectRecord(writer,secondPath,"second",2,"shared","object-b");
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);

        DetectRecord query;
        query.setMode(DetectRecord::ClassRecord);
        parameter<XString>(query,"category")->setValue("shared");
        parameter<XString>(query,"databasePath")->setValue(firstPath);
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(query,"recordCount")->value(),1);
        QCOMPARE(recordAt(query,0)->recordId(),QString("first"));
        parameter<XString>(query,"databasePath")->setValue(secondPath);
        QCOMPARE(query.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(query,"recordCount")->value(),1);
        QCOMPARE(recordAt(query,0)->recordId(),QString("second"));
        QVERIFY(!hasOperatorConnection());
    }
};

QTEST_GUILESS_MAIN(DetectRecordTest)
#include "tst_detectrecord.moc"
