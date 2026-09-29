#include "OpenCvImageOperatorWdg.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include <limits>

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include "XRotateRectRoi.h"
#include <XObjectBaseType>

using namespace XvCore;

namespace
{
QList<QPair<QString,int>> integerChoices(const QString &name)
{
    if(name=="operation") return {{"Create",0},{"Find",1}};
    if(name=="method") return {{"Recursive",0},{"Normalized convolution",1}};
    if(name=="outputVariant") return {{"Grayscale",0},{"Color",1}};
    if(name=="conversion") return {{"Grayscale",0},{"BGR",1},{"BGRA",2}};
    if(name=="channel") return {{"Blue",0},{"Green",1},{"Red",2}};
    if(name=="thresholdType")
        return {{"Binary",0},{"Binary inverse",1},{"Truncate",2},
                {"To zero",3},{"To zero inverse",4}};
    if(name=="comparison")
        return {{"Greater",0},{"Greater or equal",1},{"Equal",2},
                {"Less or equal",3},{"Less",4},{"Not equal",5}};
    if(name=="flipCode") return {{"Both axes",-1},{"Vertical",0},{"Horizontal",1}};
    if(name=="interpolation")
        return {{"Nearest",0},{"Linear",1},{"Cubic",2},{"Area",3},{"Lanczos",4}};
    if(name=="rectificationInterpolation")
        return {{"Nearest",0},{"Linear",1},{"Cubic",2},{"Lanczos",3}};
    if(name=="rotateCode")
        return {{"90 clockwise",0},{"180",1},{"90 counterclockwise",2}};
    if(name=="apertureSize") return {{"3",3},{"5",5},{"7",7}};
    if(name=="subdivisionOutput") return {{"Delaunay",0},{"Alternate",1}};
    if(name=="stitchMode") return {{"Panorama",0},{"Scans",1}};
    if(name=="kernelShape") return {{"Rectangle",0},{"Cross",1},{"Ellipse",2}};
    if(name=="borderType")
        return {{"Constant",0},{"Replicate",1},{"Reflect",2},{"Reflect 101",3}};
    if(name=="retrievalMode")
        return {{"External",0},{"List",1},{"Connected components",2},{"Tree",3}};
    if(name=="approximationMode")
        return {{"None",0},{"Simple",1},{"TC89 L1",2},{"TC89 KCOS",3}};
    if(name=="connectivity") return {{"4",4},{"8",8}};
    if(name=="connectedComponentsAlgorithm")
        return {{"Default",-1},{"WU",0},{"Grana",1},{"Bolelli",2},
                {"SAUF",3},{"BBDT",4},{"Spaghetti",5}};
    if(name=="akazeDescriptorType")
        return {{"KAZE",0},{"KAZE upright",1},{"MLDB",2},{"MLDB upright",3}};
    if(name=="diffusivity")
        return {{"PM G1",0},{"PM G2",1},{"Weickert",2},{"Charbonnier",3}};
    if(name=="orbScoreType") return {{"Harris",0},{"FAST",1}};
    return {};
}
}

OpenCvImageOperatorWdg::OpenCvImageOperatorWdg(OpenCvImageOperatorBase *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent)
{
    m_form=new QFormLayout(centralWidget());
    m_mode=new QComboBox(centralWidget());
    m_form->addRow(getUiText("Mode"),m_mode);

    const int propertyIndex=func->metaObject()->indexOfProperty("mode");
    if(propertyIndex>=0)
    {
        const QMetaProperty property=func->metaObject()->property(propertyIndex);
        const QMetaEnum values=property.enumerator();
        for(int index=0;index<values.keyCount();++index)
            m_mode->addItem(getUiText(QString::fromLatin1(values.key(index))),values.value(index));
    }
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,func](int index)
    {
        if(index<0) return;
        func->setProperty("mode",m_mode->itemData(index));
        scheduleParameterRebuild();
    });
    setMinimumSize(460,260);
}

void OpenCvImageOperatorWdg::onShow()
{
    reloadMode();
    rebuildParameters();
}

