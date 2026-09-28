#ifndef XVXMLUTILS_H
#define XVXMLUTILS_H

#include "XvCoreGlobal.h"

#include <QDomDocument>
#include <QStringList>

class QObject;
class XObject;

namespace XvCore
{
namespace XvXml
{

XVCORE_EXPORT extern const char ProjectFileFormat[];
XVCORE_EXPORT extern const char ProjectFileVersion[];
XVCORE_EXPORT extern const char ProjectFileSuffix[];
XVCORE_EXPORT extern const char FlowFileFormat[];
XVCORE_EXPORT extern const char FlowFileVersion[];
XVCORE_EXPORT extern const char FlowFileSuffix[];

XVCORE_EXPORT bool isValidId(const QString &id);

XVCORE_EXPORT bool validateAttributes(const QDomElement &element,
                                      const QStringList &allowed,
                                      const QStringList &required,
                                      QString &error);
XVCORE_EXPORT bool validateChildren(const QDomElement &element,
                                    const QStringList &allowed,
                                    const QStringList &requiredSingle,
                                    QString &error);
XVCORE_EXPORT bool requiredAttribute(const QDomElement &element,
                                     const QString &name,
                                     QString &value,
                                     QString &error);
XVCORE_EXPORT bool unsignedAttribute(const QDomElement &element,
                                     const QString &name,
                                     unsigned int &value,
                                     QString &error);
XVCORE_EXPORT bool boolAttribute(const QDomElement &element,
                                 const QString &name,
                                 bool &value,
                                 QString &error);
XVCORE_EXPORT bool realAttribute(const QDomElement &element,
                                 const QString &name,
                                 double &value,
                                 QString &error);
XVCORE_EXPORT QDomElement singleChild(const QDomElement &element,
                                     const QString &tagName);

XVCORE_EXPORT bool isSupportedScalar(const XObject *object);
XVCORE_EXPORT bool appendScalar(QDomDocument &doc,
                                QDomElement &parent,
                                const QString &tagName,
                                const XObject *object,
                                QString &error);
XVCORE_EXPORT bool readScalar(const QDomElement &element,
                              XObject *object,
                              QString &error);
XVCORE_EXPORT bool scalarTypesCompatible(const XObject *parameter,
                                         const XObject *result);

XVCORE_EXPORT bool isSupportedValue(const XObject *object);
XVCORE_EXPORT bool appendValue(QDomDocument &doc,
                               QDomElement &parent,
                               const QString &tagName,
                               const XObject *object,
                               QString &error);
XVCORE_EXPORT bool readValue(const QDomElement &element,
                             XObject *object,
                             QString &error);
XVCORE_EXPORT bool valueTypesCompatible(const XObject *parameter,
                                        const XObject *result);

XVCORE_EXPORT bool appendProperty(QDomDocument &doc,
                                  QDomElement &parent,
                                  QObject *object,
                                  const QString &name,
                                  QString &error);
XVCORE_EXPORT bool readProperty(const QDomElement &element,
                                QObject *object,
                                const QString &expectedName,
                                QString &error);

}
}

#endif // XVXMLUTILS_H
