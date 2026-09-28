#ifndef OREGIONDETECTOR_H
#define OREGIONDETECTOR_H

#include "OpenCvImageOperatorBase.h"
#include "XObjectList.h"
#include "XVisionSharedData.h"
#include <XObjectBaseType>

namespace XvCore
{
class ORegionDetectorParam:public OpenCvImageParamBase
{
public:
    ORegionDetectorParam();

    XReal *thresholdStep=nullptr;
    XReal *minThreshold=nullptr;
    XReal *maxThreshold=nullptr;
    XInt *minRepeatability=nullptr;
    XReal *minDistBetweenBlobs=nullptr;
    XBool *filterByColor=nullptr;
    XInt *blobColor=nullptr;
    XBool *filterByArea=nullptr;
    XReal *minArea=nullptr;
    XReal *maxArea=nullptr;
    XBool *filterByCircularity=nullptr;
    XReal *minCircularity=nullptr;
    XReal *maxCircularity=nullptr;
    XBool *filterByInertia=nullptr;
    XReal *minInertiaRatio=nullptr;
    XReal *maxInertiaRatio=nullptr;
    XBool *filterByConvexity=nullptr;
    XReal *minConvexity=nullptr;
    XReal *maxConvexity=nullptr;

    XReal *binaryThreshold=nullptr;
    XInt *retrievalMode=nullptr;
    XInt *approximationMode=nullptr;
    XInt *offsetX=nullptr;
    XInt *offsetY=nullptr;
    XReal *minContourArea=nullptr;

    XReal *circleDp=nullptr;
    XReal *circleMinDistance=nullptr;
    XReal *circleEdgeThreshold=nullptr;
    XReal *circleCenterThreshold=nullptr;
    XInt *minRadius=nullptr;
    XInt *maxRadius=nullptr;

    XInt *connectivity=nullptr;
    XInt *connectedComponentsAlgorithm=nullptr;
    XReal *componentMinArea=nullptr;
    XReal *componentMaxArea=nullptr;
    XBool *useRenderBlobs=nullptr;

    XReal *rho=nullptr;
    XReal *theta=nullptr;
    XInt *houghThreshold=nullptr;
    XReal *srn=nullptr;
    XReal *stn=nullptr;
    XReal *minLineLength=nullptr;
    XReal *maxLineGap=nullptr;
    XReal *targetAngle=nullptr;
    XReal *angleTolerance=nullptr;
};

class ORegionDetectorResult:public OpenCvImageResultBase
{
public:
    ORegionDetectorResult();

    XObjectList *keyPoints=nullptr;
    XObjectList *contours=nullptr;
    XObjectList *circles=nullptr;
    XObjectList *regions=nullptr;
    XObjectList *rectangles=nullptr;
    XObjectList *lines=nullptr;
    XInt *count=nullptr;
};

class XVFUNCSYSTEM_EXPORT ORegionDetector:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(ORegionDetector::Mode mode READ mode WRITE setMode)
public:
    enum Mode { Blob=0,Contours=1,HoughCircles=2,RenderBlobs=3,HoughLines=4,HoughLinesP=5 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit ORegionDetector(QObject *parent=nullptr);
    ~ORegionDetector() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;
    bool commitAdditionalResults(QString &error) override;

private:
    bool commitCandidateLists(QString &error);

    ORegionDetectorParam *m_param=nullptr;
    ORegionDetectorResult *m_result=nullptr;
    Mode m_mode=Blob;
    XObjectList m_candidateKeyPoints{"candidateKeyPoints",XKeyPoint::type()};
    XObjectList m_candidateContours{"candidateContours",XContour::type()};
    XObjectList m_candidateCircles{"candidateCircles",XCircle2D::type()};
    XObjectList m_candidateRegions{"candidateRegions",XRegion::type()};
    XObjectList m_candidateRectangles{"candidateRectangles",XRect2D::type()};
    XObjectList m_candidateLines{"candidateLines",XLine2D::type()};
    int m_candidateCount=0;
};
}

#endif // OREGIONDETECTOR_H
