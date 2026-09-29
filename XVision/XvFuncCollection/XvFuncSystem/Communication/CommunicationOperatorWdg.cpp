#include "CommunicationOperatorWdg.h"

#include "CommunicationOperatorBase.h"
#include "XVisionRuntimeData.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaProperty>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <XObjectBaseType>

using namespace XvCore;

namespace
{
QList<QPair<QString,int>> enumItems(const QString &name)
{
    if(name=="encoding")
        return {{getUiText(getLang("XvFuncSystem_Communication_Utf8","UTF-8")),0},
                {getUiText(getLang("XvFuncSystem_Communication_Latin1","Latin-1")),1}};
    if(name=="frameMode")
        return {{getUiText(getLang("XvFuncSystem_Communication_DelimiterFrame","Delimiter")),0},
                {getUiText(getLang("XvFuncSystem_Communication_FixedFrame","Fixed length")),1},
                {getUiText(getLang("XvFuncSystem_Communication_CloseQuietFrame","Close / quiet")),2}};
    if(name=="dataBits") return {{"5",5},{"6",6},{"7",7},{"8",8}};
    if(name=="parity")
        return {{getUiText(getLang("XvFuncSystem_Communication_None","None")),0},
                {getUiText(getLang("XvFuncSystem_Serial_EvenParity","Even")),2},
                {getUiText(getLang("XvFuncSystem_Serial_OddParity","Odd")),3},
                {getUiText(getLang("XvFuncSystem_Serial_SpaceParity","Space")),4},
                {getUiText(getLang("XvFuncSystem_Serial_MarkParity","Mark")),5}};
    if(name=="stopBits") return {{"1",1},{"2",2},{"1.5",3}};
    if(name=="flowControl")
        return {{getUiText(getLang("XvFuncSystem_Communication_None","None")),0},
                {getUiText(getLang("XvFuncSystem_Serial_HardwareFlow","Hardware")),1},
                {getUiText(getLang("XvFuncSystem_Serial_SoftwareFlow","Software")),2}};
    if(name=="byteOrder")
        return {{getUiText(getLang("XvFuncSystem_Modbus_BigEndian","Big endian")),0},
                {getUiText(getLang("XvFuncSystem_Modbus_LittleEndian","Little endian")),1}};
    if(name=="wordOrder")
        return {{getUiText(getLang("XvFuncSystem_Modbus_HighWordFirst","High word first")),0},
                {getUiText(getLang("XvFuncSystem_Modbus_LowWordFirst","Low word first")),1}};
    return {};
}

void spinRange(const QString &name,QSpinBox *editor)
{
    if(name=="port" || name=="localPort" || name=="remotePort") editor->setRange(1,65535);
    else if(name=="unitId") editor->setRange(1,247);
    else if(name=="address") editor->setRange(0,65535);
    else if(name=="timeoutMs" || name=="connectTimeoutMs") editor->setRange(1,600000);
    else if(name=="maxBytes" || name=="maxResponseBytes") editor->setRange(1,16*1024*1024);
    else if(name=="fixedLength") editor->setRange(1,16*1024*1024);
    else if(name=="baudRate") editor->setRange(1,4000000);
    else editor->setRange(-2147483647,2147483647);
}
}

CommunicationOperatorWdg::CommunicationOperatorWdg(CommunicationOperatorBase *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    m_form=new QFormLayout(centralWidget());
    m_mode=new QComboBox(centralWidget());
    m_form->addRow(getLang("XvFuncSystem_Communication_Mode","模式"),m_mode);
    const QStringList modeNames=func?func->communicationModeNames():QStringList();
    for(int index=0;index<modeNames.size();++index) m_mode->addItem(getUiText(modeNames.at(index)),index);
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int index)
    {
        if(!func || index<0) return;
        const int propertyIndex=func->metaObject()->indexOfProperty("mode");
        if(propertyIndex>=0)
            func->metaObject()->property(propertyIndex).write(func,m_mode->itemData(index));
        scheduleParameterRebuild();
    });
    setMinimumSize(560,360);
}

void CommunicationOperatorWdg::onShow()
{
    reloadMode();
    rebuildParameters();
}

void CommunicationOperatorWdg::reloadMode()
{
    auto func=getFunc<CommunicationOperatorBase>();
    if(!func) return;
    const int propertyIndex=func->metaObject()->indexOfProperty("mode");
    if(propertyIndex<0) return;
    const QSignalBlocker blocker(m_mode);
    m_mode->setCurrentIndex(m_mode->findData(
                                func->metaObject()->property(propertyIndex).read(func).toInt()));
}

void CommunicationOperatorWdg::rebuildParameters()
{
    auto func=getFunc<CommunicationOperatorBase>();
    if(!func || !m_form) return;
    while(m_form->rowCount()>1) m_form->removeRow(1);
    for(const QString &name:func->activeParameterNames())
    {
        XObject *object=func->getParamsByName(name);
        QWidget *editor=createParameterEditor(object);
        if(editor) editor->setObjectName(name);
        if(editor) m_form->addRow(getUiText(object->dispalyName().isEmpty()?name:object->dispalyName()),editor);
    }
    prepareParameterLayout();
}

