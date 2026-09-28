#ifndef XVISIONSHAREDDATA_H
#define XVISIONSHAREDDATA_H

#include "XObject.h"

#include <QImage>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace XvSharedDataDetail
{
inline bool finitePoint(const QPointF &point)
{
    return qIsFinite(point.x()) && qIsFinite(point.y());
}

inline bool finiteRect(const QRectF &rect)
{
    return qIsFinite(rect.x()) && qIsFinite(rect.y())
            && qIsFinite(rect.width()) && qIsFinite(rect.height())
            && rect.width()>0.0 && rect.height()>0.0;
}
}

class XVDATA_EXPORT XLine2D:public XObject
{
public:
    XLine2D(const QString &objectName,const QPointF &start=QPointF(),
            const QPointF &end=QPointF(1.0,0.0),XObjectSet *parent=nullptr,
            const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(start,end); }
    XLine2D(const QPointF &start,const QPointF &end):XObject() { setValue(start,end); }
    XLine2D():XObject() { }

    static QString type() { return "XLine2D"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XLine2D(objectName(),m_start,m_end,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XLine2D*>(object):nullptr;
        return target && target->setValue(m_start,m_end);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XLine2D*>(object):nullptr;
        return source && setValue(source->start(),source->end());
    }
    bool setValue(const QPointF &start,const QPointF &end)
    {
        if(!XvSharedDataDetail::finitePoint(start) || !XvSharedDataDetail::finitePoint(end)
                || qFuzzyIsNull(QLineF(start,end).length())) return false;
        m_start=start;
        m_end=end;
        return true;
    }
    QPointF start() const { return m_start; }
    QPointF end() const { return m_end; }
private:
    QPointF m_start;
    QPointF m_end{1.0,0.0};
};

class XVDATA_EXPORT XCircle2D:public XObject
{
public:
    XCircle2D(const QString &objectName,double centerX=0.0,double centerY=0.0,
              double radius=1.0,XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(centerX,centerY,radius); }
    XCircle2D(double centerX,double centerY,double radius):XObject() { setValue(centerX,centerY,radius); }
    XCircle2D():XObject() { }

    static QString type() { return "XCircle2D"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XCircle2D(objectName(),m_center.x(),m_center.y(),m_radius,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XCircle2D*>(object):nullptr;
        return target && target->setValue(m_center.x(),m_center.y(),m_radius);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XCircle2D*>(object):nullptr;
        return source && setValue(source->centerX(),source->centerY(),source->radius());
    }
    bool setValue(double centerX,double centerY,double radius)
    {
        if(!qIsFinite(centerX) || !qIsFinite(centerY) || !qIsFinite(radius) || radius<=0.0) return false;
        m_center=QPointF(centerX,centerY);
        m_radius=radius;
        return true;
    }
    double centerX() const { return m_center.x(); }
    double centerY() const { return m_center.y(); }
    double radius() const { return m_radius; }
private:
    QPointF m_center;
    double m_radius=1.0;
};

class XVDATA_EXPORT XRect2D:public XObject
{
public:
    XRect2D(const QString &objectName,double x=0.0,double y=0.0,double width=1.0,double height=1.0,
            XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(x,y,width,height); }
    XRect2D(double x,double y,double width,double height):XObject() { setValue(x,y,width,height); }
    XRect2D():XObject() { }

    static QString type() { return "XRect2D"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XRect2D(objectName(),m_rect.x(),m_rect.y(),m_rect.width(),m_rect.height(),nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XRect2D*>(object):nullptr;
        return target && target->setValue(m_rect.x(),m_rect.y(),m_rect.width(),m_rect.height());
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XRect2D*>(object):nullptr;
        return source && setValue(source->x(),source->y(),source->width(),source->height());
    }
    bool setValue(double x,double y,double width,double height)
    {
        const QRectF candidate(x,y,width,height);
        if(!XvSharedDataDetail::finiteRect(candidate)) return false;
        m_rect=candidate;
        return true;
    }
    double x() const { return m_rect.x(); }
    double y() const { return m_rect.y(); }
    double width() const { return m_rect.width(); }
    double height() const { return m_rect.height(); }
private:
    QRectF m_rect{0.0,0.0,1.0,1.0};
};

class XVDATA_EXPORT XContour:public XObject
{
public:
    XContour(const QString &objectName,const QVector<QPointF> &points=QVector<QPointF>(),
             XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(points); }
    XContour(const QVector<QPointF> &points):XObject() { setValue(points); }
    XContour():XObject() { }

    static QString type() { return "XContour"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XContour(objectName(),m_points,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XContour*>(object):nullptr;
        return target && target->setValue(m_points);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XContour*>(object):nullptr;
        return source && setValue(source->points());
    }
    bool setValue(const QVector<QPointF> &points)
    {
        if(points.size()<2) return false;
        for(const QPointF &point:points)
            if(!XvSharedDataDetail::finitePoint(point)) return false;
        m_points=points;
        return true;
    }
    const QVector<QPointF> &points() const { return m_points; }
private:
    QVector<QPointF> m_points;
};

class XVDATA_EXPORT XRegion:public XObject
{
public:
    XRegion(const QString &objectName,const QImage &mask=QImage(),XObjectSet *parent=nullptr,
            const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(mask); }
    XRegion(const QImage &mask):XObject() { setValue(mask); }
    XRegion():XObject() { }

