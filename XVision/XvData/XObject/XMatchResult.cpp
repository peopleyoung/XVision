#include "XMatchResult.h"

#include <QtGlobal>

XMatchResult::XMatchResult(const QString &objectName,double x,double y,
                           double angle,double score,XObjectSet *parObjectSet,
                           const QString &displayName)
    :XObject(objectName,parObjectSet,displayName)
{
    setValue(x,y,angle,score);
}

XMatchResult::XMatchResult(double x,double y,double angle,double score)
    :XObject()
{
    setValue(x,y,angle,score);
}

XMatchResult::XMatchResult()
    :XObject()
{
}

XObject *XMatchResult::clone()
{
    auto result=new XMatchResult(objectName(),m_x,m_y,m_angle,m_score,nullptr,
                                 dispalyName());
    result->setTips(tips());
    return result;
}

bool XMatchResult::getData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto target=dynamic_cast<XMatchResult*>(object);
    return target && target->setValue(m_x,m_y,m_angle,m_score);
}

bool XMatchResult::setData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto source=dynamic_cast<XMatchResult*>(object);
    return source && setValue(source->x(),source->y(),source->angle(),source->score());
}

bool XMatchResult::setValue(double x,double y,double angle,double score)
{
    if(!qIsFinite(x) || !qIsFinite(y) || !qIsFinite(angle)
            || !qIsFinite(score) || score<0.0 || score>1.0)
    {
        return false;
    }
    m_x=x;
    m_y=y;
    m_angle=angle;
    m_score=score;
    return true;
}
