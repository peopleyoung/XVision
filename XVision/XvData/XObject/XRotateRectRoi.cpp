#include "XRotateRectRoi.h"

#include <QtGlobal>

XRotateRectRoi::XRotateRectRoi(const QString &objectName,double centerX,
                               double centerY,double length1,double length2,
                               double angle,XObjectSet *parObjectSet,
                               const QString &displayName)
    :XObject(objectName,parObjectSet,displayName)
{
    setValue(centerX,centerY,length1,length2,angle);
}

XRotateRectRoi::XRotateRectRoi(double centerX,double centerY,double length1,
                               double length2,double angle)
    :XObject()
{
    setValue(centerX,centerY,length1,length2,angle);
}

XRotateRectRoi::XRotateRectRoi()
    :XObject()
{
}

XObject *XRotateRectRoi::clone()
{
    auto result=new XRotateRectRoi(objectName(),m_centerX,m_centerY,m_length1,
                                   m_length2,m_angle,nullptr,dispalyName());
    result->setTips(tips());
    return result;
}

bool XRotateRectRoi::getData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto target=dynamic_cast<XRotateRectRoi*>(object);
    return target && target->setValue(m_centerX,m_centerY,m_length1,m_length2,m_angle);
}

bool XRotateRectRoi::setData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto source=dynamic_cast<XRotateRectRoi*>(object);
    return source && setValue(source->centerX(),source->centerY(),source->length1(),
                              source->length2(),source->angle());
}

bool XRotateRectRoi::setValue(double centerX,double centerY,double length1,
                              double length2,double angle)
{
    if(!qIsFinite(centerX) || !qIsFinite(centerY)
            || !qIsFinite(length1) || !qIsFinite(length2)
            || !qIsFinite(angle) || length1<=0.0 || length2<=0.0)
    {
        return false;
    }
    m_centerX=centerX;
    m_centerY=centerY;
    m_length1=length1;
    m_length2=length2;
    m_angle=angle;
    return true;
}
