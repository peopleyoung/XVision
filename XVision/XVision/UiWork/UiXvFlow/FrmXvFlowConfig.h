#ifndef FRMXVFLOWCONFIG_H
#define FRMXVFLOWCONFIG_H

#include "XFramelessWidget.h"
#include <QPointer>

namespace Ui {
class FrmXvFlowConfig;
}
namespace XvCore {
class XvFlow;
}
using namespace XvCore;
class FrmXvFlowConfig : public XFramelessWidget
{
    Q_OBJECT

public:
    explicit FrmXvFlowConfig(XvFlow *flow,QWidget *parent = nullptr);
    ~FrmXvFlowConfig();
protected:
    void initFrm();
signals:
    void sgClose();
    // QWidget interface
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::FrmXvFlowConfig *ui;
    QPointer<XvFlow> m_flow;


};

#endif // FRMXVFLOWCONFIG_H
