#include "GeometryMeasurementUtils.h"

#include <QLineF>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
constexpr double RelativeEpsilon=1.0e-12;

bool finitePoint(const QPointF &point)
{
    return qIsFinite(point.x()) && qIsFinite(point.y());
}

double cross(const QPointF &left,const QPointF &right)
{
    return left.x()*right.y()-left.y()*right.x();
}

double dot(const QPointF &left,const QPointF &right)
{
    return left.x()*right.x()+left.y()*right.y();
}

double coordinateScale(std::initializer_list<QPointF> points)
{
    double scale=1.0;
    for(const QPointF &point:points)
        scale=std::max({scale,qAbs(point.x()),qAbs(point.y())});
    return scale;
}

bool lineDirection(const QPointF &start,const QPointF &end,QPointF &direction,
                   double &length,QString &error)
{
    if(!finitePoint(start) || !finitePoint(end))
    {
        error="Line endpoints must be finite";
        return false;
    }
    direction=end-start;
    length=std::hypot(direction.x(),direction.y());
    if(!qIsFinite(length)
            || length<=RelativeEpsilon*coordinateScale({start,end}))
    {
        error="Line endpoints must define a non-degenerate line";
        return false;
    }
    return true;
}

bool validCircle(const QPointF &center,double radius,QString &error)
{
    if(!finitePoint(center) || !qIsFinite(radius) || radius<=0.0)
    {
        error="Circle center must be finite and radius must be positive";
        return false;
    }
    return true;
}

bool finish(GeometryMeasurementUtils::Candidate &candidate,QString &error)
{
    if(!qIsFinite(candidate.rawValue) || !finitePoint(candidate.annotationStart)
            || !finitePoint(candidate.annotationEnd))
    {
        error="Geometry measurement produced a non-finite result";
        return false;
    }
    error.clear();
    return true;
}

QPointF projection(const QPointF &point,const QPointF &lineStart,
                   const QPointF &direction)
{
    const double denominator=dot(direction,direction);
    return lineStart+direction*(dot(point-lineStart,direction)/denominator);
}

QPointF unitOrPositiveX(const QPointF &vector)
{
    const double length=std::hypot(vector.x(),vector.y());
    return length>RelativeEpsilon ? vector/length : QPointF(1.0,0.0);
}
}

