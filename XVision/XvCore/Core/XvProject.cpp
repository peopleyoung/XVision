#include "XvProject.h"

#include <QUuid>
#include <QElapsedTimer>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QThread>
#include <QWaitCondition>
#include <algorithm>
#include <atomic>
#include <limits>

#include "XvFlow.h"
#include "XvXmlUtils.h"

#include "LangDef.h"

#include "XConcurrentManager.h"

using namespace XvCore;

#define XvProjectThreadGroup "XvCore::XvProject"
/**************************************************************/
//* [XvProjectPrivate]
/**************************************************************/
namespace XvCore
{
class XvProjectPrivate
{
    Q_DISABLE_COPY(XvProjectPrivate)
    Q_DECLARE_PUBLIC(XvProject)

public:
    XvProjectPrivate(XvProject *q):q_ptr(q)
    {

    };
    ~XvProjectPrivate(){};
    ///父指针
    XvProject              *const q_ptr;
    ///流程字典
    QMap<QString,XvFlow*>   mapFlow;
    ///项目运行配置
    XvProjectConfig        projectConfig;
    XvGlobalState          globalState;
    quint64                globalRevision=0;
    ///项目运行线程及状态
    QPointer<XThread>      runThread;
    QPointer<XvFlow>       currentFlow;
    std::atomic_bool       running{false};
    std::atomic_bool       stopRequested{false};
    mutable QMutex         runMutex;
    QWaitCondition         intervalWait;
    XvProjectRunInfo       runInfo;
};
}

namespace
{
QString errorPolicyText(EXvProjectFlowErrorPolicy policy)
{
    return policy==EXvProjectFlowErrorPolicy::Continue?"continue":"stop";
}

bool parseErrorPolicy(const QString &text,EXvProjectFlowErrorPolicy &policy)
{
    if(text=="stop")
    {
        policy=EXvProjectFlowErrorPolicy::Stop;
        return true;
    }
    if(text=="continue")
    {
        policy=EXvProjectFlowErrorPolicy::Continue;
        return true;
    }
    return false;
}
}
/**************************************************************/
//* [XvProject]
/**************************************************************/

XvProject::XvProject(const QString &name,QObject *parent)
    : QObject{parent},
      _projectId(QUuid::createUuid().toString(QUuid::StringFormat::Id128)),
      _projectName(name),
      d_ptr(new XvProjectPrivate(this))
{
    registerTokenMsgAble();
}

XvProject::~XvProject()
{
    unRegisterTokenMsgAble();
    release();
}
RetXv XvProject::release()
{
    Q_D(XvProject);
    if(isRunning())
    {
        stop();
        wait();
    }
    foreach (auto flow, d->mapFlow)
    {
        if(flow->isRunning())
        {
            flow->stop();
            flow->wait();
        }
        if(flow->release())
        {
            flow->deleteLater();
        }
        else
        {
            delete flow;
            flow=nullptr;
        }
    }
    return true;
}

RetXv XvProject::runOnce()
{
    return startRun(false);
}

RetXv XvProject::runLoop()
{
    return startRun(true);
}

RetXv XvProject::startRun(bool loop)
{
    Q_D(XvProject);
    if(isRunning()) return Ret_Xv_ProjectRunning;

    const XvProjectConfig config=projectConfig();
    for(XvFlow *flow:getXvFlows())
    {
        if(flow && flow->isRunning())
        {
            setLastErrorMsg(QString("流程[%1]正在运行，不能启动项目").arg(flow->flowName()));
            return Ret_Xv_ProjectRunning;
        }
    }
    for(const XvProjectFlowEntry &entry:config.mainFlows)
    {
        if(!entry.enabled) continue;
        XvFlow *flow=getXvFlow(entry.flowId);
        if(!flow || !flow->checkFlowLegal())
        {
            const QString reason=flow?flow->lastErrorMsg():QString("流程不存在");
            setLastErrorMsg(QString("项目流程[%1]不合法: %2")
                            .arg(entry.flowId,reason));
            return Ret_Xv_ProjectIllegal;
        }
    }

    bool expected=false;
    if(!d->running.compare_exchange_strong(expected,true))
    {
        return Ret_Xv_ProjectRunning;
    }
    d->stopRequested.store(false);

    unsigned int runIdx=0;
    {
        QMutexLocker locker(&d->runMutex);
        runIdx=d->runInfo.runIdx+1;
        d->runInfo=XvProjectRunInfo();
        d->runInfo.runIdx=runIdx;
        d->runInfo.runStatus=EXvProjectRunStatus::Running;
        d->runInfo.runMsg=getLang(Core_XvProject_RunStatusRunning,"项目正在运行");
    }

    const QString threadName=QString("ProjectRun_%1_%2")
            .arg(projectId()).arg(runIdx);
    d->runThread=XConcurrentMgr->createThreadByFunction(
                XvProjectThreadGroup,threadName,true,true,
                &XvProject::runProjectThread,this,loop,config);
    if(!d->runThread)
    {
        d->running.store(false);
        QMutexLocker locker(&d->runMutex);
        d->runInfo.runStatus=EXvProjectRunStatus::Error;
        d->runInfo.runCode=Ret_Xv_ProjectThreadError;
        d->runInfo.runMsg=getLang(Core_XvProject_RunThreadError,
                                  "无法创建项目运行线程");
        setLastErrorMsg(d->runInfo.runMsg);
        return Ret_Xv_ProjectThreadError;
    }
    d->runThread->start(QThread::HighPriority);
    return Ret_Xv_Success;
}

