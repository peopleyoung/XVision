#include "HModelMatchWdg.h"
#include "ui_HModelMatchWdg.h"

HModelMatchWdg::HModelMatchWdg(HModelMatch *func,QWidget *parent)
    :BaseSystemFuncWdg(func,parent),ui(new Ui::HModelMatchWdg)
{
    ui->setupUi(this->centralWidget());
    initFrm();
}

HModelMatchWdg::~HModelMatchWdg()
{
    delete ui;
}

void HModelMatchWdg::initFrm()
{
    initFixedSize();
    auto func=getFunc<HModelMatch>();
    if(!func) return;

    updateLbTextWithXObject(ui->lbInputImage,func->param->inputImage,":");
    updateLbTextWithXObject(ui->lbMode,func->param->mode,":");
    updateLbTextWithXObject(ui->lbUseRoi,func->param->useTemplateRoi,":");
    updateLbTextWithXObject(ui->lbMinScore,func->param->minScore,":");
    updateLbTextWithXObject(ui->lbAngleStart,func->param->angleStart,":");
    updateLbTextWithXObject(ui->lbAngleExtent,func->param->angleExtent,":");
    updateLbTextWithXObject(ui->lbNumMatches,func->param->numMatches,":");
    updateLbTextWithXObject(ui->lbMaxOverlap,func->param->maxOverlap,":");
    updateLbTextWithXObject(ui->lbGreediness,func->param->greediness,":");
    ui->lbRoiX->setText(getLang("XvFuncSystem_HModelMatch_RoiX","中心 X")+":");
    ui->lbRoiSource->setText(getLang("XvFuncSystem_HModelMatch_RoiSource","模板区域来源")+":");
    ui->lbRoiY->setText(getLang("XvFuncSystem_HModelMatch_RoiY","中心 Y")+":");
    ui->lbLength1->setText(getLang("XvFuncSystem_HModelMatch_Length1","半长 1")+":");
    ui->lbLength2->setText(getLang("XvFuncSystem_HModelMatch_Length2","半长 2")+":");
    ui->lbRoiAngle->setText(getLang("XvFuncSystem_HModelMatch_RoiAngle","ROI 角度(弧度)")+":");
    ui->lbTemplateState->setText(getLang("XvFuncSystem_HModelMatch_TemplateState","模板状态")+":");
    ui->lbMatchCount->setText(getLang("XvFuncSystem_HModelMatch_MatchCount","匹配数量")+":");
    ui->ckbUseRoi->setText(getLang("XvFuncSystem_HModelMatch_EnableRoi","启用"));

    ui->cmbMode->addItem(getLang("XvFuncSystem_HModelMatch_CreateMode","创建模板"),
                         HModelMatch::CreateTemplate);
    ui->cmbMode->addItem(getLang("XvFuncSystem_HModelMatch_FindMode","查找模板"),
                         HModelMatch::FindTemplate);

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
            func->paramSubscribe(func->param->inputImage->objectName(),tag.func,tag.resultName);
        }
    });
    connect(ui->cmbMode,&QComboBox::currentIndexChanged,this,[=]()
    {
        if(m_bShowing) func->param->mode->setValue(ui->cmbMode->currentData().toInt());
    });
    connect(ui->cmbTemplateRoi,&QComboBox::currentIndexChanged,this,[=]()
    {
        if(!m_bShowing) return;
        if(ui->cmbTemplateRoi->currentIndex()==0)
        {
            func->paramUnSubscribe(func->param->templateRoi->objectName());
        }
        else
        {
            SBindResultTag tag;
            if(getCmbBindResultTag(ui->cmbTemplateRoi,tag))
            {
                func->paramSubscribe(func->param->templateRoi->objectName(),tag.func,
                                     tag.resultName);
            }
        }
        updateRoiControlsEnabled();
    });
    connect(ui->ckbUseRoi,&QCheckBox::toggled,this,[=](bool checked)
    {
        if(m_bShowing) func->param->useTemplateRoi->setValue(checked);
    });
    connect(ui->dsbRoiX,&QDoubleSpinBox::valueChanged,this,[=](double){ if(m_bShowing) updateRoiValue(); });
    connect(ui->dsbRoiY,&QDoubleSpinBox::valueChanged,this,[=](double){ if(m_bShowing) updateRoiValue(); });
    connect(ui->dsbLength1,&QDoubleSpinBox::valueChanged,this,[=](double){ if(m_bShowing) updateRoiValue(); });
    connect(ui->dsbLength2,&QDoubleSpinBox::valueChanged,this,[=](double){ if(m_bShowing) updateRoiValue(); });
    connect(ui->dsbRoiAngle,&QDoubleSpinBox::valueChanged,this,[=](double){ if(m_bShowing) updateRoiValue(); });
    connect(ui->dsbMinScore,&QDoubleSpinBox::valueChanged,this,[=](double value){ if(m_bShowing) func->param->minScore->setValue(value); });
    connect(ui->dsbAngleStart,&QDoubleSpinBox::valueChanged,this,[=](double value){ if(m_bShowing) func->param->angleStart->setValue(value); });
    connect(ui->dsbAngleExtent,&QDoubleSpinBox::valueChanged,this,[=](double value){ if(m_bShowing) func->param->angleExtent->setValue(value); });
    connect(ui->spbNumMatches,&QSpinBox::valueChanged,this,[=](int value){ if(m_bShowing) func->param->numMatches->setValue(value); });
    connect(ui->dsbMaxOverlap,&QDoubleSpinBox::valueChanged,this,[=](double value){ if(m_bShowing) func->param->maxOverlap->setValue(value); });
    connect(ui->dsbGreediness,&QDoubleSpinBox::valueChanged,this,[=](double value){ if(m_bShowing) func->param->greediness->setValue(value); });
}

