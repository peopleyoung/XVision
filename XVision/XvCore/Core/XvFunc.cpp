#include "XvFunc.h"
#include <QUuid>
#include <QElapsedTimer>
#include <QSet>
#include <QPixmap>

#include "XvFuncAssembly.h"
#include "XvFlow.h"
#include "XObject.h"
#include "XObjectSet.h"
#include "XLogger.h"
#include "XvXmlUtils.h"

using namespace XvCore;

namespace
{
bool structureEditAllowed(const XvFunc *function)
{
    XvFlow *flow=function?function->parFlow():nullptr;
    return !flow || flow->isEditAllowed();
}
}

XvFunc::XvFunc(QObject *parent)
    :QObject{parent},
      _funcId(QUuid::createUuid().toString(QUuid::StringFormat::Id128)),
      _funcRole(""),
      _funcType(EXvFuncType::Null),
      _funcName("")
{
    registerTokenMsgAble();
}

XvFunc::~XvFunc()
{
    unRegisterTokenMsgAble();
    emit sgXvFuncDestroyed(this);
}

QString XvFunc::lastErrorMsg()
{
    const QString message=_lastErrorMsg;
    _lastErrorMsg.clear();
    return message;
}

void XvFunc::setLastErrorMsg(const QString &msg)
{
    _lastErrorMsg=msg;
    Log_Trace(QString("<%1>[%2]:%3").arg(funcId(),funcName(),msg));
}

bool XvFunc::appendPersistentData(QDomDocument &,QDomElement &,QString &)
const
{
    return true;
}

bool XvFunc::readPersistentData(const QDomElement &dataElement,QString &error)
{
    if(dataElement.isNull()) return true;
    error=QString("operator role '%1' does not support <PersistentData>").arg(funcRole());
    return false;
}

/**********************字段/属性定义**********************/
void XvFunc::setFuncName(const QString &name)
{
    if(_funcName!=name)
    {
        _funcName=name;
        emit funcNameChanged(_funcName);
    }

}

QPixmap XvFunc::funcIcon()
{
    return QPixmap(":/image/XvFuncIcon.svg");
}

void XvFunc::setParFlow(XvFlow *flow)
{
    m_parFlow=flow;
}


/**********************算子连接定义**********************/

QList<XvFunc *> XvFunc::fatherFuncs() const
{
    return _lstFatherFunc;
}

QList<XvFunc *> XvFunc::sonFuncs() const
{
    return _lstSonFunc;
}

QStringList XvFunc::outputPorts() const
{
    return {"default"};
}

QString XvFunc::defaultOutputPortForNewLink() const
{
    const QStringList ports=outputPorts();
    return ports.contains("default")?QString("default"):ports.value(0);
}

bool XvFunc::isOutputPortDeclared(const QString &port) const
{
    if(port.isEmpty() || port!=port.trimmed()) return false;
    return outputPorts().count(port)==1;
}

bool XvFunc::addSonFunc(XvFunc *sonFunc)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改算子连接");
        return false;
    }
    const QString port=defaultOutputPortForNewLink();
    if(sonFunc==nullptr || !isOutputPortDeclared(port)) return false;
    if(this->parFlow()!=sonFunc->parFlow())//非同流程算子无法连接
    {
        return false;
    }
    if(sonFunc==this && !canConnectSelf()) return false;
    if(!_lstSonFunc.contains(sonFunc))
    {
        _lstSonFunc.append(sonFunc);
        _mapSonFuncPort.insert(sonFunc,port);
        connect(sonFunc,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onSonFuncDestroyed);
        if(!sonFunc->existFatherFunc(this))//子连接算子不存在本算子父连接算子,进行添加
        {
            if(!sonFunc->addFatherFunc(this))
            {
                disconnect(sonFunc,&XvFunc::sgXvFuncDestroyed,
                           this,&XvFunc::onSonFuncDestroyed);
                _mapSonFuncPort.remove(sonFunc);
                _lstSonFunc.removeOne(sonFunc);
                return false;
            }
        }
        emit this->sgSonFuncAdd(sonFunc);
        emit this->sgSonFuncChanged(this);
        return true;
    }
    else
    {
        return false;
    }
}

