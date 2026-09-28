#ifndef FRMXVPROJECTCONFIG_H
#define FRMXVPROJECTCONFIG_H

#include "XFramelessDialog.h"
#include "XvProjectConfig.h"

#include <QPointer>

namespace Ui {
class FrmXvProjectConfig;
}

namespace XvCore {
class XvProject;
}

class FrmXvProjectConfig : public XFramelessDialog
{
    Q_OBJECT

public:
    explicit FrmXvProjectConfig(XvCore::XvProject *project,
                                QWidget *parent=nullptr);
    ~FrmXvProjectConfig();

private:
    void initFrm();
    void refreshFlowTable(int selectedRow=-1);
    void moveCurrentFlow(int offset);

private slots:
    void applyAndAccept();

private:
    QPointer<XvCore::XvProject> m_project;
    XvCore::XvProjectConfig m_config;
    bool m_refreshing=false;
    Ui::FrmXvProjectConfig *ui;
};

#endif // FRMXVPROJECTCONFIG_H
