#ifndef DETECTRECORDWDG_H
#define DETECTRECORDWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;
class QFormLayout;

namespace XvCore { class DetectRecord; }

class DetectRecordWdg:public BaseSystemFuncWdg
{
public:
    explicit DetectRecordWdg(XvCore::DetectRecord *func,QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    void reloadMode();
    void rebuildParameters();
    QWidget *createParameterEditor(XObject *object);

    QFormLayout *m_form=nullptr;
    QComboBox *m_mode=nullptr;
};

#endif // DETECTRECORDWDG_H
