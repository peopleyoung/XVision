#include <QtTest>
#include <QDomDocument>

#include <limits>
#include <memory>

#include "XObjectList.h"
#include "XVisionSharedData.h"
#include "XVisionRuntimeData.h"
#include "XvXmlUtils.h"

namespace
{
class FailingCloneValue:public XObject
{
public:
    explicit FailingCloneValue(int value,bool failClone=false)
        :XObject(),m_value(value),m_failClone(failClone) { }

    QString typeName() override { return "FailingCloneValue"; }
    XObject *clone() override
    {
        return m_failClone?nullptr:new FailingCloneValue(m_value,false);
    }
    bool getData(XObject *object) override
    {
        auto target=object?dynamic_cast<FailingCloneValue*>(object):nullptr;
        if(!target) return false;
        target->m_value=m_value;
        return true;
    }
    bool setData(XObject *object) override
    {
        auto source=object?dynamic_cast<FailingCloneValue*>(object):nullptr;
        if(!source) return false;
        m_value=source->m_value;
        return true;
    }
    int value() const { return m_value; }

private:
    int m_value=0;
    bool m_failClone=false;
};
}

class VisionMasterDataTests:public QObject
{
    Q_OBJECT
private slots:
    void invalidValuesPreserveData();
    void valuesCloneIndependently();
    void structuredValuesRoundTrip();
    void typedListRoundTrip();
    void listCopyRollsBack();
    void runtimeValuesOwnData();
};

void VisionMasterDataTests::invalidValuesPreserveData()
{
    XCircle2D circle("circle",2.0,3.0,4.0);
    QVERIFY(!circle.setValue(2.0,3.0,0.0));
    QCOMPARE(circle.centerX(),2.0);
    QCOMPARE(circle.centerY(),3.0);
    QCOMPARE(circle.radius(),4.0);

    XLine2D line("line",QPointF(0.0,0.0),QPointF(1.0,1.0));
    QVERIFY(!line.setValue(QPointF(0.0,0.0),QPointF(0.0,0.0)));
    QCOMPARE(line.end(),QPointF(1.0,1.0));

    XRect2D rect("rect",1.0,2.0,3.0,4.0);
    QVERIFY(!rect.setValue(1.0,2.0,-1.0,4.0));
    QCOMPARE(rect.width(),3.0);

    XContour contour("contour",{QPointF(0.0,0.0),QPointF(1.0,1.0)});
    QVERIFY(!contour.setValue({QPointF(0.0,0.0),
                               QPointF(std::numeric_limits<double>::infinity(),1.0)}));
    QCOMPARE(contour.points().at(1),QPointF(1.0,1.0));

    XKeyPoint keyPoint("keypoint",1.0,2.0,3.0,0.2,0.8,1,2);
    QVERIFY(!keyPoint.setValue(1.0,2.0,0.0,0.2,0.8,1,2));
    QCOMPARE(keyPoint.size(),3.0);

    XMeasurementResult measurement("measurement",12.0,"mm",10.0,15.0,true);
    QVERIFY(!measurement.setValue(12.0,"mm",16.0,15.0,true));
    QCOMPARE(measurement.lower(),10.0);

    XClassificationResult classification("class",2,"bolt",0.8);
    QVERIFY(!classification.setValue(2,"",0.8));
    QCOMPARE(classification.className(),QString("bolt"));
    QVERIFY(!classification.setValue(2,"bolt",1.1));
    QCOMPARE(classification.score(),0.8);
}

void VisionMasterDataTests::valuesCloneIndependently()
{
    XContour contour("contour",{QPointF(0.0,0.0),QPointF(2.0,0.0),QPointF(2.0,2.0)});
    std::unique_ptr<XContour> clone(dynamic_cast<XContour*>(contour.clone()));
    QVERIFY(clone);
    QVERIFY(clone->setValue({QPointF(0.0,0.0),QPointF(5.0,0.0)}));
    QCOMPARE(contour.points().at(1),QPointF(2.0,0.0));

    XKeyPoint keyPoint("point",4.0,5.0,2.0,0.2,0.9,1,3);
    std::unique_ptr<XKeyPoint> keyPointClone(dynamic_cast<XKeyPoint*>(keyPoint.clone()));
    QVERIFY(keyPointClone);
    QVERIFY(keyPointClone->setValue(8.0,9.0,2.0,0.2,0.9,1,3));
    QCOMPARE(keyPoint.x(),4.0);
}