void XvProject::runProjectThread(bool loop,const XvProjectConfig &config)
{
    Q_D(XvProject);
    QElapsedTimer timer;
    timer.start();
    bool hadFailure=false;
    bool hadError=false;
    bool abortForFailure=false;
    bool hasEnabledFlow=false;
    for(const XvProjectFlowEntry &entry:config.mainFlows)
    {
        if(entry.enabled)
        {
            hasEnabledFlow=true;
            break;
        }
    }
    emit sgProjectRunStart();

    do
    {
        for(const XvProjectFlowEntry &entry:config.mainFlows)
        {
            if(d->stopRequested.load() || abortForFailure) break;
            if(!entry.enabled) continue;

            XvFlow *flow=getXvFlow(entry.flowId);
            if(!flow)
            {
                hadError=true;
                abortForFailure=config.flowErrorPolicy==EXvProjectFlowErrorPolicy::Stop;
                QMutexLocker locker(&d->runMutex);
                if(!d->runInfo.failedFlowIds.contains(entry.flowId))
                    d->runInfo.failedFlowIds.append(entry.flowId);
                continue;
            }

            {
                QMutexLocker locker(&d->runMutex);
                d->currentFlow=flow;
                d->runInfo.currentFlowId=flow->flowId();
            }
            emit sgProjectFlowRunStart(flow);
            const RetXv result=flow->runOnceSynchronously();
            const XvFlowRunInfo flowInfo=flow->getXvFuncRunInfo();
            emit sgProjectFlowRunEnd(flow);
            {
                QMutexLocker locker(&d->runMutex);
                d->currentFlow.clear();
                d->runInfo.currentFlowId.clear();
            }

            const bool flowError=result!=Ret_Xv_Success
                    || flowInfo.runStatus==EXvFlowRunStatus::Error;
            const bool flowFailure=flowError
                    || flowInfo.runStatus==EXvFlowRunStatus::Fail;
            if(flowFailure)
            {
                hadFailure=true;
                hadError=hadError || flowError;
                {
                    QMutexLocker locker(&d->runMutex);
                    if(!d->runInfo.failedFlowIds.contains(flow->flowId()))
                        d->runInfo.failedFlowIds.append(flow->flowId());
                }
                if(config.flowErrorPolicy==EXvProjectFlowErrorPolicy::Stop)
                    abortForFailure=true;
            }
        }

        if(!loop || d->stopRequested.load() || abortForFailure) break;
        QMutexLocker locker(&d->runMutex);
        const unsigned long interval=config.loopInterval==0
                && !hasEnabledFlow?1:config.loopInterval;
        if(!d->stopRequested.load() && interval>0)
            d->intervalWait.wait(&d->runMutex,interval);
    } while(!d->stopRequested.load());

    {
        QMutexLocker locker(&d->runMutex);
        d->currentFlow.clear();
        d->runInfo.currentFlowId.clear();
        d->runInfo.runElapsed=timer.nsecsElapsed()/1000000.0;
        if(d->stopRequested.load())
        {
            d->runInfo.runStatus=EXvProjectRunStatus::Stopped;
            d->runInfo.runCode=Ret_Xv_ProjectStopped;
            d->runInfo.runMsg=getLang(Core_XvProject_RunStatusStopped,"项目已停止");
        }
        else if(hadError)
        {
            d->runInfo.runStatus=EXvProjectRunStatus::Error;
            d->runInfo.runCode=Ret_Xv_ProjectFlowFailed;
            d->runInfo.runMsg=getLang(Core_XvProject_RunStatusError,"项目运行错误");
        }
        else if(hadFailure)
        {
            d->runInfo.runStatus=EXvProjectRunStatus::Fail;
            d->runInfo.runCode=Ret_Xv_ProjectFlowFailed;
            d->runInfo.runMsg=getLang(Core_XvProject_RunStatusFail,"项目运行失败");
        }
        else
        {
            d->runInfo.runStatus=EXvProjectRunStatus::Ok;
            d->runInfo.runCode=Ret_Xv_Success;
            d->runInfo.runMsg=getLang(Core_XvProject_RunStatusOk,"项目运行成功");
        }
    }
    d->running.store(false);
    emit sgProjectRunEnd();
}

