#include "ImageAcquisitionWdg.h"
#include "ui_ImageAcquisitionWdg.h"
#include <QFileDialog>
#include <QCheckBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include "XMatTabs.h"
#include "ImageAcquisition_p.h"
#include "XvCameraManager.h"

ImageAcquisitionWdg::ImageAcquisitionWdg(ImageAcquisition *func,QWidget *parent) :
    BaseSystemFuncWdg(func,parent),
    ui(new Ui::ImageAcquisitionWdg)
{
    ui->setupUi(this->centralWidget());
    initFrm();
}


ImageAcquisitionWdg::~ImageAcquisitionWdg()
{
    delete ui;
}

void ImageAcquisitionWdg::initFrm()
{
    initFixedSize();

    auto func=getFunc<ImageAcquisition>();
    if(!func) return;

    auto lyWdg= (QVBoxLayout*)this->centralWidget()->layout();
    if(!lyWdg) return;

    m_tabs=new XMatTabs(this);
    m_tabs->setObjectName("tabsAcqType");
    m_tabs->setHaloVisible(false);
    m_tabs->addTab(getLang("XvFuncSystem_ImageAcquisitionc_AcqTypeFile","文件"));
    m_tabs->addTab(getLang("XvFuncSystem_ImageAcquisitionc_AcqTypeDir","目录"));
    m_tabs->addTab(getLang("XvFuncSystem_ImageAcquisitionc_AcqTypeCamera","相机"));
    m_tabs->addTab(getLang("XvFuncSystem_ImageAcquisitionc_AcqTypeVideo","视频"));
    m_videoPage=new QWidget(ui->staWdg);
    auto videoLayout=new QVBoxLayout(m_videoPage);
    videoLayout->setContentsMargins(0,0,0,0);
    auto videoPathLayout=new QHBoxLayout();
    m_videoPath=new QLineEdit(m_videoPage);
    m_videoPath->setReadOnly(true);
    auto videoOpen=new QToolButton(m_videoPage);
    videoOpen->setText(getLang("XvFuncSystem_ImageAcquisition_VideoOpen","打开"));
    videoPathLayout->addWidget(m_videoPath);
    videoPathLayout->addWidget(videoOpen);
    videoLayout->addLayout(videoPathLayout);
    auto videoForm=new QFormLayout();
    m_videoStart=new QSpinBox(m_videoPage);
    m_videoStart->setRange(0,2147483647);
    m_videoEnd=new QSpinBox(m_videoPage);
    m_videoEnd->setRange(-1,2147483647);
    m_videoStep=new QSpinBox(m_videoPage);
    m_videoStep->setRange(1,1000000);
    m_videoLoop=new QCheckBox(m_videoPage);
    videoForm->addRow(getLang("XvFuncSystem_ImageAcquisition_VideoStartFrame","起始帧"),m_videoStart);
    videoForm->addRow(getLang("XvFuncSystem_ImageAcquisition_VideoEndFrame","结束帧"),m_videoEnd);
    videoForm->addRow(getLang("XvFuncSystem_ImageAcquisition_VideoFrameStep","帧步长"),m_videoStep);
    videoForm->addRow(getLang("XvFuncSystem_ImageAcquisition_VideoLoop","循环播放"),m_videoLoop);
    videoLayout->addLayout(videoForm);
    videoLayout->addStretch(1);
    ui->staWdg->addWidget(m_videoPage);
    lyWdg->insertWidget(0,m_tabs);
    connect(m_tabs,&XMatTabs::currentChanged,this,[=]()
    {
        int idx=m_tabs->currentIndex();
        if(idx<0||idx>3)
        {
            return;
        }
        ui->staWdg->setCurrentIndex(idx);
        if(m_bShowing) func->setAcqType(ImageAcquisition::AcqType(idx));
    });


    ui->btnLocalImageOpen->setText(getLang("XvFuncSystem_ImageAcquisitionc_OpenFile","打开文件"));
    ui->btnLocalDirOpen->setText(getLang("XvFuncSystem_ImageAcquisitionc_OpenDir","打开目录"));
    ui->btnLocalDirLast->setToolTip(getLang("XvFuncSystem_ImageAcquisitionc_LastImage","上一张"));
    ui->btnLocalDirLast->setIcon(QIcon(":/images/ImageAcquisitionWdgDirLast.svg"));
    ui->btnLocalDirNext->setToolTip(getLang("XvFuncSystem_ImageAcquisitionc_NextImage","下一张"));
    ui->btnLocalDirNext->setIcon(QIcon(":/images/ImageAcquisitionWdgDirNext.svg"));
    ui->lbCameraDevice->setText(
                getLang("XvFuncSystem_ImageAcquisition_CameraDevice","相机设备"));
    ui->lbCameraTimeout->setText(
                getLang("XvFuncSystem_ImageAcquisition_CameraTimeout","单帧超时"));
    ui->btnCameraRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->btnCameraRefresh->setToolTip(
                getLang("XvFuncSystem_ImageAcquisition_CameraRefresh","刷新相机列表"));
    ui->spbCameraTimeout->setRange(0,60000);
    ui->spbCameraTimeout->setSuffix(" ms");

    connect(ui->btnCameraRefresh,&QToolButton::clicked,
            this,&ImageAcquisitionWdg::refreshCameraDevices);
    connect(XvCameraMgr,&XvCamera::XvCameraManager::devicesChanged,
            this,&ImageAcquisitionWdg::refreshCameraDevices);
    connect(ui->cmbCameraDevice,qOverload<int>(&QComboBox::currentIndexChanged),
            this,[=](int index)
    {
        if(!m_bShowing || index<0) return;
        func->setCameraDeviceId(ui->cmbCameraDevice->itemData(index).toString());
        refreshCameraDevices();
    });
    connect(ui->spbCameraTimeout,&QSpinBox::valueChanged,this,[=](int value)
    {
        if(m_bShowing) func->setCameraTimeoutMs(value);
    });

    connect(videoOpen,&QToolButton::clicked,this,[=]()
    {
        const QString path=QFileDialog::getOpenFileName(this,
                getLang("XvFuncSystem_ImageAcquisition_VideoOpen","打开视频"),
                QString(),"Video Files(*.avi *.mp4 *.mov *.mkv);;All Files(*.*)");
        if(path.isEmpty()) return;
        func->setVideoPath(path);
        m_videoPath->setText(path);
    });
    connect(m_videoStart,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    {
        if(m_bShowing) func->setVideoStartFrame(value);
    });
    connect(m_videoEnd,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    {
        if(m_bShowing) func->setVideoEndFrame(value);
    });
    connect(m_videoStep,qOverload<int>(&QSpinBox::valueChanged),this,[=](int value)
    {
        if(m_bShowing) func->setVideoFrameStep(value);
    });
    connect(m_videoLoop,&QCheckBox::toggled,this,[=](bool value)
    {
        if(m_bShowing) func->setVideoLoop(value);
    });

    connect(ui->btnLocalImageOpen,&QAbstractButton::clicked,this,[=]()
    {
        QString openFile;
        openFile = QFileDialog::getOpenFileName(this,
                "",
                "",
                "Image Files(*.*)");
        if(openFile != "")
        {
            func->setLocalFile(openFile);
            ui->ptxLocalImage->setPlainText(openFile);
        }
    });

    connect(ui->btnLocalDirOpen,&QAbstractButton::clicked,this,[=]()
    {
        QString openDir = QFileDialog::getExistingDirectory(this,"","");
        if(openDir != "")
        {
            func->setLocalDir(openDir);
            ui->ptxLocalDir->setPlainText(openDir);
        }
    });


    connect(ui->btnLocalDirLast,&QAbstractButton::clicked,this,[=]()
    {
        int idx=func->m_sDirImage->getIdx();
        func->m_sDirImage->setIdx(idx-1);
        func->runXvFunc();
        idx=func->m_sDirImage->getIdx();
        func->m_sDirImage->setIdx(idx-1);
    });

    connect(ui->btnLocalDirNext,&QAbstractButton::clicked,this,[=]()
    {
        int idx=func->m_sDirImage->getIdx();
        func->m_sDirImage->setIdx(idx+1);
        func->runXvFunc();
        idx=func->m_sDirImage->getIdx();
        func->m_sDirImage->setIdx(idx-1);
    });
}

