#include "NotificationOutputWdg.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

using namespace XvCore;

NotificationOutputWdg::NotificationOutputWdg(NotificationOutput *function,
                                             QWidget *parent)
    :BaseSystemFuncWdg(function,parent)
{
    auto layout=new QVBoxLayout(centralWidget());
    auto form=new QFormLayout();
    m_mode=new QComboBox(centralWidget());
    m_imageBinding=new QComboBox(centralWidget());
    m_messageBinding=new QComboBox(centralWidget());
    m_message=new QPlainTextEdit(centralWidget());
    m_message->setMaximumHeight(80);
    form->addRow(getUiText("Mode"),m_mode);
    form->addRow(getUiText("Input image"),m_imageBinding);
    form->addRow(getUiText("Message source"),m_messageBinding);
    form->addRow(getUiText("Message"),m_message);
    layout->addLayout(form);
    initFrm();
}

void NotificationOutputWdg::initFrm()
{
    setFixedSize(440,260);
    auto function=getFunc<NotificationOutput>();
    if(!function) return;

    const QStringList modes={"OK","NG","Info","Success","Warning",
                             "Error","Fatal","Dialog"};
    for(int index=0;index<modes.size();++index) m_mode->addItem(getUiText(modes.at(index)),index);

    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(m_bShowing && index>=0) function->setMode(NotificationOutput::Mode(index));
    });
    connect(m_imageBinding,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        if(index==0)
        {
            function->paramUnSubscribe(function->m_param->inputImage->objectName());
            return;
        }
        SBindResultTag tag;
        if(getCmbBindResultTag(m_imageBinding,tag))
            function->paramSubscribe(function->m_param->inputImage->objectName(),
                                     tag.func,tag.resultName);
    });
    connect(m_messageBinding,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]()
    {
        setCmbWithPetEnable(m_messageBinding,m_message,
                            function->m_param->message->objectName());
    });
    connect(m_message,&QPlainTextEdit::textChanged,this,[=]()
    {
        if(m_bShowing && m_messageBinding->currentIndex()==0)
            function->m_param->message->setValue(m_message->toPlainText());
    });
}

void NotificationOutputWdg::onShow()
{
    auto function=getFunc<NotificationOutput>();
    if(!function) return;
    m_mode->setCurrentIndex(static_cast<int>(function->mode()));
    initCmbBindResultTag(function,m_imageBinding,{XImage::type()},true);
    initCmbBindResultTag(function,m_messageBinding,{XString::type()},true);
    setCmbBind(function->m_param->inputImage,m_imageBinding);
    setCmbBind(function->m_param->message,m_messageBinding);
    m_message->setPlainText(function->m_param->message->value());
    m_message->setEnabled(m_messageBinding->currentIndex()==0);
}
