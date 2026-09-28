#include "XvDisplayRotateRectRoi.h"

#include <QPainter>
#include <QtMath>

namespace
{
constexpr double Pi=3.14159265358979323846;
}

XvDisplayRotateRectRoi::XvDisplayRotateRectRoi(QPointF centerPos,double len1,
                                               double len2,double angle,
                                               QObject *parent)
    :XvDisplayBaseRoiItem(centerPos,parent),
      m_len1(1.0),
      m_len2(1.0),
      m_angle(0.0)
{
    if(qIsFinite(len1) && qIsFinite(len2) && qIsFinite(angle)
            && len1>0.0 && len2>0.0)
    {
        m_len1=len1;
        m_len2=len2;
        m_angle=angle;
    }
    updateControlGeometry();

    m_controlItem1=new XvDisplayControlItem(this,m_pt1,XvDisplayControlItem::Control,
                                            XvDisplayControlItem::Circle);
    m_controlItem2=new XvDisplayControlItem(this,m_pt2,XvDisplayControlItem::Control,
                                            XvDisplayControlItem::Circle);
    m_controlItem3=new XvDisplayControlItem(this,m_pt3,XvDisplayControlItem::Control,
                                            XvDisplayControlItem::Circle);
    m_controlItem4=new XvDisplayControlItem(this,m_pt4,XvDisplayControlItem::Control,
                                            XvDisplayControlItem::Circle);
}

XRotateRectRoi XvDisplayRotateRectRoi::geometry() const
{
    const QPointF center=mapToScene(m_ptCenterPos);
    return XRotateRectRoi(center.x(),center.y(),m_len1,m_len2,m_angle);
}

bool XvDisplayRotateRectRoi::setGeometry(const XRotateRectRoi &geometry)
{
    if(geometry.length1()<=0.0 || geometry.length2()<=0.0
            || !qIsFinite(geometry.centerX()) || !qIsFinite(geometry.centerY())
            || !qIsFinite(geometry.length1()) || !qIsFinite(geometry.length2())
            || !qIsFinite(geometry.angle()))
    {
        return false;
    }

    prepareGeometryChange();
    m_ptCenterPos=mapFromScene(QPointF(geometry.centerX(),geometry.centerY()));
    m_len1=geometry.length1();
    m_len2=geometry.length2();
    m_angle=geometry.angle();
    updateControlGeometry();
    update();
    return true;
}

QRectF XvDisplayRotateRectRoi::boundingRect() const
{
    qreal minX=m_ptCenterPos.x();
    qreal maxX=m_ptCenterPos.x();
    qreal minY=m_ptCenterPos.y();
    qreal maxY=m_ptCenterPos.y();
    const QPointF points[]={m_pt1,m_pt2,m_pt3,m_pt4,m_ptArrow};
    for(const QPointF &point:points)
    {
        minX=qMin(minX,point.x());
        maxX=qMax(maxX,point.x());
        minY=qMin(minY,point.y());
        maxY=qMax(maxY,point.y());
    }
    const qreal margin=qMax(qreal(ControlSize_Default)/2.0,
                            qreal(RotateArrowLen)/2.0)+roiLineWidth();
    return QRectF(QPointF(minX-margin,minY-margin),
                  QPointF(maxX+margin,maxY+margin)).normalized();
}

bool XvDisplayRotateRectRoi::updateRoi(XvDisplayControlItem *controlItem)
{
    if(!controlItem || !XvDisplayBaseRoiItem::updateRoi(controlItem)) return false;
    const QPointF point=controlItem->getPos();
    const qreal dx=point.x()-m_ptCenterPos.x();
    const qreal dy=point.y()-m_ptCenterPos.y();
    const qreal length=qSqrt(dx*dx+dy*dy);
    if(length<=0.0) return false;

    prepareGeometryChange();
    const double direction=-qAtan2(dy,dx);
    if(controlItem==m_controlItem1)
    {
        m_angle=direction+Pi/2.0;
        m_len2=length;
    }
    else if(controlItem==m_controlItem2)
    {
        m_angle=direction;
        m_len1=length;
    }
    else if(controlItem==m_controlItem3)
    {
        m_angle=direction-Pi/2.0;
        m_len2=length;
    }
    else if(controlItem==m_controlItem4)
    {
        m_angle=direction-Pi;
        m_len1=length;
    }
    else
    {
        return false;
    }

    updateControlGeometry();
    update();
    return true;
}

void XvDisplayRotateRectRoi::updateControlGeometry()
{
    const double rSin=qSin(-m_angle);
    const double rCos=qCos(-m_angle);
    m_pt1=m_ptCenterPos+QPointF(-m_len2*rSin,m_len2*rCos);
    m_pt2=m_ptCenterPos+QPointF(m_len1*rCos,m_len1*rSin);
    m_pt3=m_ptCenterPos+QPointF(m_len2*rSin,-m_len2*rCos);
    m_pt4=m_ptCenterPos+QPointF(-m_len1*rCos,-m_len1*rSin);
    m_ptArrow=m_ptCenterPos+QPointF((m_len1+RotateArrowLen)*rCos,
                                    (m_len1+RotateArrowLen)*rSin);

    if(m_centerControlItem) m_centerControlItem->updatePos(m_ptCenterPos);
    if(m_controlItem1) m_controlItem1->updatePos(m_pt1);
    if(m_controlItem2) m_controlItem2->updatePos(m_pt2);
    if(m_controlItem3) m_controlItem3->updatePos(m_pt3);
    if(m_controlItem4) m_controlItem4->updatePos(m_pt4);
}

void XvDisplayRotateRectRoi::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    updatePainter(painter);

    painter->drawLine(m_ptArrow,m_pt2);
    const double arrowLength=RotateArrowLen/2.0;
    const double angle1=qAtan2(m_ptArrow.y()-m_pt2.y(),m_ptArrow.x()-m_pt2.x());
    const double angle2=qAtan2(m_ptArrow.x()-m_pt2.x(),m_ptArrow.y()-m_pt2.y());
    const QPointF arrow1(m_ptArrow.x()-arrowLength*qCos(angle1-0.5),
                         m_ptArrow.y()-arrowLength*qSin(angle1-0.5));
    const QPointF arrow2(m_ptArrow.x()-arrowLength*qSin(angle2-0.5),
                         m_ptArrow.y()-arrowLength*qCos(angle2-0.5));
    painter->drawLine(m_ptArrow,arrow1);
    painter->drawLine(m_ptArrow,arrow2);

    painter->save();
    painter->translate(m_ptCenterPos);
    painter->rotate(-m_angle*180.0/Pi);
    painter->drawRect(QRectF(-m_len1,-m_len2,m_len1*2.0,m_len2*2.0));
    painter->restore();
}