void VisionMasterDataTests::structuredValuesRoundTrip()
{
    QString error;

    XLine2D lineSource("line",QPointF(1.0,2.0),QPointF(3.0,4.0));
    XLine2D lineTarget("line");
    XCircle2D circleSource("circle",4.0,5.0,6.0);
    XCircle2D circleTarget("circle");
    XRect2D rectSource("rect",2.0,3.0,7.0,8.0);
    XRect2D rectTarget("rect");
    XKeyPoint keyPointSource("keypoint",4.0,5.0,2.0,0.3,0.9,2,7);
    XKeyPoint keyPointTarget("keypoint");
    XMeasurementResult measurementSource("measurement",12.5,"mm",10.0,15.0,true);
    XMeasurementResult measurementTarget("measurement");
    XClassificationResult classificationSource("classification",3,"bolt",0.75);
    XClassificationResult classificationTarget("classification");
    const QList<QPair<XObject*,XObject*>> values={
        {&lineSource,&lineTarget},
        {&circleSource,&circleTarget},
        {&rectSource,&rectTarget},
        {&keyPointSource,&keyPointTarget},
        {&measurementSource,&measurementTarget},
        {&classificationSource,&classificationTarget}
    };

    for(const auto &value:values)
    {
        QDomDocument sourceDocument("test");
        QDomElement sourceRoot=sourceDocument.createElement("Root");
        sourceDocument.appendChild(sourceRoot);
        QVERIFY2(XvCore::XvXml::appendValue(sourceDocument,sourceRoot,"Value",
                                            value.first,error),qPrintable(error));
        QVERIFY2(XvCore::XvXml::readValue(sourceRoot.firstChildElement("Value"),
                                          value.second,error),qPrintable(error));

        QDomDocument targetDocument("test");
        QDomElement targetRoot=targetDocument.createElement("Root");
        targetDocument.appendChild(targetRoot);
        QVERIFY2(XvCore::XvXml::appendValue(targetDocument,targetRoot,"Value",
                                            value.second,error),qPrintable(error));
        QCOMPARE(targetDocument.toByteArray(),sourceDocument.toByteArray());
    }

    QDomDocument document("test");
    QDomElement root=document.createElement("Root");
    document.appendChild(root);
    QVERIFY(XvCore::XvXml::appendValue(document,root,"Value",&measurementSource,error));
    QDomElement invalid=root.firstChildElement("Value");
    invalid.setAttribute("unexpected","1");
    QVERIFY(!XvCore::XvXml::readValue(invalid,&measurementTarget,error));
    QCOMPARE(measurementTarget.value(),12.5);

    invalid.removeAttribute("unexpected");
    invalid.appendChild(document.createTextNode("unexpected"));
    QVERIFY(!XvCore::XvXml::readValue(invalid,&measurementTarget,error));
    QCOMPARE(measurementTarget.value(),12.5);

    QDomDocument keyPointDocument("test");
    QDomElement keyPointRoot=keyPointDocument.createElement("Root");
    keyPointDocument.appendChild(keyPointRoot);
    QVERIFY(XvCore::XvXml::appendValue(keyPointDocument,keyPointRoot,"Value",
                                      &keyPointSource,error));
    keyPointRoot.firstChildElement("Value").setAttribute("octave","02");
    QVERIFY(!XvCore::XvXml::readValue(keyPointRoot.firstChildElement("Value"),
                                     &keyPointTarget,error));
    QCOMPARE(keyPointTarget.octave(),2);
}

