#include "LoopFlowWdg.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSpinBox>

#include <limits>

#include "LoopFlow.h"

using namespace XvCore;

LoopFlowWdg::LoopFlowWdg(LoopFlow *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    auto layout=new QFormLayout(centralWidget());
    m_mode=new QComboBox(centralWidget());
    m_mode->addItem(getUiText("For"),LoopFlow::For);
    m_mode->addItem(getUiText("Foreach images"),LoopFlow::ForeachImages);
    m_start=new QSpinBox(centralWidget());
    m_end=new QSpinBox(centralWidget());
    m_step=new QSpinBox(centralWidget());
    for(QSpinBox *spin:{m_start,m_end,m_step})
        spin->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
    m_images=new QComboBox(centralWidget());
    layout->addRow(getUiText("Mode"),m_mode);
    layout->addRow(getUiText("Start"),m_start);
    layout->addRow(getUiText("End"),m_end);
    layout->addRow(getUiText("Step"),m_step);
    layout->addRow(getUiText("Images"),m_images);
    setFixedSize(420,260);

    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int index)
    {
        func->setMode(static_cast<LoopFlow::Mode>(m_mode->itemData(index).toInt()));
        updateModeControls();
    });
    connect(m_start,qOverload<int>(&QSpinBox::valueChanged),this,
            [func](int value) { func->m_param->start->setValue(value); });
    connect(m_end,qOverload<int>(&QSpinBox::valueChanged),this,
            [func](int value) { func->m_param->end->setValue(value); });
    connect(m_step,qOverload<int>(&QSpinBox::valueChanged),this,
            [func](int value) { func->m_param->step->setValue(value); });
    connect(m_images,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int)
    {
        if(m_images->currentIndex()==0)
        {
            if(m_bShowing) func->paramUnSubscribe("images");
            return;
        }
        SBindResultTag tag;
        if(getCmbBindResultTag(m_images,tag))
            func->paramSubscribe("images",tag.func,tag.resultName);
    });
}

void LoopFlowWdg::onShow()
{
    auto func=getFunc<LoopFlow>();
    if(!func) return;
    const QSignalBlocker modeBlocker(m_mode);
    const QSignalBlocker startBlocker(m_start);
    const QSignalBlocker endBlocker(m_end);
    const QSignalBlocker stepBlocker(m_step);
    const QSignalBlocker imagesBlocker(m_images);
    m_mode->setCurrentIndex(m_mode->findData(func->mode()));
    m_start->setValue(func->m_param->start->value());
    m_end->setValue(func->m_param->end->value());
    m_step->setValue(func->m_param->step->value());
    initCmbBindResultTag(func,m_images,{XObjectList::type()},true);
    XvFunc::SubscribeInfo info;
    if(func->getParamSubscribe("images",info))
        setCmbBindResultTag(m_images,SBindResultTag(info.first,info.second));
    updateModeControls();
}

void LoopFlowWdg::updateModeControls()
{
    const bool forMode=m_mode->currentData().toInt()==LoopFlow::For;
    m_start->setEnabled(forMode);
    m_end->setEnabled(forMode);
    m_step->setEnabled(forMode);
    m_images->setEnabled(!forMode);
}
