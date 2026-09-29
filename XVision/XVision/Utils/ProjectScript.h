#ifndef PROJECTSCRIPT_H
#define PROJECTSCRIPT_H
#include "XvGlobalState.h"
#include <QPointer>
#include <QVector>
namespace XvCore { class XvProject; }
struct ProjectValue {
    QString key,group,name,type;
    QJsonValue value;
    bool parameter=false;
};
struct ProjectScriptSnapshot {
    QPointer<XvCore::XvProject> project;
    quint64 revision=0;
    QJsonObject input;
    QVector<ProjectValue> values;
};
bool captureProjectScript(XvCore::XvProject *project,ProjectScriptSnapshot &snapshot,QString &error);
bool applyProjectScript(const ProjectScriptSnapshot &snapshot,const QJsonObject &result,QString &error);
QString scalarValueText(const QJsonValue &value);
bool parseScalarText(const QString &type,const QString &text,QJsonValue &value,QString &error);
#endif