bool XvFunc::addSonFunc(XvFunc *sonFunc,const QString &port)
{
    if(!isOutputPortDeclared(port))
    {
        setLastErrorMsg(QString("输出端口[%1]未声明").arg(port));
        return false;
    }
    if(!addSonFunc(sonFunc)) return false;
    if(sonFuncPort(sonFunc)==port) return true;
    if(setSonFuncPort(sonFunc,port)) return true;
    delSonFunc(sonFunc);
    return false;
}

bool XvFunc::setSonFuncPort(XvFunc *sonFunc,const QString &port)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改算子连接");
        return false;
    }
    if(!_lstSonFunc.contains(sonFunc) || !_mapSonFuncPort.contains(sonFunc))
        return false;
    if(!isOutputPortDeclared(port))
    {
        setLastErrorMsg(QString("输出端口[%1]未声明").arg(port));
        return false;
    }
    if(_mapSonFuncPort.value(sonFunc)==port) return true;
    _mapSonFuncPort.insert(sonFunc,port);
    emit sgSonFuncPortChanged(sonFunc,port);
    emit sgSonFuncChanged(this);
    return true;
}

QString XvFunc::sonFuncPort(XvFunc *sonFunc) const
{
    if(!_lstSonFunc.contains(sonFunc)) return QString();
    return _mapSonFuncPort.value(sonFunc);
}

bool XvFunc::delSonFunc(XvFunc *sonFunc)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改算子连接");
        return false;
    }
    if(_lstSonFunc.contains(sonFunc))
    {
        const QString previousPort=_mapSonFuncPort.value(sonFunc);
        bool bRet= _lstSonFunc.removeOne(sonFunc);
        if(bRet)
        {
            _mapSonFuncPort.remove(sonFunc);
            if(sonFunc->existFatherFunc(this))//子连接算子存在本算子父连接算子,进行删除
            {
                if(!sonFunc->delFatherFunc(this))
                {
                    _lstSonFunc.append(sonFunc);
                    _mapSonFuncPort.insert(sonFunc,previousPort);
                    return false;
                }
            }
            disconnect(sonFunc,&XvFunc::sgXvFuncDestroyed,
                       this,&XvFunc::onSonFuncDestroyed);
            emit this->sgSonFuncDel(sonFunc);
            emit this->sgSonFuncChanged(this);
        }
        return bRet;
    }
    else
    {
        return false;
    }
}

bool XvFunc::existSonFunc(XvFunc *sonFunc)
{
    return _lstSonFunc.contains(sonFunc);
}

bool XvFunc::addFatherFunc(XvFunc *fatherFunc)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改算子连接");
        return false;
    }
    if(fatherFunc==nullptr) return false;
    if(this->parFlow()!=fatherFunc->parFlow())//非同流程算子无法连接
    {
        return false;
    }
    if(!_lstFatherFunc.contains(fatherFunc))
    {
        _lstFatherFunc.append(fatherFunc);
        connect(fatherFunc,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onFatherFuncDestroyed);
        if(!fatherFunc->existSonFunc(this))//父连接算子不存在本算子子连接算子,进行添加
        {
            if(!fatherFunc->addSonFunc(this))
            {
                disconnect(fatherFunc,&XvFunc::sgXvFuncDestroyed,
                           this,&XvFunc::onFatherFuncDestroyed);
                _lstFatherFunc.removeOne(fatherFunc);
                return false;
            }
        }
        emit this->sgFatherFuncAdd(fatherFunc);
        emit this->sgFatherFuncChanged(this);
        return true;
    }
    else
    {
        return false;
    }
}