RetXv XvProject::stop()
{
    Q_D(XvProject);
    if(!isRunning()) return Ret_Xv_ProjectNoRun;
    if(d->stopRequested.exchange(true)) return Ret_Xv_Success;

    QPointer<XvFlow> currentFlow;
    {
        QMutexLocker locker(&d->runMutex);
        currentFlow=d->currentFlow;
        d->intervalWait.wakeAll();
    }
    if(currentFlow && currentFlow->isRunning()) currentFlow->stop();
    emit sgProjectRunStop();
    return Ret_Xv_Success;
}

RetXv XvProject::wait(unsigned long ms)
{
    Q_D(XvProject);
    QPointer<XThread> thread=d->runThread;
    if(!thread) return Ret_Xv_ProjectNoRun;
    if(QThread::currentThread()==thread.data()) return Ret_Xv_ProjectWaitTimeOut;
    const unsigned long timeout=ms==0
            ?std::numeric_limits<unsigned long>::max():ms;
    return thread->wait(timeout)?Ret_Xv_Success:Ret_Xv_ProjectWaitTimeOut;
}

bool XvProject::isRunning() const
{
    Q_D(const XvProject);
    return d->running.load();
}

XvProjectRunInfo XvProject::projectRunInfo() const
{
    Q_D(const XvProject);
    QMutexLocker locker(&d->runMutex);
    return d->runInfo;
}



/**********************流程操作**********************/

XvFlow *XvProject::getXvFlow(const QString &id)
{
    Q_D(XvProject);
    if(d->mapFlow.contains(id))
    {
        return d->mapFlow[id];
    }
    else
    {
        return nullptr;
    }
}

QList<XvFlow *> XvProject::getXvFlows(const QString &name) const
{
    Q_D(const XvProject);
    QList<XvFlow*> lstFlow;
    foreach (auto flow, d->mapFlow)
    {
        if(flow->flowName()==name)
        {
            lstFlow.append(flow);
        }
    }
    return lstFlow;
}

QList<XvFlow *> XvProject::getXvFlows() const
{
    Q_D(const XvProject);
    QList<XvFlow*> lstFlow;
    foreach (auto flow, d->mapFlow)
    {
        lstFlow.append(flow);
    }
    return lstFlow;
}

int XvProject::xvFlowCount() const
{
    Q_D(const XvProject);
    return d->mapFlow.count();
}

XvFlow *XvProject::createXvFlow(const QString &name)
{
    return createXvFlow(name,QString());
}

XvFlow *XvProject::createXvFlow(const QString &name,const QString &restoredId)
{
    Q_D(XvProject);
    if(isRunning())
    {
        setLastErrorMsg("项目正在运行，不能创建流程");
        return nullptr;
    }
    if(!restoredId.isEmpty()
            && (!XvXml::isValidId(restoredId) || d->mapFlow.contains(restoredId)))
    {
        setLastErrorMsg(QString("创建流程错误，ID[%1]无效或重复").arg(restoredId));
        return nullptr;
    }
    XvFlow* flow=new XvFlow(this,name,this);
    if(!restoredId.isEmpty())
    {
        flow->unRegisterTokenMsgAble();
        flow->_flowId=restoredId;
        flow->registerTokenMsgAble();
    }
    d->mapFlow.insert(flow->flowId(),flow);
    d->projectConfig.mainFlows.append(XvProjectFlowEntry(flow->flowId()));
    emit this->sgXvFlowCreated(flow);
    emit projectConfigChanged();
    return flow;
}