QWidget *CommunicationOperatorWdg::createParameterEditor(XObject *object)
{
    auto func=getFunc<CommunicationOperatorBase>();
    if(!func || !object) return nullptr;
    if(auto value=dynamic_cast<XInt *>(object))
    {
        const QList<QPair<QString,int>> items=enumItems(object->objectName());
        if(!items.isEmpty())
        {
            auto editor=new QComboBox(centralWidget());
            for(const auto &item:items) editor->addItem(item.first,item.second);
            editor->setCurrentIndex(editor->findData(value->value()));
            connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,
                    [this,value,editor](int index)
            {
                if(index<0) return;
                value->setValue(editor->itemData(index).toInt());
                if(value->objectName()=="frameMode") scheduleParameterRebuild();
            });
            return bindableEditor(object,editor);
        }
        auto editor=new QSpinBox(centralWidget());
        spinRange(object->objectName(),editor);
        if(object->objectName()=="maxBytes" && func->funcRole()=="UdpText")
            editor->setMaximum(65507);
        editor->setValue(value->value());
        connect(editor,qOverload<int>(&QSpinBox::valueChanged),this,
                [value](int candidate) { value->setValue(candidate); });
        return bindableEditor(object,editor);
    }
    if(auto value=dynamic_cast<XBool *>(object))
    {
        auto editor=new QCheckBox(centralWidget());
        editor->setChecked(value->value());
        connect(editor,&QCheckBox::toggled,this,[this,value](bool candidate)
        {
            value->setValue(candidate);
            if(value->objectName()=="useJsonInput" || value->objectName()=="useByteInput"
                    || value->objectName()=="appendDelimiter") scheduleParameterRebuild();
        });
        return bindableEditor(object,editor);
    }
    if(auto value=dynamic_cast<XString *>(object))
    {
        if(object->objectName()=="headersJson" || object->objectName()=="payloadJson")
        {
            auto editor=new QPlainTextEdit(value->value(),centralWidget());
            editor->setMinimumHeight(70);
            connect(editor,&QPlainTextEdit::textChanged,this,
                    [value,editor]() { value->setValue(editor->toPlainText()); });
            return bindableEditor(object,editor);
        }
        auto editor=new QLineEdit(value->value(),centralWidget());
        connect(editor,&QLineEdit::editingFinished,this,
                [value,editor]() { value->setValue(editor->text()); });
        return bindableEditor(object,editor);
    }
    QString valueType;
    if(dynamic_cast<XJsonValue *>(object)) valueType=XJsonValue::type();
    else if(dynamic_cast<XByteArray *>(object)) valueType=XByteArray::type();
    if(!valueType.isEmpty())
    {
        auto editor=new QComboBox(centralWidget());
        initCmbBindResultTag(func,editor,{valueType},true);
        XvFunc::SubscribeInfo info;
        if(func->getParamSubscribe(object->objectName(),info))
            setCmbBindResultTag(editor,SBindResultTag(info.first,info.second));
        connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,
                [this,func,object,editor](int index)
        {
            if(index==0)
            {
                if(m_bShowing && func->isParamSubscribe(object->objectName()))
                    func->paramUnSubscribe(object->objectName());
                return;
            }
            SBindResultTag tag;
            if(getCmbBindResultTag(editor,tag))
                func->paramSubscribe(object->objectName(),tag.func,tag.resultName);
        });
        return editor;
    }
    return nullptr;
}

QWidget *CommunicationOperatorWdg::bindableEditor(XObject *object,QWidget *directEditor)
{
    auto func=getFunc<CommunicationOperatorBase>();
    if(!func || !object || !directEditor) return directEditor;
    auto container=new QWidget(centralWidget());
    auto layout=new QHBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);
    auto binding=new QComboBox(container);
    initCmbBindResultTag(func,binding,{object->typeName()},true);
    XvFunc::SubscribeInfo info;
    if(func->getParamSubscribe(object->objectName(),info))
        setCmbBindResultTag(binding,SBindResultTag(info.first,info.second));
    directEditor->setEnabled(!func->isParamSubscribe(object->objectName()));
    layout->addWidget(binding,1);
    layout->addWidget(directEditor,2);
    connect(binding,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func,object,binding,directEditor](int index)
    {
        if(index==0)
        {
            if(m_bShowing && func->isParamSubscribe(object->objectName()))
                func->paramUnSubscribe(object->objectName());
            directEditor->setEnabled(true);
            return;
        }
        SBindResultTag tag;
        if(getCmbBindResultTag(binding,tag))
        {
            func->paramSubscribe(object->objectName(),tag.func,tag.resultName);
            directEditor->setEnabled(false);
        }
    });
    return container;
}