void OpenCvImageOperatorWdg::reloadMode()
{
    auto func=getFunc<OpenCvImageOperatorBase>();
    if(!func) return;
    const QSignalBlocker blocker(m_mode);
    m_mode->setCurrentIndex(m_mode->findData(func->property("mode")));
}

void OpenCvImageOperatorWdg::rebuildParameters()
{
    auto func=getFunc<OpenCvImageOperatorBase>();
    if(!func || !m_form) return;
    while(m_form->rowCount()>1) m_form->removeRow(1);
    for(const QString &name:func->activeParameterNames())
    {
        XObject *object=func->getParamsByName(name);
        if(!object) continue;
        QWidget *editor=createParameterEditor(object);
        if(editor) editor->setObjectName(name);
        if(editor) m_form->addRow(getUiText(object->dispalyName().isEmpty()?name:object->dispalyName()),editor);
    }
    prepareParameterLayout();
}

QWidget *OpenCvImageOperatorWdg::createParameterEditor(XObject *object)
{
    auto func=getFunc<OpenCvImageOperatorBase>();
    if(!func || !object) return nullptr;

    if(auto value=dynamic_cast<XInt*>(object))
    {
        const QList<QPair<QString,int>> choices=integerChoices(object->objectName());
        if(!choices.isEmpty())
        {
            auto editor=new QComboBox(centralWidget());
            for(const auto &choice:choices) editor->addItem(getUiText(choice.first),choice.second);
            editor->setCurrentIndex(editor->findData(value->value()));
            connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,
                    [value,editor](int index)
            {
                if(index>=0) value->setValue(editor->itemData(index).toInt());
            });
            return editor;
        }
        auto editor=new QSpinBox(centralWidget());
        editor->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
        editor->setValue(value->value());
        connect(editor,qOverload<int>(&QSpinBox::valueChanged),this,
                [value](int candidate) { value->setValue(candidate); });
        return editor;
    }
    if(auto value=dynamic_cast<XReal*>(object))
    {
        auto editor=new QDoubleSpinBox(centralWidget());
        editor->setRange(-1000000000.0,1000000000.0);
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
        const bool rebuildOnChange=object->objectName()=="useKernel";
        connect(editor,&QCheckBox::toggled,this,
                [this,value,rebuildOnChange](bool candidate)
        {
            value->setValue(candidate);
            if(rebuildOnChange)
                scheduleParameterRebuild();
        });
        return editor;
    }
    if(auto value=dynamic_cast<XString*>(object))
    {
        if(object->objectName()=="algorithm")
        {
            auto editor=new QComboBox(centralWidget());
            editor->addItems({"edsr","espcn","fsrcnn","lapsrn"});
            editor->setCurrentText(value->value().trimmed().toLower());
            connect(editor,&QComboBox::currentTextChanged,this,
                    [value](const QString &candidate) { value->setValue(candidate); });
            return editor;
        }
        auto editor=new QLineEdit(value->value(),centralWidget());
        connect(editor,&QLineEdit::editingFinished,this,
                [value,editor]() { value->setValue(editor->text()); });
        if(object->objectName().endsWith("Path"))
        {
            auto container=new QWidget(centralWidget());
            auto layout=new QHBoxLayout(container);
            layout->setContentsMargins(0,0,0,0);
            auto browse=new QToolButton(container);
            browse->setIcon(QIcon(":/images/UiOpen.svg"));
            browse->setToolTip(getUiText("Select file"));
            layout->addWidget(editor,1);
            layout->addWidget(browse);
            connect(browse,&QToolButton::clicked,this,[value,editor,this]()
            {
                const QString path=QFileDialog::getOpenFileName(this,getUiText("Select file"),editor->text());
                if(path.isEmpty()) return;
                editor->setText(path);
                value->setValue(path);
            });
            return container;
        }
        return editor;
    }
    if(dynamic_cast<XImage*>(object) || dynamic_cast<XObjectList*>(object)
            || dynamic_cast<XRotateRectRoi*>(object))
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