bool XvProject::adoptXvFlow(XvFlow *flow)
{
    Q_D(XvProject);
    if(isRunning() || !flow || flow->parProject()!=this || flow->parent()
            || !XvXml::isValidId(flow->flowId())
            || d->mapFlow.contains(flow->flowId()))
    {
        setLastErrorMsg("无法接纳无效或重复的流程");
        return false;
    }

    const QString id=flow->flowId();
    flow->setParent(this);
    d->mapFlow.insert(id,flow);
    d->projectConfig.mainFlows.append(XvProjectFlowEntry(id));
    emit sgXvFlowCreated(flow);
    emit projectConfigChanged();
    return true;
}

/**********************XML序列化**********************/
QDomElement XvProject::toXmlElement(QDomDocument &doc)
{
    Q_D(XvProject);
    QDomElement projectElement=doc.createElement("Project");
    projectElement.setAttribute("id",projectId());
    projectElement.setAttribute("name",projectName());

    QString configError;
    if(!setProjectConfig(d->projectConfig))
    {
        configError=lastErrorMsg();
        setLastErrorMsg(QString("项目配置序列化失败: %1").arg(configError));
        return QDomElement();
    }
    QDomElement configElement=doc.createElement("Config");
    configElement.setAttribute("loopInterval",d->projectConfig.loopInterval);
    configElement.setAttribute("flowErrorPolicy",
                               errorPolicyText(d->projectConfig.flowErrorPolicy));
    QDomElement mainFlowsElement=doc.createElement("MainFlows");
    for(const XvProjectFlowEntry &entry:d->projectConfig.mainFlows)
    {
        QDomElement flowRefElement=doc.createElement("FlowRef");
        flowRefElement.setAttribute("id",entry.flowId);
        flowRefElement.setAttribute("enabled",entry.enabled?"true":"false");
        mainFlowsElement.appendChild(flowRefElement);
    }
    configElement.appendChild(mainFlowsElement);
    projectElement.appendChild(configElement);

    QList<XvFlow*> flows=getXvFlows();
    std::sort(flows.begin(),flows.end(),[](XvFlow *left,XvFlow *right)
    {
        return left->flowId()<right->flowId();
    });
    QDomElement flowsElement=doc.createElement("Flows");
    for(XvFlow *flow:flows)
    {
        QDomElement flowElement=flow->toXmlElement(doc);
        if(flowElement.isNull())
        {
            setLastErrorMsg(QString("流程[%1]序列化失败: %2")
                            .arg(flow->flowName(),flow->lastErrorMsg()));
            return QDomElement();
        }
        flowsElement.appendChild(flowElement);
    }
    projectElement.appendChild(flowsElement);
    projectElement.appendChild(d->globalState.toXml(doc));
    return projectElement;
}

XvGlobalState XvProject::globalState() const { Q_D(const XvProject); return d->globalState; }
quint64 XvProject::globalRevision() const { Q_D(const XvProject); return d->globalRevision; }
bool XvProject::hasActiveExecution() const {
    if(isRunning()) return true;
    for(auto flow:getXvFlows()) if(flow->isRunning()) return true;
    return false;
}
bool XvProject::setGlobalState(const XvGlobalState &state) {
    Q_D(XvProject);
    if(QThread::currentThread()!=thread() || hasActiveExecution()) {setLastErrorMsg(QStringLiteral("运行期间不能修改全局配置"));return false;}
    QString error;if(!state.validate(error)) {setLastErrorMsg(error);return false;}
    d->globalState=state;++d->globalRevision;emit globalStateChanged();return true;
}