void HModelMatchWdg::updateRoiValue()
{
    auto func=getFunc<HModelMatch>();
    if(!func) return;
    func->param->templateRoi->setValue(ui->dsbRoiX->value(),ui->dsbRoiY->value(),
                                      ui->dsbLength1->value(),ui->dsbLength2->value(),
                                      ui->dsbRoiAngle->value());
}

void HModelMatchWdg::updateRoiControlsEnabled()
{
    const bool enabled=ui->cmbTemplateRoi->currentIndex()==0;
    ui->dsbRoiX->setEnabled(enabled);
    ui->dsbRoiY->setEnabled(enabled);
    ui->dsbLength1->setEnabled(enabled);
    ui->dsbLength2->setEnabled(enabled);
    ui->dsbRoiAngle->setEnabled(enabled);
}

void HModelMatchWdg::updateStatus()
{
    auto func=getFunc<HModelMatch>();
    if(!func) return;
    ui->lbTemplateStateVal->setText(func->hasTemplateModel()
            ?getLang("XvFuncSystem_HModelMatch_TemplateReady","已创建")
            :getLang("XvFuncSystem_HModelMatch_TemplateEmpty","未创建"));
    ui->lbMatchCountVal->setText(QString::number(func->result->matchCount->value()));
}

void HModelMatchWdg::onFuncRunUpdate()
{
    updateStatus();
}

void HModelMatchWdg::onShow()
{
    auto func=getFunc<HModelMatch>();
    if(!func) return;
    initCmbBindResultTag(func,ui->cmbInputImage,{XImage::type()},true);
    setCmbBind(func->param->inputImage,ui->cmbInputImage);
    initCmbBindResultTag(func,ui->cmbTemplateRoi,{XRotateRectRoi::type()},true);
    setCmbBind(func->param->templateRoi,ui->cmbTemplateRoi);
    updateRoiControlsEnabled();
    const int modeIndex=ui->cmbMode->findData(func->param->mode->value());
    ui->cmbMode->setCurrentIndex(modeIndex<0 ? 0 : modeIndex);
    ui->ckbUseRoi->setChecked(func->param->useTemplateRoi->value());
    const XRotateRectRoi *roi=func->param->templateRoi;
    ui->dsbRoiX->setValue(roi->centerX());
    ui->dsbRoiY->setValue(roi->centerY());
    ui->dsbLength1->setValue(roi->length1());
    ui->dsbLength2->setValue(roi->length2());
    ui->dsbRoiAngle->setValue(roi->angle());
    ui->dsbMinScore->setValue(func->param->minScore->value());
    ui->dsbAngleStart->setValue(func->param->angleStart->value());
    ui->dsbAngleExtent->setValue(func->param->angleExtent->value());
    ui->spbNumMatches->setValue(func->param->numMatches->value());
    ui->dsbMaxOverlap->setValue(func->param->maxOverlap->value());
    ui->dsbGreediness->setValue(func->param->greediness->value());
    updateStatus();
}
