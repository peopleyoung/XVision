#include "NOnnxOperatorWdg.h"

#include "NClassification.h"
#include "NInference.h"
#include "NObjectDetection.h"
#include "NSemanticSegmentation.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

using namespace XvCore;

namespace
{
QDoubleSpinBox *unitSpin(QWidget *parent)
{
    auto spin=new QDoubleSpinBox(parent);
    spin->setRange(0.0,1.0);
    spin->setDecimals(4);
    spin->setSingleStep(0.05);
    return spin;
}

QPlainTextEdit *classEditor(QWidget *parent)
{
    auto editor=new QPlainTextEdit(parent);
    editor->setMinimumHeight(80);
    editor->setMaximumHeight(140);
    return editor;
}
}

NOnnxOperatorWdg::NOnnxOperatorWdg(NOnnxBase *function,QWidget *parent)
    :BaseSystemFuncWdg(function,parent)
{
    auto content=centralWidget();
    auto contentLayout=new QVBoxLayout(content);
    auto commonGroup=new QGroupBox(getLang("XvFuncSystem_NOnnx_Preprocessing","模型与预处理"),content);
    auto commonForm=new QFormLayout(commonGroup);

    m_mode=new QComboBox(content);
    m_inputBinding=new QComboBox(content);
    auto modelRow=new QWidget(content);
    auto modelLayout=new QHBoxLayout(modelRow);
    modelLayout->setContentsMargins(0,0,0,0);
    m_modelPath=new QLineEdit(modelRow);
    m_modelPath->setReadOnly(true);
    m_selectModel=new QToolButton(modelRow);
    m_selectModel->setIcon(QIcon(":/images/UiOpen.svg"));
    m_selectModel->setToolTip(getLang("XvFuncSystem_NOnnx_SelectModel","选择ONNX模型"));
    modelLayout->addWidget(m_modelPath,1);
    modelLayout->addWidget(m_selectModel);
    m_modelStatus=new QLabel(content);
    m_modelStatus->setWordWrap(true);
    m_inputLayout=new QComboBox(content);
    m_inputWidth=new QSpinBox(content);
    m_inputHeight=new QSpinBox(content);
    m_channelOrder=new QComboBox(content);
    m_resizeMode=new QComboBox(content);
    m_paddingValue=new QSpinBox(content);
    m_pixelScale=new QDoubleSpinBox(content);
    m_meanValues=new QLineEdit(content);
    m_stdValues=new QLineEdit(content);

    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Mode","模式")+":",m_mode);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Input","输入")+":",m_inputBinding);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Model","模型")+":",modelRow);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_ModelStatus","模型状态")+":",m_modelStatus);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_InputLayout","输入布局")+":",m_inputLayout);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_InputWidth","动态输入宽度")+":",m_inputWidth);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_InputHeight","动态输入高度")+":",m_inputHeight);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_ChannelOrder","通道顺序")+":",m_channelOrder);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_ResizeMode","缩放方式")+":",m_resizeMode);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Padding","填充值")+":",m_paddingValue);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_PixelScale","像素比例")+":",m_pixelScale);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Mean","均值")+":",m_meanValues);
    commonForm->addRow(getLang("XvFuncSystem_NOnnx_Std","标准差")+":",m_stdValues);
    contentLayout->addWidget(commonGroup);

    m_classificationPanel=new QGroupBox(getLang("XvFuncSystem_NOnnx_ClassificationResults","分类输出"),content);
    auto classificationForm=new QFormLayout(m_classificationPanel);
    m_applySoftmax=new QCheckBox(m_classificationPanel);
    m_topK=new QSpinBox(m_classificationPanel);
    m_minScore=unitSpin(m_classificationPanel);
    m_classificationNames=classEditor(m_classificationPanel);
    classificationForm->addRow(getLang("XvFuncSystem_NClassification_Softmax","应用Softmax")+":",m_applySoftmax);
    classificationForm->addRow(getLang("XvFuncSystem_NClassification_TopK","前K项")+":",m_topK);
    classificationForm->addRow(getLang("XvFuncSystem_NClassification_MinScore","最小分数")+":",m_minScore);
    classificationForm->addRow(getLang("XvFuncSystem_NOnnx_ClassNames","类别名称")+":",m_classificationNames);
    contentLayout->addWidget(m_classificationPanel);

    m_detectionPanel=new QGroupBox(getLang("XvFuncSystem_NOnnx_DetectionResults","检测输出"),content);
    auto detectionForm=new QFormLayout(m_detectionPanel);
    m_normalizedCoordinates=new QCheckBox(m_detectionPanel);
    m_confidenceThreshold=unitSpin(m_detectionPanel);
    m_iouThreshold=unitSpin(m_detectionPanel);
    m_classAgnostic=new QCheckBox(m_detectionPanel);
    m_maximumDetections=new QSpinBox(m_detectionPanel);
    m_detectionNames=classEditor(m_detectionPanel);
    detectionForm->addRow(getLang("XvFuncSystem_NObjectDetection_Normalized","归一化坐标")+":",m_normalizedCoordinates);
    detectionForm->addRow(getLang("XvFuncSystem_NObjectDetection_Confidence","置信度阈值")+":",m_confidenceThreshold);
    detectionForm->addRow(getLang("XvFuncSystem_NObjectDetection_Iou","IoU阈值")+":",m_iouThreshold);
    detectionForm->addRow(getLang("XvFuncSystem_NObjectDetection_Agnostic","类别无关NMS")+":",m_classAgnostic);
    detectionForm->addRow(getLang("XvFuncSystem_NObjectDetection_Maximum","最大检测数")+":",m_maximumDetections);
    detectionForm->addRow(getLang("XvFuncSystem_NOnnx_ClassNames","类别名称")+":",m_detectionNames);
    contentLayout->addWidget(m_detectionPanel);

    m_segmentationPanel=new QGroupBox(getLang("XvFuncSystem_NOnnx_SegmentationResults","分割输出"),content);
    auto segmentationForm=new QFormLayout(m_segmentationPanel);
    m_outputLayout=new QComboBox(m_segmentationPanel);
    m_binaryThreshold=unitSpin(m_segmentationPanel);
    m_overlayOpacity=unitSpin(m_segmentationPanel);
    m_segmentationNames=classEditor(m_segmentationPanel);
    segmentationForm->addRow(getLang("XvFuncSystem_NSemanticSegmentation_OutputLayout","输出布局")+":",m_outputLayout);
    segmentationForm->addRow(getLang("XvFuncSystem_NSemanticSegmentation_BinaryThreshold","二值阈值")+":",m_binaryThreshold);
    segmentationForm->addRow(getLang("XvFuncSystem_NSemanticSegmentation_Opacity","叠加透明度")+":",m_overlayOpacity);
    segmentationForm->addRow(getLang("XvFuncSystem_NOnnx_ClassNames","类别名称")+":",m_segmentationNames);
    contentLayout->addWidget(m_segmentationPanel);
    contentLayout->addStretch(1);
    initFrm();
}

