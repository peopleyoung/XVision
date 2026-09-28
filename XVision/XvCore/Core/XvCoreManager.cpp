#include "XvCoreManager.h"
#include <QDomDocument>
#include <QFile>
#include <QIcon>
#include <QMap>
#include <QMutexLocker>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include "LangDef.h"

#include "XvPluginManager.h"
#include "XvFuncAssembly.h"
#include "XvProject.h"
#include "XvFlow.h"
#include "XvFunc.h"
#include "XvXmlUtils.h"


using namespace XvCore;
/**************************************************************/
//* [XvCoreManagerPrivate]
/**************************************************************/
namespace XvCore
{
class XvCoreManagerPrivate
{
    Q_DISABLE_COPY(XvCoreManagerPrivate)
    Q_DECLARE_PUBLIC(XvCoreManager)

public:
    XvCoreManagerPrivate(XvCoreManager *q):q_ptr(q)
    {
        project=nullptr;
    };
    ~XvCoreManagerPrivate(){};

    XvCoreManager              *const q_ptr;
    XvProject                  *project;
    QString                    lastErrorMsg;
};
}

namespace
{
XvProject *readProjectFile(const QString &path,QString &error)
{
    if(path.isEmpty())
    {
        error="项目文件路径为空";
        return nullptr;
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
    {
        error=QString("无法读取项目文件[%1]: %2").arg(path,file.errorString());
        return nullptr;
    }

    QDomDocument document;
    QString parseError;
    int errorLine=0;
    int errorColumn=0;
    if(!document.setContent(&file,&parseError,&errorLine,&errorColumn))
    {
        error=QString("项目XML解析失败(%1:%2): %3")
                .arg(errorLine).arg(errorColumn).arg(parseError);
        return nullptr;
    }

    QDomElement root=document.documentElement();
    if(root.tagName()!="XVisionProject"
            || !XvXml::validateAttributes(root,{"format","version"},
                                          {"format","version"},error)
            || !XvXml::validateChildren(root,{"Project"},{"Project"},error))
    {
        if(error.isEmpty()) error="项目文件根节点必须为<XVisionProject>";
        return nullptr;
    }
    if(root.attribute("format")!=XvXml::ProjectFileFormat)
    {
        error=QString("不支持的项目文件格式[%1]").arg(root.attribute("format"));
        return nullptr;
    }
    if(root.attribute("version")!=XvXml::ProjectFileVersion)
    {
        error=QString("不支持的项目文件版本[%1]").arg(root.attribute("version"));
        return nullptr;
    }

    XvProject *candidate=new XvProject();
    QDomElement projectElement=root.firstChildElement("Project");
    if(!candidate->fromXmlElement(projectElement))
    {
        error=candidate->lastErrorMsg();
        delete candidate;
        return nullptr;
    }
    return candidate;
}

QString createPersistentId()
{
    return QUuid::createUuid().toString(QUuid::StringFormat::Id128);
}

bool remapFlowIds(QDomElement &flowElement,QString &error)
{
    QString sourceFlowId;
    if(!XvXml::requiredAttribute(flowElement,"id",sourceFlowId,error)
            || !XvXml::isValidId(sourceFlowId))
    {
        if(error.isEmpty()) error=QString("流程ID[%1]无效").arg(sourceFlowId);
        return false;
    }

    QDomElement functionsElement=flowElement.firstChildElement("Functions");
    QMap<QString,QString> idMap;
    QSet<QString> generatedIds;
    for(QDomElement function=functionsElement.firstChildElement("Function");
        !function.isNull();function=function.nextSiblingElement("Function"))
    {
        QString sourceId;
        if(!XvXml::requiredAttribute(function,"id",sourceId,error)
                || !XvXml::isValidId(sourceId) || idMap.contains(sourceId))
        {
            if(error.isEmpty())
            {
                error=QString("算子ID[%1]无效或重复").arg(sourceId);
            }
            return false;
        }
        QString targetId;
        do
        {
            targetId=createPersistentId();
        } while(generatedIds.contains(targetId));
        generatedIds.insert(targetId);
        idMap.insert(sourceId,targetId);
        function.setAttribute("id",targetId);
    }

    QDomElement linksElement=flowElement.firstChildElement("Links");
    for(QDomElement link=linksElement.firstChildElement("Link");
        !link.isNull();link=link.nextSiblingElement("Link"))
    {
        const QString from=link.attribute("from");
        const QString to=link.attribute("to");
        if(!idMap.contains(from) || !idMap.contains(to))
        {
            error=QString("流程包含外部或无效连接[%1 -> %2]").arg(from,to);
            return false;
        }
        link.setAttribute("from",idMap.value(from));
        link.setAttribute("to",idMap.value(to));
    }

    for(QDomElement function=functionsElement.firstChildElement("Function");
        !function.isNull();function=function.nextSiblingElement("Function"))
    {
        QDomElement subscriptions=function.firstChildElement("Subscriptions");
        for(QDomElement subscription=subscriptions.firstChildElement("Subscription");
            !subscription.isNull();
            subscription=subscription.nextSiblingElement("Subscription"))
        {
            const QString source=subscription.attribute("sourceFunction");
            if(!idMap.contains(source))
            {
                error=QString("流程包含外部或无效订阅源[%1]").arg(source);
                return false;
            }
            subscription.setAttribute("sourceFunction",idMap.value(source));
        }
    }
    return true;
}

XvFlow *readFlowFile(const QString &path,XvProject *project,QString &error)
{
    if(path.isEmpty())
    {
        error="流程文件路径为空";
        return nullptr;
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
    {
        error=QString("无法读取流程文件[%1]: %2").arg(path,file.errorString());
        return nullptr;
    }

    QDomDocument document;
    QString parseError;
    int errorLine=0;
    int errorColumn=0;
    if(!document.setContent(&file,&parseError,&errorLine,&errorColumn))
    {
        error=QString("流程XML解析失败(%1:%2): %3")
                .arg(errorLine).arg(errorColumn).arg(parseError);
        return nullptr;
    }

    QDomElement root=document.documentElement();
    if(root.tagName()!="XVisionFlow"
            || !XvXml::validateAttributes(root,{"format","version"},
                                          {"format","version"},error)
            || !XvXml::validateChildren(root,{"Flow"},{"Flow"},error))
    {
        if(error.isEmpty()) error="流程文件根节点必须为<XVisionFlow>";
        return nullptr;
    }
    if(root.attribute("format")!=XvXml::FlowFileFormat)
    {
        error=QString("不支持的流程文件格式[%1]").arg(root.attribute("format"));
        return nullptr;
    }
    if(root.attribute("version")!=XvXml::FlowFileVersion)
    {
        error=QString("不支持的流程文件版本[%1]").arg(root.attribute("version"));
        return nullptr;
    }

    QDomElement flowElement=root.firstChildElement("Flow");
    if(!remapFlowIds(flowElement,error)) return nullptr;

    XvFlow *candidate=new XvFlow(project,flowElement.attribute("name"));
    flowElement.setAttribute("id",candidate->flowId());
    if(!candidate->fromXmlElement(flowElement))
    {
        error=candidate->lastErrorMsg();
        delete candidate;
        return nullptr;
    }
    return candidate;
}
}

/**************************************************************/
//* [XvCoreManager]
/**************************************************************/

XvCoreManager::XvCoreManager(QObject *parent)
    : QObject{parent},d_ptr(new XvCoreManagerPrivate(this))
{
    registerTokenMsgAble();
}

XvCoreManager::~XvCoreManager()
{
    unRegisterTokenMsgAble();
};

XvCoreManager *XvCoreManager::s_Instance = NULL;
XvCoreManager *XvCoreManager::getInstance() {
  if (!s_Instance) {
     QMutex s_Mutex;
    QMutexLocker locker(&s_Mutex);
    if (!s_Instance) {
      s_Instance = new XvCoreManager();
    }
  }
  return s_Instance;
}

XvPluginManager *XvCoreManager::getPlgMgr() const
{
    return XvPlgMgr;
}

XvFuncAssembly *XvCoreManager::getXvFuncAsm() const
{
    return XvFuncAsm;
}

bool XvCoreManager::init()
{
    XvPlgMgr->init();
    return true;
}

bool XvCoreManager::uninit()
{
    XvPlgMgr->uninit();
    Q_D(XvCoreManager);
    if(d->project)
    {
       if(d->project->release())
       {
           d->project->deleteLater();
       }
       else
       {
           delete d->project;
           d->project=nullptr;
       }
    }
    return true;
}

XvProject* XvCoreManager::getXvProject() const
{
    Q_D(const XvCoreManager);
    return d->project;
}

QString XvCoreManager::projectFileSuffix()
{
    return QString::fromLatin1(XvXml::ProjectFileSuffix);
}

QString XvCoreManager::flowFileSuffix()
{
    return QString::fromLatin1(XvXml::FlowFileSuffix);
}

XvProject* XvCoreManager::createNewXvProject(const QString &name)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return nullptr;
    }
    XvProject *project=new XvProject(name,this);
    if(d->project)
    {
        emit sgXvProjectAboutToReplace(d->project);
        delete d->project;
    }
    d->project=project;
    emit sgUpdateXvProject(d->project);
    return d->project;
}

