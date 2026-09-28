#include "DetectRecord.h"

#include "DetectRecordRepository.h"
#include "DetectRecordWdg.h"
#include "XLanguage.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QtMath>

#include <cmath>

using namespace XvCore;

namespace
{
constexpr double MaxExactTimestamp=9007199254740991.0;

bool directRecord(const DetectRecordParam *param,
                  DetectRecordRepository::Record &candidate,QString &error)
{
    const double timestamp=param->timestampMs->value();
    if(!qIsFinite(timestamp) || timestamp<0.0 || timestamp>MaxExactTimestamp
            || std::floor(timestamp)!=timestamp)
    {
        error="Record timestamp must be a nonnegative exactly represented integer";
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument details=QJsonDocument::fromJson(
                param->detailsJson->value().toUtf8(),&parseError);
    if(parseError.error!=QJsonParseError::NoError || !details.isObject())
    {
        error="Record details must be a valid JSON object";
        return false;
    }
    candidate.recordId=param->recordId->value().trimmed();
    candidate.timestampMs=qint64(timestamp);
    candidate.outcome=param->outcome->value().trimmed();
    candidate.details=details.object();
    return true;
}

bool subscribedRecord(const DetectRecordParam *param,
                      DetectRecordRepository::Record &candidate,QString &error)
{
    if(!param->record)
    {
        error="Subscribed detect-record input is unavailable";
        return false;
    }
    candidate.recordId=param->record->recordId();
    candidate.timestampMs=param->record->timestampMs();
    candidate.outcome=param->record->outcome();
    candidate.details=param->record->details();
    return true;
}
}

DetectRecordParam::DetectRecordParam()
{
    databasePath=new XString("databasePath","detect-records.sqlite",this,
                             getLang("XvFuncSystem_DetectRecord_DatabasePath",
                                     "数据库文件"));
    busyTimeoutMs=new XInt("busyTimeoutMs",5000,this,
                           getLang("XvFuncSystem_DetectRecord_BusyTimeout",
                                   "繁忙超时(ms)"));
    category=new XString("category","default",this,
                         getLang("XvFuncSystem_DetectRecord_Category","分类"));
    objectId=new XString("objectId","object",this,
                         getLang("XvFuncSystem_DetectRecord_ObjectId","对象标识"));
    useRecordInput=new XBool("useRecordInput",false,this,
                             getLang("XvFuncSystem_DetectRecord_UseInput",
                                     "使用记录输入"));
    record=new XDetectRecord("record","record",0,"unknown",QJsonObject(),this,
                             getLang("XvFuncSystem_DetectRecord_RecordInput",
                                     "检测记录输入"));
    recordId=new XString("recordId","record",this,
                         getLang("XvFuncSystem_DetectRecord_RecordId","记录标识"));
    timestampMs=new XReal("timestampMs",0.0,this,
                          getLang("XvFuncSystem_DetectRecord_Timestamp",
                                  "时间戳(ms)"));
    outcome=new XString("outcome","unknown",this,
                        getLang("XvFuncSystem_DetectRecord_Outcome","检测结果"));
    detailsJson=new XString("detailsJson","{}",this,
                            getLang("XvFuncSystem_DetectRecord_Details",
                                    "详情(JSON)"));
    limit=new XInt("limit",100,this,
                   getLang("XvFuncSystem_DetectRecord_Limit","查询上限"));
}

DetectRecordResult::DetectRecordResult()
{
    records=new XObjectList("records",XDetectRecord::type(),this,
                            getLang("XvFuncSystem_DetectRecord_Records","检测记录列表"));
    latestRecord=new XDetectRecord(
                "latestRecord","record",0,"unknown",QJsonObject(),this,
                getLang("XvFuncSystem_DetectRecord_Latest","最新检测记录"));
    recordCount=new XInt("recordCount",0,this,
                         getLang("XvFuncSystem_DetectRecord_Count","记录数量"));
    exists=new XBool("exists",false,this,
                     getLang("XvFuncSystem_DetectRecord_Exists","存在记录"));
    insertedRecordId=new XString(
                "insertedRecordId","",this,
                getLang("XvFuncSystem_DetectRecord_InsertedId","已写入记录标识"));
}

DetectRecord::DetectRecord(QObject *parent)
    :XvFunc(parent),m_param(new DetectRecordParam()),m_result(new DetectRecordResult())
{
    _funcRole="DetectRecord";
    _funcName=getLang("XvFuncSystem_DetectRecord_Name","检测记录");
    _funcType=EXvFuncType::DataProcessing;
}

DetectRecord::~DetectRecord()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList DetectRecord::activeParameterNames() const
{
    QStringList names={"databasePath","busyTimeoutMs"};
    switch(m_mode)
    {
    case Record:
        names << "category" << "objectId" << "useRecordInput";
        if(m_param && m_param->useRecordInput && m_param->useRecordInput->value())
            names << "record";
        else
            names << "recordId" << "timestampMs" << "outcome" << "detailsJson";
        break;
    case ClassRecord:
        names << "category" << "limit";
        break;
    case ObjectRecord:
        names << "objectId" << "limit";
        break;
    case HasRecord:
        names << "category" << "objectId";
        break;
    }
    return names;
}

EXvFuncRunStatus DetectRecord::run()
{
    if(!m_param || !m_result || !m_param->databasePath || !m_param->busyTimeoutMs
            || !m_param->category || !m_param->objectId || !m_param->useRecordInput
            || !m_param->record || !m_param->recordId || !m_param->timestampMs
            || !m_param->outcome || !m_param->detailsJson || !m_param->limit
            || !m_result->records || !m_result->latestRecord || !m_result->recordCount
            || !m_result->exists || !m_result->insertedRecordId)
    {
        setRunMsg(getLang("XvFuncSystem_DetectRecord_Incomplete",
                          "Detect-record parameters or results are incomplete"));
        return EXvFuncRunStatus::Error;
    }
    if(m_mode<Record || m_mode>HasRecord)
    {
        setRunMsg(getLang("XvFuncSystem_DetectRecord_InvalidMode",
                          "Detect-record mode is invalid"));
        return EXvFuncRunStatus::Error;
    }

    DetectRecordRepository repository;
    QList<DetectRecordRepository::Record> candidateRecords;
    bool candidateExists=false;
    QString candidateInsertedId;
    QString error;
    const QString databasePath=m_param->databasePath->value();
    const int busyTimeoutMs=m_param->busyTimeoutMs->value();
    switch(m_mode)
    {
    case Record:
    {
        DetectRecordRepository::Record candidate;
        const bool valid=m_param->useRecordInput->value()
                ?subscribedRecord(m_param,candidate,error)
                :directRecord(m_param,candidate,error);
        if(!valid) break;
        candidate.category=m_param->category->value().trimmed();
        candidate.objectId=m_param->objectId->value().trimmed();
        if(!repository.append(databasePath,busyTimeoutMs,candidate,error)) break;
        candidateRecords.append(candidate);
        candidateExists=true;
        candidateInsertedId=candidate.recordId;
        break;
    }
    case ClassRecord:
        if(repository.queryByCategory(databasePath,busyTimeoutMs,
                                      m_param->category->value(),m_param->limit->value(),
                                      candidateRecords,error))
            candidateExists=!candidateRecords.isEmpty();
        break;
    case ObjectRecord:
        if(repository.queryByObject(databasePath,busyTimeoutMs,
                                    m_param->objectId->value(),m_param->limit->value(),
                                    candidateRecords,error))
            candidateExists=!candidateRecords.isEmpty();
        break;
    case HasRecord:
        repository.exists(databasePath,busyTimeoutMs,m_param->category->value(),
                          m_param->objectId->value(),candidateExists,error);
        break;
    }
    if(!error.isEmpty())
    {
        setRunMsg(getLang("XvFuncSystem_DetectRecord_DatabaseError",error));
        return EXvFuncRunStatus::Error;
    }

    XObjectList recordsCandidate("records",XDetectRecord::type());
    for(int index=0;index<candidateRecords.size();++index)
    {
        const DetectRecordRepository::Record &record=candidateRecords.at(index);
        auto value=new XDetectRecord(QString("record_%1").arg(index),record.recordId,
                                     record.timestampMs,record.outcome,record.details);
        if(!recordsCandidate.addValue(value))
        {
            delete value;
            setRunMsg(getLang("XvFuncSystem_DetectRecord_ResultError",
                              "Detect-record result candidate is invalid"));
            return EXvFuncRunStatus::Error;
        }
    }
    XDetectRecord latestCandidate;
    if(!candidateRecords.isEmpty())
    {
        const DetectRecordRepository::Record &latest=candidateRecords.first();
        if(!latestCandidate.setValue(latest.recordId,latest.timestampMs,
                                     latest.outcome,latest.details))
        {
            setRunMsg(getLang("XvFuncSystem_DetectRecord_ResultError",
                              "Detect-record result candidate is invalid"));
            return EXvFuncRunStatus::Error;
        }
    }
    if(!m_result->records->setData(&recordsCandidate)
            || !m_result->latestRecord->setData(&latestCandidate))
    {
        setRunMsg(getLang("XvFuncSystem_DetectRecord_CommitError",
                          "Detect-record results could not be committed"));
        return EXvFuncRunStatus::Error;
    }
    m_result->recordCount->setValue(m_mode==HasRecord?(candidateExists?1:0)
                                                    :candidateRecords.size());
    m_result->exists->setValue(candidateExists);
    m_result->insertedRecordId->setValue(candidateInsertedId);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

QPixmap DetectRecord::funcIcon()
{
    return QPixmap(":/images/BaseDataWriter.svg");
}

void DetectRecord::onShowFunc()
{
    if(!m_widget) m_widget=new DetectRecordWdg(this);
    m_widget->show();
    m_widget->raise();
}