void NOnnxOperatorWdg::initFrm()
{
    setMinimumSize(400,280);
    resize(700,680);
    auto function=getFunc<NOnnxBase>();
    if(!function) return;

    m_inputLayout->addItem(getUiText("Auto"),NOnnxBase::Auto);
    m_inputLayout->addItem("NCHW",NOnnxBase::Nchw);
    m_inputLayout->addItem("NHWC",NOnnxBase::Nhwc);
    m_outputLayout->addItem(getUiText("Auto"),NOnnxBase::Auto);
    m_outputLayout->addItem("NCHW",NOnnxBase::Nchw);
    m_outputLayout->addItem("NHWC",NOnnxBase::Nhwc);
    m_channelOrder->addItem("RGB",NOnnxBase::Rgb);
    m_channelOrder->addItem("BGR",NOnnxBase::Bgr);
    m_resizeMode->addItem(getLang("XvFuncSystem_NOnnx_Stretch","拉伸"),NOnnxBase::Stretch);
    m_resizeMode->addItem(getUiText("Letterbox"),NOnnxBase::Letterbox);
    m_inputWidth->setRange(0,32768);
    m_inputHeight->setRange(0,32768);
    m_paddingValue->setRange(0,255);
    m_pixelScale->setRange(-1000000.0,1000000.0);
    m_pixelScale->setDecimals(9);
    m_topK->setRange(1,100000);
    m_maximumDetections->setRange(1,100000);

    if(qobject_cast<NInference*>(function))
    {
        m_mode->addItem(getLang("XvFuncSystem_NOnnx_Generic","通用"),NInference::Generic);
        m_mode->addItem(getLang("XvFuncSystem_NInference_Age","年龄"),NInference::Age);
    }
    else if(qobject_cast<NClassification*>(function))
    {
        m_mode->addItem(getLang("XvFuncSystem_NOnnx_Generic","通用"),NClassification::Generic);
        m_mode->addItem(getLang("XvFuncSystem_NClassification_Gender","性别"),NClassification::Gender);
    }
    else if(qobject_cast<NObjectDetection*>(function))
    {
        m_mode->addItem(getLang("XvFuncSystem_NOnnx_Generic","通用"),NObjectDetection::Generic);
        m_mode->addItem("YOLOv3",NObjectDetection::Yolov3);
        m_mode->addItem("YOLOv5",NObjectDetection::Yolov5);
        m_mode->addItem(getUiText("YOLOv5 Face"),NObjectDetection::Yolov5Face);
    }
    else if(qobject_cast<NSemanticSegmentation*>(function))
    {
        m_mode->addItem(getLang("XvFuncSystem_NOnnx_Generic","通用"),NSemanticSegmentation::Generic);
        m_mode->addItem(getLang("XvFuncSystem_NSemanticSegmentation_Human","人体"),NSemanticSegmentation::Human);
    }

    connect(m_selectModel,&QToolButton::clicked,this,&NOnnxOperatorWdg::selectModel);
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        const int mode=m_mode->itemData(index).toInt();
        if(auto value=qobject_cast<NInference*>(function)) value->setMode(NInference::Mode(mode));
        else if(auto value=qobject_cast<NClassification*>(function)) value->setMode(NClassification::Mode(mode));
        else if(auto value=qobject_cast<NObjectDetection*>(function)) value->setMode(NObjectDetection::Mode(mode));
        else if(auto value=qobject_cast<NSemanticSegmentation*>(function)) value->setMode(NSemanticSegmentation::Mode(mode));
        const bool wasShowing=m_bShowing;
        m_bShowing=false;
        onShow();
        m_bShowing=wasShowing;
    });
    connect(m_inputBinding,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        XObject *parameter=inputParameter();
        if(!parameter) return;
        if(index==0)
        {
            function->paramUnSubscribe(parameter->objectName());
            return;
        }
        SBindResultTag tag;
        if(getCmbBindResultTag(m_inputBinding,tag))
            function->paramSubscribe(parameter->objectName(),tag.func,tag.resultName);
    });
    connect(m_inputLayout,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(m_bShowing && index>=0) function->setInputLayout(
                    NOnnxBase::InputLayout(m_inputLayout->itemData(index).toInt()));
    });
    connect(m_inputWidth,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    { if(m_bShowing) function->setInputWidth(value); });
    connect(m_inputHeight,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    { if(m_bShowing) function->setInputHeight(value); });
    connect(m_channelOrder,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(m_bShowing && index>=0) function->setChannelOrder(
                    NOnnxBase::ImageChannelOrder(m_channelOrder->itemData(index).toInt()));
    });
    connect(m_resizeMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(m_bShowing && index>=0) function->setResizeMode(
                    NOnnxBase::ImageResizeMode(m_resizeMode->itemData(index).toInt()));
    });
    connect(m_paddingValue,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    { if(m_bShowing) function->setPaddingValue(value); });
    connect(m_pixelScale,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double value)
    { if(m_bShowing) function->setPixelScale(value); });
    connect(m_meanValues,&QLineEdit::editingFinished,this,[=]()
    { if(m_bShowing) function->setMeanValues(m_meanValues->text()); });
    connect(m_stdValues,&QLineEdit::editingFinished,this,[=]()
    { if(m_bShowing) function->setStdValues(m_stdValues->text()); });

    if(auto value=qobject_cast<NClassification*>(function))
    {
        connect(m_applySoftmax,&QCheckBox::toggled,this,[=](bool checked)
        { if(m_bShowing) value->setApplySoftmax(checked); });
        connect(m_topK,qOverload<int>(&QSpinBox::valueChanged),this,[=](int number)
        { if(m_bShowing) value->m_param->topK->setValue(number); });
        connect(m_minScore,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double number)
        { if(m_bShowing) value->m_param->minScore->setValue(number); });
        connect(m_classificationNames,&QPlainTextEdit::textChanged,this,[=]()
        { if(m_bShowing) value->setClassNames(m_classificationNames->toPlainText()); });
    }
    if(auto value=qobject_cast<NObjectDetection*>(function))
    {
        connect(m_normalizedCoordinates,&QCheckBox::toggled,this,[=](bool checked)
        { if(m_bShowing) value->setNormalizedCoordinates(checked); });
        connect(m_confidenceThreshold,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double number)
        { if(m_bShowing) value->m_param->confidenceThreshold->setValue(number); });
        connect(m_iouThreshold,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double number)
        { if(m_bShowing) value->m_param->iouThreshold->setValue(number); });
        connect(m_classAgnostic,&QCheckBox::toggled,this,[=](bool checked)
        { if(m_bShowing) value->m_param->classAgnosticNms->setValue(checked); });
        connect(m_maximumDetections,qOverload<int>(&QSpinBox::valueChanged),this,[=](int number)
        { if(m_bShowing) value->m_param->maximumDetections->setValue(number); });
        connect(m_detectionNames,&QPlainTextEdit::textChanged,this,[=]()
        { if(m_bShowing) value->setClassNames(m_detectionNames->toPlainText()); });
    }
    if(auto value=qobject_cast<NSemanticSegmentation*>(function))
    {
        connect(m_outputLayout,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index)
        {
            if(m_bShowing && index>=0) value->setOutputLayout(
                        NOnnxBase::InputLayout(m_outputLayout->itemData(index).toInt()));
        });
        connect(m_binaryThreshold,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double number)
        { if(m_bShowing) value->m_param->binaryThreshold->setValue(number); });
        connect(m_overlayOpacity,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double number)
        { if(m_bShowing) value->m_param->overlayOpacity->setValue(number); });
        connect(m_segmentationNames,&QPlainTextEdit::textChanged,this,[=]()
        { if(m_bShowing) value->setClassNames(m_segmentationNames->toPlainText()); });
    }
}

