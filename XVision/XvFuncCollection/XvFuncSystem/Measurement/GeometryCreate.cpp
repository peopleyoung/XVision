#include "GeometryCreate.h"

#include "XLanguage.h"

#include <QtMath>

using namespace XvCore;

GeometryCreateParam::GeometryCreateParam()
{
    pointX=new XReal("pointX",0.0,this,
                     getLang("XvFuncSystem_GeometryCreate_PointX","点 X"));
    pointY=new XReal("pointY",0.0,this,
                     getLang("XvFuncSystem_GeometryCreate_PointY","点 Y"));
    lineX1=new XReal("lineX1",0.0,this,
                     getLang("XvFuncSystem_GeometryCreate_LineX1","线起点 X"));
    lineY1=new XReal("lineY1",0.0,this,
                     getLang("XvFuncSystem_GeometryCreate_LineY1","线起点 Y"));
    lineX2=new XReal("lineX2",100.0,this,
                     getLang("XvFuncSystem_GeometryCreate_LineX2","线终点 X"));
    lineY2=new XReal("lineY2",0.0,this,
                     getLang("XvFuncSystem_GeometryCreate_LineY2","线终点 Y"));
    circleCenterX=new XReal("circleCenterX",0.0,this,
                            getLang("XvFuncSystem_GeometryCreate_CircleX","圆心 X"));
    circleCenterY=new XReal("circleCenterY",0.0,this,
                            getLang("XvFuncSystem_GeometryCreate_CircleY","圆心 Y"));
    circleRadius=new XReal("circleRadius",10.0,this,
                           getLang("XvFuncSystem_GeometryCreate_Radius","半径"));
    rectangleX=new XReal("rectangleX",0.0,this,
                         getLang("XvFuncSystem_GeometryCreate_RectangleX","矩形 X"));
    rectangleY=new XReal("rectangleY",0.0,this,
                         getLang("XvFuncSystem_GeometryCreate_RectangleY","矩形 Y"));
    rectangleWidth=new XReal("rectangleWidth",100.0,this,
                             getLang("XvFuncSystem_GeometryCreate_Width","矩形宽度"));
    rectangleHeight=new XReal("rectangleHeight",80.0,this,
                              getLang("XvFuncSystem_GeometryCreate_Height","矩形高度"));
}

GeometryCreateResult::GeometryCreateResult()
{
    point=new XPoint2D("point",0.0,0.0,this,
                       getLang("XvFuncSystem_GeometryCreate_ResultPoint","点"));
    line=new XLine2D("line",QPointF(0.0,0.0),QPointF(1.0,0.0),this,
                     getLang("XvFuncSystem_GeometryCreate_ResultLine","线"));
    circle=new XCircle2D("circle",0.0,0.0,1.0,this,
                         getLang("XvFuncSystem_GeometryCreate_ResultCircle","圆"));
    rectangle=new XRect2D("rectangle",0.0,0.0,1.0,1.0,this,
                           getLang("XvFuncSystem_GeometryCreate_ResultRectangle","矩形"));
    createdType=new XInt("createdType",GeometryCreate::Point,this,
                         getLang("XvFuncSystem_GeometryCreate_CreatedType","创建类型"));
    created=new XBool("created",false,this,
                      getLang("XvFuncSystem_GeometryCreate_Created","创建成功"));
}

GeometryCreate::GeometryCreate(QObject *parent)
    :GeometryOperatorBase(parent),m_param(new GeometryCreateParam()),
      m_result(new GeometryCreateResult())
{
    _funcRole="GeometryCreate";
    _funcName=getLang("XvFuncSystem_GeometryCreate_Name","几何创建");
}

GeometryCreate::~GeometryCreate()
{
    delete m_param;
    delete m_result;
}

QStringList GeometryCreate::activeParameterNames() const
{
    switch(m_shapeType)
    {
    case Point: return {"pointX","pointY"};
    case Line: return {"lineX1","lineY1","lineX2","lineY2"};
    case Circle: return {"circleCenterX","circleCenterY","circleRadius"};
    case Rectangle:
        return {"rectangleX","rectangleY","rectangleWidth","rectangleHeight"};
    }
    return {};
}

EXvFuncRunStatus GeometryCreate::run()
{
    if(!m_param || !m_result || m_shapeType<Point || m_shapeType>Rectangle)
    {
        setRunMsg(getLang("XvFuncSystem_GeometryCreate_InvalidType",
                          "Geometry shape type is invalid"));
        return EXvFuncRunStatus::Error;
    }
    for(const QString &name:activeParameterNames())
    {
        auto value=dynamic_cast<XReal*>(getParamsByName(name));
        if(!value || !qIsFinite(value->value()))
        {
            setRunMsg(getLang("XvFuncSystem_GeometryCreate_InvalidScalar",
                              "Geometry coordinates and sizes must be finite"));
            return EXvFuncRunStatus::Error;
        }
    }

    XPoint2D pointCandidate(0.0,0.0);
    XLine2D lineCandidate(QPointF(0.0,0.0),QPointF(1.0,0.0));
    XCircle2D circleCandidate(0.0,0.0,1.0);
    XRect2D rectangleCandidate(0.0,0.0,1.0,1.0);
    bool valid=false;
    switch(m_shapeType)
    {
    case Point:
        valid=pointCandidate.setValue(m_param->pointX->value(),m_param->pointY->value());
        break;
    case Line:
        valid=lineCandidate.setValue(QPointF(m_param->lineX1->value(),m_param->lineY1->value()),
                                     QPointF(m_param->lineX2->value(),m_param->lineY2->value()));
        break;
    case Circle:
        valid=circleCandidate.setValue(m_param->circleCenterX->value(),
                                       m_param->circleCenterY->value(),
                                       m_param->circleRadius->value());
        break;
    case Rectangle:
        valid=rectangleCandidate.setValue(m_param->rectangleX->value(),
                                          m_param->rectangleY->value(),
                                          m_param->rectangleWidth->value(),
                                          m_param->rectangleHeight->value());
        break;
    }
    if(!valid)
    {
        setRunMsg(getLang("XvFuncSystem_GeometryCreate_Degenerate",
                          "Geometry is degenerate or has an invalid size"));
        return EXvFuncRunStatus::Error;
    }

    if(!m_result->point->setData(&pointCandidate)
            || !m_result->line->setData(&lineCandidate)
            || !m_result->circle->setData(&circleCandidate)
            || !m_result->rectangle->setData(&rectangleCandidate))
    {
        setRunMsg(getLang("XvFuncSystem_GeometryCreate_CommitFailed",
                          "Geometry result could not be committed"));
        return EXvFuncRunStatus::Error;
    }
    m_result->createdType->setValue(static_cast<int>(m_shapeType));
    m_result->created->setValue(true);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}
