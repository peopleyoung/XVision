#ifndef IMAGEACQUISITIONWDG_H
#define IMAGEACQUISITIONWDG_H

#include "BaseSystemFuncWdg.h"
#include "ImageAcquisition.h"

using namespace XvCore;

namespace Ui {
class ImageAcquisitionWdg;
}
class XMatTabs;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QWidget;
class ImageAcquisitionWdg : public BaseSystemFuncWdg
{
    Q_OBJECT

public:
    explicit ImageAcquisitionWdg(ImageAcquisition *func,QWidget *parent = nullptr);
    ~ImageAcquisitionWdg();

protected:
    void initFrm() override;
    void refreshCameraDevices();
protected slots:
    void onShow() override;


private:
    Ui::ImageAcquisitionWdg *ui;
    XMatTabs* m_tabs=nullptr;
    QWidget *m_industrialSettings=nullptr;
    QDoubleSpinBox *m_cameraExposure=nullptr;
    QDoubleSpinBox *m_cameraGain=nullptr;
    QComboBox *m_cameraTrigger=nullptr;
    QWidget* m_videoPage=nullptr;
    QLineEdit* m_videoPath=nullptr;
    QSpinBox* m_videoStart=nullptr;
    QSpinBox* m_videoEnd=nullptr;
    QSpinBox* m_videoStep=nullptr;
    QCheckBox* m_videoLoop=nullptr;
};

#endif // IMAGEACQUISITIONWDG_H
