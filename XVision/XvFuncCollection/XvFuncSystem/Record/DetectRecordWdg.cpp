#include "DetectRecordWdg.h"

#include "DetectRecord.h"
#include "XVisionRuntimeData.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <XObjectBaseType>

using namespace XvCore;

DetectRecordWdg::DetectRecordWdg(DetectRecord *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    m_form=new QFormLayout(centralWidget());
    m_mode=new QComboBox(centralWidget());
    m_form->addRow(getLang("XvFuncSystem_DetectRecord_Mode","模式"),m_mode);
    const int propertyIndex=func->metaObject()->indexOfProperty("mode");
    if(propertyIndex>=0)
    {
        const QMetaEnum values=func->metaObject()->property(propertyIndex).enumerator();
        for(int index=0;index<values.keyCount();++index)
            m_mode->addItem(getUiText(QString::fromLatin1(values.key(index))),values.value(index));
    }
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func](int index)
    {
        if(index<0) return;
        func->setMode(DetectRecord::Mode(m_mode->itemData(index).toInt()));
        rebuildParameters();
    });
    setMinimumSize(520,320);
}

void DetectRecordWdg::onShow()
{
    reloadMode();
    rebuildParameters();
}

void DetectRecordWdg::reloadMode()
{
    auto func=getFunc<DetectRecord>();
    if(!func) return;
    const QSignalBlocker blocker(m_mode);
    m_mode->setCurrentIndex(m_mode->findData(int(func->mode())));
}

void DetectRecordWdg::rebuildParameters()
{
    auto func=getFunc<DetectRecord>();
    if(!func || !m_form) return;
    while(m_form->rowCount()>1) m_form->removeRow(1);
    for(const QString &name:func->activeParameterNames())
    {
        XObject *object=func->getParamsByName(name);
        if(!object) continue;
        QWidget *editor=createParameterEditor(object);
        if(editor)
            m_form->addRow(getUiText(object->dispalyName().isEmpty()?name:object->dispalyName()),editor);
    }
    adjustSize();
    resize(qMax(width(),520),qMax(height(),320));
}

QWidget *DetectRecordWdg::createParameterEditor(XObject *object)
{
    auto func=getFunc<DetectRecord>();
    if(!func || !object) return nullptr;
    if(auto value=dynamic_cast<XInt*>(object))
    {
        auto editor=new QSpinBox(centralWidget());
        if(object->objectName()=="busyTimeoutMs") editor->setRange(0,60000);
        else if(object->objectName()=="limit") editor->setRange(1,10000);
        else editor->setRange(-2147483647,2147483647);
        editor->setValue(value->value());
        connect(editor,qOverload<int>(&QSpinBox::valueChanged),this,
                [value](int candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XReal*>(object))
    {
        auto editor=new QDoubleSpinBox(centralWidget());
        editor->setRange(0.0,9007199254740991.0);
        editor->setDecimals(0);
        editor->setValue(value->value());
        connect(editor,qOverload<double>(&QDoubleSpinBox::valueChanged),this,
                [value](double candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XBool*>(object))
    {
        auto editor=new QCheckBox(centralWidget());
        editor->setChecked(value->value());
        connect(editor,&QCheckBox::toggled,this,[this,value](bool candidate)
        {
            value->setValue(candidate);
            if(value->objectName()=="useRecordInput") rebuildParameters();
        });
        return editor;
    }
    if(auto value=dynamic_cast<XString*>(object))
    {
        if(object->objectName()=="detailsJson")
        {
            auto editor=new QPlainTextEdit(value->value(),centralWidget());
            editor->setMinimumHeight(90);
            connect(editor,&QPlainTextEdit::textChanged,this,
                    [value,editor]() { value->setValue(editor->toPlainText()); });
            return editor;
        }
        auto editor=new QLineEdit(value->value(),centralWidget());
        connect(editor,&QLineEdit::editingFinished,this,
                [value,editor]() { value->setValue(editor->text()); });
        if(object->objectName()!="databasePath") return editor;
        auto container=new QWidget(centralWidget());
        auto layout=new QHBoxLayout(container);
        layout->setContentsMargins(0,0,0,0);
        auto browse=new QToolButton(container);
        browse->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
        browse->setToolTip(getLang("XvFuncSystem_DetectRecord_SelectDatabase",
                                  "选择数据库文件"));
        layout->addWidget(editor,1);
        layout->addWidget(browse);
        connect(browse,&QToolButton::clicked,this,[this,value,editor]()
        {
            const QString path=QFileDialog::getSaveFileName(
                        this,getLang("XvFuncSystem_DetectRecord_SelectDatabase",
                                     "选择数据库文件"),editor->text(),
                        getLang("XvFuncSystem_DetectRecord_DatabaseFilter",
                                "SQLite 数据库 (*.sqlite *.db);;所有文件 (*)"));
            if(path.isEmpty()) return;
            editor->setText(path);
            value->setValue(path);
        });
        return container;
    }
    if(dynamic_cast<XDetectRecord*>(object))
    {
        auto editor=new QComboBox(centralWidget());
        initCmbBindResultTag(func,editor,{XDetectRecord::type()},true);
        XvFunc::SubscribeInfo info;
        if(func->getParamSubscribe(object->objectName(),info))
            setCmbBindResultTag(editor,SBindResultTag(info.first,info.second));
        connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,
                [this,func,object,editor](int index)
        {
            if(index==0)
            {
                if(m_bShowing) func->paramUnSubscribe(object->objectName());
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