bool XvProject::fromXmlElement(QDomElement &xmlEle)
{
    Q_D(XvProject);
    QString error;
    if(xmlEle.tagName()!="Project"
            || !XvXml::validateAttributes(xmlEle,{"id","name"},{"id","name"},error)
            || !XvXml::validateChildren(xmlEle,{"Config","Flows","Globals"},{"Flows"},error))
    {
        setLastErrorMsg(error.isEmpty()?"无效的<Project>节点":error);
        return false;
    }
    if(!d->mapFlow.isEmpty())
    {
        setLastErrorMsg("只能向空项目加载XML数据");
        return false;
    }
    const QString restoredId=xmlEle.attribute("id");
    if(!XvXml::isValidId(restoredId))
    {
        setLastErrorMsg(QString("项目ID[%1]无效").arg(restoredId));
        return false;
    }
    unRegisterTokenMsgAble();
    _projectId=restoredId;
    registerTokenMsgAble();

    QDomElement flowsElement=XvXml::singleChild(xmlEle,"Flows");
    if(!XvXml::validateAttributes(flowsElement,{}, {},error)
            || !XvXml::validateChildren(flowsElement,{"Flow"}, {},error))
    {
        setLastErrorMsg(error);
        return false;
    }
    QMap<QString,QDomElement> flowElements;
    for(QDomElement flowElement=flowsElement.firstChildElement("Flow");
        !flowElement.isNull();flowElement=flowElement.nextSiblingElement("Flow"))
    {
        QString id;
        QString name;
        if(!XvXml::requiredAttribute(flowElement,"id",id,error)
                || !XvXml::requiredAttribute(flowElement,"name",name,error)
                || !XvXml::isValidId(id) || flowElements.contains(id))
        {
            setLastErrorMsg(error.isEmpty()
                            ?QString("流程ID[%1]无效或重复").arg(id)
                            :error);
            return false;
        }
        XvFlow *flow=createXvFlow(name,id);
        if(!flow)
        {
            setLastErrorMsg(QString("无法创建流程ID[%1]").arg(id));
            return false;
        }
        flowElements.insert(id,flowElement);
    }

    for(auto iterator=flowElements.begin();iterator!=flowElements.end();++iterator)
    {
        XvFlow *flow=getXvFlow(iterator.key());
        QDomElement flowElement=iterator.value();
        if(!flow || !flow->fromXmlElement(flowElement))
        {
            setLastErrorMsg(QString("流程ID[%1]加载失败: %2")
                            .arg(iterator.key(),flow?flow->lastErrorMsg():QString()));
            return false;
        }
    }

    QDomElement configElement=XvXml::singleChild(xmlEle,"Config");
    if(!configElement.isNull())
    {
        if(!configElement.nextSiblingElement("Config").isNull()
                || !XvXml::validateAttributes(configElement,
                                               {"loopInterval","flowErrorPolicy"},
                                               {"loopInterval","flowErrorPolicy"},error)
                || !XvXml::validateChildren(configElement,{"MainFlows"},
                                             {"MainFlows"},error))
        {
            setLastErrorMsg(error.isEmpty()?"项目包含重复的<Config>节点":error);
            return false;
        }

        XvProjectConfig config;
        if(!XvXml::unsignedAttribute(configElement,"loopInterval",
                                     config.loopInterval,error)
                || config.loopInterval>XvProjectConfig::MaximumLoopInterval)
        {
            setLastErrorMsg(error.isEmpty()?"项目循环间隔超出允许范围":error);
            return false;
        }
        if(!parseErrorPolicy(configElement.attribute("flowErrorPolicy"),
                             config.flowErrorPolicy))
        {
            setLastErrorMsg("项目错误策略必须为'stop'或'continue'");
            return false;
        }

        QDomElement mainFlowsElement=XvXml::singleChild(configElement,"MainFlows");
        if(!XvXml::validateAttributes(mainFlowsElement,{}, {},error)
                || !XvXml::validateChildren(mainFlowsElement,{"FlowRef"}, {},error))
        {
            setLastErrorMsg(error);
            return false;
        }
        for(QDomElement flowRef=mainFlowsElement.firstChildElement("FlowRef");
            !flowRef.isNull();flowRef=flowRef.nextSiblingElement("FlowRef"))
        {
            QString flowId;
            bool enabled=false;
            if(!XvXml::validateAttributes(flowRef,{"id","enabled"},
                                          {"id","enabled"},error)
                    || !XvXml::requiredAttribute(flowRef,"id",flowId,error)
                    || !XvXml::boolAttribute(flowRef,"enabled",enabled,error))
            {
                setLastErrorMsg(error);
                return false;
            }
            config.mainFlows.append(XvProjectFlowEntry(flowId,enabled));
        }
        if(!setProjectConfig(config)) return false;
    }
    else
    {
        XvProjectConfig legacyConfig;
        const QList<XvFlow*> flows=getXvFlows();
        for(XvFlow *flow:flows)
        {
            legacyConfig.mainFlows.append(XvProjectFlowEntry(flow->flowId()));
        }
        if(!setProjectConfig(legacyConfig)) return false;
    }
    XvGlobalState globals;
    if(!XvGlobalState::fromXml(xmlEle.firstChildElement("Globals"),globals,error) || !setGlobalState(globals)) {
        if(!error.isEmpty()) setLastErrorMsg(error);return false;
    }
    setProjectName(xmlEle.attribute("name"));
    return true;
}

