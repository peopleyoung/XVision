#include <QtTest>

#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QSet>

#include <memory>

#include "OCascadeDetector.h"
#include "OCodeDetector.h"
#include "OMorphology.h"
#include "OPointFeature.h"
#include "ORegionDetector.h"
#include "OpenCvImageOperatorBase.h"
#include "XBool.h"
#include "XImage.h"
#include "XInt.h"
#include "XObjectList.h"
#include "XPoint2D.h"
#include "XReal.h"
#include "XString.h"
#include "XVisionSharedData.h"

using namespace XvCore;

namespace
{
std::unique_ptr<OpenCvImageOperatorBase> createRole(const QString &role)
{
    if(role=="OMorphology") return std::make_unique<OMorphology>();
    if(role=="ORegionDetector") return std::make_unique<ORegionDetector>();
    if(role=="OCodeDetector") return std::make_unique<OCodeDetector>();
    if(role=="OPointFeature") return std::make_unique<OPointFeature>();
    if(role=="OCascadeDetector") return std::make_unique<OCascadeDetector>();
    return nullptr;
}

XImage *inputImage(XvFunc *function)
{
    return dynamic_cast<XImage*>(function->getParamsByName("inputImage"));
}

XImage *outputImage(XvFunc *function)
{
    return dynamic_cast<XImage*>(function->getResultsByName("outputImage"));
}

QImage featureImage()
{
    QImage image(160,120,QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing,false);
    painter.setPen(QPen(Qt::white,3));
    painter.setBrush(Qt::white);
    painter.drawRect(QRect(15,15,45,35));
    painter.drawEllipse(QPoint(105,55),22,22);
    painter.drawLine(QPoint(5,105),QPoint(150,80));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(75,75,30,25));
    return image;
}

QImage seedImage()
{
    QImage image(2,2,QImage::Format_Grayscale8);
    image.fill(73);
    return image;
}
}

class OpenCvMorphologyDetectorTests:public QObject
{
    Q_OBJECT
private slots:
    void allCanonicalModesHaveCompleteContracts();
    void disabledBackendPreservesStructuredResults();
    void generatedImagesExerciseAvailableModes();
    void registeredQrAssetDecodes();
    void registeredCascadeAssetLoads();
    void invalidInputsPreserveExistingResults();
};

void OpenCvMorphologyDetectorTests::allCanonicalModesHaveCompleteContracts()
{
    const QMap<QString,int> roleModeCounts={
        {"OMorphology",7},
        {"ORegionDetector",6},
        {"OCodeDetector",1},
        {"OPointFeature",9},
        {"OCascadeDetector",2}
    };
    int totalModes=0;
    for(auto iterator=roleModeCounts.constBegin();iterator!=roleModeCounts.constEnd();++iterator)
    {
        std::unique_ptr<OpenCvImageOperatorBase> function=createRole(iterator.key());
        QVERIFY2(function,qPrintable(iterator.key()));
        QCOMPARE(function->funcRole(),iterator.key());
        QCOMPARE(function->persistentPropertyNames(),QStringList({"mode"}));
        const int propertyIndex=function->metaObject()->indexOfProperty("mode");
        QVERIFY(propertyIndex>=0);
        const QMetaProperty property=function->metaObject()->property(propertyIndex);
        QVERIFY(property.isEnumType());
        const QMetaEnum values=property.enumerator();
        QCOMPARE(values.keyCount(),iterator.value());
        totalModes+=values.keyCount();
        for(int index=0;index<values.keyCount();++index)
        {
            QVERIFY(property.write(function.get(),values.value(index)));
            const QStringList names=function->activeParameterNames();
            QVERIFY2(names.contains("inputImage"),values.key(index));
            QSet<QString> uniqueNames;
            for(const QString &name:names)
            {
                QVERIFY2(!uniqueNames.contains(name),qPrintable(name));
                uniqueNames.insert(name);
                QVERIFY2(function->getParamsByName(name),qPrintable(name));
            }
        }
    }
    QCOMPARE(totalModes,25);

    OMorphology morphology;
    QVERIFY(morphology.activeParameterNames().contains("kernelShape"));
    QVERIFY(!morphology.activeParameterNames().contains("kernelValues"));
    dynamic_cast<XBool*>(morphology.getParamsByName("useKernel"))->setValue(true);
    QVERIFY(!morphology.activeParameterNames().contains("kernelShape"));
    QVERIFY(morphology.activeParameterNames().contains("kernelValues"));
}

