#include <QtTest>

#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QSet>

#include <cmath>

#include "GeometryCreate.h"
#include "GeometryMeasure.h"
#include "GeometryMeasurementUtils.h"
#include "XImage.h"
#include "XPoint2D.h"
#include "XVisionSharedData.h"

using namespace XvCore;

namespace
{
constexpr double Pi=3.14159265358979323846;

template<class T>
T *parameter(XvFunc &function,const QString &name)
{
    return dynamic_cast<T*>(function.getParamsByName(name));
}

template<class T>
T *result(XvFunc &function,const QString &name)
{
    return dynamic_cast<T*>(function.getResultsByName(name));
}

void near(double actual,double expected,double tolerance=1.0e-9)
{
    QVERIFY2(qAbs(actual-expected)<=tolerance,
             qPrintable(QString("actual=%1 expected=%2").arg(actual,0,'g',17)
                        .arg(expected,0,'g',17)));
}
}

class GeometryMeasurementTests:public QObject
{
    Q_OBJECT
private slots:
    void pureDistanceFormulas();
    void pureAnglesAndDegenerateInputs();
    void canonicalContractsExposeAllModes();
    void geometryCreateBuildsFourTypesAndRollsBack();
    void geometryMeasureRunsSevenModesAndConversions();
    void toleranceAnnotationAndFailureRollback();
};

void GeometryMeasurementTests::pureDistanceFormulas()
{
    GeometryMeasurementUtils::Candidate candidate;
    QString error;
    QVERIFY2(GeometryMeasurementUtils::pointPoint(
                 QPointF(0.0,0.0),QPointF(3.0,4.0),candidate,error),qPrintable(error));
    near(candidate.rawValue,5.0);

    QVERIFY(GeometryMeasurementUtils::pointLine(
                QPointF(2.0,5.0),QPointF(0.0,0.0),QPointF(10.0,0.0),candidate,error));
    near(candidate.rawValue,5.0);
    near(candidate.annotationStart.x(),2.0);
    near(candidate.annotationStart.y(),0.0);
    QVERIFY(GeometryMeasurementUtils::pointLine(
                QPointF(2.0,-3.0),QPointF(0.0,0.0),QPointF(10.0,0.0),candidate,error));
    near(candidate.rawValue,-3.0);

    QVERIFY(GeometryMeasurementUtils::pointCircle(
                QPointF(8.0,0.0),QPointF(0.0,0.0),5.0,candidate,error));
    near(candidate.rawValue,3.0);
    QVERIFY(GeometryMeasurementUtils::pointCircle(
                QPointF(5.0,0.0),QPointF(0.0,0.0),5.0,candidate,error));
    near(candidate.rawValue,0.0);
    QVERIFY(GeometryMeasurementUtils::pointCircle(
                QPointF(2.0,0.0),QPointF(0.0,0.0),5.0,candidate,error));
    near(candidate.rawValue,-3.0);

    QVERIFY(GeometryMeasurementUtils::lineCircle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),QPointF(2.0,10.0),3.0,
                candidate,error));
    near(candidate.rawValue,7.0);
    QVERIFY(GeometryMeasurementUtils::lineCircle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),QPointF(2.0,3.0),3.0,
                candidate,error));
    near(candidate.rawValue,0.0);
    QVERIFY(GeometryMeasurementUtils::lineCircle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),QPointF(2.0,2.0),3.0,
                candidate,error));
    near(candidate.rawValue,-1.0);

    QVERIFY(GeometryMeasurementUtils::circleCircle(
                QPointF(0.0,0.0),5.0,QPointF(12.0,0.0),7.0,candidate,error));
    near(candidate.rawValue,0.0);
    QVERIFY(GeometryMeasurementUtils::circleCircle(
                QPointF(0.0,0.0),5.0,QPointF(20.0,0.0),7.0,candidate,error));
    near(candidate.rawValue,8.0);
    QVERIFY(GeometryMeasurementUtils::circleCircle(
                QPointF(0.0,0.0),5.0,QPointF(3.0,0.0),7.0,candidate,error));
    near(candidate.rawValue,-9.0);

    QVERIFY(GeometryMeasurementUtils::lineLine(
                QPointF(0.0,0.0),QPointF(10.0,0.0),
                QPointF(0.0,4.0),QPointF(10.0,4.0),candidate,error));
    near(candidate.rawValue,4.0);
    QVERIFY(GeometryMeasurementUtils::lineLine(
                QPointF(0.0,0.0),QPointF(10.0,0.0),
                QPointF(5.0,-5.0),QPointF(5.0,5.0),candidate,error));
    near(candidate.rawValue,0.0);
    near(candidate.annotationStart.x(),5.0);
    near(candidate.annotationStart.y(),0.0);
}

