#include "XvGlobalState.h"
#include "XvXmlUtils.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>
#include <limits>
using namespace XvCore;
namespace {
bool xmlTextValid(const QString &text) {
    for(int index=0;index<text.size();++index) {
        const auto code=text.at(index).unicode();
        if((code<0x20 && code!=9 && code!=10 && code!=13) || code==0xfffe || code==0xffff)return false;
        if(QChar::isHighSurrogate(code)) {if(++index>=text.size() || !text.at(index).isLowSurrogate())return false;}
        else if(QChar::isLowSurrogate(code))return false;
    }
    return true;
}
}
bool XvGlobalState::validateValue(const QString &type,const QJsonValue &value,QString &error) {
    bool valid=false;
    if(type=="bool") valid=value.isBool();
    if(type=="string") valid=value.isString() && value.toString().size()<=65536 && xmlTextValid(value.toString());
    if(type=="real") valid=value.isDouble() && std::isfinite(value.toDouble());
    if(type=="int") {const double n=value.toDouble(); valid=value.isDouble() && std::isfinite(n) && std::floor(n)==n && n>=std::numeric_limits<int>::min() && n<=std::numeric_limits<int>::max();}
    if(!valid) error=QStringLiteral("值与类型 %1 不匹配或超出范围").arg(type);
    return valid;
}
bool XvGlobalState::validate(QString &error) const {
    if(!xmlTextValid(script)){error=QStringLiteral("脚本含有无法保存到项目文件的控制字符");return false;}
    if(variables.size()>256 || script.size()>262144) {error=QStringLiteral("全局变量最多 256 个，脚本最多 256K 字符");return false;}
    const QRegularExpression name("^[A-Za-z_][A-Za-z0-9_]{0,63}$");
    for(auto it=variables.begin();it!=variables.end();++it) {
        if(!name.match(it.key()).hasMatch() || !it.value().isObject()) {error=QStringLiteral("变量名必须为字母或下划线开头的 1–64 位标识符");return false;}
        const auto variable=it.value().toObject();
        if(variable.size()!=3 || !variable.value("type").isString() || !variable.value("description").isString() || variable.value("description").toString().size()>1024 || !xmlTextValid(variable.value("description").toString()) || !validateValue(variable.value("type").toString(),variable.value("value"),error)) {
            if(error.isEmpty()) error=QStringLiteral("变量定义无效");error=it.key()+": "+error;return false;
        }
    }
    return true;
}
QDomElement XvGlobalState::toXml(QDomDocument &doc) const {
    auto root=doc.createElement("Globals");
    for(auto it=variables.begin();it!=variables.end();++it) {
        const auto data=it.value().toObject();auto variable=doc.createElement("Variable");
        variable.setAttribute("name",it.key());variable.setAttribute("type",data.value("type").toString());
        variable.setAttribute("description",data.value("description").toString());
        // JSON preserves the exact scalar type, whitespace and multiline strings.
        variable.appendChild(doc.createTextNode(QString::fromUtf8(QJsonDocument(QJsonObject{{"value",data.value("value")}}).toJson(QJsonDocument::Compact))));root.appendChild(variable);
    }
    auto source=doc.createElement("Script");source.setAttribute("language","javascript");
    source.appendChild(doc.createTextNode(script));root.appendChild(source);return root;
}
bool XvGlobalState::fromXml(const QDomElement &element,XvGlobalState &state,QString &error) {
    XvGlobalState parsed;
    if(element.isNull()) {state=parsed;return true;}
    if(!element.nextSiblingElement("Globals").isNull() || !XvXml::validateAttributes(element,{}, {},error) || !XvXml::validateChildren(element,{"Variable","Script"},{"Script"},error)) {if(error.isEmpty())error=QStringLiteral("全局配置节点重复或无效");return false;}
    for(auto node=element.firstChildElement("Variable");!node.isNull();node=node.nextSiblingElement("Variable")) {
        if(!XvXml::validateAttributes(node,{"name","type","description"},{"name","type","description"},error) || !node.firstChildElement().isNull()) {if(error.isEmpty())error=QStringLiteral("变量节点无效");return false;}
        auto name=node.attribute("name");if(parsed.variables.contains(name)) {error=QStringLiteral("全局变量重名: ")+name;return false;}
        QJsonParseError jsonError;const auto data=QJsonDocument::fromJson(node.text().toUtf8(),&jsonError);
        if(jsonError.error!=QJsonParseError::NoError || !data.isObject() || data.object().size()!=1 || !data.object().contains("value")) {error=QStringLiteral("全局变量值无效: ")+name;return false;}
        parsed.variables.insert(name,QJsonObject{{"type",node.attribute("type")},{"description",node.attribute("description")},{"value",data.object().value("value")}});
    }
    const auto script=element.firstChildElement("Script");
    if(!script.nextSiblingElement("Script").isNull() || !XvXml::validateAttributes(script,{"language"},{"language"},error) || script.attribute("language")!="javascript" || !script.firstChildElement().isNull()) {error=QStringLiteral("脚本语言或节点无效");return false;}
    parsed.script=script.text();if(!parsed.validate(error))return false;state=parsed;return true;
}