    static QString type() { return "XRegion"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XRegion(objectName(),m_mask.copy(),nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XRegion*>(object):nullptr;
        return target && target->setValue(m_mask);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XRegion*>(object):nullptr;
        return source && setValue(source->mask());
    }
    bool setValue(const QImage &mask)
    {
        if(!mask.isNull() && mask.format()!=QImage::Format_Grayscale8) return false;
        m_mask=mask.isNull()?QImage():mask.copy();
        return true;
    }
    const QImage &mask() const { return m_mask; }
private:
    QImage m_mask;
};

class XVDATA_EXPORT XKeyPoint:public XObject
{
public:
    XKeyPoint(const QString &objectName,double x=0.0,double y=0.0,double size=1.0,
              double angle=0.0,double response=0.0,int octave=0,int classId=-1,
              XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName)
    { setValue(x,y,size,angle,response,octave,classId); }
    XKeyPoint(double x,double y,double size=1.0,double angle=0.0,double response=0.0,
              int octave=0,int classId=-1):XObject()
    { setValue(x,y,size,angle,response,octave,classId); }
    XKeyPoint():XObject() { }

    static QString type() { return "XKeyPoint"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XKeyPoint(objectName(),m_x,m_y,m_size,m_angle,m_response,m_octave,m_classId,
                                 nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XKeyPoint*>(object):nullptr;
        return target && target->setValue(m_x,m_y,m_size,m_angle,m_response,m_octave,m_classId);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XKeyPoint*>(object):nullptr;
        return source && setValue(source->x(),source->y(),source->size(),source->angle(),
                                   source->response(),source->octave(),source->classId());
    }
    bool setValue(double x,double y,double size,double angle,double response,int octave,int classId)
    {
        if(!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(size) || size<=0.0
                || !qIsFinite(angle) || !qIsFinite(response) || octave<0 || classId<-1) return false;
        m_x=x; m_y=y; m_size=size; m_angle=angle; m_response=response;
        m_octave=octave; m_classId=classId;
        return true;
    }
    double x() const { return m_x; }
    double y() const { return m_y; }
    double size() const { return m_size; }
    double angle() const { return m_angle; }
    double response() const { return m_response; }
    int octave() const { return m_octave; }
    int classId() const { return m_classId; }
private:
    double m_x=0.0;
    double m_y=0.0;
    double m_size=1.0;
    double m_angle=0.0;
    double m_response=0.0;
    int m_octave=0;
    int m_classId=-1;
};

class XVDATA_EXPORT XMeasurementResult:public XObject
{
public:
    XMeasurementResult(const QString &objectName,double value=0.0,const QString &unit="px",
                       double lower=0.0,double upper=0.0,bool passed=true,
                       XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(value,unit,lower,upper,passed); }
    XMeasurementResult(double value,const QString &unit,double lower,double upper,bool passed=true):XObject()
    { setValue(value,unit,lower,upper,passed); }
    XMeasurementResult():XObject() { }

    static QString type() { return "XMeasurementResult"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XMeasurementResult(objectName(),m_value,m_unit,m_lower,m_upper,m_passed,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XMeasurementResult*>(object):nullptr;
        return target && target->setValue(m_value,m_unit,m_lower,m_upper,m_passed);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XMeasurementResult*>(object):nullptr;
        return source && setValue(source->value(),source->unit(),source->lower(),source->upper(),source->passed());
    }
    bool setValue(double value,const QString &unit,double lower,double upper,bool passed)
    {
        if(!qIsFinite(value) || !qIsFinite(lower) || !qIsFinite(upper)
                || lower>upper || unit.trimmed().isEmpty()) return false;
        m_value=value; m_unit=unit.trimmed(); m_lower=lower; m_upper=upper; m_passed=passed;
        return true;
    }
    double value() const { return m_value; }
    QString unit() const { return m_unit; }
    double lower() const { return m_lower; }
    double upper() const { return m_upper; }
    bool passed() const { return m_passed; }
private:
    double m_value=0.0;
    QString m_unit="px";
    double m_lower=0.0;
    double m_upper=0.0;
    bool m_passed=true;
};

class XVDATA_EXPORT XClassificationResult:public XObject
{
public:
    XClassificationResult(const QString &objectName,int classId=0,const QString &className="Class 0",
                          double score=0.0,XObjectSet *parent=nullptr,const QString &displayName="")
        :XObject(objectName,parent,displayName) { setValue(classId,className,score); }
    XClassificationResult(int classId,const QString &className,double score):XObject()
    { setValue(classId,className,score); }
    XClassificationResult():XObject() { }

    static QString type() { return "XClassificationResult"; }
    QString typeName() override { return type(); }
    XObject *clone() override
    {
        auto value=new XClassificationResult(objectName(),m_classId,m_className,m_score,nullptr,dispalyName());
        value->setTips(tips());
        return value;
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<XClassificationResult*>(object):nullptr;
        return target && target->setValue(m_classId,m_className,m_score);
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<XClassificationResult*>(object):nullptr;
        return source && setValue(source->classId(),source->className(),source->score());
    }
    bool setValue(int classId,const QString &className,double score)
    {
        const QString normalizedName=className.trimmed();
        if(classId<0 || normalizedName.isEmpty() || !qIsFinite(score) || score<0.0 || score>1.0) return false;
        m_classId=classId; m_className=normalizedName; m_score=score;
        return true;
    }
    int classId() const { return m_classId; }
    QString className() const { return m_className; }
    double score() const { return m_score; }
private:
    int m_classId=0;
    QString m_className="Class 0";
    double m_score=0.0;
};

#endif // XVISIONSHAREDDATA_H
