#include "GeometryMeasure.h"

#include "GeometryMeasurementUtils.h"
#include "XLanguage.h"

#include <QPainter>
#include <QtMath>

#include <cmath>

using namespace XvCore;

namespace
{
constexpr double Pi=3.14159265358979323846;

QPointF center(const XCircle2D *circle)
{
    return QPointF(circle->centerX(),circle->centerY());
}

void drawPoint(QPainter &painter,const QPointF &point)
{
    painter.drawEllipse(point,3.0,3.0);
}

void drawLine(QPainter &painter,const XLine2D *line)
{
    painter.drawLine(line->start(),line->end());
}

bool drawCircle(QPainter &painter,const XCircle2D *circle)
{
    const QRectF bounds(circle->centerX()-circle->radius(),
                        circle->centerY()-circle->radius(),
                        circle->radius()*2.0,circle->radius()*2.0);
    if(!qIsFinite(bounds.x()) || !qIsFinite(bounds.y())
            || !qIsFinite(bounds.width()) || !qIsFinite(bounds.height())) return false;
    painter.drawEllipse(bounds);
    return true;
}

bool annotateImage(const GeometryMeasureParam *param,GeometryMeasure::Mode mode,
                   const GeometryMeasurementUtils::Candidate &candidate,
                   double value,const QString &unit,bool passed,int precision,
                   QImage &output,QString &error)
{
    if(param->inputImage->value().isNull())
    {
        output=QImage();
        return true;
    }
    output=param->inputImage->value().convertToFormat(QImage::Format_ARGB32);
    if(output.isNull())
    {
        error="Input image could not be copied for annotation";
        return false;
    }
    QPainter painter(&output);
    if(!painter.isActive())
    {
        error="Measurement annotation painter could not be initialized";
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0,190,210),2.0));
    bool drawable=true;
    switch(mode)
    {
    case GeometryMeasure::CircleCircle:
        drawable=drawCircle(painter,param->circle1) && drawCircle(painter,param->circle2);
        break;
    case GeometryMeasure::LineCircle:
        drawLine(painter,param->line1);
        drawable=drawCircle(painter,param->circle1);
        break;
    case GeometryMeasure::LineLineAngle:
    case GeometryMeasure::LineLine:
        drawLine(painter,param->line1);
        drawLine(painter,param->line2);
        break;
    case GeometryMeasure::PointCircle:
        drawPoint(painter,QPointF(param->point1->x(),param->point1->y()));
        drawable=drawCircle(painter,param->circle1);
        break;
    case GeometryMeasure::PointLine:
        drawPoint(painter,QPointF(param->point1->x(),param->point1->y()));
        drawLine(painter,param->line1);
        break;
    case GeometryMeasure::PointPoint:
        drawPoint(painter,QPointF(param->point1->x(),param->point1->y()));
        drawPoint(painter,QPointF(param->point2->x(),param->point2->y()));
        break;
    }
    if(!drawable)
    {
        painter.end();
        error="Circle is too large to annotate safely";
        output=QImage();
        return false;
    }

    const QColor statusColor=passed?QColor(20,185,80):QColor(220,55,55);
    painter.setPen(QPen(statusColor,2.0));
    painter.drawLine(candidate.annotationStart,candidate.annotationEnd);
    painter.setBrush(statusColor);
    drawPoint(painter,candidate.annotationStart);
    drawPoint(painter,candidate.annotationEnd);
    const QString status=passed
            ?getLang("XvFuncSystem_GeometryMeasure_OK","OK")
            :getLang("XvFuncSystem_GeometryMeasure_NG","NG");
    const QString label=QString("%1 %2 %3")
            .arg(value,0,'f',precision).arg(unit,status);
    QPointF labelPoint=(candidate.annotationStart+candidate.annotationEnd)*0.5
            +QPointF(5.0,-5.0);
    labelPoint.setX(qBound(4.0,labelPoint.x(),qMax(4.0,double(output.width()-4))));
    labelPoint.setY(qBound(14.0,labelPoint.y(),qMax(14.0,double(output.height()-4))));
    painter.drawText(labelPoint,label);
    painter.end();
    return true;
}