void GeometryMeasurementTests::pureAnglesAndDegenerateInputs()
{
    GeometryMeasurementUtils::Candidate candidate;
    QString error;
    QVERIFY(GeometryMeasurementUtils::lineLineAngle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),
                QPointF(0.0,0.0),QPointF(0.0,10.0),candidate,error));
    near(candidate.rawValue,Pi/2.0);
    QVERIFY(GeometryMeasurementUtils::lineLineAngle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),
                QPointF(0.0,0.0),QPointF(10.0,10.0),candidate,error));
    near(candidate.rawValue,Pi/4.0);
    QVERIFY(GeometryMeasurementUtils::lineLineAngle(
                QPointF(0.0,0.0),QPointF(10.0,0.0),
                QPointF(0.0,0.0),QPointF(-10.0,0.0),candidate,error));
    near(candidate.rawValue,0.0);

    candidate.rawValue=42.0;
    QVERIFY(!GeometryMeasurementUtils::pointLine(
                QPointF(1.0,1.0),QPointF(2.0,2.0),QPointF(2.0,2.0),
                candidate,error));
    QVERIFY(!error.isEmpty());
    near(candidate.rawValue,42.0);
    QVERIFY(!GeometryMeasurementUtils::circleCircle(
                QPointF(),0.0,QPointF(3.0,0.0),1.0,candidate,error));
    QVERIFY(!GeometryMeasurementUtils::pointPoint(
                QPointF(qQNaN(),0.0),QPointF(),candidate,error));
}

void GeometryMeasurementTests::canonicalContractsExposeAllModes()
{
    GeometryCreate creator;
    GeometryMeasure measure;
    QCOMPARE(creator.funcRole(),QString("GeometryCreate"));
    QCOMPARE(measure.funcRole(),QString("GeometryMeasure"));
    QCOMPARE(creator.persistentPropertyNames(),QStringList({"shapeType"}));
    QCOMPARE(measure.persistentPropertyNames(),QStringList({"mode"}));

    const QList<QPair<GeometryOperatorBase*,int>> operators={{&creator,4},{&measure,7}};
    for(const auto &entry:operators)
    {
        const QByteArray propertyName=entry.first->selectorPropertyName().toLatin1();
        const int index=entry.first->metaObject()->indexOfProperty(propertyName.constData());
        QVERIFY(index>=0);
        const QMetaProperty property=entry.first->metaObject()->property(index);
        QVERIFY(property.isEnumType());
        QCOMPARE(property.enumerator().keyCount(),entry.second);
        for(int key=0;key<property.enumerator().keyCount();++key)
        {
            QVERIFY(property.write(entry.first,property.enumerator().value(key)));
            const QStringList names=entry.first->activeParameterNames();
            QVERIFY(!names.isEmpty());
            QSet<QString> unique;
            for(const QString &name:names)
            {
                QVERIFY2(entry.first->getParamsByName(name),qPrintable(name));
                QVERIFY2(!unique.contains(name),qPrintable(name));
                unique.insert(name);
            }
        }
    }
}