bool GeometryMeasurementUtils::pointPoint(const QPointF &first,const QPointF &second,
                                           Candidate &candidate,QString &error)
{
    if(!finitePoint(first) || !finitePoint(second))
    {
        error="Points must be finite";
        return false;
    }
    Candidate prepared;
    prepared.rawValue=QLineF(first,second).length();
    prepared.annotationStart=first;
    prepared.annotationEnd=second;
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::pointLine(const QPointF &point,const QPointF &lineStart,
                                          const QPointF &lineEnd,Candidate &candidate,
                                          QString &error)
{
    if(!finitePoint(point))
    {
        error="Point must be finite";
        return false;
    }
    QPointF direction;
    double length=0.0;
    if(!lineDirection(lineStart,lineEnd,direction,length,error)) return false;
    Candidate prepared;
    prepared.rawValue=cross(direction,point-lineStart)/length;
    prepared.annotationStart=projection(point,lineStart,direction);
    prepared.annotationEnd=point;
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::pointCircle(const QPointF &point,const QPointF &center,
                                            double radius,Candidate &candidate,
                                            QString &error)
{
    if(!finitePoint(point))
    {
        error="Point must be finite";
        return false;
    }
    if(!validCircle(center,radius,error)) return false;
    const QPointF centerToPoint=point-center;
    const double distance=std::hypot(centerToPoint.x(),centerToPoint.y());
    Candidate prepared;
    prepared.rawValue=distance-radius;
    prepared.annotationStart=center+unitOrPositiveX(centerToPoint)*radius;
    prepared.annotationEnd=point;
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::lineCircle(const QPointF &lineStart,const QPointF &lineEnd,
                                           const QPointF &center,double radius,
                                           Candidate &candidate,QString &error)
{
    if(!validCircle(center,radius,error)) return false;
    QPointF direction;
    double length=0.0;
    if(!lineDirection(lineStart,lineEnd,direction,length,error)) return false;
    const QPointF projected=projection(center,lineStart,direction);
    const double signedDistance=cross(direction,center-lineStart)/length;
    const QPointF lineToCenter=center-projected;
    QPointF towardLine=unitOrPositiveX(projected-center);
    if(std::hypot(lineToCenter.x(),lineToCenter.y())<=RelativeEpsilon)
        towardLine=unitOrPositiveX(QPointF(direction.y(),-direction.x()));
    Candidate prepared;
    prepared.rawValue=qAbs(signedDistance)-radius;
    prepared.annotationStart=projected;
    prepared.annotationEnd=center+towardLine*radius;
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::lineLine(const QPointF &firstStart,const QPointF &firstEnd,
                                         const QPointF &secondStart,const QPointF &secondEnd,
                                         Candidate &candidate,QString &error)
{
    QPointF firstDirection,secondDirection;
    double firstLength=0.0,secondLength=0.0;
    if(!lineDirection(firstStart,firstEnd,firstDirection,firstLength,error)
            || !lineDirection(secondStart,secondEnd,secondDirection,secondLength,error))
        return false;
    Candidate prepared;
    const double determinant=cross(firstDirection,secondDirection);
    if(qAbs(determinant)<=RelativeEpsilon*firstLength*secondLength)
    {
        prepared.rawValue=cross(firstDirection,secondStart-firstStart)/firstLength;
        prepared.annotationStart=projection(secondStart,firstStart,firstDirection);
        prepared.annotationEnd=secondStart;
    }
    else
    {
        const double firstParameter=cross(secondStart-firstStart,secondDirection)
                /determinant;
        const QPointF intersection=firstStart+firstDirection*firstParameter;
        prepared.rawValue=0.0;
        prepared.annotationStart=intersection;
        prepared.annotationEnd=intersection;
    }
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::lineLineAngle(const QPointF &firstStart,
                                              const QPointF &firstEnd,
                                              const QPointF &secondStart,
                                              const QPointF &secondEnd,
                                              Candidate &candidate,QString &error)
{
    QPointF firstDirection,secondDirection;
    double firstLength=0.0,secondLength=0.0;
    if(!lineDirection(firstStart,firstEnd,firstDirection,firstLength,error)
            || !lineDirection(secondStart,secondEnd,secondDirection,secondLength,error))
        return false;
    const double normalized=qBound(0.0,qAbs(dot(firstDirection,secondDirection)
                                            /(firstLength*secondLength)),1.0);
    Candidate prepared;
    prepared.rawValue=std::acos(normalized);
    const double determinant=cross(firstDirection,secondDirection);
    if(qAbs(determinant)>RelativeEpsilon*firstLength*secondLength)
    {
        const double firstParameter=cross(secondStart-firstStart,secondDirection)
                /determinant;
        prepared.annotationStart=firstStart+firstDirection*firstParameter;
        prepared.annotationEnd=prepared.annotationStart;
    }
    else
    {
        prepared.annotationStart=firstStart;
        prepared.annotationEnd=secondStart;
    }
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}

bool GeometryMeasurementUtils::circleCircle(const QPointF &firstCenter,
                                             double firstRadius,
                                             const QPointF &secondCenter,
                                             double secondRadius,
                                             Candidate &candidate,QString &error)
{
    if(!validCircle(firstCenter,firstRadius,error)
            || !validCircle(secondCenter,secondRadius,error)) return false;
    const QPointF firstToSecond=secondCenter-firstCenter;
    const double distance=std::hypot(firstToSecond.x(),firstToSecond.y());
    const QPointF direction=unitOrPositiveX(firstToSecond);
    Candidate prepared;
    prepared.rawValue=distance-firstRadius-secondRadius;
    prepared.annotationStart=firstCenter+direction*firstRadius;
    prepared.annotationEnd=secondCenter-direction*secondRadius;
    if(!finish(prepared,error)) return false;
    candidate=prepared;
    return true;
}
