#ifndef ORECTIFICATION_H
#define ORECTIFICATION_H

#include "OpenCvImageOperatorBase.h"

#include "XRotateRectRoi.h"
#include "XVisionRuntimeData.h"
#include "XVisionSharedData.h"

#include <array>
#include <XObjectBaseType>

namespace XvCore
{
class ORectificationParam:public OpenCvImageParamBase
{
public:
    ORectificationParam();

    XRotateRectRoi *rotatedRect=nullptr;
    XInt *foregroundThreshold=nullptr;
    XBool *invertForeground=nullptr;
    XReal *minimumForegroundArea=nullptr;
    XInt *outputWidth=nullptr;
    XInt *outputHeight=nullptr;
    XInt *rectificationInterpolation=nullptr;
    XReal *borderValue=nullptr;
};

class ORectificationResult:public OpenCvImageResultBase
{
public:
    ORectificationResult();

    XImage *foregroundMask=nullptr;
    XRegion *foregroundRegion=nullptr;
    XRotateRectRoi *rectifiedRoi=nullptr;
    XTensor *transform=nullptr;
};

class XVFUNCSYSTEM_EXPORT ORectification:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(ORectification::Mode mode READ mode WRITE setMode)
public:
    enum Mode { ForegroundRotatedRect=0,ForegroundExtract=1,RotatedRect=2 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit ORectification(QObject *parent=nullptr);
    ~ORectification() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;
    void clearResultsAfterFailure() override;

private:
    ORectificationParam *m_param=nullptr;
    ORectificationResult *m_result=nullptr;
    Mode m_mode=ForegroundRotatedRect;

    QImage m_candidateMask;
    XRegion m_candidateRegion{"candidateRegion"};
    XRotateRectRoi m_candidateRoi;
    XTensor m_candidateTransform;
};
}

#endif // ORECTIFICATION_H
