#include "ConditionalFlowWdg.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>

#include "ConditionalFlow.h"

using namespace XvCore;

ConditionalFlowWdg::ConditionalFlowWdg(ConditionalFlow *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    auto layout=new QFormLayout(centralWidget());
    m_source=new QComboBox(centralWidget());
    m_value=new QComboBox(centralWidget());
    initCmbByBool(m_value);
    layout->addRow(new QLabel("Condition source",centralWidget()),m_source);
    layout->addRow(new QLabel("Condition",centralWidget()),m_value);
    setFixedSize(420,180);

    connect(m_source,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int)
    {
        setCmbWithCmbEnable(m_source,m_value,func->m_param->condition->objectName());
    });
    connect(m_value,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int index)
    {
        if(m_source->currentIndex()==0)
            func->m_param->condition->setValue(index==1);
    });
}

void ConditionalFlowWdg::onShow()
{
    auto func=getFunc<ConditionalFlow>();
    if(!func) return;
    const QSignalBlocker sourceBlocker(m_source);
    const QSignalBlocker valueBlocker(m_value);
    initCmbBindResultTag(func,m_source,{XBool::type()},true);
    XvFunc::SubscribeInfo info;
    if(func->getParamSubscribe("condition",info))
        setCmbBindResultTag(m_source,SBindResultTag(info.first,info.second));
    m_value->setCurrentIndex(func->m_param->condition->value()?1:0);
    m_value->setEnabled(m_source->currentIndex()==0);
}