XObject *NOnnxOperatorWdg::inputParameter() const
{
    if(auto value=qobject_cast<NInference*>(m_func))
        return value->mode()==NInference::Generic
                ?static_cast<XObject*>(value->m_param->inputs)
               :static_cast<XObject*>(value->m_param->inputImage);
    if(auto value=qobject_cast<NClassification*>(m_func)) return value->m_param->inputImage;
    if(auto value=qobject_cast<NObjectDetection*>(m_func)) return value->m_param->inputImage;
    if(auto value=qobject_cast<NSemanticSegmentation*>(m_func)) return value->m_param->inputImage;
    return nullptr;
}

void NOnnxOperatorWdg::refreshBinding()
{
    auto function=getFunc<NOnnxBase>();
    XObject *parameter=inputParameter();
    if(!function || !parameter) return;
    initCmbBindResultTag(function,m_inputBinding,{parameter->typeName()},true);
    setCmbBind(parameter,m_inputBinding);
}

void NOnnxOperatorWdg::refreshStatus()
{
    auto function=getFunc<NOnnxBase>();
    if(!function) return;
    m_modelPath->setText(function->modelPath());
    m_modelStatus->setText(function->modelSummary());
}

void NOnnxOperatorWdg::selectModel()
{
    auto function=getFunc<NOnnxBase>();
    if(!function) return;
    const QString path=QFileDialog::getOpenFileName(
                this,getLang("XvFuncSystem_NOnnx_SelectModel","选择ONNX模型"),
                function->modelPath(),"ONNX (*.onnx)");
    if(path.isEmpty()) return;
    QString error;
    if(!function->configureModel(path,error))
    {
        QMessageBox::warning(this,getLang("XvFuncSystem_NOnnx_ConfigFailed",
                                          "ONNX模型配置失败"),error);
        return;
    }
    refreshStatus();
}

