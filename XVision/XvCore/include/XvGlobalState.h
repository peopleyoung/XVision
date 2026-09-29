#ifndef XVGLOBALSTATE_H
#define XVGLOBALSTATE_H
#include "XvCoreGlobal.h"
#include <QJsonObject>
#include <QDomElement>
namespace XvCore {
// Each variable has a stable name and {type,value,description}; scripts use JSON scalars.
struct XVCORE_EXPORT XvGlobalState {
    QJsonObject variables;
    QString script;
    bool validate(QString &error) const;
    QDomElement toXml(QDomDocument &doc) const;
    static bool fromXml(const QDomElement &element,XvGlobalState &state,QString &error);
    static bool validateValue(const QString &type,const QJsonValue &value,QString &error);
};
}
#endif
