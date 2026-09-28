#ifndef LOOPFLOWWDG_H
#define LOOPFLOWWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;
class QSpinBox;

namespace XvCore { class LoopFlow; }

class LoopFlowWdg:public BaseSystemFuncWdg
{
    Q_OBJECT
public:
    explicit LoopFlowWdg(XvCore::LoopFlow *func,QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    void updateModeControls();

    QComboBox *m_mode=nullptr;
    QSpinBox *m_start=nullptr;
    QSpinBox *m_end=nullptr;
    QSpinBox *m_step=nullptr;
    QComboBox *m_images=nullptr;
};

#endif // LOOPFLOWWDG_H
