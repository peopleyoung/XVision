#include "XvFlow.h"

//Qt
#include <QUuid>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QThread>
//std
#include <algorithm>
#include <functional>
#include <limits>

//XvCore
#include "XvFunc.h"
#include "XvFuncAssembly.h"
#include "XvProject.h"
#include "XvCoreManager.h"
#include "XObject.h"
#include "XvXmlUtils.h"

#include "LangDef.h"

//XConcurrent
#include "XConcurrentManager.h"

using namespace XvCore;


#define XvFlowThreadGroup "XvCore::XvFlow"
/**************************************************************/
//* [XvFlowPrivate]
/**************************************************************/
namespace XvCore
{
class XvFlowPrivate
{
    Q_DISABLE_COPY(XvFlowPrivate)
    Q_DECLARE_PUBLIC(XvFlow)

public:
    XvFlowPrivate(XvFlow *q):q_ptr(q)
    {
        runThread=nullptr;
        flowConfig=new XvFlowConfig;
    };
    ~XvFlowPrivate(){ delete flowConfig; };
    ///父指针
    XvFlow              *const q_ptr;
    ///算子字典:QString-算子id  XvFunc-算子
    QMap<QString,XvFunc*>   mapFunc;
    ///运行线程
    QPointer<XThread>    runThread;
    ///流程配置
    XvFlowConfig        *flowConfig;
};
}
/**************************************************************/
//* [XvFlow]
/**************************************************************/

///获取开始节点
inline static QList<XvFunc*> getFlowGraphStartFunc(QMap<QString,XvFunc*> &map)
{
    QList<XvFunc*> lst;
    foreach (auto func, map)
    {
       if(func->fatherFuncs().count()==0)
       {
           lst.append(func);
       }

    }
    return lst;
}

XvFlow::XvFlow(XvProject *project,const QString &name,QObject *parent)
    : QObject{parent},m_parProject(project),
      _flowId(QUuid::createUuid().toString(QUuid::StringFormat::Id128)),
      _flowName(name),_running(false),
      d_ptr(new XvFlowPrivate(this))
{
    registerTokenMsgAble();
}

XvFlow::~XvFlow()
{
    unRegisterTokenMsgAble();
    release();
}

RetXv XvFlow::release()
{
    Q_D(XvFlow);
    if(isRunning())
    {
        stop();
        wait();
    }
    foreach (auto func, d->mapFunc)
    {
       if(func->release())
       {
           func->deleteLater();
       }
       else
       {
           delete func;
           func=nullptr;
       }
    }
    return true;
}

XvFlowConfig *XvFlow::getFlowConfig()
{
    Q_D(XvFlow);
    if(!d->flowConfig)
    {
        d->flowConfig=new XvFlowConfig;
    }
    return d->flowConfig;
}

void XvFlow::setFlowName(const QString &name)
{
   if(name!=_flowName)
   {
       _flowName=name;
       emit sgFlowNameChanged(this);
   }
}

QString XvFlow::lastErrorMsg()
{
    QString str=_lastErrorMsg;
    _lastErrorMsg="";
    return str;
}

void XvFlow::setParProject(XvProject *project)
{
    m_parProject=project;
}

void XvFlow::setLastErrorMsg(const QString &msg)
{
    _lastErrorMsg=msg;
    Log_Trace(QString("<%1>[%2]:%3").arg(this->flowId()).arg(this->flowName()).arg(msg));
}

/**********************算子操作**********************/
XvFunc *XvFlow::getXvFunc(const QString &id)
{
    Q_D(XvFlow);
    if(d->mapFunc.contains(id))
    {
        return d->mapFunc[id];
    }
    return nullptr;

}

QList<XvFunc *> XvFlow::getXvFuncs() const
{
    Q_D(const XvFlow);
    return d->mapFunc.values();
}

int XvFlow::xvFuncCount() const
{
    Q_D(const XvFlow);
    return d->mapFunc.count();
}

XvFunc *XvFlow::createXvFunc(const QString &role)
{
    return createXvFunc(role,QString());
}

