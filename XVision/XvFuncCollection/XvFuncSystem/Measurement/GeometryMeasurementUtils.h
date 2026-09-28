#ifndef GEOMETRYMEASUREMENTUTILS_H
#define GEOMETRYMEASUREMENTUTILS_H

#include "XVFuncSystemGlobal.h"

#include <QPointF>
#include <QString>

namespace GeometryMeasurementUtils
{
struct XVFUNCSYSTEM_EXPORT Candidate
{
    double rawValue=0.0;
    QPointF annotationStart;
    QPointF annotationEnd;
};

XVFUNCSYSTEM_EXPORT bool pointPoint(const QPointF &first,const QPointF &second,
                                    Candidate &candidate,QString &error);
XVFUNCSYSTEM_EXPORT bool pointLine(const QPointF &point,const QPointF &lineStart,
                                   const QPointF &lineEnd,Candidate &candidate,
                                   QString &error);
XVFUNCSYSTEM_EXPORT bool pointCircle(const QPointF &point,const QPointF &center,
                                     double radius,Candidate &candidate,
                                     QString &error);
XVFUNCSYSTEM_EXPORT bool lineCircle(const QPointF &lineStart,const QPointF &lineEnd,
                                    const QPointF &center,double radius,
                                    Candidate &candidate,QString &error);
XVFUNCSYSTEM_EXPORT bool lineLine(const QPointF &firstStart,const QPointF &firstEnd,
                                  const QPointF &secondStart,const QPointF &secondEnd,
                                  Candidate &candidate,QString &error);
XVFUNCSYSTEM_EXPORT bool lineLineAngle(const QPointF &firstStart,const QPointF &firstEnd,
                                       const QPointF &secondStart,const QPointF &secondEnd,
                                       Candidate &candidate,QString &error);
XVFUNCSYSTEM_EXPORT bool circleCircle(const QPointF &firstCenter,double firstRadius,
                                      const QPointF &secondCenter,double secondRadius,
                                      Candidate &candidate,QString &error);
}

#endif // GEOMETRYMEASUREMENTUTILS_H
