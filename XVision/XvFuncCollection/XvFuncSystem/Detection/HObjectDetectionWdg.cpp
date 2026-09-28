#include "HObjectDetectionWdg.h"
#include "ui_HObjectDetectionWdg.h"

#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStyle>

using namespace XvCore;

HObjectDetectionWdg::HObjectDetectionWdg(
        HObjectDetection *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent),ui(new Ui::HObjectDetectionWdg)
{
    ui->setupUi(centralWidget());
    initFrm();
}

HObjectDetectionWdg::~HObjectDetectionWdg()
{
    delete ui;
}

void HObjectDetectionWdg::initFrm()
{
    initFixedSize();
    auto func=getFunc<HObjectDetection>();
    if(!func) return;

    updateLbTextWithXObject(ui->lbInputImage,func->param->inputImage,":");
    updateLbTextWithXObject(ui->lbMinConfidence,func->param->minConfidence,":");
    updateLbTextWithXObject(ui->lbMaxOverlap,func->param->maxOverlap,":");
    updateLbTextWithXObject(ui->lbClassAgnostic,
                            func->param->maxOverlapClassAgnostic,":");
    updateLbTextWithXObject(ui->lbMaxDetections,
                            func->param->maxNumDetections,":");
    ui->lbModel->setText(getLang(
                "XvFuncSystem_HObjectDetection_Model","模型文件")+":");
    ui->lbPreprocess->setText(getLang(
                "XvFuncSystem_HObjectDetection_Preprocess","预处理文件")+":");
    ui->lbRuntime->setText(getLang(
                "XvFuncSystem_HObjectDetection_Runtime","运行设备")+":");
    ui->lbAssetState->setText(getLang(
                "XvFuncSystem_HObjectDetection_AssetState","资产状态")+":");
    ui->lbClasses->setText(getLang(
                "XvFuncSystem_HObjectDetection_Classes","类别")+":");

    const QIcon openIcon=style()->standardIcon(QStyle::SP_DialogOpenButton);
    ui->btnModel->setIcon(openIcon);
    ui->btnModel->setText(QString());
    ui->btnModel->setToolTip(getLang(
                "XvFuncSystem_HObjectDetection_SelectModel","选择HALCON检测模型"));
    ui->btnPreprocess->setIcon(openIcon);
    ui->btnPreprocess->setText(QString());
    ui->btnPreprocess->setToolTip(getLang(
                "XvFuncSystem_HObjectDetection_SelectPreprocess",
                "选择HALCON预处理字典"));

    ui->cmbRuntime->addItem("CPU",HObjectDetection::Cpu);
    ui->cmbRuntime->addItem("GPU",HObjectDetection::Gpu);
    ui->dsbMinConfidence->setRange(0.0,1.0);
    ui->dsbMinConfidence->setDecimals(3);
    ui->dsbMinConfidence->setSingleStep(0.05);
    ui->dsbMaxOverlap->setRange(0.0,1.0);
    ui->dsbMaxOverlap->setDecimals(3);
    ui->dsbMaxOverlap->setSingleStep(0.05);
    ui->spbMaxDetections->setRange(1,10000);

    connect(ui->cmbInputImage,&QComboBox::currentIndexChanged,this,[=]()
    {
        if(!m_bShowing) return;
        if(ui->cmbInputImage->currentIndex()==0)
        {
            func->paramUnSubscribe(func->param->inputImage->objectName());
            return;
        }
        SBindResultTag tag;
        if(getCmbBindResultTag(ui->cmbInputImage,tag))
        {
            func->paramSubscribe(func->param->inputImage->objectName(),
                                 tag.func,tag.resultName);
        }
    });
    connect(ui->btnModel,&QAbstractButton::clicked,
            this,&HObjectDetectionWdg::selectModel);
    connect(ui->btnPreprocess,&QAbstractButton::clicked,
            this,&HObjectDetectionWdg::selectPreprocess);
    connect(ui->cmbRuntime,&QComboBox::currentIndexChanged,this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        func->setRuntime(HObjectDetection::Runtime(
                             ui->cmbRuntime->itemData(index).toInt()));
    });
    connect(ui->dsbMinConfidence,&QDoubleSpinBox::valueChanged,
            this,[=](double value)
    {
        if(m_bShowing) func->param->minConfidence->setValue(value);
    });
    connect(ui->dsbMaxOverlap,&QDoubleSpinBox::valueChanged,
            this,[=](double value)
    {
        if(m_bShowing) func->param->maxOverlap->setValue(value);
    });
    connect(ui->chbClassAgnostic,&QCheckBox::toggled,this,[=](bool checked)
    {
        if(m_bShowing) func->param->maxOverlapClassAgnostic->setValue(checked);
    });
    connect(ui->spbMaxDetections,&QSpinBox::valueChanged,this,[=](int value)
    {
        if(m_bShowing) func->param->maxNumDetections->setValue(value);
    });
}

