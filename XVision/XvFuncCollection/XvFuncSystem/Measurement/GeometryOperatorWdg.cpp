#include "GeometryOperatorWdg.h"

#include "GeometryOperatorBase.h"
#include "XPoint2D.h"
#include "XVisionSharedData.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QSignalBlocker>
#include <QSpinBox>
#include <XObjectBaseType>

#include <limits>

using namespace XvCore;

namespace
{
bool isBindableGeometry(const XObject *object)
{
    return dynamic_cast<const XImage*>(object)
            || dynamic_cast<const XPoint2D*>(object)
            || dynamic_cast<const XLine2D*>(object)
            || dynamic_cast<const XCircle2D*>(object)
            || dynamic_cast<const XRect2D*>(object);
}
}

GeometryOperatorWdg::GeometryOperatorWdg(GeometryOperatorBase *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    m_form=new QFormLayout(centralWidget());
    m_selector=new QComboBox(centralWidget());
    m_form->addRow(getLang("XvFuncSystem_Geometry_Selector","类型"),m_selector);

    const QByteArray propertyName=func->selectorPropertyName().toLatin1();
    const int propertyIndex=func->metaObject()->indexOfProperty(propertyName.constData());
    if(propertyIndex>=0)
    {
        const QMetaProperty property=func->metaObject()->property(propertyIndex);
        const QMetaEnum values=property.enumerator();
        for(int index=0;index<values.keyCount();++index)
            m_selector->addItem(getUiText(QString::fromLatin1(values.key(index))),values.value(index));
    }
    connect(m_selector,qOverload<int>(&QComboBox::currentIndexChanged),this,
            [this,func,propertyName](int index)
    {
        if(index<0) return;
        func->setProperty(propertyName.constData(),m_selector->itemData(index));
        rebuildParameters();
    });
    setMinimumSize(480,280);
}

void GeometryOperatorWdg::onShow()
{
    reloadSelector();
    rebuildParameters();
}

void GeometryOperatorWdg::reloadSelector()
{
    auto func=getFunc<GeometryOperatorBase>();
    if(!func) return;
    const QSignalBlocker blocker(m_selector);
    m_selector->setCurrentIndex(m_selector->findData(
                                    func->property(func->selectorPropertyName().toLatin1().constData())));
}

void GeometryOperatorWdg::rebuildParameters()
{
    auto func=getFunc<GeometryOperatorBase>();
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
    resize(qMax(width(),480),qMax(height(),280));
}

QWidget *GeometryOperatorWdg::createParameterEditor(XObject *object)
{
    auto func=getFunc<GeometryOperatorBase>();
    if(!func || !object) return nullptr;

    if(auto value=dynamic_cast<XInt*>(object))
    {
        if(object->objectName()=="angleUnit")
        {
            auto editor=new QComboBox(centralWidget());
            editor->addItem(getLang("XvFuncSystem_Geometry_Degrees","度"),0);
            editor->addItem(getLang("XvFuncSystem_Geometry_Radians","弧度"),1);
            editor->setCurrentIndex(editor->findData(value->value()));
            connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,
                    [value,editor](int index)
            {
                if(index>=0) value->setValue(editor->itemData(index).toInt());
            });
            return editor;
        }
        auto editor=new QSpinBox(centralWidget());
        if(object->objectName()=="precision") editor->setRange(0,9);
        else editor->setRange(std::numeric_limits<int>::min(),
                              std::numeric_limits<int>::max());
        editor->setValue(value->value());
        connect(editor,qOverload<int>(&QSpinBox::valueChanged),this,
                [value](int candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XReal*>(object))
    {
        auto editor=new QDoubleSpinBox(centralWidget());
        editor->setRange(-1.0e12,1.0e12);
        editor->setDecimals(6);
        editor->setValue(value->value());
        connect(editor,qOverload<double>(&QDoubleSpinBox::valueChanged),this,
                [value](double candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XBool*>(object))
    {
        auto editor=new QCheckBox(centralWidget());
        editor->setChecked(value->value());
        connect(editor,&QCheckBox::toggled,this,
                [value](bool candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XString*>(object))
    {
        auto editor=new QLineEdit(value->value(),centralWidget());
        connect(editor,&QLineEdit::editingFinished,this,
                [value,editor]() { value->setValue(editor->text()); });
        return editor;
    }
    if(isBindableGeometry(object))
    {
        auto editor=new QComboBox(centralWidget());
        initCmbBindResultTag(func,editor,{object->typeName()},true);
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
