#ifndef NOTIFICATIONOUTPUTWDG_H
#define NOTIFICATIONOUTPUTWDG_H

#include "BaseSystemFuncWdg.h"
#include "NotificationOutput.h"

class QComboBox;
class QPlainTextEdit;

class NotificationOutputWdg:public BaseSystemFuncWdg
{
    Q_OBJECT
public:
    explicit NotificationOutputWdg(XvCore::NotificationOutput *function,
                                   QWidget *parent=nullptr);

protected:
    void initFrm() override;
    void onShow() override;

private:
    QComboBox *m_mode=nullptr;
    QComboBox *m_imageBinding=nullptr;
    QComboBox *m_messageBinding=nullptr;
    QPlainTextEdit *m_message=nullptr;
};

#endif // NOTIFICATIONOUTPUTWDG_H
