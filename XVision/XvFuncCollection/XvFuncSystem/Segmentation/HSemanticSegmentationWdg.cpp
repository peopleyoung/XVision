#include "HSemanticSegmentationWdg.h"
#include "ui_HSemanticSegmentationWdg.h"

#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStyle>

using namespace XvCore;

HSemanticSegmentationWdg::HSemanticSegmentationWdg(
        HSemanticSegmentation *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent),ui(new Ui::HSemanticSegmentationWdg)
{
    ui->setupUi(centralWidget());
    initFrm();
}

HSemanticSegmentationWdg::~HSemanticSegmentationWdg()
{
    delete ui;
}

void HSemanticSegmentationWdg::initFrm()
{
    initFixedSize();
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;

    updateLbTextWithXObject(ui->lbInputImage,func->param->inputImage,":");
    ui->lbModel->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_Model","模型文件")+":");
    ui->lbPreprocess->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_Preprocess","预处理文件")+":");
    ui->lbRuntime->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_Runtime","运行设备")+":");
    ui->lbOpacity->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_Opacity","叠加透明度")+":");
    ui->lbAssetState->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_AssetState","资产状态")+":");
    ui->lbClasses->setText(getLang(
                "XvFuncSystem_HSemanticSegmentation_Classes","类别")+":");

    const QIcon openIcon=style()->standardIcon(QStyle::SP_DialogOpenButton);
    ui->btnModel->setIcon(openIcon);
    ui->btnModel->setText(QString());
    ui->btnModel->setToolTip(getLang(
                "XvFuncSystem_HSemanticSegmentation_SelectModel","选择HALCON模型"));
    ui->btnPreprocess->setIcon(openIcon);
    ui->btnPreprocess->setText(QString());
    ui->btnPreprocess->setToolTip(getLang(
                "XvFuncSystem_HSemanticSegmentation_SelectPreprocess",
                "选择HALCON预处理字典"));

    ui->cmbRuntime->addItem("CPU",HSemanticSegmentation::Cpu);
    ui->cmbRuntime->addItem("GPU",HSemanticSegmentation::Gpu);
    ui->dsbOpacity->setRange(0.0,1.0);
    ui->dsbOpacity->setDecimals(2);
    ui->dsbOpacity->setSingleStep(0.05);

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
            this,&HSemanticSegmentationWdg::selectModel);
    connect(ui->btnPreprocess,&QAbstractButton::clicked,
            this,&HSemanticSegmentationWdg::selectPreprocess);
    connect(ui->cmbRuntime,&QComboBox::currentIndexChanged,this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        func->setRuntime(HSemanticSegmentation::Runtime(
                             ui->cmbRuntime->itemData(index).toInt()));
        updateStatus();
    });
    connect(ui->dsbOpacity,&QDoubleSpinBox::valueChanged,this,[=](double value)
    {
        if(m_bShowing) func->setOverlayOpacity(value);
    });
}

void HSemanticSegmentationWdg::selectModel()
{
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;
    const QString path=QFileDialog::getOpenFileName(
                this,getLang("XvFuncSystem_HSemanticSegmentation_SelectModel",
                             "选择HALCON模型"),func->modelPath(),
                getLang("XvFuncSystem_HSemanticSegmentation_ModelFilter",
                        "HALCON深度学习模型 (*.hdl)"));
    if(path.isEmpty()) return;
    QString preprocess=func->preprocessPath();
    if(preprocess.isEmpty())
    {
        preprocess=QFileDialog::getOpenFileName(
                    this,getLang(
                        "XvFuncSystem_HSemanticSegmentation_SelectPreprocess",
                        "选择HALCON预处理字典"),QString(),
                    getLang("XvFuncSystem_HSemanticSegmentation_DictFilter",
                            "HALCON字典 (*.hdict)"));
    }
    if(!preprocess.isEmpty()) configureCandidate(path,preprocess);
}

void HSemanticSegmentationWdg::selectPreprocess()
{
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;
    const QString path=QFileDialog::getOpenFileName(
                this,getLang(
                    "XvFuncSystem_HSemanticSegmentation_SelectPreprocess",
                    "选择HALCON预处理字典"),func->preprocessPath(),
                getLang("XvFuncSystem_HSemanticSegmentation_DictFilter",
                        "HALCON字典 (*.hdict)"));
    if(path.isEmpty()) return;
    QString model=func->modelPath();
    if(model.isEmpty())
    {
        model=QFileDialog::getOpenFileName(
                    this,getLang("XvFuncSystem_HSemanticSegmentation_SelectModel",
                                 "选择HALCON模型"),QString(),
                    getLang("XvFuncSystem_HSemanticSegmentation_ModelFilter",
                            "HALCON深度学习模型 (*.hdl)"));
    }
    if(!model.isEmpty()) configureCandidate(model,path);
}

void HSemanticSegmentationWdg::configureCandidate(
        const QString &modelPath,const QString &preprocessPath)
{
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;
    QString error;
    if(!func->configureAssets(modelPath,preprocessPath,error))
    {
        QMessageBox::warning(
                    this,getLang("XvFuncSystem_HSemanticSegmentation_ConfigFailed",
                                 "语义分割配置失败"),error);
        return;
    }
    updateStatus();
}

void HSemanticSegmentationWdg::updateStatus()
{
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;
    ui->letModel->setText(func->modelPath());
    ui->letPreprocess->setText(func->preprocessPath());
    ui->lbAssetStateVal->setText(func->hasConfiguredAssets()
            ?func->assetSummary()
            :getLang("XvFuncSystem_HSemanticSegmentation_NotConfigured","未配置"));
    ui->ptxClasses->setPlainText(func->classSummary().join('\n'));
}

void HSemanticSegmentationWdg::onFuncRunUpdate()
{
    updateStatus();
}

void HSemanticSegmentationWdg::onShow()
{
    auto func=getFunc<HSemanticSegmentation>();
    if(!func) return;
    initCmbBindResultTag(func,ui->cmbInputImage,{XImage::type()},true);
    setCmbBind(func->param->inputImage,ui->cmbInputImage);
    const QSignalBlocker runtimeBlocker(ui->cmbRuntime);
    const QSignalBlocker opacityBlocker(ui->dsbOpacity);
    const int runtimeIndex=ui->cmbRuntime->findData(func->runtime());
    ui->cmbRuntime->setCurrentIndex(runtimeIndex<0?0:runtimeIndex);
    ui->dsbOpacity->setValue(func->overlayOpacity());
    updateStatus();
}
