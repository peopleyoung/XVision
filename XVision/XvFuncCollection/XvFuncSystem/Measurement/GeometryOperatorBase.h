#ifndef GEOMETRYOPERATORBASE_H
#define GEOMETRYOPERATORBASE_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"

class GeometryOperatorWdg;

namespace XvCore
{
class XVFUNCSYSTEM_EXPORT GeometryOperatorBase:public XvFunc
{
    Q_OBJECT
    friend class ::GeometryOperatorWdg;
public:
    explicit GeometryOperatorBase(QObject *parent=nullptr);
    ~GeometryOperatorBase() override;

    virtual QString selectorPropertyName() const=0;
    virtual QStringList activeParameterNames() const=0;

public slots:
    void onShowFunc() override;

protected:
    QPixmap funcIcon() override;

private:
    GeometryOperatorWdg *m_widget=nullptr;
};
}

#endif // GEOMETRYOPERATORBASE_H
