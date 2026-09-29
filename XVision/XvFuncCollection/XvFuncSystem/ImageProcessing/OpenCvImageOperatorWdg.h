#ifndef OPENCVIMAGEOPERATORWDG_H
#define OPENCVIMAGEOPERATORWDG_H

#include "BaseSystemFuncWdg.h"

class QComboBox;
class QFormLayout;

namespace XvCore
{
class OpenCvImageOperatorBase;
}

class OpenCvImageOperatorWdg:public BaseSystemFuncWdg
{
public:
    explicit OpenCvImageOperatorWdg(XvCore::OpenCvImageOperatorBase *func,
                                    QWidget *parent=nullptr);

protected slots:
    void onShow() override;

private:
    void reloadMode();
    void rebuildParameters() override;
    QWidget *createParameterEditor(XObject *object);

    QFormLayout *m_form=nullptr;
    QComboBox *m_mode=nullptr;
};

#endif // OPENCVIMAGEOPERATORWDG_H