bool XvCoreManager::validateXvProjectFile(const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }
    QString error;
    XvProject *candidate=readProjectFile(path,error);
    if(!candidate)
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ValidateProjectError,
                                       "项目文件校验失败: %1")).arg(error);
        Log_Error(d->lastErrorMsg);
        return false;
    }
    delete candidate;
    return true;
}

bool XvCoreManager::loadXvProject(const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }

    QString error;
    XvProject *candidate=readProjectFile(path,error);
    if(!candidate)
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_LoadProjectError,
                                       "打开项目失败: %1")).arg(error);
        Log_Error(d->lastErrorMsg);
        return false;
    }
    if(hasRunningXvFlow())
    {
        delete candidate;
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }

    XvProject *oldProject=d->project;
    if(oldProject) emit sgXvProjectAboutToReplace(oldProject);
    candidate->setParent(this);
    d->project=candidate;
    delete oldProject;
    candidate->registerTokenMsgAble();
    for(XvFlow *flow:candidate->getXvFlows())
    {
        if(!flow) continue;
        flow->registerTokenMsgAble();
        for(XvFunc *function:flow->getXvFuncs())
        {
            if(function) function->registerTokenMsgAble();
        }
    }
    emit sgUpdateXvProject(d->project);
    Log_Event(QString(getLang(Core_XvCoreMgr_LoadProjectSuccess,
                             "项目[%1]打开成功")).arg(d->project->projectName()));
    return true;
}