void GeometryMeasurementTests::geometryCreateBuildsFourTypesAndRollsBack()
{
    GeometryCreate creator;
    creator.setShapeType(GeometryCreate::Point);
    parameter<XReal>(creator,"pointX")->setValue(12.5);
    parameter<XReal>(creator,"pointY")->setValue(-4.25);
    QCOMPARE(creator.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XPoint2D>(creator,"point")->x(),12.5);
    near(result<XPoint2D>(creator,"point")->y(),-4.25);

    creator.setShapeType(GeometryCreate::Line);
    parameter<XReal>(creator,"lineX1")->setValue(1.0);
    parameter<XReal>(creator,"lineY1")->setValue(2.0);
    parameter<XReal>(creator,"lineX2")->setValue(8.0);
    parameter<XReal>(creator,"lineY2")->setValue(9.0);
    QCOMPARE(creator.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XLine2D>(creator,"line")->start(),QPointF(1.0,2.0));
    QCOMPARE(result<XLine2D>(creator,"line")->end(),QPointF(8.0,9.0));

    creator.setShapeType(GeometryCreate::Circle);
    parameter<XReal>(creator,"circleCenterX")->setValue(11.0);
    parameter<XReal>(creator,"circleCenterY")->setValue(13.0);
    parameter<XReal>(creator,"circleRadius")->setValue(7.0);
    QCOMPARE(creator.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XCircle2D>(creator,"circle")->centerX(),11.0);
    near(result<XCircle2D>(creator,"circle")->radius(),7.0);

    creator.setShapeType(GeometryCreate::Rectangle);
    parameter<XReal>(creator,"rectangleX")->setValue(3.0);
    parameter<XReal>(creator,"rectangleY")->setValue(5.0);
    parameter<XReal>(creator,"rectangleWidth")->setValue(40.0);
    parameter<XReal>(creator,"rectangleHeight")->setValue(20.0);
    QCOMPARE(creator.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XRect2D>(creator,"rectangle")->width(),40.0);
    QCOMPARE(result<XInt>(creator,"createdType")->value(),int(GeometryCreate::Rectangle));
    QVERIFY(result<XBool>(creator,"created")->value());

    const double previousWidth=result<XRect2D>(creator,"rectangle")->width();
    creator.setShapeType(GeometryCreate::Line);
    parameter<XReal>(creator,"lineX2")->setValue(parameter<XReal>(creator,"lineX1")->value());
    parameter<XReal>(creator,"lineY2")->setValue(parameter<XReal>(creator,"lineY1")->value());
    QCOMPARE(creator.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(!creator.getXvFuncRunMsg().isEmpty());
    near(result<XRect2D>(creator,"rectangle")->width(),previousWidth);
    QCOMPARE(result<XInt>(creator,"createdType")->value(),int(GeometryCreate::Rectangle));
}

void GeometryMeasurementTests::geometryMeasureRunsSevenModesAndConversions()
{
    GeometryMeasure measure;
    const QList<QPair<GeometryMeasure::Mode,double>> expected={
        {GeometryMeasure::CircleCircle,10.0},
        {GeometryMeasure::LineCircle,10.0},
        {GeometryMeasure::LineLineAngle,0.0},
        {GeometryMeasure::LineLine,10.0},
        {GeometryMeasure::PointCircle,10.0},
        {GeometryMeasure::PointLine,0.0},
        {GeometryMeasure::PointPoint,5.0}
    };
    for(const auto &entry:expected)
    {
        measure.setMode(entry.first);
        QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
        near(result<XReal>(measure,"value")->value(),entry.second);
        QVERIFY(result<XMeasurementResult>(measure,"measurement"));
    }

    measure.setMode(GeometryMeasure::PointLine);
    QVERIFY(parameter<XPoint2D>(measure,"point1")->setValue(2.0,-2.0));
    parameter<XBool>(measure,"absoluteValue")->setValue(true);
    parameter<XReal>(measure,"lowerLimit")->setValue(2.0);
    parameter<XReal>(measure,"upperLimit")->setValue(2.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XReal>(measure,"rawValue")->value(),-2.0);
    near(result<XReal>(measure,"value")->value(),2.0);
    QVERIFY(result<XBool>(measure,"passed")->value());

    parameter<XBool>(measure,"absoluteValue")->setValue(false);
    parameter<XReal>(measure,"scale")->setValue(0.5);
    parameter<XString>(measure,"unit")->setValue(" mm ");
    parameter<XReal>(measure,"lowerLimit")->setValue(-1.0);
    parameter<XReal>(measure,"upperLimit")->setValue(-1.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XReal>(measure,"rawValue")->value(),-2.0);
    near(result<XReal>(measure,"value")->value(),-1.0);
    QVERIFY(result<XBool>(measure,"passed")->value());
    QCOMPARE(result<XMeasurementResult>(measure,"measurement")->unit(),QString("mm"));

    measure.setMode(GeometryMeasure::LineLineAngle);
    QVERIFY(parameter<XLine2D>(measure,"line2")->setValue(
                QPointF(0.0,0.0),QPointF(10.0,10.0)));
    parameter<XReal>(measure,"lowerLimit")->setValue(45.0);
    parameter<XReal>(measure,"upperLimit")->setValue(45.0);
    parameter<XInt>(measure,"angleUnit")->setValue(GeometryMeasure::Degrees);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XReal>(measure,"rawValue")->value(),Pi/4.0);
    near(result<XReal>(measure,"value")->value(),45.0);
    QCOMPARE(result<XMeasurementResult>(measure,"measurement")->unit(),QString("deg"));
    parameter<XInt>(measure,"angleUnit")->setValue(GeometryMeasure::Radians);
    parameter<XReal>(measure,"lowerLimit")->setValue(Pi/4.0);
    parameter<XReal>(measure,"upperLimit")->setValue(Pi/4.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    near(result<XReal>(measure,"value")->value(),Pi/4.0);
    QCOMPARE(result<XMeasurementResult>(measure,"measurement")->unit(),QString("rad"));
}

void GeometryMeasurementTests::toleranceAnnotationAndFailureRollback()
{
    GeometryMeasure measure;
    measure.setMode(GeometryMeasure::PointPoint);
    QVERIFY(parameter<XPoint2D>(measure,"point1")->setValue(20.0,30.0));
    QVERIFY(parameter<XPoint2D>(measure,"point2")->setValue(80.0,30.0));
    parameter<XReal>(measure,"lowerLimit")->setValue(60.0);
    parameter<XReal>(measure,"upperLimit")->setValue(60.0);
    QImage input(120,80,QImage::Format_RGB32);
    input.fill(Qt::white);
    parameter<XImage>(measure,"inputImage")->setValue(input);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(result<XBool>(measure,"passed")->value());
    const QImage annotation=result<XImage>(measure,"outputImage")->value();
    QVERIFY(!annotation.isNull());
    QCOMPARE(annotation.size(),input.size());
    QVERIFY(annotation.convertToFormat(QImage::Format_RGB32)!=input);
    input.fill(Qt::black);
    QCOMPARE(annotation,result<XImage>(measure,"outputImage")->value());

    parameter<XReal>(measure,"lowerLimit")->setValue(61.0);
    parameter<XReal>(measure,"upperLimit")->setValue(70.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(!result<XBool>(measure,"passed")->value());
    const QImage ngAnnotation=result<XImage>(measure,"outputImage")->value();
    QVERIFY(ngAnnotation!=annotation);

    parameter<XReal>(measure,"lowerLimit")->setValue(70.0);
    parameter<XReal>(measure,"upperLimit")->setValue(69.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Error);
    near(result<XReal>(measure,"value")->value(),60.0);
    QVERIFY(!result<XBool>(measure,"passed")->value());
    QCOMPARE(result<XImage>(measure,"outputImage")->value(),ngAnnotation);

    parameter<XReal>(measure,"lowerLimit")->setValue(0.0);
    parameter<XReal>(measure,"upperLimit")->setValue(60.0);
    parameter<XReal>(measure,"scale")->setValue(0.0);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Error);
    near(result<XReal>(measure,"value")->value(),60.0);
    parameter<XReal>(measure,"scale")->setValue(1.0);
    parameter<XString>(measure,"unit")->setValue("   ");
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Error);
    near(result<XReal>(measure,"value")->value(),60.0);
    parameter<XString>(measure,"unit")->setValue("px");
    parameter<XInt>(measure,"precision")->setValue(10);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Error);
    measure.setMode(GeometryMeasure::LineLineAngle);
    parameter<XInt>(measure,"precision")->setValue(3);
    parameter<XInt>(measure,"angleUnit")->setValue(2);
    QCOMPARE(measure.runXvFunc(),EXvFuncRunStatus::Error);
    near(result<XReal>(measure,"value")->value(),60.0);
}

QTEST_MAIN(GeometryMeasurementTests)
#include "tst_geometrymeasurement.moc"