void OpenCvMorphologyDetectorTests::disabledBackendPreservesStructuredResults()
{
#if defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is enabled");
#else
    const QStringList roles={"OMorphology","ORegionDetector","OCodeDetector",
                             "OPointFeature","OCascadeDetector"};
    for(const QString &role:roles)
    {
        std::unique_ptr<OpenCvImageOperatorBase> function=createRole(role);
        QVERIFY(inputImage(function.get()));
        QVERIFY(outputImage(function.get()));
        inputImage(function.get())->setValue(featureImage());
        outputImage(function.get())->setValue(seedImage());
        if(auto count=dynamic_cast<XInt*>(function->getResultsByName("count"))) count->setValue(9);
        if(auto text=dynamic_cast<XString*>(function->getResultsByName("text")))
            text->setValue("previous");
        if(auto points=dynamic_cast<XObjectList*>(function->getResultsByName("keyPoints")))
            QVERIFY(points->addValue(new XKeyPoint(1.0,1.0)));
        if(auto corners=dynamic_cast<XObjectList*>(function->getResultsByName("corners")))
            QVERIFY(corners->addValue(new XPoint2D(1.0,1.0)));
        if(auto rectangles=dynamic_cast<XObjectList*>(
                    function->getResultsByName("rectangles")))
            QVERIFY(rectangles->addValue(new XRect2D(1.0,1.0,2.0,2.0)));

        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY2(function->getXvFuncRunMsg().contains("XVISION_ENABLE_OPENCV=ON"),
                 qPrintable(function->getXvFuncRunMsg()));
        QCOMPARE(outputImage(function.get())->value(),seedImage());
        if(auto count=dynamic_cast<XInt*>(function->getResultsByName("count")))
            QCOMPARE(count->value(),9);
        if(auto text=dynamic_cast<XString*>(function->getResultsByName("text")))
            QCOMPARE(text->value(),QString("previous"));
        if(auto points=dynamic_cast<XObjectList*>(function->getResultsByName("keyPoints")))
            QCOMPARE(points->count(),qsizetype(1));
        if(auto corners=dynamic_cast<XObjectList*>(function->getResultsByName("corners")))
            QCOMPARE(corners->count(),qsizetype(1));
        if(auto rectangles=dynamic_cast<XObjectList*>(
                    function->getResultsByName("rectangles")))
            QCOMPARE(rectangles->count(),qsizetype(1));
    }
#endif
}

