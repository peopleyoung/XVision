#include "BaseSystemFuncWdg.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QAbstractSpinBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#include "XTitleBar.h"
#include <QToolButton>

#include "XvFlow.h"
#include "XvProject.h"

BaseSystemFuncWdg::BaseSystemFuncWdg(XvFunc *func, QWidget *parent)
    :XFramelessWidget(parent),m_func(func)
{
    m_parameterScroll=new QScrollArea(this);
    m_parameterScroll->setObjectName("operatorParameterScroll");
    m_parameterScroll->setWidgetResizable(true);
    m_parameterScroll->setFrameShape(QFrame::NoFrame);
    m_parameterScroll->setAlignment(Qt::AlignTop|Qt::AlignLeft);
    m_parameterContent=new QWidget(m_parameterScroll);
    m_parameterContent->setObjectName("operatorParameterContent");
    m_parameterContent->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    m_parameterScroll->setWidget(m_parameterContent);
    XFramelessWidget::setCentralWidget(m_parameterScroll);
    setMinimumSize(400,280);
    resize(660,560);
    this->initFrm();
    if(func)
    {
        connect(func,&XvFunc::sgFuncRunEnd,this,[this]() {
            if(isVisible()) onFuncRunUpdate();
        });
        connect(func,&QObject::destroyed,this,[this]() {
            m_func.clear();
            setEnabled(false);
            hide();
        });
        if(auto flow=func->parFlow())
        {
            const auto updateEnabled=[this]() {
                auto flow=m_func?m_func->parFlow():nullptr;
                const bool enabled=flow && flow->isEditAllowed();
                centralWidget()->setEnabled(enabled);
                if(auto button=findChild<QToolButton*>("btnRun")) button->setEnabled(enabled);
            };
            connect(flow,&XvFlow::sgFlowRunStart,this,updateEnabled);
            connect(flow,&XvFlow::sgFlowRunEnd,this,updateEnabled);
            if(auto project=flow->parProject())
            {
                connect(project,&XvProject::sgProjectRunStart,this,updateEnabled);
                connect(project,&XvProject::sgProjectRunEnd,this,updateEnabled);
            }
        }
    }
}

BaseSystemFuncWdg::~BaseSystemFuncWdg()
{

}

void BaseSystemFuncWdg::initCmbBindResultTag(XvFunc *func,QComboBox *cmb,const QStringList &types,const bool &canCustom)
{
    if(!func||!cmb) return;
    cmb->clear();
    if(canCustom)
    {
        cmb->addItem(getLang("XvFuncSystem_BaseSystemFuncWdg_Custom","自定义"));
    }
    foreach (auto type, types)
    {
        auto funcAnstResults=  func->getAncestorsResultByType(type);
        foreach (auto func, funcAnstResults.keys())
        {
            auto lstRet=funcAnstResults[func];
            foreach (auto ret, lstRet)
            {
                SBindResultTag tag(func,ret->objectName());
                QVariant var=QVariant::fromValue(tag);
                QString itemStr=QString("%1.%2").arg(func->funcName()).arg(getUiText(ret->dispalyName()));
                cmb->addItem(itemStr,var);
            }
        }
    }
}

bool BaseSystemFuncWdg::getCmbBindResultTag(QComboBox *cmb, SBindResultTag &tag)
{
    if(!cmb) return false;
    auto idx=cmb->currentIndex();
    auto var=cmb->itemData(idx);
    if(!var.isValid())
    {
        return false;
    }
    auto tagTemp=var.value<SBindResultTag>();
    if(!tagTemp.isValid())
    {
        return false;
    }
    tag=tagTemp;
    return true;
}

bool BaseSystemFuncWdg::setCmbBindResultTag(QComboBox *cmb, const SBindResultTag &tag)
{
    if(!cmb) return false;
    auto count=cmb->count();
    for (int i = 0; i < count; ++i)
    {
        auto var=cmb->itemData(i);
        if(var.isValid())
        {
            auto tagTemp=var.value<SBindResultTag>();
            if(tagTemp.isValid())
            {
                if(tagTemp.func==tag.func&&tagTemp.resultName==tag.resultName)
                {
                    cmb->setCurrentIndex(i);
                    return true;
                }
            }
        }

    }
    return false;
}

