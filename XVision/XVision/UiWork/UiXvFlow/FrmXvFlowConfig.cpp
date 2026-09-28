#include "FrmXvFlowConfig.h"
#include "ui_FrmXvFlowConfig.h"
#include "XvFlow.h"
#include "XvProject.h"
#include "LangDef.h"
#include <QSignalBlocker>

FrmXvFlowConfig::FrmXvFlowConfig(XvFlow *flow,QWidget *parent) :
    XFramelessWidget(parent),m_flow(flow),
    ui(new Ui::FrmXvFlowConfig)
{
    ui->setupUi(this->centralWidget());
    initFrm();
}

FrmXvFlowConfig::~FrmXvFlowConfig()
{
    delete ui;
}

void FrmXvFlowConfig::initFrm()
{
    if(!m_flow) return;
    this->setWindowTitle(getLang(App_FrmXvFlowConfig_Title,"流程配置")+":"+m_flow->flowName());
    this->setFixedSize(this->centralWidget()->width()+20,this->centralWidget()->height()+10);
    ui->lbFlowLoopInterval->setText(getLang(App_FrmXvFlowConfig_FlowLoopInterval,"循环间隔(ms)")+":");
    ui->ckbFuncErrorInterruptRun->setText(getLang(App_FrmXvFlowConfig_FuncErrorInterruptRun,"算子错误中断运行"));
    auto config=m_flow->getFlowConfig();
    ui->spbFlowLoopInterval->setMinimum(0);
    ui->spbFlowLoopInterval->setMaximum(99999);
    ui->spbFlowLoopInterval->setValue(config->loopInterval);
    ui->ckbFuncErrorInterruptRun->setChecked(config->funcErrorInterruptRun);
    connect(m_flow,&XvFlow::destroyed,this,&FrmXvFlowConfig::close);
    if(m_flow->parProject())
    {
        connect(m_flow->parProject(),&XvProject::sgProjectRunStart,this,[this]()
        {
            if(ui) centralWidget()->setEnabled(false);
        });
        connect(m_flow->parProject(),&XvProject::sgProjectRunEnd,this,[this]()
        {
            if(ui && m_flow) centralWidget()->setEnabled(m_flow->isEditAllowed());
        });
    }
    centralWidget()->setEnabled(m_flow->isEditAllowed());
    connect(ui->ckbFuncErrorInterruptRun,&XMatCheckBox::toggled,this,[this,config]()
    {
        if(!m_flow || !m_flow->isEditAllowed())
        {
            QSignalBlocker blocker(ui->ckbFuncErrorInterruptRun);
            ui->ckbFuncErrorInterruptRun->setChecked(config->funcErrorInterruptRun);
            return;
        }
        auto bRet=ui->ckbFuncErrorInterruptRun->isChecked();
        config->funcErrorInterruptRun=bRet;
    });
    connect(ui->spbFlowLoopInterval,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this,config](int)
    {
        if(!m_flow || !m_flow->isEditAllowed())
        {
            QSignalBlocker blocker(ui->spbFlowLoopInterval);
            ui->spbFlowLoopInterval->setValue(static_cast<int>(config->loopInterval));
            return;
        }
        auto nVal=ui->spbFlowLoopInterval->value();
        config->loopInterval=nVal;
    });
}

void FrmXvFlowConfig::closeEvent(QCloseEvent *event)
{
    XFramelessWidget::closeEvent(event);
    this->deleteLater();
}
