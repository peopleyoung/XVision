#include "XObjectList.h"

XObjectList::XObjectList(const QString &objectName,const QString &valueType,XObjectSet *parObjectSet, const QString &dispalyName)
 :XObject{objectName,parObjectSet,dispalyName},_valueType(valueType)
{

}

XObjectList::XObjectList()
    :XObject{}
{

}

XObjectList::~XObjectList()
{

}

bool XObjectList::init(const QString &objectName, const QString &valueType, XObjectSet *parObjectSet, const QString &dispalyName)
{
    _valueType=valueType;
    return XObject::init(objectName,parObjectSet,dispalyName);
}

bool XObjectList::getData(XObject *object)
{
    if(!object)
    {
        return false;
    }
    if(object->typeName()!=this->typeName())
    {
        return false;
    }
    auto temp=dynamic_cast<XObjectList*>(object);
    if(!temp)
    {
        return false;
    }
    if(temp->valueType()!=this->valueType())
    {
        return false;
    }
    return temp->copyValuesFrom(*this);
}

bool XObjectList::setData(XObject *object)
{
    if(!object)
    {
        return false;
    }
    if(object->typeName()!=this->typeName())
    {
        return false;
    }
    auto temp=dynamic_cast<XObjectList*>(object);
    if(!temp)
    {
        return false;
    }
    if(temp->valueType()!=this->valueType())
    {
        return false;
    }
    return copyValuesFrom(*temp);
}

XObject *XObjectList::clone()
{
    auto result=new XObjectList(objectName(),valueType(),nullptr,dispalyName());
    result->setTips(tips());
    if(!result->copyValuesFrom(*this))
    {
        delete result;
        return nullptr;
    }
    return result;
}

XObject *XObjectList::value(qsizetype idx)
{
    return _lst.at(idx);
}

bool XObjectList::addValue(XObject *object)
{
    if(!object)
    {
        return false;
    }
    if(object->typeName()!=this->valueType())
    {
        return false;
    }
    if(!object->setParObjectSet(this))
    {
        return false;
    }
    _lst.append(object);
    return true;
}

bool XObjectList::removeValue(XObject *object,bool del)
{
    if(!object)
    {
        return false;
    }
    if(object->typeName()!=this->valueType())
    {
        return false;
    }
    if(!_lst.contains(object))
    {
        return false;
    }
    bool bRet= _lst.removeOne(object);
    if(del)
    {
        delete object;
        object=nullptr;
    }
    return bRet;
}

void XObjectList::clear(bool del)
{
    const QList<XObject*> valuesToRemove=_lst;
    _lst.clear();
    foreach (auto obj, valuesToRemove)
    {
        if(del)
        {
            delete obj;
        }
    }
}

qsizetype XObjectList::count() const
{
    return _lst.count();
}

bool XObjectList::copyValuesFrom(const XObjectList &source)
{
    QList<XObject*> clones;
    for(XObject *object:source._lst)
    {
        XObject *copy=object?object->clone():nullptr;
        if(!copy || copy->typeName()!=valueType())
        {
            delete copy;
            for(XObject *created:clones) delete created;
            return false;
        }
        if(!copy->setParObjectSet(this))
        {
            delete copy;
            for(XObject *created:clones) delete created;
            return false;
        }
        clones.append(copy);
    }

    clear();
    _lst=clones;
    return true;
}