void BaseSystemFuncWdg::updateLbTextWithXObject(QLabel* lb,XObject *obj,const QString &suffix)
{
    if(lb&&obj)
    {
        lb->setText(getUiText(obj->dispalyName())+suffix);
    }
}

void BaseSystemFuncWdg::initCmbByBool(QComboBox *cmb)
{
    cmb->clear();
    cmb->addItem(getUiText("False"));
    cmb->addItem(getUiText("True"));
}

void BaseSystemFuncWdg::setCmbBind(XObject *param, QComboBox *cmb)
{
    XvFunc::SubscribeInfo info;
    if(m_func && param && m_func->getParamSubscribe(param->objectName(),info))
    {
      setCmbBindResultTag(cmb,SBindResultTag(info.first,info.second));
    }
}

void BaseSystemFuncWdg::setCmbWithCmbEnable(QComboBox *cmbBind, QComboBox *cmbVal, const QString &paramName)
{
    if(!m_func) return;
    auto idx=cmbBind->currentIndex();
    if(idx<0) return;
    if(idx==0)
    {
        if(m_bShowing)
        {
            m_func->paramUnSubscribe(paramName);
        }
        cmbVal->setEnabled(true);
    }
    else
    {
        cmbVal->setEnabled(false);
        SBindResultTag tag;
        if(getCmbBindResultTag(cmbBind,tag))
        {
            m_func->paramSubscribe(paramName,tag.func,tag.resultName);
        }

    }
}

void BaseSystemFuncWdg::setCmbWithLetEnable(QComboBox *cmbBind, QLineEdit *letVal, const QString &paramName)
{
    if(!m_func) return;
    auto idx=cmbBind->currentIndex();
    if(idx<0) return;
    if(idx==0)
    {
        if(m_bShowing)
        {
            m_func->paramUnSubscribe(paramName);
        }
        letVal->setEnabled(true);
    }
    else
    {
        letVal->setEnabled(false);
        SBindResultTag tag;
        if(getCmbBindResultTag(cmbBind,tag))
        {
            m_func->paramSubscribe(paramName,tag.func,tag.resultName);
        }

    }
}

void BaseSystemFuncWdg::setCmbWithPetEnable(QComboBox *cmbBind, QPlainTextEdit *letVal, const QString &paramName)
{
    if(!m_func) return;
    auto idx=cmbBind->currentIndex();
    if(idx<0) return;
    if(idx==0)
    {
        if(m_bShowing)
        {
            m_func->paramUnSubscribe(paramName);
        }
        letVal->setEnabled(true);
    }
    else
    {
        letVal->setEnabled(false);
        SBindResultTag tag;
        if(getCmbBindResultTag(cmbBind,tag))
        {
            m_func->paramSubscribe(paramName,tag.func,tag.resultName);
        }

    }
}

void BaseSystemFuncWdg::initFrm()
{
    auto tb =this->titleBar();
    auto size=tb->height()-2;
    auto btnRun=new QToolButton(this);
    btnRun->setText("");
    btnRun->setObjectName("btnRun");
    btnRun->setMinimumSize(QSize(size, size));
    btnRun->setIconSize(QSize(size-4,size-4));
    btnRun->setToolTip(getLang("XvFuncSystem_BaseSystemFuncWdg_RunBtn","运行"));
    btnRun->setIcon(QIcon(":/images/XvFuncRun.svg"));
    connect(btnRun,&QToolButton::clicked,this,[=]()
    {
        if(m_func)
        {
            if(auto flow=m_func->parFlow())
            {
                if(flow->runFunctionOnce(m_func->funcId())==Ret_Xv_Success)
                    centralWidget()->setEnabled(false);
            }
        }

    });
    auto ckb=new QToolButton(this);
    ckb->setObjectName("btnPin");
    ckb->setCheckable(true);
    ckb->setIcon(QIcon(":/images/UiPin.svg"));
    ckb->setIconSize(QSize(18,18));
    ckb->setText("");
    QString strSetTop=getLang("XvFuncSystem_BaseSystemFuncWdg_SetTop","置顶");
    QString strSetTopCancel=getLang("XvFuncSystem_BaseSystemFuncWdg_SetTopCancel","取消置顶");
    ckb->setToolTip(strSetTop);
    connect(ckb,&QToolButton::toggled,this,[=]()
    {
        bool bChecked=ckb->isChecked();
        if(bChecked)
        {
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
            ckb->setToolTip(strSetTopCancel);
        }
        else
        {
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
            ckb->setToolTip(strSetTop);
        }
        this->show();

    });

    tb->rightLayout()->addWidget(btnRun);
    tb->rightLayout()->addWidget(ckb);   
}