void OpenCvMorphologyDetectorTests::generatedImagesExerciseAvailableModes()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QImage source=featureImage();

    OMorphology morphology;
    inputImage(&morphology)->setValue(source);
    for(int mode=OMorphology::BlackHat;mode<=OMorphology::TopHat;++mode)
    {
        morphology.setMode(static_cast<OMorphology::Mode>(mode));
        QCOMPARE(morphology.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(outputImage(&morphology)->value().size(),source.size());
    }

    ORegionDetector region;
    inputImage(&region)->setValue(source);
    dynamic_cast<XInt*>(region.getParamsByName("houghThreshold"))->setValue(10);
    dynamic_cast<XReal*>(region.getParamsByName("circleCenterThreshold"))->setValue(10.0);
    dynamic_cast<XInt*>(region.getParamsByName("minRadius"))->setValue(10);
    dynamic_cast<XInt*>(region.getParamsByName("maxRadius"))->setValue(35);
    for(int mode=ORegionDetector::Blob;mode<=ORegionDetector::HoughLinesP;++mode)
    {
        region.setMode(static_cast<ORegionDetector::Mode>(mode));
        QCOMPARE(region.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(outputImage(&region)->value().size(),source.size());
        auto count=dynamic_cast<XInt*>(region.getResultsByName("count"));
        QVERIFY(count && count->value()>=0);
    }

    OCodeDetector code;
    inputImage(&code)->setValue(source);
    QCOMPARE(code.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(dynamic_cast<XString*>(code.getResultsByName("text"))->value(),QString());
    QCOMPARE(dynamic_cast<XObjectList*>(code.getResultsByName("corners"))->count(),qsizetype(0));

    OPointFeature feature;
    inputImage(&feature)->setValue(source);
    for(int mode=OPointFeature::Harris;mode<=OPointFeature::Star;++mode)
    {
        feature.setMode(static_cast<OPointFeature::Mode>(mode));
        QCOMPARE(feature.runXvFunc(),EXvFuncRunStatus::Ok);
        auto count=dynamic_cast<XInt*>(feature.getResultsByName("count"));
        auto points=dynamic_cast<XObjectList*>(feature.getResultsByName("keyPoints"));
        QVERIFY(count && points);
        QCOMPARE(points->count(),qsizetype(count->value()));
        QCOMPARE(outputImage(&feature)->value().size(),source.size());
    }

    OCascadeDetector cascade;
    inputImage(&cascade)->setValue(source);
    QCOMPARE(cascade.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(cascade.getXvFuncRunMsg().contains("absolute readable file"));
#endif
}

void OpenCvMorphologyDetectorTests::registeredQrAssetDecodes()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QString path=qEnvironmentVariable("XVISION_TEST_QR_IMAGE");
    if(path.isEmpty()) QSKIP("XVISION_TEST_QR_IMAGE is not registered");
    const QImage source(path);
    QVERIFY2(!source.isNull(),qPrintable(path));

    OCodeDetector code;
    inputImage(&code)->setValue(source);
    QCOMPARE(code.runXvFunc(),EXvFuncRunStatus::Ok);
    auto text=dynamic_cast<XString*>(code.getResultsByName("text"));
    auto corners=dynamic_cast<XObjectList*>(code.getResultsByName("corners"));
    QVERIFY(text && !text->value().isEmpty());
    QVERIFY(corners && corners->count()>=4);
#endif
}

void OpenCvMorphologyDetectorTests::registeredCascadeAssetLoads()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QString path=qEnvironmentVariable("XVISION_TEST_CASCADE_PATH");
    if(path.isEmpty()) QSKIP("XVISION_TEST_CASCADE_PATH is not registered");

    OCascadeDetector cascade;
    inputImage(&cascade)->setValue(featureImage());
    dynamic_cast<XString*>(cascade.getParamsByName("cascadePath"))->setValue(path);
    for(int mode=OCascadeDetector::Haar;mode<=OCascadeDetector::Lbp;++mode)
    {
        cascade.setMode(static_cast<OCascadeDetector::Mode>(mode));
        QCOMPARE(cascade.runXvFunc(),EXvFuncRunStatus::Ok);
        auto count=dynamic_cast<XInt*>(cascade.getResultsByName("count"));
        auto rectangles=dynamic_cast<XObjectList*>(cascade.getResultsByName("rectangles"));
        QVERIFY(count && rectangles);
        QCOMPARE(rectangles->count(),qsizetype(count->value()));
        QCOMPARE(outputImage(&cascade)->value().size(),featureImage().size());
    }
#endif
}

void OpenCvMorphologyDetectorTests::invalidInputsPreserveExistingResults()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QImage source=featureImage();
    const QImage previous=seedImage();

    OMorphology morphology;
    inputImage(&morphology)->setValue(source);
    outputImage(&morphology)->setValue(previous);
    dynamic_cast<XInt*>(morphology.getParamsByName("kernelWidth"))->setValue(2);
    QCOMPARE(morphology.runXvFunc(),EXvFuncRunStatus::Error);
    QCOMPARE(outputImage(&morphology)->value(),previous);
    dynamic_cast<XInt*>(morphology.getParamsByName("kernelWidth"))->setValue(3);
    dynamic_cast<XReal*>(morphology.getParamsByName("borderValue"))->setValue(qQNaN());
    QCOMPARE(morphology.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(morphology.getXvFuncRunMsg().contains("finite"));
    QCOMPARE(outputImage(&morphology)->value(),previous);

    ORegionDetector region;
    region.setMode(ORegionDetector::RenderBlobs);
    inputImage(&region)->setValue(source);
    outputImage(&region)->setValue(previous);
    auto regions=dynamic_cast<XObjectList*>(region.getResultsByName("regions"));
    QVERIFY(regions->addValue(new XRegion(seedImage())));
    dynamic_cast<XInt*>(region.getResultsByName("count"))->setValue(1);
    dynamic_cast<XInt*>(region.getParamsByName("connectivity"))->setValue(6);
    QCOMPARE(region.runXvFunc(),EXvFuncRunStatus::Error);
    QCOMPARE(outputImage(&region)->value(),previous);
    QCOMPARE(regions->count(),qsizetype(1));
    QCOMPARE(dynamic_cast<XInt*>(region.getResultsByName("count"))->value(),1);

    OPointFeature feature;
    feature.setMode(OPointFeature::Fast);
    inputImage(&feature)->setValue(source);
    outputImage(&feature)->setValue(previous);
    auto points=dynamic_cast<XObjectList*>(feature.getResultsByName("keyPoints"));
    QVERIFY(points->addValue(new XKeyPoint(1.0,1.0)));
    dynamic_cast<XInt*>(feature.getResultsByName("count"))->setValue(1);
    dynamic_cast<XInt*>(feature.getParamsByName("fastThreshold"))->setValue(-1);
    QCOMPARE(feature.runXvFunc(),EXvFuncRunStatus::Error);
    QCOMPARE(outputImage(&feature)->value(),previous);
    QCOMPARE(points->count(),qsizetype(1));

    OCascadeDetector cascade;
    inputImage(&cascade)->setValue(source);
    outputImage(&cascade)->setValue(previous);
    auto rectangles=dynamic_cast<XObjectList*>(cascade.getResultsByName("rectangles"));
    QVERIFY(rectangles->addValue(new XRect2D(1.0,1.0,2.0,2.0)));
    dynamic_cast<XInt*>(cascade.getResultsByName("count"))->setValue(1);
    QCOMPARE(cascade.runXvFunc(),EXvFuncRunStatus::Error);
    QCOMPARE(outputImage(&cascade)->value(),previous);
    QCOMPARE(rectangles->count(),qsizetype(1));
    QCOMPARE(dynamic_cast<XInt*>(cascade.getResultsByName("count"))->value(),1);
#endif
}

QTEST_MAIN(OpenCvMorphologyDetectorTests)
#include "tst_opencvmorphologydetectors.moc"