void ImageAcquisitionWdg::refreshCameraDevices()
{
    auto func=getFunc<ImageAcquisition>();
    if(!func) return;

    const QString selectedId=func->cameraDeviceId();
    const QList<XvCamera::XvCameraDeviceInfo> devices=XvCameraMgr->devices();
    QSignalBlocker blocker(ui->cmbCameraDevice);
    ui->cmbCameraDevice->clear();

    ui->cmbCameraDevice->addItem(getLang(
                "XvFuncSystem_ImageAcquisition_CameraNotSelected",
                "未选择相机"),QString());
    int selectedIndex=selectedId.isEmpty()?0:-1;
    for(const XvCamera::XvCameraDeviceInfo &device:devices)
    {
        const QString text=device.displayName.isEmpty()
                ?device.deviceId:device.displayName;
        ui->cmbCameraDevice->addItem(text,device.deviceId);
        const int index=ui->cmbCameraDevice->count()-1;
        ui->cmbCameraDevice->setItemData(index,device.deviceId,Qt::ToolTipRole);
        if(device.deviceId==selectedId) selectedIndex=index;
    }

    bool selectedAvailable=selectedIndex>=0;
    if(!selectedId.isEmpty() && !selectedAvailable)
    {
        const QString unavailable=getLang(
                    "XvFuncSystem_ImageAcquisition_CameraUnavailable",
                    "设备不可用");
        ui->cmbCameraDevice->addItem(unavailable+": "+selectedId,selectedId);
        selectedIndex=ui->cmbCameraDevice->count()-1;
        ui->cmbCameraDevice->setItemData(selectedIndex,selectedId,Qt::ToolTipRole);
    }
    if(selectedIndex<0) selectedIndex=0;
    ui->cmbCameraDevice->setCurrentIndex(selectedIndex);

    if(selectedId.isEmpty())
    {
        ui->lbCameraStatus->setText(devices.isEmpty()
                ?getLang("XvFuncSystem_ImageAcquisition_CameraNone","无可用相机")
                :getLang("XvFuncSystem_ImageAcquisition_CameraNotSelected",
                         "未选择相机"));
    }
    else if(!selectedAvailable)
    {
        ui->lbCameraStatus->setText(getLang(
                    "XvFuncSystem_ImageAcquisition_CameraMissing",
                    "已保存的相机当前不可用"));
    }
    else
    {
        for(const XvCamera::XvCameraDeviceInfo &device:devices)
        {
            if(device.deviceId!=selectedId) continue;
            QStringList details;
            if(!device.vendor.isEmpty()) details.append(device.vendor);
            if(!device.model.isEmpty()) details.append(device.model);
            ui->lbCameraStatus->setText(details.isEmpty()
                    ?device.deviceId:details.join(" / "));
            break;
        }
    }
}

void ImageAcquisitionWdg::onShow()
{
    auto func=getFunc<ImageAcquisition>();
    if(!func) return;
    auto acqType=func->acqType();
    m_tabs->setCurrentTab(acqType);
    ui->ptxLocalImage->setPlainText(func->localFlie());
    ui->ptxLocalDir->setPlainText(func->localDir());
    ui->spbCameraTimeout->setValue(func->cameraTimeoutMs());
    m_videoPath->setText(func->videoPath());
    m_videoStart->setValue(func->videoStartFrame());
    m_videoEnd->setValue(func->videoEndFrame());
    m_videoStep->setValue(func->videoFrameStep());
    m_videoLoop->setChecked(func->videoLoop());
    refreshCameraDevices();
}
