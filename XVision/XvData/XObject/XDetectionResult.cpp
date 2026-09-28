#include "XDetectionResult.h"

#include <QtGlobal>

XDetectionResult::XDetectionResult(const QString &objectName,double x,double y,
                                   double width,double height,int classId,
                                   const QString &className,double score,
                                   XObjectSet *parObjectSet,
                                   const QString &displayName)
    :XObject(objectName,parObjectSet,displayName)
{
    setValue(x,y,width,height,classId,className,score);
}

XDetectionResult::XDetectionResult(double x,double y,double width,double height,
                                   int classId,const QString &className,
                                   double score)
    :XObject()
{
    setValue(x,y,width,height,classId,className,score);
}

XDetectionResult::XDetectionResult()
    :XObject()
{
}

XObject *XDetectionResult::clone()
{
    auto result=new XDetectionResult(objectName(),m_x,m_y,m_width,m_height,
                                     m_classId,m_className,m_score,nullptr,
                                     dispalyName());
    result->setTips(tips());
    return result;
}

bool XDetectionResult::getData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto target=dynamic_cast<XDetectionResult*>(object);
    return target && target->setValue(m_x,m_y,m_width,m_height,m_classId,
                                      m_className,m_score);
}

bool XDetectionResult::setData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto source=dynamic_cast<XDetectionResult*>(object);
    return source && setValue(source->x(),source->y(),source->width(),
                              source->height(),source->classId(),
                              source->className(),source->score());
}

bool XDetectionResult::setValue(double x,double y,double width,double height,
                                int classId,const QString &className,double score)
{
    const QString normalizedName=className.trimmed();
    if(!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(width)
            || !qIsFinite(height) || width<=0.0 || height<=0.0
            || classId<0 || normalizedName.isEmpty() || !qIsFinite(score)
            || score<0.0 || score>1.0)
    {
        return false;
    }
    m_x=x;
    m_y=y;
    m_width=width;
    m_height=height;
    m_classId=classId;
    m_className=normalizedName;
    m_score=score;
    return true;
}
