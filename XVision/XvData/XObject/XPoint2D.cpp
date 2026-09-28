#include "XPoint2D.h"

#include <QtGlobal>

XPoint2D::XPoint2D(const QString &objectName,double x,double y,
                   XObjectSet *parObjectSet,const QString &displayName)
    :XObject(objectName,parObjectSet,displayName)
{
    setValue(x,y);
}

XPoint2D::XPoint2D(double x,double y)
    :XObject()
{
    setValue(x,y);
}

XPoint2D::XPoint2D()
    :XObject()
{
}

XObject *XPoint2D::clone()
{
    auto result=new XPoint2D(objectName(),m_x,m_y,nullptr,dispalyName());
    result->setTips(tips());
    return result;
}

bool XPoint2D::getData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto target=dynamic_cast<XPoint2D*>(object);
    return target && target->setValue(m_x,m_y);
}

bool XPoint2D::setData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto source=dynamic_cast<XPoint2D*>(object);
    return source && setValue(source->x(),source->y());
}

bool XPoint2D::setValue(double x,double y)
{
    if(!qIsFinite(x) || !qIsFinite(y)) return false;
    m_x=x;
    m_y=y;
    return true;
}
