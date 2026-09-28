#ifndef XVISIONRUNTIMEDATA_H
#define XVISIONRUNTIMEDATA_H

#include "XObject.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>

#include <limits>

class XVDATA_EXPORT XByteArray:public XObject
{
public:
    XByteArray(const QString &objectName,const QByteArray &value=QByteArray(),
               XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName),m_value(value) { }
    XByteArray(const QByteArray &value):XObject(),m_value(value) { }
    XByteArray():XObject() { }
    static QString type() { return "XByteArray"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XByteArray(objectName(),m_value,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XByteArray*>(object):nullptr;
        if(!target) return false;
        target->setValue(m_value);
        return true;
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XByteArray*>(object):nullptr;
        if(!source) return false;
        setValue(source->value());
        return true;
    }
    void setValue(const QByteArray &value) { m_value=value; }
    const QByteArray &value() const { return m_value; }
private:
    QByteArray m_value;
};

class XVDATA_EXPORT XJsonValue:public XObject
{
public:
    XJsonValue(const QString &objectName,const QJsonDocument &value=QJsonDocument(),
               XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName),m_value(value) { }
    XJsonValue(const QJsonDocument &value):XObject(),m_value(value) { }
    XJsonValue():XObject() { }
    static QString type() { return "XJsonValue"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XJsonValue(objectName(),m_value,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XJsonValue*>(object):nullptr;
        return target && target->setValue(m_value);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XJsonValue*>(object):nullptr;
        return source && setValue(source->value());
    }
    bool setValue(const QJsonDocument &value)
    {
        m_value=value;
        return true;
    }
    const QJsonDocument &value() const { return m_value; }
private:
    QJsonDocument m_value;
};

class XVDATA_EXPORT XTensor:public XObject
{
public:
    XTensor(const QString &objectName,const QString &elementType="float32",
            const QVector<qint64> &dimensions=QVector<qint64>{1},
            const QByteArray &bytes=QByteArray(4,0),XObjectSet *parent=nullptr,
            const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(elementType,dimensions,bytes); }
    XTensor():XObject() { }
    static QString type() { return "XTensor"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XTensor(objectName(),m_elementType,m_dimensions,m_bytes,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XTensor*>(object):nullptr;
        return target && target->setValue(m_elementType,m_dimensions,m_bytes);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XTensor*>(object):nullptr;
        return source && setValue(source->elementType(),source->dimensions(),source->bytes());
    }
    bool setValue(const QString &elementType,const QVector<qint64> &dimensions,
                  const QByteArray &bytes)
    {
        const int bytesPerElement=elementSize(elementType);
        if(bytesPerElement==0) return false;
        qint64 count=1;
        for(qint64 dimension:dimensions)
        {
            if(dimension<=0 || count>std::numeric_limits<qint64>::max()/dimension) return false;
            count*=dimension;
        }
        if(count>std::numeric_limits<qint64>::max()/bytesPerElement
                || count*bytesPerElement!=static_cast<qint64>(bytes.size())) return false;
        m_elementType=elementType;
        m_dimensions=dimensions;
        m_bytes=bytes;
        return true;
    }
    static int elementSize(const QString &elementType)
    {
        if(elementType=="uint8" || elementType=="int8" || elementType=="bool") return 1;
        if(elementType=="uint16" || elementType=="int16"
                || elementType=="float16" || elementType=="bfloat16") return 2;
        if(elementType=="uint32" || elementType=="int32" || elementType=="float32") return 4;
        if(elementType=="uint64" || elementType=="int64"
                || elementType=="float64" || elementType=="complex64") return 8;
        if(elementType=="complex128") return 16;
        return 0;
    }
    QString elementType() const { return m_elementType; }
    const QVector<qint64> &dimensions() const { return m_dimensions; }
    const QByteArray &bytes() const { return m_bytes; }
private:
    QString m_elementType="float32";
    QVector<qint64> m_dimensions{1};
    QByteArray m_bytes=QByteArray(4,0);
};

class XVDATA_EXPORT XDetectRecord:public XObject
{
public:
    XDetectRecord(const QString &objectName,const QString &recordId="record",
                  qint64 timestampMs=0,const QString &outcome="unknown",
                  const QJsonObject &details=QJsonObject(),XObjectSet *parent=nullptr,
                  const QString &displayName="")
        :XObject(objectName,parent,displayName)
    { setValue(recordId,timestampMs,outcome,details); }
    XDetectRecord():XObject() { }
    static QString type() { return "XDetectRecord"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XDetectRecord(objectName(),m_recordId,m_timestampMs,m_outcome,m_details,
                                     nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XDetectRecord*>(object):nullptr;
        return target && target->setValue(m_recordId,m_timestampMs,m_outcome,m_details);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XDetectRecord*>(object):nullptr;
        return source && setValue(source->recordId(),source->timestampMs(),source->outcome(),source->details());
    }
    bool setValue(const QString &recordId,qint64 timestampMs,const QString &outcome,
                  const QJsonObject &details)
    {
        const QString normalizedId=recordId.trimmed();
        const QString normalizedOutcome=outcome.trimmed();
        if(normalizedId.isEmpty() || timestampMs<0 || normalizedOutcome.isEmpty()) return false;
        m_recordId=normalizedId;
        m_timestampMs=timestampMs;
        m_outcome=normalizedOutcome;
        m_details=details;
        return true;
    }
    QString recordId() const { return m_recordId; }
    qint64 timestampMs() const { return m_timestampMs; }
    QString outcome() const { return m_outcome; }
    const QJsonObject &details() const { return m_details; }
private:
    QString m_recordId="record";
    qint64 m_timestampMs=0;
    QString m_outcome="unknown";
    QJsonObject m_details;
};

#endif // XVISIONRUNTIMEDATA_H