bool XvProject::removeXvFlow(const QString &id)
{
    Q_D(XvProject);
    if(isRunning())
    {
        setLastErrorMsg("项目正在运行，不能移除流程");
        return false;
    }
    if(d->mapFlow.contains(id))
    {
       auto flow= d->mapFlow[id];
       if(flow->release())
       {
          emit this->sgRemoveXvFlowStart(flow);
          flow->deleteLater();
          d->mapFlow.remove(id);
          for(int index=0;index<d->projectConfig.mainFlows.count();++index)
          {
              if(d->projectConfig.mainFlows.at(index).flowId==id)
              {
                  d->projectConfig.mainFlows.removeAt(index);
                  break;
              }
          }
          emit this->sgRemoveXvFlowEnd(id);
          emit projectConfigChanged();
          return true;
       }
       else
       {
           setLastErrorMsg(QString(getLang(Core_XvProject_RemoveXvFuncError1,"移除流程[%1]错误，流程无法释放")).arg(flow->flowName()));
       }
    }
    else
    {
        setLastErrorMsg(QString(getLang(Core_XvProject_RemoveXvFuncError2,"移除流程错误，流程Id[%1]不存在")).arg(id));
    }
    return false;
}

XvProjectConfig XvProject::projectConfig() const
{
    Q_D(const XvProject);
    return d->projectConfig;
}

bool XvProject::setProjectConfig(const XvProjectConfig &config)
{
    Q_D(XvProject);
    if(isRunning())
    {
        setLastErrorMsg("项目正在运行，不能修改项目配置");
        return false;
    }
    if(config.loopInterval>XvProjectConfig::MaximumLoopInterval)
    {
        setLastErrorMsg(QString("项目循环间隔[%1]超出允许范围[0,%2]")
                        .arg(config.loopInterval)
                        .arg(XvProjectConfig::MaximumLoopInterval));
        return false;
    }
    if(config.flowErrorPolicy!=EXvProjectFlowErrorPolicy::Stop
            && config.flowErrorPolicy!=EXvProjectFlowErrorPolicy::Continue)
    {
        setLastErrorMsg("项目错误策略无效");
        return false;
    }

    QSet<QString> configuredIds;
    for(const XvProjectFlowEntry &entry:config.mainFlows)
    {
        if(!XvXml::isValidId(entry.flowId) || !d->mapFlow.contains(entry.flowId))
        {
            setLastErrorMsg(QString("项目配置引用了无效或不存在的流程ID[%1]")
                            .arg(entry.flowId));
            return false;
        }
        if(configuredIds.contains(entry.flowId))
        {
            setLastErrorMsg(QString("项目配置包含重复流程ID[%1]").arg(entry.flowId));
            return false;
        }
        configuredIds.insert(entry.flowId);
    }
    if(configuredIds.count()!=d->mapFlow.count())
    {
        setLastErrorMsg("项目配置必须且只能包含项目中的全部流程");
        return false;
    }

    if(d->projectConfig==config) return true;
    d->projectConfig=config;
    emit projectConfigChanged();
    return true;
}

int XvProject::removeXvFlows(const QString &name)
{
    Q_D(XvProject);
    int nRemoveCount=0;
    QList<QString> lstRemoveId;
    foreach (auto flow, d->mapFlow)
    {
        if(flow->flowName()==name)
        {
            lstRemoveId.append(flow->flowId());
        }
    }
    foreach (auto id, lstRemoveId)
    {
        bool bRet=removeXvFlow(id);
        if(bRet)
        {
            nRemoveCount++;
        }
    }
    return nRemoveCount;
}



void XvProject::setProjectName(const QString &name)
{
    if(name!=_projectName)
    {
        _projectName=name;
        emit projectNameChanged(name);
    }
}

QString XvProject::lastErrorMsg()
{
    QString str=_lastErrorMsg;
    _lastErrorMsg="";
    return str;
}

void XvProject::setLastErrorMsg(const QString &msg)
{
    _lastErrorMsg=msg;
    Log_Trace(QString("<%1>[%2]:%3").arg(this->projectId()).arg(this->projectName()).arg(msg));
}
