#include "ProjectScript.h"
#include "XvProject.h"
#include "XvFlow.h"
#include "XvFunc.h"
#include <XObjectBaseType>
#include <QJsonDocument>
#include <QThread>
#include <memory>
#include <vector>
namespace {
QString scalarType(XObject *object) {
    if(dynamic_cast<XInt*>(object))return "int";
    if(dynamic_cast<XReal*>(object))return "real";
    if(dynamic_cast<XBool*>(object))return "bool";
    if(dynamic_cast<XString*>(object))return "string";
    return {};
}
QJsonValue scalarValue(XObject *object) {
    if(auto value=dynamic_cast<XInt*>(object))return value->value();
    if(auto value=dynamic_cast<XReal*>(object))return value->value();
    if(auto value=dynamic_cast<XBool*>(object))return value->value();
    if(auto value=dynamic_cast<XString*>(object))return value->value();
    return {};
}
std::unique_ptr<XObject> makeScalar(const QString &type,const QJsonValue &value) {
    if(type=="int")return std::unique_ptr<XObject>(new XInt(value.toInt()));
    if(type=="real")return std::unique_ptr<XObject>(new XReal(value.toDouble()));
    if(type=="bool")return std::unique_ptr<XObject>(new XBool(value.toBool()));
    return std::unique_ptr<XObject>(new XString(QString(),value.toString(),nullptr));
}
}
QString scalarValueText(const QJsonValue &value) {
    if(value.isBool())return value.toBool()?"true":"false";
    if(value.isDouble())return QString::number(value.toDouble(),'g',17);
    return value.toString();
}
bool parseScalarText(const QString &type,const QString &text,QJsonValue &value,QString &error) {
    if(type=="string")value=text;
    else if(type=="bool") {if(text=="true" || text==QStringLiteral("真"))value=true;else if(text=="false" || text==QStringLiteral("假"))value=false;else {error=QStringLiteral("布尔值请填写 true 或 false");return false;}}
    else {bool ok=false;double number=text.toDouble(&ok);if(!ok){error=QStringLiteral("请输入有效数值");return false;}value=number;}
    return XvCore::XvGlobalState::validateValue(type,value,error);
}
bool captureProjectScript(XvCore::XvProject *project,ProjectScriptSnapshot &snapshot,QString &error) {
    if(!project || QThread::currentThread()!=project->thread() || project->hasActiveExecution()) {error=QStringLiteral("请先停止项目和流程运行，再读取变量");return false;}
    ProjectScriptSnapshot next;next.project=project;next.revision=project->globalRevision();
    QJsonObject globals,parameters,results;
    const auto state=project->globalState();
    for(auto it=state.variables.begin();it!=state.variables.end();++it) {
        const auto variable=it.value().toObject();globals.insert(it.key(),variable.value("value"));
        next.values.append({it.key(),QStringLiteral("全局变量"),it.key(),variable.value("type").toString(),variable.value("value"),false});
    }
    for(auto flow:project->getXvFlows()) for(auto function:flow->getXvFuncs()) {
        for(int kind=0;kind<2;++kind) {
            XObjectSet *set=kind==0?static_cast<XObjectSet*>(function->getParam()):static_cast<XObjectSet*>(function->getResult());if(!set)continue;
            for(auto object:set->childObjects()) {
                const QString type=scalarType(object);if(type.isEmpty())continue;
                const auto value=scalarValue(object);QString validation;
                // Transient non-finite results cannot cross the JSON boundary.
                if(!XvCore::XvGlobalState::validateValue(type,value,validation))continue;
                const QString key=flow->flowId()+"/"+function->funcId()+"/"+object->objectName();
                (kind==0?parameters:results).insert(key,value);
                next.values.append({key,flow->flowName()+" / "+function->funcName()+(kind==0?QStringLiteral(" / 参数"):QStringLiteral(" / 结果")),object->dispalyName().isEmpty()?object->objectName():object->dispalyName(),type,value,kind==0});
            }
        }
    }
    next.input={{"globals",globals},{"parameters",parameters},{"results",results}};
    if(QJsonDocument(next.input).toJson(QJsonDocument::Compact).size()>3*1024*1024) {error=QStringLiteral("项目标量数据超过脚本输入大小限制");return false;}
    snapshot=next;return true;
}
bool applyProjectScript(const ProjectScriptSnapshot &snapshot,const QJsonObject &result,QString &error) {
    if(!snapshot.project || snapshot.project->globalRevision()!=snapshot.revision) {error=QStringLiteral("项目或全局配置已更改；脚本结果未应用");return false;}
    ProjectScriptSnapshot current;if(!captureProjectScript(snapshot.project,current,error))return false;
    if(current.input!=snapshot.input) {error=QStringLiteral("项目变量已更改；脚本结果未应用");return false;}
    if(!result.value("globals").isObject() || !result.value("parameters").isObject()) {error=QStringLiteral("脚本结果结构无效");return false;}
    auto state=snapshot.project->globalState();const auto globals=result.value("globals").toObject();
    if(globals.keys()!=state.variables.keys()) {error=QStringLiteral("脚本不能增加或删除全局变量");return false;}
    for(auto it=globals.begin();it!=globals.end();++it) {auto definition=state.variables.value(it.key()).toObject();definition.insert("value",it.value());state.variables.insert(it.key(),definition);}
    if(!state.validate(error))return false;
    struct Change {XvCore::XvFunc *function;QString name;std::unique_ptr<XObject> before,after;};
    std::vector<Change> changes;const auto parameters=result.value("parameters").toObject();
    for(auto it=parameters.begin();it!=parameters.end();++it) {
        const auto parts=it.key().split('/');
        auto flow=parts.size()==3?snapshot.project->getXvFlow(parts.at(0)):nullptr;
        XvCore::XvFunc *function=nullptr;if(flow)for(auto candidate:flow->getXvFuncs())if(candidate->funcId()==parts.at(1))function=candidate;
        auto object=function?function->getParamsByName(parts.at(2)):nullptr;
        if(!object || function->isParamSubscribe(parts.at(2)) || scalarType(object).isEmpty()) {error=QStringLiteral("参数不存在、不支持或已绑定其他算子: ")+it.key();return false;}
        const auto type=scalarType(object);if(!XvCore::XvGlobalState::validateValue(type,it.value(),error)) {error=it.key()+": "+error;return false;}
        changes.push_back({function,parts.at(2),makeScalar(type,scalarValue(object)),makeScalar(type,it.value())});
    }
    size_t written=0;
    for(auto &change:changes) {
        if(!change.function->updataParam(change.name,change.after.get())) {
            for(size_t i=0;i<written;++i)changes[i].function->updataParam(changes[i].name,changes[i].before.get());
            error=QStringLiteral("参数更新失败，已回滚");return false;
        }
        ++written;
    }
    if(!snapshot.project->setGlobalState(state)) {
        for(auto &change:changes)change.function->updataParam(change.name,change.before.get());
        error=snapshot.project->lastErrorMsg();return false;
    }
    return true;
}