XvFunc *XvFlow::createXvFunc(const QString &role,const QString &restoredId)
{
    Q_D(XvFlow);
    if(!isEditAllowed())
    {
        setLastErrorMsg("流程或项目正在运行，不能创建算子");
        return nullptr;
    }
    if(!restoredId.isEmpty()
            && (!XvXml::isValidId(restoredId) || d->mapFunc.contains(restoredId)))
    {
        setLastErrorMsg(QString("创建算子错误，ID[%1]无效或重复").arg(restoredId));
        return nullptr;
    }
    auto func=XvCoreMgr->getXvFuncAsm()->createNewXvFunc(role);
    if(func==nullptr)
    {
       const QString factoryError=XvCoreMgr->getXvFuncAsm()->lastErrorMsg();
       setLastErrorMsg(factoryError.isEmpty()
               ?QString(getLang(Core_XvFlow_CreateXvFuncError1,
                                "创建算子错误，标识符[%1]无效")).arg(role)
               :factoryError);
       return nullptr;
    }
    if(!restoredId.isEmpty())
    {
        func->unRegisterTokenMsgAble();
        func->_funcId=restoredId;
        func->registerTokenMsgAble();
    }
    func->setParent(this);
    func->setParFlow(this);
    d->mapFunc.insert(func->funcId(),func);
    connect(func,&XvFunc::sgSonFuncDel,this,&XvFlow::onUpdateAllXvFuncLink);
    emit this->sgXvFuncCreated(func);
    return func;
}

/**********************XML序列化**********************/
QDomElement XvFlow::toXmlElement(QDomDocument &doc)
{
    QDomElement flowElement=doc.createElement("Flow");
    flowElement.setAttribute("id",flowId());
    flowElement.setAttribute("name",flowName());

    XvFlowConfig *config=getFlowConfig();
    QDomElement configElement=doc.createElement("Config");
    configElement.setAttribute("loopInterval",config->loopInterval);
    configElement.setAttribute("funcErrorInterruptRun",
                               config->funcErrorInterruptRun?"true":"false");
    flowElement.appendChild(configElement);

    QList<XvFunc*> functions=getXvFuncs();
    std::sort(functions.begin(),functions.end(),[](XvFunc *left,XvFunc *right)
    {
        return left->funcId()<right->funcId();
    });

    QDomElement functionsElement=doc.createElement("Functions");
    for(XvFunc *function:functions)
    {
        QDomElement functionElement=function->toXmlElement(doc);
        if(functionElement.isNull())
        {
            setLastErrorMsg(QString("算子[%1]序列化失败: %2")
                            .arg(function->funcName(),function->lastErrorMsg()));
            return QDomElement();
        }
        functionsElement.appendChild(functionElement);
    }
    flowElement.appendChild(functionsElement);

    QDomElement linksElement=doc.createElement("Links");
    for(XvFunc *function:functions)
    {
        QList<XvFunc*> sons=function->linkedSonFuncs();
        std::sort(sons.begin(),sons.end(),[](XvFunc *left,XvFunc *right)
        {
            return left->funcId()<right->funcId();
        });
        for(XvFunc *son:sons)
        {
            QDomElement linkElement=doc.createElement("Link");
            linkElement.setAttribute("from",function->funcId());
            linkElement.setAttribute("to",son->funcId());
            const QString port=function->sonFuncPort(son);
            if(port!="default") linkElement.setAttribute("fromPort",port);
            linksElement.appendChild(linkElement);
        }
    }
    flowElement.appendChild(linksElement);
    return flowElement;
}

