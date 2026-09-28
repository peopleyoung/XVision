#ifndef GEOMETRYMEASURE_H
#define GEOMETRYMEASURE_H

#include "GeometryOperatorBase.h"

#include "XPoint2D.h"
#include "XVisionSharedData.h"
#include <XObjectBaseType>

namespace XvCore
{
class GeometryMeasureParam:public XvBaseParam
{
public:
    GeometryMeasureParam();

    XImage *inputImage=nullptr;
    XPoint2D *point1=nullptr;
    XPoint2D *point2=nullptr;
    XLine2D *line1=nullptr;
    XLine2D *line2=nullptr;
    XCircle2D *circle1=nullptr;
    XCircle2D *circle2=nullptr;
    XBool *absoluteValue=nullptr;
    XReal *scale=nullptr;
    XString *unit=nullptr;
    XInt *angleUnit=nullptr;
    XReal *lowerLimit=nullptr;
    XReal *upperLimit=nullptr;
    XInt *precision=nullptr;
};

class GeometryMeasureResult:public XvBaseResult
{
public:
    GeometryMeasureResult();

    XImage *outputImage=nullptr;
    XReal *value=nullptr;
    XReal *rawValue=nullptr;
    XBool *passed=nullptr;
    XMeasurementResult *measurement=nullptr;
    XPoint2D *annotationStart=nullptr;
    XPoint2D *annotationEnd=nullptr;
};

class XVFUNCSYSTEM_EXPORT GeometryMeasure:public GeometryOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(GeometryMeasure::Mode mode READ mode WRITE setMode)
public:
    enum Mode {
        CircleCircle=0,
        LineCircle=1,
        LineLineAngle=2,
        LineLine=3,
        PointCircle=4,
        PointLine=5,
        PointPoint=6
    };
    Q_ENUM(Mode)
    enum AngleUnit { Degrees=0,Radians=1 };
    Q_ENUM(AngleUnit)

    Q_INVOKABLE explicit GeometryMeasure(QObject *parent=nullptr);
    ~GeometryMeasure() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QString selectorPropertyName() const override { return "mode"; }
    QStringList activeParameterNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;

private:
    GeometryMeasureParam *m_param=nullptr;
    GeometryMeasureResult *m_result=nullptr;
    Mode m_mode=CircleCircle;
};
}

#endif // GEOMETRYMEASURE_H