void VisionMasterDataTests::typedListRoundTrip()
{
    QDomDocument document("test");
    QDomElement root=document.createElement("Root");
    document.appendChild(root);
    QString error;
    XObjectList source("lines",XLine2D::type());
    QVERIFY(source.addValue(new XLine2D(QPointF(0.0,0.0),QPointF(2.0,0.0))));
    QVERIFY(XvCore::XvXml::appendValue(document,root,"Value",&source,error));

    XObjectList restored("lines",XLine2D::type());
    QVERIFY(XvCore::XvXml::readValue(root.firstChildElement("Value"),&restored,error));
    QCOMPARE(restored.count(),qsizetype(1));
    auto line=dynamic_cast<XLine2D*>(restored.value(0));
    QVERIFY(line);
    QCOMPARE(line->end(),QPointF(2.0,0.0));

    QDomElement item=root.firstChildElement("Value").firstChildElement("Item");
    item.setAttribute("type",XCircle2D::type());
    QVERIFY(!XvCore::XvXml::readValue(root.firstChildElement("Value"),&restored,error));
    QCOMPARE(restored.count(),qsizetype(1));
}

void VisionMasterDataTests::listCopyRollsBack()
{
    XObjectList source("source","FailingCloneValue");
    QVERIFY(source.addValue(new FailingCloneValue(1)));
    QVERIFY(source.addValue(new FailingCloneValue(2,true)));

    XObjectList target("target","FailingCloneValue");
    QVERIFY(target.addValue(new FailingCloneValue(9)));
    QVERIFY(!target.setData(&source));
    QCOMPARE(target.count(),qsizetype(1));
    auto preserved=dynamic_cast<FailingCloneValue*>(target.value(0));
    QVERIFY(preserved);
    QCOMPARE(preserved->value(),9);
}

void VisionMasterDataTests::runtimeValuesOwnData()
{
    QByteArray bytes("abcd",4);
    XByteArray byteValue("bytes",bytes);
    bytes[0]='z';
    QCOMPARE(byteValue.value(),QByteArray("abcd",4));

    XTensor tensor("tensor","float32",{1},QByteArray(4,0));
    QVERIFY(!tensor.setValue("float32",{2},QByteArray(4,0)));
    QCOMPARE(tensor.dimensions(),QVector<qint64>{1});
    QVERIFY(!tensor.setValue("string",{1},QByteArray(1,0)));
    QCOMPARE(tensor.elementType(),QString("float32"));
    QVERIFY(tensor.setValue("float16",{2},QByteArray(4,0)));
    QCOMPARE(tensor.elementType(),QString("float16"));
    QVERIFY(tensor.setValue("float32",{},QByteArray(4,0)));
    QVERIFY(tensor.dimensions().isEmpty());
    QCOMPARE(tensor.bytes().size(),4);
    QVERIFY(!tensor.setValue("float32",{},QByteArray(8,0)));
    QVERIFY(tensor.dimensions().isEmpty());

    QImage sourceMask(2,2,QImage::Format_Grayscale8);
    sourceMask.fill(QColor(10,10,10));
    XRegion region("region",sourceMask);
    sourceMask.fill(QColor(20,20,20));
    QCOMPARE(region.mask().constScanLine(0)[0],uchar(10));
    std::unique_ptr<XRegion> regionClone(dynamic_cast<XRegion*>(region.clone()));
    QVERIFY(regionClone);
    QImage replacement(2,2,QImage::Format_Grayscale8);
    replacement.fill(QColor(30,30,30));
    QVERIFY(regionClone->setValue(replacement));
    QCOMPARE(region.mask().constScanLine(0)[0],uchar(10));
    QImage invalidMask(2,2,QImage::Format_RGB888);
    QVERIFY(!region.setValue(invalidMask));
    QCOMPARE(region.mask().constScanLine(0)[0],uchar(10));
    QVERIFY(region.setValue(QImage()));
    QVERIFY(region.mask().isNull());

    QJsonObject details;
    details.insert("class","bolt");
    XDetectRecord record("record","id-1",123,"ok",details);
    QVERIFY(!record.setValue("",123,"ok",details));
    QCOMPARE(record.recordId(),QString("id-1"));

    XJsonValue json("json",QJsonDocument(details));
    std::unique_ptr<XJsonValue> clone(dynamic_cast<XJsonValue*>(json.clone()));
    QVERIFY(clone);
    QCOMPARE(clone->value().object().value("class").toString(),QString("bolt"));
    QVERIFY(json.setValue(QJsonDocument()));
    QVERIFY(json.value().isNull());
}

QTEST_MAIN(VisionMasterDataTests)
#include "tst_visionmasterdata.moc"
