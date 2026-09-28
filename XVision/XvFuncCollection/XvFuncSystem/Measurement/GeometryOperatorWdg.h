#ifndef GEOMETRYOPERATORWDG_H
#define GEOMETRYOPERATORWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;
class QFormLayout;

namespace XvCore
{
class GeometryOperatorBase;
}

class GeometryOperatorWdg:public BaseSystemFuncWdg
{
public:
    explicit GeometryOperatorWdg(XvCore::GeometryOperatorBase *func,
                                 QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    void reloadSelector();
    void rebuildParameters();
    QWidget *createParameterEditor(XObject *object);

    QFormLayout *m_form=nullptr;
    QComboBox *m_selector=nullptr;
};

#endif // GEOMETRYOPERATORWDG_H