void NOnnxOperatorWdg::onShow()
{
    auto function=getFunc<NOnnxBase>();
    if(!function) return;
    int mode=0;
    if(auto value=qobject_cast<NInference*>(function)) mode=value->mode();
    else if(auto value=qobject_cast<NClassification*>(function)) mode=value->mode();
    else if(auto value=qobject_cast<NObjectDetection*>(function)) mode=value->mode();
    else if(auto value=qobject_cast<NSemanticSegmentation*>(function)) mode=value->mode();
    m_mode->setCurrentIndex(m_mode->findData(mode));
    refreshBinding();
    m_inputLayout->setCurrentIndex(m_inputLayout->findData(function->inputLayout()));
    m_inputWidth->setValue(function->inputWidth());
    m_inputHeight->setValue(function->inputHeight());
    m_channelOrder->setCurrentIndex(m_channelOrder->findData(function->channelOrder()));
    m_resizeMode->setCurrentIndex(m_resizeMode->findData(function->resizeMode()));
    m_paddingValue->setValue(function->paddingValue());
    m_pixelScale->setValue(function->pixelScale());
    m_meanValues->setText(function->meanValues());
    m_stdValues->setText(function->stdValues());
    m_classificationPanel->setVisible(qobject_cast<NClassification*>(function));
    m_detectionPanel->setVisible(qobject_cast<NObjectDetection*>(function));
    m_segmentationPanel->setVisible(qobject_cast<NSemanticSegmentation*>(function));
    if(auto value=qobject_cast<NClassification*>(function))
    {
        m_applySoftmax->setChecked(value->applySoftmax());
        m_topK->setValue(value->m_param->topK->value());
        m_minScore->setValue(value->m_param->minScore->value());
        m_classificationNames->setPlainText(value->classNames());
    }
    if(auto value=qobject_cast<NObjectDetection*>(function))
    {
        m_normalizedCoordinates->setChecked(value->normalizedCoordinates());
        m_confidenceThreshold->setValue(value->m_param->confidenceThreshold->value());
        m_iouThreshold->setValue(value->m_param->iouThreshold->value());
        m_classAgnostic->setChecked(value->m_param->classAgnosticNms->value());
        m_maximumDetections->setValue(value->m_param->maximumDetections->value());
        m_detectionNames->setPlainText(value->classNames());
    }
    if(auto value=qobject_cast<NSemanticSegmentation*>(function))
    {
        m_outputLayout->setCurrentIndex(m_outputLayout->findData(value->outputLayout()));
        m_binaryThreshold->setValue(value->m_param->binaryThreshold->value());
        m_overlayOpacity->setValue(value->m_param->overlayOpacity->value());
        m_segmentationNames->setPlainText(value->classNames());
    }
    refreshStatus();
}

void NOnnxOperatorWdg::onFuncRunUpdate()
{
    refreshStatus();
}
