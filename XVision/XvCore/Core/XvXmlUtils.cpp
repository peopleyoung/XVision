#include "XvXmlUtils.h"

#include <QLocale>
#include <QMap>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QRegularExpression>

#include "XBool.h"
#include "XDetectionResult.h"
#include "XInt.h"
#include "XMatchResult.h"
#include "XObject.h"
#include "XObjectList.h"
#include "XPoint2D.h"
#include "XReal.h"
#include "XRotateRectRoi.h"
#include "XString.h"
#include "XVisionSharedData.h"

namespace XvCore
{
namespace XvXml
{

const char ProjectFileFormat[] = "xvision-project";
const char ProjectFileVersion[] = "1";
const char ProjectFileSuffix[] = "xvproj";
const char FlowFileFormat[] = "xvision-flow";
const char FlowFileVersion[] = "1";
const char FlowFileSuffix[] = "xvflow";

namespace
{
QString elementLabel(const QDomElement &element)
{
    return QString("<%1>").arg(element.tagName());
}

bool parseBool(const QString &text,bool &value)
{
    if(text=="true")
    {
        value=true;
        return true;
    }
    if(text=="false")
    {
        value=false;
        return true;
    }
    return false;
}

bool parseCanonicalInt(const QString &text,int &value)
{
    bool ok=false;
    const int parsed=text.toInt(&ok,10);
    if(!ok || QString::number(parsed)!=text) return false;
    value=parsed;
    return true;
}

QString scalarText(const XObject *object)
{
    if(auto value=dynamic_cast<const XBool*>(object))
    {
        return value->value()?"true":"false";
    }
    if(auto value=dynamic_cast<const XInt*>(object))
    {
        return QString::number(value->value());
    }
    if(auto value=dynamic_cast<const XReal*>(object))
    {
        return QString::number(value->value(),'g',17);
    }
    if(auto value=dynamic_cast<const XString*>(object))
    {
        return value->value();
    }
    return QString();
}

QString propertyTypeName(const QMetaProperty &property)
{
    if(property.isEnumType()) return "int";
    switch(property.userType())
    {
    case QMetaType::Bool: return "bool";
    case QMetaType::Int: return "int";
    case QMetaType::Double: return "double";
    case QMetaType::QString: return "QString";
    default: return QString();
    }
}

QString realText(double value)
{
    return QString::number(value,'g',17);
}

bool hasDirectText(const QDomElement &element)
{
    for(QDomNode node=element.firstChild();!node.isNull();node=node.nextSibling())
    {
        if(node.isText() && !node.nodeValue().trimmed().isEmpty()) return true;
    }
    return false;
}

bool isStructuredValueType(const QString &type)
{
    return type==XPoint2DType || type==XRotateRectRoiType
            || type==XMatchResultType || type==XDetectionResultType
            || type==XLine2D::type() || type==XCircle2D::type()
            || type==XRect2D::type() || type==XKeyPoint::type()
            || type==XMeasurementResult::type()
            || type==XClassificationResult::type();
}

bool appendStructuredFields(QDomElement &element,const XObject *object,
                            QString &error)
{
    if(auto value=dynamic_cast<const XPoint2D*>(object))
    {
        element.setAttribute("x",realText(value->x()));
        element.setAttribute("y",realText(value->y()));
        return true;
    }
    if(auto value=dynamic_cast<const XRotateRectRoi*>(object))
    {
        element.setAttribute("centerX",realText(value->centerX()));
        element.setAttribute("centerY",realText(value->centerY()));
        element.setAttribute("length1",realText(value->length1()));
        element.setAttribute("length2",realText(value->length2()));
        element.setAttribute("angle",realText(value->angle()));
        return true;
    }
    if(auto value=dynamic_cast<const XMatchResult*>(object))
    {
        element.setAttribute("x",realText(value->x()));
        element.setAttribute("y",realText(value->y()));
        element.setAttribute("angle",realText(value->angle()));
        element.setAttribute("score",realText(value->score()));
        return true;
    }
    if(auto value=dynamic_cast<const XDetectionResult*>(object))
    {
        element.setAttribute("x",realText(value->x()));
        element.setAttribute("y",realText(value->y()));
        element.setAttribute("width",realText(value->width()));
        element.setAttribute("height",realText(value->height()));
        element.setAttribute("classId",QString::number(value->classId()));
        element.setAttribute("className",value->className());
        element.setAttribute("score",realText(value->score()));
        return true;
    }
    if(auto value=dynamic_cast<const XLine2D*>(object))
    {
        element.setAttribute("x1",realText(value->start().x()));
        element.setAttribute("y1",realText(value->start().y()));
        element.setAttribute("x2",realText(value->end().x()));
        element.setAttribute("y2",realText(value->end().y()));
        return true;
    }
    if(auto value=dynamic_cast<const XCircle2D*>(object))
    {
        element.setAttribute("centerX",realText(value->centerX()));
        element.setAttribute("centerY",realText(value->centerY()));
        element.setAttribute("radius",realText(value->radius()));
        return true;
    }
    if(auto value=dynamic_cast<const XRect2D*>(object))
    {
        element.setAttribute("x",realText(value->x()));
        element.setAttribute("y",realText(value->y()));
        element.setAttribute("width",realText(value->width()));
        element.setAttribute("height",realText(value->height()));
        return true;
    }
    if(auto value=dynamic_cast<const XKeyPoint*>(object))
    {
        element.setAttribute("x",realText(value->x()));
        element.setAttribute("y",realText(value->y()));
        element.setAttribute("size",realText(value->size()));
        element.setAttribute("angle",realText(value->angle()));
        element.setAttribute("response",realText(value->response()));
        element.setAttribute("octave",QString::number(value->octave()));
        element.setAttribute("classId",QString::number(value->classId()));
        return true;
    }
    if(auto value=dynamic_cast<const XMeasurementResult*>(object))
    {
        element.setAttribute("value",realText(value->value()));
        element.setAttribute("unit",value->unit());
        element.setAttribute("lower",realText(value->lower()));
        element.setAttribute("upper",realText(value->upper()));
        element.setAttribute("passed",value->passed()?"true":"false");
        return true;
    }
    if(auto value=dynamic_cast<const XClassificationResult*>(object))
    {
        element.setAttribute("classId",QString::number(value->classId()));
        element.setAttribute("className",value->className());
        element.setAttribute("score",realText(value->score()));
        return true;
    }
    error="unsupported structured value object";
    return false;
}

bool readStructuredFields(const QDomElement &element,XObject *object,
                          bool named,QString &error)
{
    if(!object)
    {
        error="target structured value is null";
        return false;
    }
    const QString nameAttribute=named?"name":QString();
    QStringList commonAllowed={"type"};
    QStringList commonRequired={"type"};
    if(named)
    {
        commonAllowed.prepend(nameAttribute);
        commonRequired.prepend(nameAttribute);
    }
    if(element.attribute("type")!=object->typeName())
    {
        error=QString("stored type '%1' does not match '%2'")
                .arg(element.attribute("type"),object->typeName());
        return false;
    }
    if(!validateChildren(element,{}, {},error) || hasDirectText(element))
    {
        if(error.isEmpty()) error=QString("%1 contains unexpected text").arg(elementLabel(element));
        return false;
    }

    if(auto value=dynamic_cast<XPoint2D*>(object))
    {
        QStringList allowed=commonAllowed+QStringList{"x","y"};
        QStringList required=commonRequired+QStringList{"x","y"};
        double x=0.0;
        double y=0.0;
        if(!validateAttributes(element,allowed,required,error)
                || !realAttribute(element,"x",x,error)
                || !realAttribute(element,"y",y,error)
                || !value->setValue(x,y))
        {
            if(error.isEmpty()) error="invalid point value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XRotateRectRoi*>(object))
    {
        QStringList fields={"centerX","centerY","length1","length2","angle"};
        double centerX=0.0;
        double centerY=0.0;
        double length1=0.0;
        double length2=0.0;
        double angle=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"centerX",centerX,error)
                || !realAttribute(element,"centerY",centerY,error)
                || !realAttribute(element,"length1",length1,error)
                || !realAttribute(element,"length2",length2,error)
                || !realAttribute(element,"angle",angle,error)
                || !value->setValue(centerX,centerY,length1,length2,angle))
        {
            if(error.isEmpty()) error="invalid rotate-rectangle value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XMatchResult*>(object))
    {
        QStringList fields={"x","y","angle","score"};
        double x=0.0;
        double y=0.0;
        double angle=0.0;
        double score=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"x",x,error)
                || !realAttribute(element,"y",y,error)
                || !realAttribute(element,"angle",angle,error)
                || !realAttribute(element,"score",score,error)
                || !value->setValue(x,y,angle,score))
        {
            if(error.isEmpty()) error="invalid match-result value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XDetectionResult*>(object))
    {
        QStringList fields={"x","y","width","height","classId",
                            "className","score"};
        double x=0.0;
        double y=0.0;
        double width=0.0;
        double height=0.0;
        double score=0.0;
        const QString classIdText=element.attribute("classId");
        int classId=0;
        const QString className=element.attribute("className");
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"x",x,error)
                || !realAttribute(element,"y",y,error)
                || !realAttribute(element,"width",width,error)
                || !realAttribute(element,"height",height,error)
                || !parseCanonicalInt(classIdText,classId) || classId<0
                || className.isEmpty() || className!=className.trimmed()
                || !realAttribute(element,"score",score,error)
                || !value->setValue(x,y,width,height,classId,className,score))
        {
            if(error.isEmpty()) error="invalid detection-result value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XLine2D*>(object))
    {
        QStringList fields={"x1","y1","x2","y2"};
        double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"x1",x1,error)
                || !realAttribute(element,"y1",y1,error)
                || !realAttribute(element,"x2",x2,error)
                || !realAttribute(element,"y2",y2,error)
                || !value->setValue(QPointF(x1,y1),QPointF(x2,y2)))
        {
            if(error.isEmpty()) error="invalid line value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XCircle2D*>(object))
    {
        QStringList fields={"centerX","centerY","radius"};
        double centerX=0.0,centerY=0.0,radius=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"centerX",centerX,error)
                || !realAttribute(element,"centerY",centerY,error)
                || !realAttribute(element,"radius",radius,error)
                || !value->setValue(centerX,centerY,radius))
        {
            if(error.isEmpty()) error="invalid circle value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XRect2D*>(object))
    {
        QStringList fields={"x","y","width","height"};
        double x=0.0,y=0.0,width=0.0,height=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"x",x,error)
                || !realAttribute(element,"y",y,error)
                || !realAttribute(element,"width",width,error)
                || !realAttribute(element,"height",height,error)
                || !value->setValue(x,y,width,height))
        {
            if(error.isEmpty()) error="invalid rectangle value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XKeyPoint*>(object))
    {
        QStringList fields={"x","y","size","angle","response","octave","classId"};
        double x=0.0,y=0.0,size=0.0,angle=0.0,response=0.0;
        int octave=0,classId=0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"x",x,error)
                || !realAttribute(element,"y",y,error)
                || !realAttribute(element,"size",size,error)
                || !realAttribute(element,"angle",angle,error)
                || !realAttribute(element,"response",response,error)
                || !parseCanonicalInt(element.attribute("octave"),octave) || octave<0
                || !parseCanonicalInt(element.attribute("classId"),classId) || classId<-1
                || !value->setValue(x,y,size,angle,response,octave,classId))
        {
            if(error.isEmpty()) error="invalid keypoint value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XMeasurementResult*>(object))
    {
        QStringList fields={"value","unit","lower","upper","passed"};
        double measurement=0.0,lower=0.0,upper=0.0;
        bool passed=false;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !realAttribute(element,"value",measurement,error)
                || !realAttribute(element,"lower",lower,error)
                || !realAttribute(element,"upper",upper,error)
                || !boolAttribute(element,"passed",passed,error)
                || !value->setValue(measurement,element.attribute("unit"),lower,upper,passed))
        {
            if(error.isEmpty()) error="invalid measurement value";
            return false;
        }
        return true;
    }
    if(auto value=dynamic_cast<XClassificationResult*>(object))
    {
        QStringList fields={"classId","className","score"};
        int classId=0;
        double score=0.0;
        if(!validateAttributes(element,commonAllowed+fields,commonRequired+fields,error)
                || !parseCanonicalInt(element.attribute("classId"),classId) || classId<0
                || element.attribute("className").trimmed().isEmpty()
                || element.attribute("className")!=element.attribute("className").trimmed()
                || !realAttribute(element,"score",score,error)
                || !value->setValue(classId,element.attribute("className"),score))
        {
            if(error.isEmpty()) error="invalid classification value";
            return false;
        }
        return true;
    }
    error="target object is not a supported structured value";
    return false;
}

XObject *createStructuredValue(const QString &type)
{
    if(type==XPoint2DType) return new XPoint2D();
    if(type==XRotateRectRoiType) return new XRotateRectRoi();
    if(type==XMatchResultType) return new XMatchResult();
    if(type==XDetectionResultType) return new XDetectionResult();
    if(type==XLine2D::type()) return new XLine2D();
    if(type==XCircle2D::type()) return new XCircle2D();
    if(type==XRect2D::type()) return new XRect2D();
    if(type==XKeyPoint::type()) return new XKeyPoint();
    if(type==XMeasurementResult::type()) return new XMeasurementResult();
    if(type==XClassificationResult::type()) return new XClassificationResult();
    return nullptr;
}
}

bool isValidId(const QString &id)
{
    static const QRegularExpression expression("^[0-9A-Fa-f]{32}$");
    return expression.match(id).hasMatch();
}

bool validateAttributes(const QDomElement &element,const QStringList &allowed,
                        const QStringList &required,QString &error)
{
    for(const QString &name:required)
    {
        if(!element.hasAttribute(name))
        {
            error=QString("%1 missing required attribute '%2'")
                    .arg(elementLabel(element),name);
            return false;
        }
    }

    const QDomNamedNodeMap attributes=element.attributes();
    for(int index=0;index<attributes.count();++index)
    {
        const QString name=attributes.item(index).nodeName();
        if(!allowed.contains(name))
        {
            error=QString("%1 contains unsupported attribute '%2'")
                    .arg(elementLabel(element),name);
            return false;
        }
    }
    return true;
}

bool validateChildren(const QDomElement &element,const QStringList &allowed,
                      const QStringList &requiredSingle,QString &error)
{
    QMap<QString,int> counts;
    for(QDomElement child=element.firstChildElement();!child.isNull();
        child=child.nextSiblingElement())
    {
        const QString tagName=child.tagName();
        if(!allowed.contains(tagName))
        {
            error=QString("%1 contains unsupported child <%2>")
                    .arg(elementLabel(element),tagName);
            return false;
        }
        counts[tagName]++;
    }

    for(const QString &tagName:requiredSingle)
    {
        if(counts.value(tagName)!=1)
        {
            error=QString("%1 requires exactly one <%2> child")
                    .arg(elementLabel(element),tagName);
            return false;
        }
    }
    return true;
}

bool requiredAttribute(const QDomElement &element,const QString &name,
                       QString &value,QString &error)
{
    if(!element.hasAttribute(name))
    {
        error=QString("%1 missing required attribute '%2'")
                .arg(elementLabel(element),name);
        return false;
    }
    value=element.attribute(name);
    return true;
}

bool unsignedAttribute(const QDomElement &element,const QString &name,
                       unsigned int &value,QString &error)
{
    QString text;
    if(!requiredAttribute(element,name,text,error)) return false;
    bool ok=false;
    const uint parsed=text.toUInt(&ok,10);
    if(!ok)
    {
        error=QString("%1 attribute '%2' is not an unsigned integer")
                .arg(elementLabel(element),name);
        return false;
    }
    value=parsed;
    return true;
}

bool boolAttribute(const QDomElement &element,const QString &name,
                   bool &value,QString &error)
{
    QString text;
    if(!requiredAttribute(element,name,text,error)) return false;
    if(!parseBool(text,value))
    {
        error=QString("%1 attribute '%2' must be 'true' or 'false'")
                .arg(elementLabel(element),name);
        return false;
    }
    return true;
}

bool realAttribute(const QDomElement &element,const QString &name,
                   double &value,QString &error)
{
    QString text;
    if(!requiredAttribute(element,name,text,error)) return false;
    bool ok=false;
    const double parsed=QLocale::c().toDouble(text,&ok);
    if(!ok || !qIsFinite(parsed))
    {
        error=QString("%1 attribute '%2' is not a finite number")
                .arg(elementLabel(element),name);
        return false;
    }
    value=parsed;
    return true;
}

QDomElement singleChild(const QDomElement &element,const QString &tagName)
{
    return element.firstChildElement(tagName);
}

bool isSupportedScalar(const XObject *object)
{
    return dynamic_cast<const XBool*>(object)
            || dynamic_cast<const XInt*>(object)
            || dynamic_cast<const XReal*>(object)
            || dynamic_cast<const XString*>(object);
}

bool appendScalar(QDomDocument &doc,QDomElement &parent,const QString &tagName,
                  const XObject *object,QString &error)
{
    if(!object || !isSupportedScalar(object))
    {
        error="unsupported scalar object";
        return false;
    }
    QDomElement element=doc.createElement(tagName);
    element.setAttribute("name",object->objectName());
    element.setAttribute("type",const_cast<XObject*>(object)->typeName());
    element.appendChild(doc.createTextNode(scalarText(object)));
    parent.appendChild(element);
    return true;
}

bool readScalar(const QDomElement &element,XObject *object,QString &error)
{
    if(!object || !isSupportedScalar(object))
    {
        error="target object is not a supported scalar";
        return false;
    }
    if(!validateAttributes(element,{"name","type"},{"name","type"},error)
            || !validateChildren(element,{}, {},error))
    {
        return false;
    }
    const QString storedType=element.attribute("type");
    if(storedType!=object->typeName())
    {
        error=QString("value '%1' type '%2' does not match '%3'")
                .arg(element.attribute("name"),storedType,object->typeName());
        return false;
    }

    const QString text=element.text();
    if(auto value=dynamic_cast<XBool*>(object))
    {
        bool parsed=false;
        if(!parseBool(text,parsed))
        {
            error=QString("value '%1' is not a Boolean")
                    .arg(element.attribute("name"));
            return false;
        }
        value->setValue(parsed);
        return true;
    }
    if(auto value=dynamic_cast<XInt*>(object))
    {
        bool ok=false;
        const int parsed=text.toInt(&ok,10);
        if(!ok)
        {
            error=QString("value '%1' is not an integer")
                    .arg(element.attribute("name"));
            return false;
        }
        value->setValue(parsed);
        return true;
    }
    if(auto value=dynamic_cast<XReal*>(object))
    {
        bool ok=false;
        const double parsed=QLocale::c().toDouble(text,&ok);
        if(!ok || !qIsFinite(parsed))
        {
            error=QString("value '%1' is not a finite real number")
                    .arg(element.attribute("name"));
            return false;
        }
        value->setValue(parsed);
        return true;
    }
    if(auto value=dynamic_cast<XString*>(object))
    {
        value->setValue(text);
        return true;
    }
    return false;
}

bool scalarTypesCompatible(const XObject *parameter,const XObject *result)
{
    if(!parameter || !result) return false;
    const QString parameterType=const_cast<XObject*>(parameter)->typeName();
    const QString resultType=const_cast<XObject*>(result)->typeName();
    if(parameterType==resultType) return true;
    return (parameterType==XIntType && resultType==XRealType)
            || (parameterType==XRealType && resultType==XIntType);
}

bool isSupportedValue(const XObject *object)
{
    if(!object) return false;
    if(isSupportedScalar(object)
            || dynamic_cast<const XPoint2D*>(object)
            || dynamic_cast<const XRotateRectRoi*>(object)
            || dynamic_cast<const XMatchResult*>(object)
            || dynamic_cast<const XDetectionResult*>(object)
            || dynamic_cast<const XLine2D*>(object)
            || dynamic_cast<const XCircle2D*>(object)
            || dynamic_cast<const XRect2D*>(object)
            || dynamic_cast<const XKeyPoint*>(object)
            || dynamic_cast<const XMeasurementResult*>(object)
            || dynamic_cast<const XClassificationResult*>(object))
    {
        return true;
    }
    auto list=dynamic_cast<const XObjectList*>(object);
    return list && isStructuredValueType(list->valueType());
}

bool appendValue(QDomDocument &doc,QDomElement &parent,const QString &tagName,
                 const XObject *object,QString &error)
{
    if(isSupportedScalar(object))
    {
        return appendScalar(doc,parent,tagName,object,error);
    }
    if(!object || !isSupportedValue(object))
    {
        error="unsupported persistent value object";
        return false;
    }

    QDomElement element=doc.createElement(tagName);
    element.setAttribute("name",object->objectName());
    element.setAttribute("type",const_cast<XObject*>(object)->typeName());
    if(auto list=dynamic_cast<const XObjectList*>(object))
    {
        element.setAttribute("valueType",list->valueType());
        for(XObject *value:list->values())
        {
            if(!value || value->typeName()!=list->valueType())
            {
                error=QString("list '%1' contains an incompatible value")
                        .arg(object->objectName());
                return false;
            }
            QDomElement item=doc.createElement("Item");
            item.setAttribute("type",value->typeName());
            if(!appendStructuredFields(item,value,error)) return false;
            element.appendChild(item);
        }
    }
    else if(!appendStructuredFields(element,object,error))
    {
        return false;
    }
    parent.appendChild(element);
    return true;
}

bool readValue(const QDomElement &element,XObject *object,QString &error)
{
    if(isSupportedScalar(object)) return readScalar(element,object,error);
    if(!object || !isSupportedValue(object))
    {
        error="target object is not a supported persistent value";
        return false;
    }
    if(auto list=dynamic_cast<XObjectList*>(object))
    {
        if(!validateAttributes(element,{"name","type","valueType"},
                               {"name","type","valueType"},error)
                || !validateChildren(element,{"Item"}, {},error)
                || element.attribute("type")!=XObjectListType
                || element.attribute("valueType")!=list->valueType()
                || hasDirectText(element))
        {
            if(error.isEmpty()) error=QString("list value '%1' has an incompatible type")
                    .arg(element.attribute("name"));
            return false;
        }

        XObjectList candidate("",list->valueType());
        for(QDomElement item=element.firstChildElement("Item");!item.isNull();
            item=item.nextSiblingElement("Item"))
        {
            if(item.attribute("type")!=list->valueType())
            {
                error=QString("list value '%1' contains item type '%2', expected '%3'")
                        .arg(element.attribute("name"),item.attribute("type"),
                             list->valueType());
                return false;
            }
            XObject *value=createStructuredValue(list->valueType());
            if(!value || !readStructuredFields(item,value,false,error)
                    || !candidate.addValue(value))
            {
                delete value;
                if(error.isEmpty()) error="failed to construct list item";
                return false;
            }
        }
        if(!list->setData(&candidate))
        {
            error=QString("failed to replace list value '%1'").arg(element.attribute("name"));
            return false;
        }
        return true;
    }
    return readStructuredFields(element,object,true,error);
}

bool valueTypesCompatible(const XObject *parameter,const XObject *result)
{
    if(!parameter || !result) return false;
    auto parameterList=dynamic_cast<const XObjectList*>(parameter);
    auto resultList=dynamic_cast<const XObjectList*>(result);
    if(parameterList || resultList)
    {
        return parameterList && resultList
                && parameterList->valueType()==resultList->valueType();
    }
    return scalarTypesCompatible(parameter,result);
}

bool appendProperty(QDomDocument &doc,QDomElement &parent,QObject *object,
                    const QString &name,QString &error)
{
    if(!object)
    {
        error="property owner is null";
        return false;
    }
    const QMetaObject *metaObject=object->metaObject();
    const int propertyIndex=metaObject->indexOfProperty(name.toUtf8().constData());
    if(propertyIndex<0)
    {
        error=QString("property '%1' does not exist on %2")
                .arg(name,metaObject->className());
        return false;
    }
    const QMetaProperty property=metaObject->property(propertyIndex);
    if(!property.isReadable() || !property.isWritable())
    {
        error=QString("property '%1' on %2 is not readable and writable")
                .arg(name,metaObject->className());
        return false;
    }
    const QString typeName=propertyTypeName(property);
    if(typeName.isEmpty())
    {
        error=QString("property '%1' on %2 has an unsupported type")
                .arg(name,metaObject->className());
        return false;
    }
    const QVariant value=property.read(object);
    if(!value.isValid())
    {
        error=QString("property '%1' on %2 cannot be read")
                .arg(name,metaObject->className());
        return false;
    }

    QDomElement element=doc.createElement("Property");
    element.setAttribute("name",name);
    element.setAttribute("type",typeName);
    QString text;
    if(property.isEnumType() || typeName=="int") text=QString::number(value.toInt());
    else if(typeName=="bool") text=value.toBool()?"true":"false";
    else if(typeName=="double") text=QString::number(value.toDouble(),'g',17);
    else text=value.toString();
    element.appendChild(doc.createTextNode(text));
    parent.appendChild(element);
    return true;
}

bool readProperty(const QDomElement &element,QObject *object,
                  const QString &expectedName,QString &error)
{
    if(!object
            || !validateAttributes(element,{"name","type"},{"name","type"},error)
            || !validateChildren(element,{}, {},error))
    {
        if(!object) error="property owner is null";
        return false;
    }
    if(element.attribute("name")!=expectedName)
    {
        error=QString("unexpected property '%1', expected '%2'")
                .arg(element.attribute("name"),expectedName);
        return false;
    }
    const QMetaObject *metaObject=object->metaObject();
    const int propertyIndex=metaObject->indexOfProperty(expectedName.toUtf8().constData());
    if(propertyIndex<0)
    {
        error=QString("property '%1' does not exist on %2")
                .arg(expectedName,metaObject->className());
        return false;
    }
    const QMetaProperty property=metaObject->property(propertyIndex);
    const QString typeName=propertyTypeName(property);
    if(!property.isWritable() || typeName.isEmpty()
            || element.attribute("type")!=typeName)
    {
        error=QString("property '%1' on %2 has an incompatible stored type")
                .arg(expectedName,metaObject->className());
        return false;
    }

    const QString text=element.text();
    QVariant value;
    if(property.isEnumType())
    {
        bool ok=false;
        const int parsed=text.toInt(&ok,10);
        if(!ok || !property.enumerator().valueToKey(parsed))
        {
            error=QString("property '%1' has an invalid enum value")
                    .arg(expectedName);
            return false;
        }
        value=parsed;
    }
    else if(typeName=="int")
    {
        bool ok=false;
        const int parsed=text.toInt(&ok,10);
        if(!ok)
        {
            error=QString("property '%1' is not an integer").arg(expectedName);
            return false;
        }
        value=parsed;
    }
    else if(typeName=="bool")
    {
        bool parsed=false;
        if(!parseBool(text,parsed))
        {
            error=QString("property '%1' is not a Boolean").arg(expectedName);
            return false;
        }
        value=parsed;
    }
    else if(typeName=="double")
    {
        bool ok=false;
        const double parsed=QLocale::c().toDouble(text,&ok);
        if(!ok || !qIsFinite(parsed))
        {
            error=QString("property '%1' is not a finite number").arg(expectedName);
            return false;
        }
        value=parsed;
    }
    else
    {
        value=text;
    }

    if(!property.write(object,value))
    {
        error=QString("property '%1' on %2 cannot be written")
                .arg(expectedName,metaObject->className());
        return false;
    }
    return true;
}

}
}
