#ifndef DETECTRECORD_H
#define DETECTRECORD_H

#include "XVFuncSystemGlobal.h"
#include "XVisionRuntimeData.h"
#include "XvFunc.h"

#include <XObjectBaseType>

class DetectRecordWdg;

namespace XvCore
{
class DetectRecordParam:public XvBaseParam
{
public:
    DetectRecordParam();

    XString *databasePath=nullptr;
    XInt *busyTimeoutMs=nullptr;
    XString *category=nullptr;
    XString *objectId=nullptr;
    XBool *useRecordInput=nullptr;
    XDetectRecord *record=nullptr;
    XString *recordId=nullptr;
    XReal *timestampMs=nullptr;
    XString *outcome=nullptr;
    XString *detailsJson=nullptr;
    XInt *limit=nullptr;
};

class DetectRecordResult:public XvBaseResult
{
public:
    DetectRecordResult();

    XObjectList *records=nullptr;
    XDetectRecord *latestRecord=nullptr;
    XInt *recordCount=nullptr;
    XBool *exists=nullptr;
    XString *insertedRecordId=nullptr;
};

class XVFUNCSYSTEM_EXPORT DetectRecord:public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Record=0,ClassRecord=1,ObjectRecord=2,HasRecord=3 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit DetectRecord(QObject *parent=nullptr);
    ~DetectRecord() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const;
    QStringList persistentPropertyNames() const override { return {"mode"}; }

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;


private:
    DetectRecordParam *m_param=nullptr;
    DetectRecordResult *m_result=nullptr;
    DetectRecordWdg *m_widget=nullptr;
    Mode m_mode=Record;
};
}

#endif // DETECTRECORD_H