void BaseSystemFuncWdg::initPreferredSize()
{
    // Designer geometry supplies a starting size, never a permanent constraint.
    const QSize hint=centralWidget()->sizeHint().expandedTo(centralWidget()->size());
    resize(qBound(480,hint.width()+36,760),qBound(320,hint.height()+64,680));
}

void BaseSystemFuncWdg::prepareParameterLayout()
{
    if(!centralWidget()) return;
    centralWidget()->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    if(auto layout=centralWidget()->layout())
    {
        layout->setContentsMargins(16,16,16,16);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
    }
    for(auto form:centralWidget()->findChildren<QFormLayout*>())
    {
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setFormAlignment(Qt::AlignTop|Qt::AlignLeft);
        form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(10);
        for(int row=0;row<form->rowCount();++row)
            if(auto item=form->itemAt(row,QFormLayout::LabelRole))
                if(auto label=qobject_cast<QLabel*>(item->widget()))
                {
                    label->setWordWrap(true);
                    label->setMaximumHeight(QWIDGETSIZE_MAX);
                }
    }
    const int rowHeight=qMax(30,fontMetrics().height()+12);
    for(auto editor:centralWidget()->findChildren<QWidget*>())
    {
        const bool input=qobject_cast<QComboBox*>(editor)
                || qobject_cast<QAbstractSpinBox*>(editor)
                || (qobject_cast<QLineEdit*>(editor)
                    && !qobject_cast<QAbstractSpinBox*>(editor->parentWidget())
                    && !qobject_cast<QComboBox*>(editor->parentWidget()));
        if(input)
        {
            editor->setMinimumHeight(rowHeight);
            editor->setMaximumHeight(qMax(editor->maximumHeight(),rowHeight));
            editor->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        }
        if(auto combo=qobject_cast<QComboBox*>(editor))
        {
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMinimumContentsLength(10);
        }
    }
    centralWidget()->updateGeometry();
}

void BaseSystemFuncWdg::scheduleParameterRebuild()
{
    if(m_rebuildPending) return;
    m_rebuildPending=true;
    QTimer::singleShot(0,this,[this]() {
        m_rebuildPending=false;
        if(!m_func) return;
        // The emitting editor has left its signal handler before deletion.
        rebuildParameters();
        prepareParameterLayout();
    });
}

void BaseSystemFuncWdg::showEvent(QShowEvent *event)
{
    if(m_func && m_func->parFlow())
        centralWidget()->setEnabled(m_func->parFlow()->isEditAllowed());
    m_bShowing=false;
    onShow();
    m_bShowing=true;
    prepareParameterLayout();
    if(!m_geometryInitialized)
    {
        m_geometryInitialized=true;
        QScreen *screen=windowHandle()?windowHandle()->screen():QGuiApplication::primaryScreen();
        if(screen)
        {
            const QRect available=screen->availableGeometry().adjusted(16,16,-16,-16);
            setMinimumSize(qMin(400,available.width()),qMin(280,available.height()));
            resize(qMin(width(),available.width()),qMin(height(),available.height()));
            move(available.center()-rect().center());
        }
    }
    if(m_func)
    {
        if(m_func->parFlow())
        {
            QString flowName=m_func->parFlow()->flowName();
            this->setWindowTitle(flowName+"."+getUiText(m_func->funcName()));
        }
        else
        {
            this->setWindowTitle(getUiText(m_func->funcName()));
        }

        this->setWindowIcon(m_func->funcIcon());
    }
    return XFramelessWidget::showEvent(event);
}

void BaseSystemFuncWdg::hideEvent(QHideEvent *event)
{
    onHide();
    return XFramelessWidget::hideEvent(event);
}

void BaseSystemFuncWdg::closeEvent(QCloseEvent *event)
{
    onClose();
    return XFramelessWidget::closeEvent(event);
}