bool prepareMeasurement(const GeometryMeasureParam *param,GeometryMeasure::Mode mode,
                        GeometryMeasurementUtils::Candidate &candidate,QString &error)
{
    const QPointF point1(param->point1->x(),param->point1->y());
    const QPointF point2(param->point2->x(),param->point2->y());
    switch(mode)
    {
    case GeometryMeasure::CircleCircle:
        return GeometryMeasurementUtils::circleCircle(
                    center(param->circle1),param->circle1->radius(),
                    center(param->circle2),param->circle2->radius(),candidate,error);
    case GeometryMeasure::LineCircle:
        return GeometryMeasurementUtils::lineCircle(
                    param->line1->start(),param->line1->end(),
                    center(param->circle1),param->circle1->radius(),candidate,error);
    case GeometryMeasure::LineLineAngle:
        return GeometryMeasurementUtils::lineLineAngle(
                    param->line1->start(),param->line1->end(),
                    param->line2->start(),param->line2->end(),candidate,error);
    case GeometryMeasure::LineLine:
        return GeometryMeasurementUtils::lineLine(
                    param->line1->start(),param->line1->end(),
                    param->line2->start(),param->line2->end(),candidate,error);
    case GeometryMeasure::PointCircle:
        return GeometryMeasurementUtils::pointCircle(
                    point1,center(param->circle1),param->circle1->radius(),candidate,error);
    case GeometryMeasure::PointLine:
        return GeometryMeasurementUtils::pointLine(
                    point1,param->line1->start(),param->line1->end(),candidate,error);
    case GeometryMeasure::PointPoint:
        return GeometryMeasurementUtils::pointPoint(point1,point2,candidate,error);
    }
    error="Geometry measurement mode is invalid";
    return false;
}
}

GeometryMeasureParam::GeometryMeasureParam()
{
    inputImage=new XImage("inputImage",QImage(),this,
                          getLang("XvFuncSystem_GeometryMeasure_InputImage","输入图像(可选)"));
    point1=new XPoint2D("point1",0.0,0.0,this,
                        getLang("XvFuncSystem_GeometryMeasure_Point1","点 1"));
    point2=new XPoint2D("point2",3.0,4.0,this,
                        getLang("XvFuncSystem_GeometryMeasure_Point2","点 2"));
    line1=new XLine2D("line1",QPointF(0.0,0.0),QPointF(100.0,0.0),this,
                      getLang("XvFuncSystem_GeometryMeasure_Line1","线 1"));
    line2=new XLine2D("line2",QPointF(0.0,10.0),QPointF(100.0,10.0),this,
                      getLang("XvFuncSystem_GeometryMeasure_Line2","线 2"));
    circle1=new XCircle2D("circle1",0.0,0.0,10.0,this,
                          getLang("XvFuncSystem_GeometryMeasure_Circle1","圆 1"));
    circle2=new XCircle2D("circle2",30.0,0.0,10.0,this,
                          getLang("XvFuncSystem_GeometryMeasure_Circle2","圆 2"));
    absoluteValue=new XBool("absoluteValue",true,this,
                            getLang("XvFuncSystem_GeometryMeasure_Absolute","绝对值"));
    scale=new XReal("scale",1.0,this,
                    getLang("XvFuncSystem_GeometryMeasure_Scale","比例尺"));
    unit=new XString("unit","px",this,
                     getLang("XvFuncSystem_GeometryMeasure_Unit","单位"));
    angleUnit=new XInt("angleUnit",GeometryMeasure::Degrees,this,
                       getLang("XvFuncSystem_GeometryMeasure_AngleUnit","角度单位"));
    lowerLimit=new XReal("lowerLimit",0.0,this,
                         getLang("XvFuncSystem_GeometryMeasure_Lower","公差下限"));
    upperLimit=new XReal("upperLimit",1.0e12,this,
                         getLang("XvFuncSystem_GeometryMeasure_Upper","公差上限"));
    precision=new XInt("precision",3,this,
                       getLang("XvFuncSystem_GeometryMeasure_Precision","显示精度"));
}

GeometryMeasureResult::GeometryMeasureResult()
{
    outputImage=new XImage("outputImage",QImage(),this,
                           getLang("XvFuncSystem_GeometryMeasure_OutputImage","标注图像"));
    value=new XReal("value",0.0,this,
                    getLang("XvFuncSystem_GeometryMeasure_Value","测量值"));
    rawValue=new XReal("rawValue",0.0,this,
                       getLang("XvFuncSystem_GeometryMeasure_RawValue","原始值"));
    passed=new XBool("passed",false,this,
                     getLang("XvFuncSystem_GeometryMeasure_Passed","公差通过"));
    measurement=new XMeasurementResult(
                "measurement",0.0,"px",0.0,1.0e12,false,this,
                getLang("XvFuncSystem_GeometryMeasure_Result","测量结果"));
    annotationStart=new XPoint2D(
                "annotationStart",0.0,0.0,this,
                getLang("XvFuncSystem_GeometryMeasure_AnnotationStart","标注起点"));
    annotationEnd=new XPoint2D(
                "annotationEnd",0.0,0.0,this,
                getLang("XvFuncSystem_GeometryMeasure_AnnotationEnd","标注终点"));
}

GeometryMeasure::GeometryMeasure(QObject *parent)
    :GeometryOperatorBase(parent),m_param(new GeometryMeasureParam()),
      m_result(new GeometryMeasureResult())
{
    _funcRole="GeometryMeasure";
    _funcName=getLang("XvFuncSystem_GeometryMeasure_Name","几何测量");
}

GeometryMeasure::~GeometryMeasure()
{
    delete m_param;
    delete m_result;
}