bool XvCoreManager::saveXvProject(const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }
    if(!d->project)
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectIsNull,
                                "保存项目失败，当前项目为空");
        Log_Error(d->lastErrorMsg);
        return false;
    }
    if(path.isEmpty())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectPathEmpty,
                                "保存项目失败，文件路径为空");
        Log_Error(d->lastErrorMsg);
        return false;
    }

    QDomDocument document;
    document.appendChild(document.createProcessingInstruction(
                             "xml","version=\"1.0\" encoding=\"UTF-8\""));
    QDomElement root=document.createElement("XVisionProject");
    root.setAttribute("format",XvXml::ProjectFileFormat);
    root.setAttribute("version",XvXml::ProjectFileVersion);
    QDomElement projectElement=d->project->toXmlElement(document);
    if(projectElement.isNull())
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_SaveProjectError,
                                       "保存项目失败: %1"))
                .arg(d->project->lastErrorMsg());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    root.appendChild(projectElement);
    document.appendChild(root);

    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_SaveProjectError,
                                       "保存项目失败: %1")).arg(file.errorString());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    const QByteArray xml=document.toByteArray(2);
    if(file.write(xml)!=xml.size() || !file.commit())
    {
        file.cancelWriting();
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_SaveProjectError,
                                       "保存项目失败: %1")).arg(file.errorString());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    Log_Event(QString(getLang(Core_XvCoreMgr_SaveProjectSuccess,
                             "项目[%1]保存成功")).arg(d->project->projectName()));
    return true;
}

bool XvCoreManager::validateXvFlowFile(const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }

    QString error;
    XvFlow *candidate=readFlowFile(path,d->project,error);
    if(!candidate)
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ValidateFlowError,
                                       "流程文件校验失败: %1")).arg(error);
        Log_Error(d->lastErrorMsg);
        return false;
    }
    delete candidate;
    return true;
}

