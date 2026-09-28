#include "FrmXvProjectConfig.h"
#include "ui_FrmXvProjectConfig.h"

#include <QHeaderView>
#include <QIcon>
#include <QTableWidgetItem>

#include "LangDef.h"
#include "UiUtils.h"
#include "XMessageBox.h"
#include "XvFlow.h"
#include "XvProject.h"

using namespace XvCore;

FrmXvProjectConfig::FrmXvProjectConfig(XvProject *project,QWidget *parent)
    :XFramelessDialog(parent),m_project(project),
      m_config(project?project->projectConfig():XvProjectConfig()),
      ui(new Ui::FrmXvProjectConfig)
{
    ui->setupUi(centralWidget());
    initFrm();
}

FrmXvProjectConfig::~FrmXvProjectConfig()
{
    delete ui;
}

void FrmXvProjectConfig::initFrm()
{
    setWindowTitle(getLang(App_FrmXvProjectConfig_Title,"项目运行配置"));
    setMinimumSize(520,420);
    ui->lbFlows->setText(getLang(App_FrmXvProjectConfig_MainFlows,"主流程"));
    ui->lbLoopInterval->setText(
                getLang(App_FrmXvProjectConfig_LoopInterval,"项目循环间隔(ms)"));
    ui->lbErrorPolicy->setText(
                getLang(App_FrmXvProjectConfig_ErrorPolicy,"流程失败时"));
    ui->btnUp->setIcon(QIcon(":/images/Ui/FrmXvFuncAsmUp.svg"));
    ui->btnUp->setToolTip(getLang(App_FrmXvProjectConfig_MoveUp,"上移流程"));
    ui->btnDown->setIcon(QIcon(":/images/Ui/FrmXvFuncAsmDown.svg"));
    ui->btnDown->setToolTip(getLang(App_FrmXvProjectConfig_MoveDown,"下移流程"));
    ui->btnConfirm->setText(getLang(App_UiCommon_Confirm,"确定"));
    ui->btnCancel->setText(getLang(App_UiCommon_Cancel,"取消"));

    ui->tbFlows->setColumnCount(2);
    ui->tbFlows->setHorizontalHeaderLabels({
        getLang(App_FrmXvProjectConfig_Enabled,"启用"),
        getLang(App_FrmXvProjectConfig_FlowName,"流程名称")});
    ui->tbFlows->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    ui->tbFlows->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    ui->tbFlows->verticalHeader()->setVisible(false);
    ui->tbFlows->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tbFlows->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tbFlows->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->spbLoopInterval->setRange(0,
                                 static_cast<int>(XvProjectConfig::MaximumLoopInterval));
    ui->spbLoopInterval->setValue(static_cast<int>(m_config.loopInterval));
    ui->cmbErrorPolicy->addItem(
                getLang(App_FrmXvProjectConfig_StopOnError,"中断项目"),
                static_cast<int>(EXvProjectFlowErrorPolicy::Stop));
    ui->cmbErrorPolicy->addItem(
                getLang(App_FrmXvProjectConfig_ContinueOnError,"继续下一流程"),
                static_cast<int>(EXvProjectFlowErrorPolicy::Continue));
    const int policyIndex=ui->cmbErrorPolicy->findData(
                static_cast<int>(m_config.flowErrorPolicy));
    ui->cmbErrorPolicy->setCurrentIndex(policyIndex<0?0:policyIndex);
    refreshFlowTable(m_config.mainFlows.isEmpty()?-1:0);

    connect(ui->tbFlows,&QTableWidget::itemChanged,this,
            [this](QTableWidgetItem *item)
    {
        if(m_refreshing || !item || item->column()!=0) return;
        const int row=item->row();
        if(row<0 || row>=m_config.mainFlows.count()) return;
        m_config.mainFlows[row].enabled=item->checkState()==Qt::Checked;
    });
    connect(ui->btnUp,&QToolButton::clicked,this,[this]() { moveCurrentFlow(-1); });
    connect(ui->btnDown,&QToolButton::clicked,this,[this]() { moveCurrentFlow(1); });
    connect(ui->btnConfirm,&QPushButton::clicked,
            this,&FrmXvProjectConfig::applyAndAccept);
    connect(ui->btnCancel,&QPushButton::clicked,this,&QDialog::reject);
    if(m_project)
    {
        connect(m_project,&QObject::destroyed,this,&QDialog::reject);
    }
    else
    {
        ui->btnConfirm->setEnabled(false);
    }
}

void FrmXvProjectConfig::refreshFlowTable(int selectedRow)
{
    m_refreshing=true;
    ui->tbFlows->setRowCount(m_config.mainFlows.count());
    for(int row=0;row<m_config.mainFlows.count();++row)
    {
        const XvProjectFlowEntry &entry=m_config.mainFlows.at(row);
        QTableWidgetItem *enabledItem=new QTableWidgetItem;
        enabledItem->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable
                              |Qt::ItemIsUserCheckable);
        enabledItem->setCheckState(entry.enabled?Qt::Checked:Qt::Unchecked);
        ui->tbFlows->setItem(row,0,enabledItem);

        QString flowName=entry.flowId;
        if(m_project)
        {
            XvFlow *flow=m_project->getXvFlow(entry.flowId);
            if(flow) flowName=flow->flowName();
        }
        QTableWidgetItem *nameItem=new QTableWidgetItem(flowName);
        nameItem->setToolTip(entry.flowId);
        ui->tbFlows->setItem(row,1,nameItem);
    }
    m_refreshing=false;

    if(selectedRow>=0 && selectedRow<ui->tbFlows->rowCount())
    {
        ui->tbFlows->selectRow(selectedRow);
    }
}

void FrmXvProjectConfig::moveCurrentFlow(int offset)
{
    const int row=ui->tbFlows->currentRow();
    const int target=row+offset;
    if(row<0 || target<0 || target>=m_config.mainFlows.count()) return;
    qSwap(m_config.mainFlows[row],m_config.mainFlows[target]);
    refreshFlowTable(target);
}

void FrmXvProjectConfig::applyAndAccept()
{
    if(!m_project) return;
    m_config.loopInterval=static_cast<unsigned int>(ui->spbLoopInterval->value());
    m_config.flowErrorPolicy=static_cast<EXvProjectFlowErrorPolicy>(
                ui->cmbErrorPolicy->currentData().toInt());
    if(!m_project->setProjectConfig(m_config))
    {
        QString error=m_project->lastErrorMsg();
        if(error.isEmpty())
        {
            error=getLang(App_FrmXvProjectConfig_ApplyFailed,"项目配置保存失败");
        }
        XMessageBox::warning(getLang(App_UiCommon_Warning,"警告"),error,this,
                             U_getXMessageBoxButtonTexts({XMessageBox::Close}),
                             XMessageBox::Close,XMessageBox::Close);
        return;
    }
    accept();
}