bool XvFunc::delFatherFunc(XvFunc *fatherFunc)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改算子连接");
        return false;
    }
    if(_lstFatherFunc.contains(fatherFunc))
    {
        bool bRet= _lstFatherFunc.removeOne(fatherFunc);
        if(bRet)
        {
            if(fatherFunc->existSonFunc(this))//父连接算子不存在本算子子连接算子,进行删除
            {
                if(!fatherFunc->delSonFunc(this))
                {
                    _lstFatherFunc.append(fatherFunc);
                    return false;
                }
            }
            disconnect(fatherFunc,&XvFunc::sgXvFuncDestroyed,
                       this,&XvFunc::onFatherFuncDestroyed);
            emit this->sgFatherFuncDel(fatherFunc);
            emit this->sgFatherFuncChanged(this);
        }
        return bRet;
    }
    else
    {
        return false;
    }
}

bool XvFunc::existFatherFunc(XvFunc *fatherFunc)
{
    return _lstFatherFunc.contains(fatherFunc);
}

bool XvFunc::canConnectSelf()
{
    return false;
}

bool XvFunc::existAncestorFunc(XvFunc *ancestorFunc)
{
    QList<XvFunc*> lst;
    getAncestorFuncs(lst);
    return lst.contains(ancestorFunc);
}

bool XvFunc::existDescendantFunc(XvFunc *descendantFunc)
{
    QList<XvFunc*> lst;
    getDescendantFuncs(lst);
    return lst.contains(descendantFunc);
}

void XvFunc::getAncestorFuncs(QList<XvFunc *> &lst) const
{
    foreach (auto father, _lstFatherFunc)
    {
        if(!lst.contains(father))
        {
            lst.append(father);            
            father->getAncestorFuncs(lst);
        }
    }
}

void XvFunc::getDescendantFuncs(QList<XvFunc *> &lst) const
{
    foreach (auto son, _lstSonFunc)
    {
        if(!lst.contains(son))
        {
            lst.append(son);
            son->getDescendantFuncs(lst);
        }
    }
}




void XvFunc::onFatherFuncDestroyed(XvFunc* func)
{
    if(func)
    {
        delFatherFunc(func);
    }
}

void XvFunc::onSonFuncDestroyed(XvFunc* func)
{
    if(func)
    {
        delSonFunc(func);
    }
}

void XvFunc::onXvFlowLinkRefresh()
{
    onRefreshParamSubscribe();//刷新连接绑定
}

/**********************算子参数结果定义**********************/
bool XvFunc::updataParam(const QString &paramName, XObject *object)
{
    if(!object) return false;
    auto paramObj=getParamsByName(paramName);    
    if(!paramObj)
    {
        return false;
    }
    return paramObj->setData(object);
}

XObject *XvFunc::getParamsByName(const QString &objectName)
{
    auto param=getParam();
    if(param)
    {
        return param->childObject(objectName);
    }
    return nullptr;
}

XObject *XvFunc::getResultsByName(const QString &objectName)
{
    auto result=getResult();
    if(result)
    {
        return result->childObject(objectName);
    }
    return nullptr;
}

QList<XObject *> XvFunc::getParamsByType(const QString &typeName)
{
    auto param=getParam();
    if(param)
    {
        return param->childObjects(typeName);
    }
    return QList<XObject*>();
}

QList<XObject *> XvFunc::getResultsByType(const QString &typeName)
{
    auto result=getResult();
    if(result)
    {
        return result->childObjects(typeName);
    }
    return QList<XObject*>();;
}

QMap<XvFunc*, QList<XObject*>> XvFunc::getAncestorsResultByType(const QString &typeName)
{
    QMap<XvFunc*, QList<XObject*>>  map;
    QList<XvFunc*> lstAncestors;
    this->getAncestorFuncs(lstAncestors);
    lstAncestors.removeOne(this);//移除自己
    foreach (auto anctor, lstAncestors)
    {
        auto results=anctor->getResultsByType(typeName);
        if(results.count()>0)
        {
            map.insert(anctor,results);
        }
    }
    return map;
}

bool XvFunc::getParamSubscribe(const QString &paramName, SubscribeInfo &subscribeInfo)
{
    if(!isParamSubscribe(paramName))
    {
        return false;
    }
    subscribeInfo=_mapParamSubscribe[paramName];
    return true;
}