XvFlow *XvCoreManager::importXvFlow(const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return nullptr;
    }
    if(!d->project)
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ImportFlowError,
                                       "导入流程失败: %1"))
                .arg(getLang(Core_XvCoreMgr_CurrentProjectIsNull,
                             "当前项目为空"));
        Log_Error(d->lastErrorMsg);
        return nullptr;
    }

    XvProject *targetProject=d->project;
    QString error;
    XvFlow *candidate=readFlowFile(path,targetProject,error);
    if(!candidate)
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ImportFlowError,
                                       "导入流程失败: %1")).arg(error);
        Log_Error(d->lastErrorMsg);
        return nullptr;
    }
    if(hasRunningXvFlow() || d->project!=targetProject)
    {
        delete candidate;
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ImportFlowError,
                                       "导入流程失败: %1"))
                .arg(getLang(Core_XvCoreMgr_ProjectStateChanged,
                             "项目状态已改变"));
        Log_Warn(d->lastErrorMsg);
        return nullptr;
    }

    const QString flowId=candidate->flowId();
    const QString flowName=candidate->flowName();
    QPointer<XvFlow> candidateGuard(candidate);
    if(!targetProject->adoptXvFlow(candidate))
    {
        if(!candidate->parent()) delete candidate;
        QString reason=targetProject->lastErrorMsg();
        if(reason.isEmpty())
        {
            reason=getLang(Core_XvCoreMgr_AdoptFlowFailed,
                           "项目无法接纳候选流程");
        }
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ImportFlowError,
                                       "导入流程失败: %1"))
                .arg(reason);
        Log_Error(d->lastErrorMsg);
        return nullptr;
    }
    if(!candidateGuard || targetProject->getXvFlow(flowId)!=candidate)
    {
        if(candidateGuard) delete candidateGuard.data();
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ImportFlowError,
                                       "导入流程失败: %1"))
                .arg(getLang(Core_XvCoreMgr_FlowUiRestoreFailed,
                             "流程界面恢复未完成"));
        Log_Error(d->lastErrorMsg);
        return nullptr;
    }

    Log_Event(QString(getLang(Core_XvCoreMgr_ImportFlowSuccess,
                             "流程[%1]导入成功")).arg(flowName));
    return candidate;
}

bool XvCoreManager::exportXvFlow(const QString &flowId,const QString &path)
{
    Q_D(XvCoreManager);
    d->lastErrorMsg.clear();
    if(hasRunningXvFlow())
    {
        d->lastErrorMsg=getLang(Core_XvCoreMgr_ProjectRunning,
                                "项目中存在正在运行的流程，请先停止所有流程");
        Log_Warn(d->lastErrorMsg);
        return false;
    }
    XvFlow *flow=d->project?d->project->getXvFlow(flowId):nullptr;
    if(!flow)
    {
        const QString reason=QString(getLang(Core_XvCoreMgr_FlowNotFound,
                                             "流程ID[%1]不存在")).arg(flowId);
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ExportFlowError,
                                       "导出流程失败: %1")).arg(reason);
        Log_Error(d->lastErrorMsg);
        return false;
    }
    if(path.isEmpty())
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ExportFlowError,
                                       "导出流程失败: %1"))
                .arg(getLang(Core_XvCoreMgr_FlowPathEmpty,
                             "文件路径为空"));
        Log_Error(d->lastErrorMsg);
        return false;
    }

    QDomDocument document;
    document.appendChild(document.createProcessingInstruction(
                             "xml","version=\"1.0\" encoding=\"UTF-8\""));
    QDomElement root=document.createElement("XVisionFlow");
    root.setAttribute("format",XvXml::FlowFileFormat);
    root.setAttribute("version",XvXml::FlowFileVersion);
    QDomElement flowElement=flow->toXmlElement(document);
    if(flowElement.isNull())
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ExportFlowError,
                                       "导出流程失败: %1"))
                .arg(flow->lastErrorMsg());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    root.appendChild(flowElement);
    document.appendChild(root);

    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))
    {
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ExportFlowError,
                                       "导出流程失败: %1")).arg(file.errorString());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    const QByteArray xml=document.toByteArray(2);
    if(file.write(xml)!=xml.size() || !file.commit())
    {
        file.cancelWriting();
        d->lastErrorMsg=QString(getLang(Core_XvCoreMgr_ExportFlowError,
                                       "导出流程失败: %1")).arg(file.errorString());
        Log_Error(d->lastErrorMsg);
        return false;
    }
    Log_Event(QString(getLang(Core_XvCoreMgr_ExportFlowSuccess,
                             "流程[%1]导出成功")).arg(flow->flowName()));
    return true;
}

bool XvCoreManager::hasRunningXvFlow() const
{
    Q_D(const XvCoreManager);
    if(!d->project) return false;
    if(d->project->isRunning()) return true;
    for(XvFlow *flow:d->project->getXvFlows())
    {
        if(flow && flow->isRunning()) return true;
    }
    return false;
}

QString XvCoreManager::lastErrorMsg()
{
    Q_D(XvCoreManager);
    const QString message=d->lastErrorMsg;
    d->lastErrorMsg.clear();
    return message;
}
