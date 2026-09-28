#ifndef CONDITIONALFLOWWDG_H
#define CONDITIONALFLOWWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;

namespace XvCore { class ConditionalFlow; }

class ConditionalFlowWdg:public BaseSystemFuncWdg
{
    Q_OBJECT
public:
    explicit ConditionalFlowWdg(XvCore::ConditionalFlow *func,
                                QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    QComboBox *m_source=nullptr;
    QComboBox *m_value=nullptr;
};

#endif // CONDITIONALFLOWWDG_H