bool XvFunc::paramSubscribe(const QString &paramName, XvFunc *target, const QString &resultName)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改参数订阅");
        return false;
    }
    if(!target) return false;
    if(!this->existAncestorFunc(target)) return false;
    auto paramObject=getParamsByName(paramName);
    if(!paramObject) return false;//不存在目标对象失败
    auto resultObject=target->getResultsByName(resultName);
    if(!resultObject) return false;//不存在结果对象失败
    if(!XvXml::valueTypesCompatible(paramObject,resultObject)) return false;
    paramUnSubscribe(paramName);
    if(!existParamSubscribe(target))
    {
        connect(target,&XvFunc::sgFuncRunEnd,this,&XvFunc::onSubscribeTargetRunEndUpdataParam,Qt::DirectConnection);
        connect(target,&XvFunc::sgFuncResultUpdate,this,&XvFunc::onSubscribeTargetRunEndUpdataParam,Qt::DirectConnection);
        connect(target,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onParamTargetDestroyed);
    }
    _mapParamSubscribe[paramName]=SubscribeInfo(target,resultName);
    return true;
}

bool XvFunc::paramUnSubscribe(const QString &paramName)
{
    if(!structureEditAllowed(this))
    {
        setLastErrorMsg("流程或项目正在运行，不能修改参数订阅");
        return false;
    }
    auto paramObject=getParamsByName(paramName);
    if(!paramObject) return false;//不存在目标对象失败
    if(!_mapParamSubscribe.contains(paramName))
    {
        return false;
    }
    auto kv=_mapParamSubscribe[paramName];
    auto target=kv.first;
    if(!target) return false;
    _mapParamSubscribe.remove(paramName);
    if(!existParamSubscribe(target))
    {
        disconnect(target,&XvFunc::sgFuncRunEnd,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
        disconnect(target,&XvFunc::sgFuncResultUpdate,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
        disconnect(target,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onParamTargetDestroyed);
    }
    return true;
}



bool XvFunc::existParamSubscribe(XvFunc *target)
{
    foreach (auto kv, _mapParamSubscribe)
    {
        if(kv.first==target)
        {
            return true;
        }
    }
    return false;
}

void XvFunc::onRefreshParamSubscribe()
{
    QList<XvFunc*> lstLoseLinkSubscriber;//丢失连接的订阅算子
    foreach (auto kv, _mapParamSubscribe)
    {
        if(!kv.first->existDescendantFunc(this))
        {
            lstLoseLinkSubscriber.append(kv.first);
        }
    }
    foreach (auto lostLinkSubscriber, lstLoseLinkSubscriber)
    {
         foreach (auto paramName, _mapParamSubscribe.keys())
         {
            auto kv=_mapParamSubscribe[paramName];
            if(kv.first==lostLinkSubscriber)
            {
                _mapParamSubscribe.remove(paramName);
            }
         }
         disconnect(lostLinkSubscriber,&XvFunc::sgFuncRunEnd,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
         disconnect(lostLinkSubscriber,&XvFunc::sgFuncResultUpdate,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
         disconnect(lostLinkSubscriber,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onParamTargetDestroyed);

    }

}

void XvFunc::onSubscribeTargetRunEndUpdataParam(XvFunc *target)
{
    foreach (auto paramName, _mapParamSubscribe.keys())
    {
        auto kv=_mapParamSubscribe[paramName];
        if(kv.first==target)
        {
            auto resultObject= target->getResultsByName(kv.second);
            this->updataParam(paramName,resultObject);
        }
    }
}



void XvFunc::onParamTargetDestroyed(XvFunc *target)
{
    if(!target)
    {
        return;
    }
    foreach (auto paramName, _mapParamSubscribe.keys())
    {
        auto kv=_mapParamSubscribe[paramName];
        if(kv.first==target)
        {
            _mapParamSubscribe.remove(paramName);
        }
    }
    if(!existParamSubscribe(target))
    {
      disconnect(target,&XvFunc::sgFuncRunEnd,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
      disconnect(target,&XvFunc::sgFuncResultUpdate,this,&XvFunc::onSubscribeTargetRunEndUpdataParam);
      disconnect(target,&XvFunc::sgXvFuncDestroyed,this,&XvFunc::onParamTargetDestroyed);
    }

}

/**********************算子运行操作及其状态更新**********************/
EXvFuncRunStatus XvFunc::runXvFunc()
{
    _runInfo.runIdx++;
    setRunStatus(EXvFuncRunStatus::Running);
    emit sgFuncRunStart(this);
    QElapsedTimer timer;
    timer.start();  
    auto runStatus=run();
    setRunStatus(runStatus);
    setRunElapsed((timer.nsecsElapsed()*1.0)/1000/1000);
    emit sgFuncRunEnd(this);
    return _runInfo.runStatus;
}

XvExecutionDirective XvFunc::executionDirective() const
{
    return XvExecutionDirective();
}

void XvFunc::publishResultUpdate()
{
    emit sgFuncResultUpdate(this);
}

bool XvFunc::prepareIteration(int,QString &error)
{
    error=QString("算子角色[%1]不支持循环迭代").arg(funcRole());
    return false;
}

void XvFunc::setRunElapsed(const double &time)
{
    _runInfo.runElapsed=time;
}

void XvFunc::setRunMsg(const QString &msg)
{
    _runInfo.runMsg=msg;
}

void XvFunc::setRunStatus(const EXvFuncRunStatus &status)
{
    _runInfo.runStatus=status;
}




bool XvFunc::release()
{
    if(getXvFuncRunStatus()==EXvFuncRunStatus::Running)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**********************XML序列化**********************/
QDomElement XvFunc::toXmlElement(QDomDocument &doc)
{
    QDomElement functionElement=doc.createElement("Function");
    functionElement.setAttribute("id",funcId());
    functionElement.setAttribute("role",funcRole());
    functionElement.setAttribute("name",funcName());
    functionElement.setAttribute("x",QString::number(canvasPosition().x(),'g',17));
    functionElement.setAttribute("y",QString::number(canvasPosition().y(),'g',17));

    QString error;
    QDomElement propertiesElement=doc.createElement("Properties");
    QSet<QString> propertyNames;
    for(const QString &name:persistentPropertyNames())
    {
        if(name.isEmpty() || propertyNames.contains(name)
                || !XvXml::appendProperty(doc,propertiesElement,this,name,error))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("duplicate or empty persistent property '%1'").arg(name)
                            :error);
            return QDomElement();
        }
        propertyNames.insert(name);
    }
    functionElement.appendChild(propertiesElement);

    QDomElement parametersElement=doc.createElement("Parameters");
    if(XvBaseParam *parameters=getParam())
    {
        for(XObject *parameter:parameters->childObjects())
        {
            if(isParamSubscribe(parameter->objectName())
                    || !XvXml::isSupportedValue(parameter))
            {
                continue;
            }
            if(!XvXml::appendValue(doc,parametersElement,"Value",parameter,error))
            {
                setLastErrorMsg(error);
                return QDomElement();
            }
        }
    }
    functionElement.appendChild(parametersElement);

    QDomElement resultsElement=doc.createElement("PersistentResults");
    QSet<QString> resultNames;
    for(const QString &name:persistentResultNames())
    {
        XObject *result=getResultsByName(name);
        if(name.isEmpty() || resultNames.contains(name) || !result
                || !XvXml::appendValue(doc,resultsElement,"Value",result,error))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("invalid persistent result '%1'").arg(name)
                            :error);
            return QDomElement();
        }
        resultNames.insert(name);
    }
    functionElement.appendChild(resultsElement);

    QDomElement subscriptionsElement=doc.createElement("Subscriptions");
    const QMap<QString,SubscribeInfo> subscriptions=getParamSubscribe();
    for(auto iterator=subscriptions.constBegin();iterator!=subscriptions.constEnd();++iterator)
    {
        if(!iterator.value().first)
        {
            setLastErrorMsg(QString("subscription '%1' has no source function")
                            .arg(iterator.key()));
            return QDomElement();
        }
        QDomElement subscriptionElement=doc.createElement("Subscription");
        subscriptionElement.setAttribute("parameter",iterator.key());
        subscriptionElement.setAttribute("sourceFunction",
                                         iterator.value().first->funcId());
        subscriptionElement.setAttribute("sourceResult",iterator.value().second);
        subscriptionsElement.appendChild(subscriptionElement);
    }
    functionElement.appendChild(subscriptionsElement);

    if(!appendPersistentData(doc,functionElement,error))
    {
        setLastErrorMsg(error.isEmpty() ? QString("invalid persistent operator data") : error);
        return QDomElement();
    }
    return functionElement;
}

bool XvFunc::fromXmlElement(QDomElement &xmlEle)
{
    QString error;
    if(xmlEle.tagName()!="Function"
            || !XvXml::validateAttributes(xmlEle,
                    {"id","role","name","x","y"},
                    {"id","role","name","x","y"},error)
            || !XvXml::validateChildren(xmlEle,
                    {"Properties","Parameters","PersistentResults","Subscriptions",
                     "PersistentData"},
                    {"Properties","Parameters","PersistentResults","Subscriptions"},error))
    {
        setLastErrorMsg(error.isEmpty()?"invalid <Function> element":error);
        return false;
    }
    if(xmlEle.attribute("id")!=funcId() || xmlEle.attribute("role")!=funcRole())
    {
        setLastErrorMsg("function ID or role does not match the constructed operator");
        return false;
    }
    double x=0.0;
    double y=0.0;
    if(!XvXml::realAttribute(xmlEle,"x",x,error)
            || !XvXml::realAttribute(xmlEle,"y",y,error))
    {
        setLastErrorMsg(error);
        return false;
    }

    QSet<QString> subscribedParameters;
    QDomElement subscriptionsElement=XvXml::singleChild(xmlEle,"Subscriptions");
    if(!XvXml::validateAttributes(subscriptionsElement,{}, {},error)
            || !XvXml::validateChildren(subscriptionsElement,{"Subscription"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    for(QDomElement subscription=subscriptionsElement.firstChildElement("Subscription");
        !subscription.isNull();subscription=subscription.nextSiblingElement("Subscription"))
    {
        if(!XvXml::validateAttributes(subscription,
                {"parameter","sourceFunction","sourceResult"},
                {"parameter","sourceFunction","sourceResult"},error)
                || !XvXml::validateChildren(subscription,{}, {},error))
        {
            setLastErrorMsg(error);
            return false;
        }
        const QString parameterName=subscription.attribute("parameter");
        if(parameterName.isEmpty() || subscribedParameters.contains(parameterName)
                || !getParamsByName(parameterName))
        {
            setLastErrorMsg(QString("invalid or duplicate subscribed parameter '%1'")
                            .arg(parameterName));
            return false;
        }
        subscribedParameters.insert(parameterName);
    }

    QDomElement propertiesElement=XvXml::singleChild(xmlEle,"Properties");
    if(!XvXml::validateAttributes(propertiesElement,{}, {},error)
            || !XvXml::validateChildren(propertiesElement,{"Property"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    QMap<QString,QDomElement> storedProperties;
    for(QDomElement property=propertiesElement.firstChildElement("Property");
        !property.isNull();property=property.nextSiblingElement("Property"))
    {
        const QString name=property.attribute("name");
        if(name.isEmpty() || storedProperties.contains(name))
        {
            setLastErrorMsg(QString("invalid or duplicate property '%1'").arg(name));
            return false;
        }
        storedProperties.insert(name,property);
    }
    const QStringList expectedProperties=persistentPropertyNames();
    QSet<QString> expectedPropertySet;
    for(const QString &name:expectedProperties) expectedPropertySet.insert(name);
    QSet<QString> storedPropertySet;
    for(const QString &name:storedProperties.keys()) storedPropertySet.insert(name);
    if(storedPropertySet!=expectedPropertySet)
    {
        setLastErrorMsg("stored operator properties do not match the registered role");
        return false;
    }
    for(const QString &name:expectedProperties)
    {
        QDomElement property=storedProperties.value(name);
        if(!XvXml::readProperty(property,this,name,error))
        {
            setLastErrorMsg(error);
            return false;
        }
    }

    QSet<QString> expectedParameters;
    if(XvBaseParam *parameters=getParam())
    {
        for(XObject *parameter:parameters->childObjects())
        {
            if(XvXml::isSupportedValue(parameter)
                    && !subscribedParameters.contains(parameter->objectName()))
            {
                expectedParameters.insert(parameter->objectName());
            }
        }
    }
    QSet<QString> optionalParameters;
    for(const QString &name:optionalPersistentParameterNames())
    {
        XObject *parameter=getParamsByName(name);
        if(name.isEmpty() || optionalParameters.contains(name)
                || !parameter || !XvXml::isSupportedValue(parameter))
        {
            setLastErrorMsg(QString("invalid optional persistent parameter '%1'").arg(name));
            return false;
        }
        optionalParameters.insert(name);
    }
    QDomElement parametersElement=XvXml::singleChild(xmlEle,"Parameters");
    if(!XvXml::validateAttributes(parametersElement,{}, {},error)
            || !XvXml::validateChildren(parametersElement,{"Value"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    QSet<QString> storedParameters;
    for(QDomElement value=parametersElement.firstChildElement("Value");
        !value.isNull();value=value.nextSiblingElement("Value"))
    {
        const QString name=value.attribute("name");
        XObject *parameter=getParamsByName(name);
        if(name.isEmpty() || storedParameters.contains(name)
                || !expectedParameters.contains(name) || !parameter
                || !XvXml::readValue(value,parameter,error))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("invalid direct parameter '%1'").arg(name)
                            :error);
            return false;
        }
        storedParameters.insert(name);
    }
    QSet<QString> requiredParameters=expectedParameters;
    for(const QString &name:optionalParameters) requiredParameters.remove(name);
    if(!(requiredParameters-storedParameters).isEmpty())
    {
        setLastErrorMsg("stored direct parameters do not match the registered role");
        return false;
    }

    QSet<QString> expectedResults;
    for(const QString &name:persistentResultNames()) expectedResults.insert(name);
    QDomElement resultsElement=XvXml::singleChild(xmlEle,"PersistentResults");
    if(!XvXml::validateAttributes(resultsElement,{}, {},error)
            || !XvXml::validateChildren(resultsElement,{"Value"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    QSet<QString> storedResults;
    for(QDomElement value=resultsElement.firstChildElement("Value");
        !value.isNull();value=value.nextSiblingElement("Value"))
    {
        const QString name=value.attribute("name");
        XObject *result=getResultsByName(name);
        if(name.isEmpty() || storedResults.contains(name)
                || !expectedResults.contains(name) || !result
                || !XvXml::readValue(value,result,error))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("invalid persistent result '%1'").arg(name)
                            :error);
            return false;
        }
        storedResults.insert(name);
    }
    if(storedResults!=expectedResults)
    {
        setLastErrorMsg("stored persistent results do not match the registered role");
        return false;
    }

    QDomElement persistentDataElement;
    for(QDomElement child=xmlEle.firstChildElement("PersistentData");
        !child.isNull();child=child.nextSiblingElement("PersistentData"))
    {
        if(!persistentDataElement.isNull())
        {
            setLastErrorMsg("duplicate <PersistentData> element");
            return false;
        }
        persistentDataElement=child;
    }
    if(!readPersistentData(persistentDataElement,error))
    {
        setLastErrorMsg(error.isEmpty() ? QString("invalid persistent operator data") : error);
        return false;
    }

    setFuncName(xmlEle.attribute("name"));
    setCanvasPosition(QPointF(x,y));
    return true;
}
