#ifndef OIMAGEANALYSIS_H
#define OIMAGEANALYSIS_H

#include "OpenCvImageOperatorBase.h"
#include "XVisionRuntimeData.h"
#include <XObjectBaseType>

#include <vector>

namespace XvCore
{
class OImageAnalysisParam:public OpenCvImageParamBase
{
public:
    OImageAnalysisParam()
    {
        threshold1=new XReal("threshold1",50.0,this,"Threshold 1");
        threshold2=new XReal("threshold2",150.0,this,"Threshold 2");
        apertureSize=new XInt("apertureSize",3,this,"Aperture size");
        l2Gradient=new XBool("l2Gradient",false,this,"L2 gradient");
        histogramBins=new XInt("histogramBins",256,this,"Histogram bins");
        hogWidth=new XInt("hogWidth",64,this,"HOG width");
        hogHeight=new XInt("hogHeight",128,this,"HOG height");
        hogBins=new XInt("hogBins",9,this,"HOG bins");
        subdivisionSpacing=new XInt("subdivisionSpacing",32,this,"Subdivision spacing");
        subdivisionOutput=new XInt("subdivisionOutput",0,this,"Subdivision output");
    }

    XReal *threshold1=nullptr;
    XReal *threshold2=nullptr;
    XInt *apertureSize=nullptr;
    XBool *l2Gradient=nullptr;
    XInt *histogramBins=nullptr;
    XInt *hogWidth=nullptr;
    XInt *hogHeight=nullptr;
    XInt *hogBins=nullptr;
    XInt *subdivisionSpacing=nullptr;
    XInt *subdivisionOutput=nullptr;
};

class OImageAnalysisResult:public OpenCvImageResultBase
{
public:
    OImageAnalysisResult()
    {
        descriptor=new XTensor("descriptor","float32",{1},QByteArray(4,0),this,"Descriptor");
        featureCount=new XInt("featureCount",0,this,"Feature count");
    }

    XTensor *descriptor=nullptr;
    XInt *featureCount=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageAnalysis:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Canny=0,Histogram=1,Hog=2,Subdiv2d=3 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageAnalysis(QObject *parent=nullptr);
    ~OImageAnalysis() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    void setCandidateDescriptor(const std::vector<float> &values,int featureCount);

    OImageAnalysisParam *m_param=nullptr;
    OImageAnalysisResult *m_result=nullptr;
    Mode m_mode=Canny;
    QByteArray m_candidateDescriptor;
    QVector<qint64> m_candidateDimensions{1};
    int m_candidateFeatureCount=0;
};
}

#endif // OIMAGEANALYSIS_H
