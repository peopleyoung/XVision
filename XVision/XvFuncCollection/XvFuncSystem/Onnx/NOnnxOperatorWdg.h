#ifndef NONNXOPERATORWDG_H
#define NONNXOPERATORWDG_H

#include "BaseSystemFuncWdg.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QToolButton;
class QWidget;

namespace XvCore { class NOnnxBase; }

class NOnnxOperatorWdg:public BaseSystemFuncWdg
{
    Q_OBJECT
public:
    explicit NOnnxOperatorWdg(XvCore::NOnnxBase *function,
                              QWidget *parent=nullptr);

protected:
    void initFrm() override;
    void onShow() override;
    void onFuncRunUpdate() override;

private:
    XObject *inputParameter() const;
    void refreshBinding();
    void refreshStatus();
    void selectModel();

    QComboBox *m_mode=nullptr;
    QComboBox *m_inputBinding=nullptr;
    QLineEdit *m_modelPath=nullptr;
    QToolButton *m_selectModel=nullptr;
    QLabel *m_modelStatus=nullptr;
    QComboBox *m_inputLayout=nullptr;
    QSpinBox *m_inputWidth=nullptr;
    QSpinBox *m_inputHeight=nullptr;
    QComboBox *m_channelOrder=nullptr;
    QComboBox *m_resizeMode=nullptr;
    QSpinBox *m_paddingValue=nullptr;
    QDoubleSpinBox *m_pixelScale=nullptr;
    QLineEdit *m_meanValues=nullptr;
    QLineEdit *m_stdValues=nullptr;

    QWidget *m_classificationPanel=nullptr;
    QCheckBox *m_applySoftmax=nullptr;
    QSpinBox *m_topK=nullptr;
    QDoubleSpinBox *m_minScore=nullptr;
    QPlainTextEdit *m_classificationNames=nullptr;

    QWidget *m_detectionPanel=nullptr;
    QCheckBox *m_normalizedCoordinates=nullptr;
    QDoubleSpinBox *m_confidenceThreshold=nullptr;
    QDoubleSpinBox *m_iouThreshold=nullptr;
    QCheckBox *m_classAgnostic=nullptr;
    QSpinBox *m_maximumDetections=nullptr;
    QPlainTextEdit *m_detectionNames=nullptr;

    QWidget *m_segmentationPanel=nullptr;
    QComboBox *m_outputLayout=nullptr;
    QDoubleSpinBox *m_binaryThreshold=nullptr;
    QDoubleSpinBox *m_overlayOpacity=nullptr;
    QPlainTextEdit *m_segmentationNames=nullptr;
};

#endif // NONNXOPERATORWDG_H