void HObjectDetectionWdg::selectModel()
{
    auto func=getFunc<HObjectDetection>();
    if(!func) return;
    const QString path=QFileDialog::getOpenFileName(
                this,getLang("XvFuncSystem_HObjectDetection_SelectModel",
                             "选择HALCON检测模型"),func->modelPath(),
                getLang("XvFuncSystem_HObjectDetection_ModelFilter",
                        "HALCON深度学习模型 (*.hdl)"));
    if(path.isEmpty()) return;
    QString preprocess=func->preprocessPath();
    if(preprocess.isEmpty())
    {
        preprocess=QFileDialog::getOpenFileName(
                    this,getLang("XvFuncSystem_HObjectDetection_SelectPreprocess",
                                 "选择HALCON预处理字典"),QString(),
                    getLang("XvFuncSystem_HObjectDetection_DictFilter",
                            "HALCON字典 (*.hdict)"));
    }
    if(!preprocess.isEmpty()) configureCandidate(path,preprocess);
}

void HObjectDetectionWdg::selectPreprocess()
{
    auto func=getFunc<HObjectDetection>();
    if(!func) return;
    const QString path=QFileDialog::getOpenFileName(
                this,getLang("XvFuncSystem_HObjectDetection_SelectPreprocess",
                             "选择HALCON预处理字典"),func->preprocessPath(),
                getLang("XvFuncSystem_HObjectDetection_DictFilter",
                        "HALCON字典 (*.hdict)"));
    if(path.isEmpty()) return;
    QString model=func->modelPath();
    if(model.isEmpty())
    {
        model=QFileDialog::getOpenFileName(
                    this,getLang("XvFuncSystem_HObjectDetection_SelectModel",
                                 "选择HALCON检测模型"),QString(),
                    getLang("XvFuncSystem_HObjectDetection_ModelFilter",
                            "HALCON深度学习模型 (*.hdl)"));
    }
    if(!model.isEmpty()) configureCandidate(model,path);
}

void HObjectDetectionWdg::configureCandidate(
        const QString &modelPath,const QString &preprocessPath)
{
    auto func=getFunc<HObjectDetection>();
    if(!func) return;
    QString error;
    if(!func->configureAssets(modelPath,preprocessPath,error))
    {
        QMessageBox::warning(
                    this,getLang("XvFuncSystem_HObjectDetection_ConfigFailed",
                                 "目标检测配置失败"),error);
        return;
    }
    updateStatus();
}

void HObjectDetectionWdg::updateStatus()
{
    auto func=getFunc<HObjectDetection>();
    if(!func) return;
    ui->letModel->setText(func->modelPath());
    ui->letPreprocess->setText(func->preprocessPath());
    ui->lbAssetStateVal->setText(func->hasConfiguredAssets()
            ?func->assetSummary()
            :getLang("XvFuncSystem_HObjectDetection_NotConfigured","未配置"));
    ui->ptxClasses->setPlainText(func->classSummary().join('\n'));
}

void HObjectDetectionWdg::onFuncRunUpdate()
{
    updateStatus();
}

void HObjectDetectionWdg::onShow()
{
    auto func=getFunc<HObjectDetection>();
    if(!func) return;
    initCmbBindResultTag(func,ui->cmbInputImage,{XImage::type()},true);
    setCmbBind(func->param->inputImage,ui->cmbInputImage);
    const QSignalBlocker runtimeBlocker(ui->cmbRuntime);
    const QSignalBlocker confidenceBlocker(ui->dsbMinConfidence);
    const QSignalBlocker overlapBlocker(ui->dsbMaxOverlap);
    const QSignalBlocker agnosticBlocker(ui->chbClassAgnostic);
    const QSignalBlocker maxBlocker(ui->spbMaxDetections);
    const int runtimeIndex=ui->cmbRuntime->findData(func->runtime());
    ui->cmbRuntime->setCurrentIndex(runtimeIndex<0?0:runtimeIndex);
    ui->dsbMinConfidence->setValue(func->param->minConfidence->value());
    ui->dsbMaxOverlap->setValue(func->param->maxOverlap->value());
    ui->chbClassAgnostic->setChecked(
                func->param->maxOverlapClassAgnostic->value());
    ui->spbMaxDetections->setValue(func->param->maxNumDetections->value());
    updateStatus();
}