QStringList GeometryMeasure::activeParameterNames() const
{
    QStringList names={"inputImage"};
    switch(m_mode)
    {
    case CircleCircle: names << "circle1" << "circle2"; break;
    case LineCircle: names << "line1" << "circle1"; break;
    case LineLineAngle: names << "line1" << "line2"; break;
    case LineLine: names << "line1" << "line2"; break;
    case PointCircle: names << "point1" << "circle1"; break;
    case PointLine: names << "point1" << "line1"; break;
    case PointPoint: names << "point1" << "point2"; break;
    }
    if(m_mode==LineLineAngle) names << "angleUnit";
    else names << "absoluteValue" << "scale" << "unit";
    names << "lowerLimit" << "upperLimit" << "precision";
    return names;
}

EXvFuncRunStatus GeometryMeasure::run()
{
    if(!m_param || !m_result || !m_param->inputImage || !m_param->point1
            || !m_param->point2 || !m_param->line1 || !m_param->line2
            || !m_param->circle1 || !m_param->circle2 || !m_param->absoluteValue
            || !m_param->scale || !m_param->unit || !m_param->angleUnit
            || !m_param->lowerLimit || !m_param->upperLimit || !m_param->precision)
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_Incomplete",
                          "Geometry measurement parameters are incomplete"));
        return EXvFuncRunStatus::Error;
    }
    if(m_mode<CircleCircle || m_mode>PointPoint)
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_InvalidMode",
                          "Geometry measurement mode is invalid"));
        return EXvFuncRunStatus::Error;
    }
    const double lower=m_param->lowerLimit->value();
    const double upper=m_param->upperLimit->value();
    const int precision=m_param->precision->value();
    if(!qIsFinite(lower) || !qIsFinite(upper) || lower>upper
            || precision<0 || precision>9)
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_InvalidTolerance",
                          "Tolerance or display precision is invalid"));
        return EXvFuncRunStatus::Error;
    }

    const bool angle=m_mode==LineLineAngle;
    QString outputUnit;
    if(angle)
    {
        if(m_param->angleUnit->value()!=Degrees && m_param->angleUnit->value()!=Radians)
        {
            setRunMsg(getLang("XvFuncSystem_GeometryMeasure_InvalidAngleUnit",
                              "Angle unit is invalid"));
            return EXvFuncRunStatus::Error;
        }
        outputUnit=m_param->angleUnit->value()==Degrees?"deg":"rad";
    }
    else
    {
        if(!qIsFinite(m_param->scale->value()) || m_param->scale->value()<=0.0
                || m_param->unit->value().trimmed().isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_GeometryMeasure_InvalidScale",
                              "Scale must be positive and distance unit must be nonempty"));
            return EXvFuncRunStatus::Error;
        }
        outputUnit=m_param->unit->value().trimmed();
    }

    GeometryMeasurementUtils::Candidate candidate;
    QString error;
    if(!prepareMeasurement(m_param,m_mode,candidate,error))
    {
        setRunMsg(error.isEmpty()
                  ?getLang("XvFuncSystem_GeometryMeasure_Failed",
                           "Geometry measurement failed")
                  :getLang("XvFuncSystem_GeometryMeasure_GeometryError",error));
        return EXvFuncRunStatus::Error;
    }
    double measured=candidate.rawValue;
    if(angle)
    {
        if(m_param->angleUnit->value()==Degrees) measured*=180.0/Pi;
    }
    else
    {
        if(m_param->absoluteValue->value()) measured=qAbs(measured);
        measured*=m_param->scale->value();
    }
    if(!qIsFinite(measured))
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_NonFinite",
                          "Converted measurement value is not finite"));
        return EXvFuncRunStatus::Error;
    }
    const bool passed=measured>=lower && measured<=upper;
    XMeasurementResult measurementCandidate;
    XPoint2D startCandidate,endCandidate;
    if(!measurementCandidate.setValue(measured,outputUnit,lower,upper,passed)
            || !startCandidate.setValue(candidate.annotationStart.x(),
                                        candidate.annotationStart.y())
            || !endCandidate.setValue(candidate.annotationEnd.x(),
                                      candidate.annotationEnd.y()))
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_InvalidResult",
                          "Measurement result candidates are invalid"));
        return EXvFuncRunStatus::Error;
    }
    QImage outputCandidate;
    if(!annotateImage(m_param,m_mode,candidate,measured,outputUnit,passed,precision,
                      outputCandidate,error))
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_AnnotationError",error));
        return EXvFuncRunStatus::Error;
    }

    if(!m_result->measurement->setData(&measurementCandidate)
            || !m_result->annotationStart->setData(&startCandidate)
            || !m_result->annotationEnd->setData(&endCandidate))
    {
        setRunMsg(getLang("XvFuncSystem_GeometryMeasure_CommitFailed",
                          "Measurement result could not be committed"));
        return EXvFuncRunStatus::Error;
    }
    m_result->rawValue->setValue(candidate.rawValue);
    m_result->value->setValue(measured);
    m_result->passed->setValue(passed);
    m_result->outputImage->setValue(outputCandidate);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}