bool XvFlow::fromXmlElement(QDomElement &xmlEle)
{
    Q_D(XvFlow);
    QString error;
    if(xmlEle.tagName()!="Flow"
            || !XvXml::validateAttributes(xmlEle,{"id","name"},{"id","name"},error)
            || !XvXml::validateChildren(xmlEle,{"Config","Functions","Links"},
                                        {"Config","Functions","Links"},error))
    {
        setLastErrorMsg(error.isEmpty()?"无效的<Flow>节点":error);
        return false;
    }
    if(xmlEle.attribute("id")!=flowId())
    {
        setLastErrorMsg("流程ID与已创建流程不匹配");
        return false;
    }
    if(!d->mapFunc.isEmpty())
    {
        setLastErrorMsg("只能向空流程加载XML数据");
        return false;
    }

    QDomElement configElement=XvXml::singleChild(xmlEle,"Config");
    unsigned int loopInterval=0;
    bool interruptOnError=false;
    if(!XvXml::validateAttributes(configElement,
                                  {"loopInterval","funcErrorInterruptRun"},
                                  {"loopInterval","funcErrorInterruptRun"},error)
            || !XvXml::validateChildren(configElement,{}, {},error)
            || !XvXml::unsignedAttribute(configElement,"loopInterval",loopInterval,error)
            || !XvXml::boolAttribute(configElement,"funcErrorInterruptRun",
                                     interruptOnError,error))
    {
        setLastErrorMsg(error);
        return false;
    }

    QDomElement functionsElement=XvXml::singleChild(xmlEle,"Functions");
    if(!XvXml::validateAttributes(functionsElement,{}, {},error)
            || !XvXml::validateChildren(functionsElement,{"Function"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }

    QMap<QString,QDomElement> functionElements;
    for(QDomElement functionElement=functionsElement.firstChildElement("Function");
        !functionElement.isNull();
        functionElement=functionElement.nextSiblingElement("Function"))
    {
        QString id;
        QString role;
        if(!XvXml::requiredAttribute(functionElement,"id",id,error)
                || !XvXml::requiredAttribute(functionElement,"role",role,error)
                || !XvXml::isValidId(id) || role.isEmpty()
                || functionElements.contains(id))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("算子ID[%1]或角色[%2]无效或重复").arg(id,role)
                            :error);
            return false;
        }
        XvFunc *function=createXvFunc(role,id);
        if(!function)
        {
            setLastErrorMsg(QString("无法通过角色[%1]创建算子ID[%2]").arg(role,id));
            return false;
        }
        functionElements.insert(id,functionElement);
    }

    for(auto iterator=functionElements.begin();iterator!=functionElements.end();++iterator)
    {
        XvFunc *function=getXvFunc(iterator.key());
        QDomElement functionElement=iterator.value();
        if(!function || !function->fromXmlElement(functionElement))
        {
            setLastErrorMsg(QString("算子ID[%1]配置加载失败: %2")
                            .arg(iterator.key(),function?function->lastErrorMsg():QString()));
            return false;
        }
    }

    QDomElement linksElement=XvXml::singleChild(xmlEle,"Links");
    if(!XvXml::validateAttributes(linksElement,{}, {},error)
            || !XvXml::validateChildren(linksElement,{"Link"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    QSet<QString> links;
    for(QDomElement linkElement=linksElement.firstChildElement("Link");
        !linkElement.isNull();linkElement=linkElement.nextSiblingElement("Link"))
    {
        if(!XvXml::validateAttributes(linkElement,{"from","to","fromPort"},
                                      {"from","to"},error)
                || !XvXml::validateChildren(linkElement,{}, {},error))
        {
            setLastErrorMsg(error);
            return false;
        }
        const QString fromId=linkElement.attribute("from");
        const QString toId=linkElement.attribute("to");
        const QString fromPort=linkElement.hasAttribute("fromPort")
                ?linkElement.attribute("fromPort"):QString("default");
        const QString linkKey=fromId+":"+toId;
        XvFunc *from=getXvFunc(fromId);
        XvFunc *to=getXvFunc(toId);
        if(!from || !to || links.contains(linkKey) || from==to
                || to->existDescendantFunc(from)
                || !from->addSonFunc(to,fromPort))
        {
            setLastErrorMsg(QString("无效或重复的算子连接[%1:%2 -> %3]")
                            .arg(fromId,fromPort,toId));
            return false;
        }
        links.insert(linkKey);
    }

    for(auto iterator=functionElements.begin();iterator!=functionElements.end();++iterator)
    {
        XvFunc *subscriber=getXvFunc(iterator.key());
        QDomElement functionElement=iterator.value();
        QDomElement subscriptionsElement=functionElement.firstChildElement("Subscriptions");
        for(QDomElement subscription=subscriptionsElement.firstChildElement("Subscription");
            !subscription.isNull();
            subscription=subscription.nextSiblingElement("Subscription"))
        {
            const QString parameterName=subscription.attribute("parameter");
            const QString sourceId=subscription.attribute("sourceFunction");
            const QString resultName=subscription.attribute("sourceResult");
            XvFunc *source=getXvFunc(sourceId);
            XObject *parameter=subscriber?subscriber->getParamsByName(parameterName):nullptr;
            XObject *result=source?source->getResultsByName(resultName):nullptr;
            if(!subscriber || !source || !parameter || !result
                    || !XvXml::valueTypesCompatible(parameter,result)
                    || !subscriber->paramSubscribe(parameterName,source,resultName))
            {
                setLastErrorMsg(QString("算子ID[%1]的参数订阅[%2 <- %3.%4]无效")
                                .arg(iterator.key(),parameterName,sourceId,resultName));
                return false;
            }
        }
    }

    getFlowConfig()->loopInterval=loopInterval;
    getFlowConfig()->funcErrorInterruptRun=interruptOnError;
    setFlowName(xmlEle.attribute("name"));
    return true;
}

bool XvFlow::removeXvFunc(const QString &id)
{
    Q_D(XvFlow);
    if(!isEditAllowed())
    {
        setLastErrorMsg("流程或项目正在运行，不能移除算子");
        return false;
    }
    if(d->mapFunc.contains(id))
    {
       auto func= d->mapFunc[id];
       if(func->release())
       {
          emit this->sgRemoveXvFuncStart(func);
          disconnect(func,&XvFunc::sgSonFuncDel,this,&XvFlow::onUpdateAllXvFuncLink);
          func->deleteLater();
          d->mapFunc.remove(id);
          emit this->sgRemoveXvFuncEnd(id);
          return true;
       }
       else
       {
          setLastErrorMsg(QString(getLang(Core_XvFlow_RemoveXvFuncError1,"移除算子[%1]错误，算子无法释放")).arg(func->funcName()));
       }
    }
    else
    {
        setLastErrorMsg(QString(getLang(Core_XvFlow_RemoveXvFuncError2,"移除算子错误，算子Id[%1]不存在")).arg(id));
    }
    return false;
}

void XvFlow::onUpdateAllXvFuncLink()
{
    Q_D(XvFlow);
    QElapsedTimer timer;
    timer.start();
    foreach (auto func, d->mapFunc)
    {
        func->onXvFlowLinkRefresh();
    }
}
RetXv XvFlow::runOnce()
{
    Q_D(XvFlow);
    if(m_parProject && m_parProject->isRunning())
    {
        return Ret_Xv_FlowRunning;
    }
    if(!checkFlowLegal())//验证流程合法性
    {
        return Ret_Xv_FlowIllegal;
    }
    bool expected=false;
    if(!_running.compare_exchange_strong(expected,true))
    {
        return Ret_Xv_FlowRunning;
    }
    const QString threadName="FlowRunOnce_"+flowId();
    d->runThread = XConcurrentMgr->getThreadsByGropuName(XvFlowThreadGroup,threadName);
    if(!d->runThread)
    {
       d->runThread=XConcurrentMgr->createThreadByFunction(
                   XvFlowThreadGroup,threadName,false,true,
                   &XvFlow::_threadRun,this,false);
    }
    if(d->runThread)
    {
        d->runThread->start(QThread::HighPriority);
        return Ret_Xv_Success;
    }
    _running.store(false);
    setLastErrorMsg("无法创建流程单次运行线程");
    return Ret_Xv_FlowIllegal;
}

RetXv XvFlow::runLoop()
{
    Q_D(XvFlow);
    if(m_parProject && m_parProject->isRunning())
    {
        return Ret_Xv_FlowRunning;
    }
    if(!checkFlowLegal())//验证流程合法性
    {
        return Ret_Xv_FlowIllegal;
    }
    bool expected=false;
    if(!_running.compare_exchange_strong(expected,true))
    {
        return Ret_Xv_FlowRunning;
    }
    const QString threadName="FlowRunLoop_"+flowId();
    d->runThread= XConcurrentMgr->getThreadsByGropuName(XvFlowThreadGroup,threadName);
    if(!d->runThread)
    {
       d->runThread=XConcurrentMgr->createThreadByFunction(
                   XvFlowThreadGroup,threadName,false,true,
                   &XvFlow::_threadRun,this,true);
    }
    if(d->runThread)
    {
        d->runThread->start(QThread::HighPriority);
        return Ret_Xv_Success;
    }
    _running.store(false);
    setLastErrorMsg("无法创建流程循环运行线程");
    return Ret_Xv_FlowIllegal;
}

RetXv XvFlow::stop()
{
    if(_running.exchange(false))
    {
        emit this->sgFlowRunStop();
        return Ret_Xv_Success;
    }
    else
    {
        return Ret_Xv_FlowNoRun;
    }
}

RetXv XvFlow::wait(unsigned long ms)
{
    Q_D(XvFlow);
    QPointer<XThread> thread=d->runThread;
    if(!thread)
    {
        return Ret_Xv_FlowNoRun;
    }
    if(QThread::currentThread()==thread.data()) return Ret_Xv_FlowWaitTimeOut;
    const unsigned long timeout=ms==0
            ?std::numeric_limits<unsigned long>::max():ms;
    bool bRet=thread->wait(timeout);
    if(!bRet)
    {
        return Ret_Xv_FlowWaitTimeOut;
    }
    return Ret_Xv_Success;
}

bool XvFlow::isEditAllowed() const
{
    return !isRunning() && (!m_parProject || !m_parProject->isRunning());
}

RetXv XvFlow::runOnceSynchronously()
{
    if(!checkFlowLegal()) return Ret_Xv_FlowIllegal;
    bool expected=false;
    if(!_running.compare_exchange_strong(expected,true))
    {
        return Ret_Xv_FlowRunning;
    }
    _threadRun(false);
    return Ret_Xv_Success;
}

bool XvFlow::checkFlowLegal()
{
    Q_D(XvFlow);
    const QList<XvFunc*> lstStart=getFlowGraphStartFunc(d->mapFunc);
    if(lstStart.count()!=1)
    {
        setLastErrorMsg(getLang(Core_XvFlow_FlowLegalError1,"流程合法性错误，起始节点(无父节点)数量不等于1"));
        return false;
    }

    for(XvFunc *function:d->mapFunc)
    {
        if(!function) return false;
        const QStringList ports=function->outputPorts();
        QSet<QString> uniquePorts;
        for(const QString &port:ports)
        {
            if(port.isEmpty() || port!=port.trimmed() || uniquePorts.contains(port))
            {
                setLastErrorMsg(QString("算子[%1]声明了无效或重复的输出端口[%2]")
                                .arg(function->funcName(),port));
                return false;
            }
            uniquePorts.insert(port);
        }
        if(ports.isEmpty())
        {
            setLastErrorMsg(QString("算子[%1]未声明输出端口").arg(function->funcName()));
            return false;
        }
        for(XvFunc *son:function->linkedSonFuncs())
        {
            const QString port=function->sonFuncPort(son);
            if(!son || !d->mapFunc.values().contains(son)
                    || !son->fatherFuncs().contains(function)
                    || ports.count(port)!=1)
            {
                setLastErrorMsg(QString("算子[%1]包含不对称连接或无效端口[%2]")
                                .arg(function->funcName(),port));
                return false;
            }
        }
        for(XvFunc *father:function->fatherFuncs())
        {
            if(!father || !d->mapFunc.values().contains(father)
                    || !father->linkedSonFuncs().contains(function))
            {
                setLastErrorMsg(QString("算子[%1]包含不对称的父连接")
                                .arg(function->funcName()));
                return false;
            }
        }
    }

    QMap<XvFunc*,int> visitState;
    std::function<bool(XvFunc*)> visit=[&](XvFunc *function)
    {
        const int state=visitState.value(function);
        if(state==1) return false;
        if(state==2) return true;
        visitState.insert(function,1);
        for(XvFunc *son:function->linkedSonFuncs())
        {
            if(!visit(son)) return false;
        }
        visitState.insert(function,2);
        return true;
    };
    if(!visit(lstStart.first()) || visitState.count()!=d->mapFunc.count())
    {
        setLastErrorMsg("流程合法性错误，流程包含环路或起始节点不可达的算子");
        return false;
    }
    return true;
}

bool XvFlow::runGraphOnce(XvFlowConfig *config)
{
    Q_D(XvFlow);
    const QList<XvFunc*> starts=getFlowGraphStartFunc(d->mapFunc);
    if(starts.count()!=1) return false;
    QSet<XvFunc*> nodes;
    for(XvFunc *function:d->mapFunc) nodes.insert(function);
    return runGraphContext(nodes,{starts.first()},config);
}

bool XvFlow::runGraphContext(const QSet<XvFunc*> &nodes,
                             const QSet<XvFunc*> &entryNodes,
                             XvFlowConfig *config)
{
    enum EdgeState { Unknown,Selected,Skipped };
    QMap<QString,EdgeState> edgeStates;
    const auto edgeKey=[](XvFunc *from,XvFunc *to)
    {
        return from->funcId()+":"+to->funcId();
    };
    for(XvFunc *function:nodes)
    {
        for(XvFunc *son:function->linkedSonFuncs())
        {
            if(nodes.contains(son))
                edgeStates.insert(edgeKey(function,son),Unknown);
        }
    }

    QSet<XvFunc*> resolved;
    while(resolved.count()<nodes.count())
    {
        if(!_running.load()) return true;

        QList<XvFunc*> ready;
        QList<XvFunc*> skipped;
        for(XvFunc *function:nodes)
        {
            if(resolved.contains(function)) continue;
            bool allResolved=true;
            bool anySelected=entryNodes.contains(function);
            bool hasIncoming=anySelected;
            for(XvFunc *father:function->fatherFuncs())
            {
                if(!nodes.contains(father)) continue;
                hasIncoming=true;
                const EdgeState state=edgeStates.value(edgeKey(father,function),Unknown);
                if(state==Unknown) allResolved=false;
                else if(state==Selected) anySelected=true;
            }
            if(hasIncoming && allResolved)
            {
                if(anySelected) ready.append(function);
                else skipped.append(function);
            }
        }

        const auto byId=[](XvFunc *left,XvFunc *right)
        {
            return left->funcId()<right->funcId();
        };
        std::sort(skipped.begin(),skipped.end(),byId);
        if(!skipped.isEmpty())
        {
            for(XvFunc *function:skipped)
            {
                resolved.insert(function);
                for(XvFunc *son:function->linkedSonFuncs())
                    edgeStates.insert(edgeKey(function,son),Skipped);
            }
            continue;
        }
        std::sort(ready.begin(),ready.end(),byId);
        if(ready.isEmpty())
        {
            _runInfo.runStatus=EXvFlowRunStatus::Error;
            _runInfo.runMsg="流程调度失败，存在未解析的连接";
            setLastErrorMsg(_runInfo.runMsg);
            _running.store(false);
            return false;
        }

        XvFunc *function=ready.first();
        const EXvFuncRunStatus status=function->runXvFunc();
        if(!_running.load()) return true;
        if(config->funcErrorInterruptRun && status==EXvFuncRunStatus::Error)
        {
            _runInfo.runStatus=EXvFlowRunStatus::Error;
            _runInfo.runMsg=getLang(Core_XvFlow_RunStatusError,"运行错误");
            _running.store(false);
            return false;
        }

        const XvExecutionDirective directive=function->executionDirective();
        QSet<QString> selectedPorts;
        if(directive.kind==XvExecutionDirective::Continue)
        {
            for(const QString &port:function->outputPorts()) selectedPorts.insert(port);
        }
        else if(directive.kind==XvExecutionDirective::SelectPorts)
        {
            for(const QString &port:directive.selectedPorts)
            {
                if(!function->outputPorts().contains(port)
                        || selectedPorts.contains(port))
                {
                    _runInfo.runStatus=EXvFlowRunStatus::Error;
                    _runInfo.runMsg=QString("算子[%1]选择了无效或重复端口[%2]")
                            .arg(function->funcName(),port);
                    setLastErrorMsg(_runInfo.runMsg);
                    _running.store(false);
                    return false;
                }
                selectedPorts.insert(port);
            }
        }
        else if(directive.kind==XvExecutionDirective::Repeat)
        {
            const QStringList outputPorts=function->outputPorts();
            if(directive.bodyPort.isEmpty() || directive.donePort.isEmpty()
                    || directive.bodyPort==directive.donePort
                    || outputPorts.count(directive.bodyPort)!=1
                    || outputPorts.count(directive.donePort)!=1
                    || directive.iterationCount<0
                    || directive.iterationCount>XvExecutionDirective::MaximumIterationCount)
            {
                _runInfo.runStatus=EXvFlowRunStatus::Error;
                _runInfo.runMsg=QString("算子[%1]返回了无效循环指令")
                        .arg(function->funcName());
                setLastErrorMsg(_runInfo.runMsg);
                _running.store(false);
                return false;
            }

            QSet<XvFunc*> bodyTargets;
            QSet<XvFunc*> doneTargets;
            for(XvFunc *son:function->linkedSonFuncs())
            {
                const QString port=function->sonFuncPort(son);
                if(port==directive.bodyPort) bodyTargets.insert(son);
                else if(port==directive.donePort) doneTargets.insert(son);
            }
            if(bodyTargets.isEmpty() || doneTargets.isEmpty())
            {
                _runInfo.runStatus=EXvFlowRunStatus::Error;
                _runInfo.runMsg=QString("循环算子[%1]缺少body或done连接")
                        .arg(function->funcName());
                setLastErrorMsg(_runInfo.runMsg);
                _running.store(false);
                return false;
            }
            for(XvFunc *doneTarget:doneTargets)
            {
                if(!nodes.contains(doneTarget))
                {
                    _runInfo.runStatus=EXvFlowRunStatus::Error;
                    _runInfo.runMsg=QString("循环算子[%1]的done越过当前执行上下文")
                            .arg(function->funcName());
                    setLastErrorMsg(_runInfo.runMsg);
                    _running.store(false);
                    return false;
                }
            }

            QSet<XvFunc*> bodyNodes;
            QList<XvFunc*> pending=bodyTargets.values();
            while(!pending.isEmpty())
            {
                XvFunc *candidate=pending.takeFirst();
                if(doneTargets.contains(candidate)) continue;
                if(!nodes.contains(candidate) || candidate==function)
                {
                    _runInfo.runStatus=EXvFlowRunStatus::Error;
                    _runInfo.runMsg=QString("循环算子[%1]的body越过当前执行上下文")
                            .arg(function->funcName());
                    setLastErrorMsg(_runInfo.runMsg);
                    _running.store(false);
                    return false;
                }
                if(bodyNodes.contains(candidate)) continue;
                bodyNodes.insert(candidate);
                for(XvFunc *son:candidate->linkedSonFuncs())
                {
                    if(!doneTargets.contains(son)) pending.append(son);
                }
            }

            bool validBody=!bodyNodes.isEmpty();
            QSet<XvFunc*> reachesDone;
            bool changed=true;
            while(changed)
            {
                changed=false;
                for(XvFunc *bodyNode:bodyNodes)
                {
                    if(reachesDone.contains(bodyNode)) continue;
                    for(XvFunc *son:bodyNode->linkedSonFuncs())
                    {
                        if(doneTargets.contains(son) || reachesDone.contains(son))
                        {
                            reachesDone.insert(bodyNode);
                            changed=true;
                            break;
                        }
                    }
                }
            }
            validBody=validBody && reachesDone.count()==bodyNodes.count();
            for(XvFunc *bodyNode:bodyNodes)
            {
                if(resolved.contains(bodyNode)) validBody=false;
                for(XvFunc *father:bodyNode->fatherFuncs())
                {
                    const bool bodyEntry=father==function
                            && bodyTargets.contains(bodyNode)
                            && function->sonFuncPort(bodyNode)==directive.bodyPort;
                    if(!bodyNodes.contains(father) && !bodyEntry) validBody=false;
                }
                for(XvFunc *son:bodyNode->linkedSonFuncs())
                {
                    if(!bodyNodes.contains(son) && !doneTargets.contains(son))
                        validBody=false;
                }
            }
            if(!validBody)
            {
                _runInfo.runStatus=EXvFlowRunStatus::Error;
                _runInfo.runMsg=QString("循环算子[%1]的body子图不是封闭的done有界DAG")
                        .arg(function->funcName());
                setLastErrorMsg(_runInfo.runMsg);
                _running.store(false);
                return false;
            }

            for(int iteration=0;iteration<directive.iterationCount;++iteration)
            {
                QString error;
                if(!_running.load()) return true;
                if(!function->prepareIteration(iteration,error))
                {
                    _runInfo.runStatus=EXvFlowRunStatus::Error;
                    _runInfo.runMsg=error.isEmpty()
                            ?QString("循环算子[%1]无法准备第%2次迭代")
                             .arg(function->funcName()).arg(iteration)
                            :error;
                    setLastErrorMsg(_runInfo.runMsg);
                    _running.store(false);
                    return false;
                }
                function->publishResultUpdate();
                if(!runGraphContext(bodyNodes,bodyTargets,config)) return false;
            }

            resolved.insert(function);
            for(XvFunc *bodyNode:bodyNodes) resolved.insert(bodyNode);
            for(XvFunc *son:function->linkedSonFuncs())
            {
                if(nodes.contains(son))
                {
                    edgeStates.insert(edgeKey(function,son),
                                      doneTargets.contains(son)?Selected:Skipped);
                }
            }
            for(XvFunc *bodyNode:bodyNodes)
            {
                for(XvFunc *son:bodyNode->linkedSonFuncs())
                {
                    if(nodes.contains(son))
                        edgeStates.insert(edgeKey(bodyNode,son),Skipped);
                }
            }
            continue;
        }
        else if(directive.kind==XvExecutionDirective::Stop)
        {
            resolved.insert(function);
            _running.store(false);
            return true;
        }
        else
        {
            _runInfo.runStatus=EXvFlowRunStatus::Error;
            _runInfo.runMsg=QString("算子[%1]返回流程错误指令")
                    .arg(function->funcName());
            setLastErrorMsg(_runInfo.runMsg);
            _running.store(false);
            return false;
        }

        resolved.insert(function);
        for(XvFunc *son:function->linkedSonFuncs())
        {
            if(!nodes.contains(son)) continue;
            const EdgeState state=selectedPorts.contains(function->sonFuncPort(son))
                    ?Selected:Skipped;
            edgeStates.insert(edgeKey(function,son),state);
        }
    }
    return true;
}

void XvFlow::_threadRun(bool bLoop)
{
    Q_D(XvFlow);
    auto config=getFlowConfig();
    do
    {
        QElapsedTimer timer;
        timer.start();
        _runInfo.runIdx++;
        _runInfo.runStatus=EXvFlowRunStatus::Running;
        _runInfo.runMsg=getLang(Core_XvFlow_RunStatusRunning,"正在运行");
        emit this->sgFlowRunStart();
        runGraphOnce(config);
        if(_runInfo.runStatus!=EXvFlowRunStatus::Error)
        {
            _runInfo.runStatus=EXvFlowRunStatus::Ok;
            _runInfo.runMsg=getLang(Core_XvFlow_RunStatusOk,"运行成功");
        }
        _runInfo.runElapsed= (timer.nsecsElapsed()*1.0)/1000/1000;
        emit this->sgFlowRunEnd();
        if(!_running)
        {
           break;
        }
        if(bLoop)
        {
            QThread::msleep(config->loopInterval);
        }
    }while(bLoop);
    _running.store(false);
}
