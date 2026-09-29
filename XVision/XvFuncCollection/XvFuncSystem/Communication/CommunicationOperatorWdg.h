#ifndef COMMUNICATIONOPERATORWDG_H
#define COMMUNICATIONOPERATORWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;
class QFormLayout;

namespace XvCore { class CommunicationOperatorBase; }

class CommunicationOperatorWdg:public BaseSystemFuncWdg
{
public:
    explicit CommunicationOperatorWdg(XvCore::CommunicationOperatorBase *func,
                                      QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    void reloadMode();
    void rebuildParameters() override;
    QWidget *createParameterEditor(XObject *object);
    QWidget *bindableEditor(XObject *object,QWidget *directEditor);

    QFormLayout *m_form=nullptr;
    QComboBox *m_mode=nullptr;
};

#endif // COMMUNICATIONOPERATORWDG_H
