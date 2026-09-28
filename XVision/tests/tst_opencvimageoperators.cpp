#include <QtTest>

#include <QMetaEnum>
#include <QMetaProperty>
#include <QSet>

#include <memory>

#include "OImageAnalysis.h"
#include "OImageArithmetic.h"
#include "OImageColor.h"
#include "OImageComposition.h"
#include "OImageFilter.h"
#include "OImageModel.h"
#include "OImageThreshold.h"
#include "OImageTransform.h"
#include "OpenCvImageOperatorBase.h"
#include "XImage.h"
#include "XInt.h"
#include "XReal.h"
#include "XVisionRuntimeData.h"

using namespace XvCore;

namespace
{
std::unique_ptr<OpenCvImageOperatorBase> createRole(const QString &role)
{
    if(role=="OImageArithmetic") return std::make_unique<OImageArithmetic>();
    if(role=="OImageFilter") return std::make_unique<OImageFilter>();
    if(role=="OImageColor") return std::make_unique<OImageColor>();
    if(role=="OImageThreshold") return std::make_unique<OImageThreshold>();
    if(role=="OImageTransform") return std::make_unique<OImageTransform>();
    if(role=="OImageAnalysis") return std::make_unique<OImageAnalysis>();
    if(role=="OImageModel") return std::make_unique<OImageModel>();
    if(role=="OImageComposition") return std::make_unique<OImageComposition>();
    return nullptr;
}

XImage *imageParameter(XvFunc *function,const QString &name="inputImage")
{
    return dynamic_cast<XImage*>(function->getParamsByName(name));
}

XImage *imageResult(XvFunc *function)
{
    return dynamic_cast<XImage*>(function->getResultsByName("outputImage"));
}

QImage grayImage(int width,int height,int value)
{
    QImage image(width,height,QImage::Format_Grayscale8);
    image.fill(value);
    return image;
}
}

class OpenCvImageOperatorTests:public QObject
{
    Q_OBJECT
private slots:
    void allCanonicalModesHaveCompleteContracts();
    void disabledBackendPreservesExistingResults();
    void statelessPixelAndStructureResults();
    void invalidInputsPreserveExistingResults();
};

void OpenCvImageOperatorTests::allCanonicalModesHaveCompleteContracts()
{
    const QMap<QString,int> roleModeCounts={
        {"OImageArithmetic",4},
        {"OImageFilter",6},
        {"OImageColor",4},
        {"OImageThreshold",2},
        {"OImageTransform",8},
        {"OImageAnalysis",4},
        {"OImageModel",3},
        {"OImageComposition",3}
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
    QCOMPARE(totalModes,34);
}

void OpenCvImageOperatorTests::disabledBackendPreservesExistingResults()
{
#if defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is enabled");
#else
    const QStringList roles={
        "OImageArithmetic","OImageFilter","OImageColor","OImageThreshold",
        "OImageTransform","OImageAnalysis","OImageModel","OImageComposition"
    };
    const QImage previous=grayImage(1,1,37);
    for(const QString &role:roles)
    {
        std::unique_ptr<OpenCvImageOperatorBase> function=createRole(role);
        QVERIFY(imageParameter(function.get()));
        QVERIFY(imageResult(function.get()));
        imageParameter(function.get())->setValue(grayImage(2,2,10));
        imageResult(function.get())->setValue(previous);
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY2(function->getXvFuncRunMsg().contains("XVISION_ENABLE_OPENCV=ON"),
                 qPrintable(function->getXvFuncRunMsg()));
        QCOMPARE(imageResult(function.get())->value(),previous);
    }
#endif
}

void OpenCvImageOperatorTests::statelessPixelAndStructureResults()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    OImageArithmetic arithmetic;
    arithmetic.setMode(OImageArithmetic::BitwiseNot);
    imageParameter(&arithmetic)->setValue(grayImage(1,1,10));
    QCOMPARE(arithmetic.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(qGray(imageResult(&arithmetic)->value().pixel(0,0)),245);

    OImageFilter filter;
    filter.setMode(OImageFilter::BoxBlur);
    QImage impulse=grayImage(3,3,0);
    impulse.setPixelColor(1,1,QColor(255,255,255));
    imageParameter(&filter)->setValue(impulse);
    QCOMPARE(filter.runXvFunc(),EXvFuncRunStatus::Ok);
    const int blurred=qGray(imageResult(&filter)->value().pixel(1,1));
    QVERIFY(blurred>0 && blurred<255);

    OImageColor color;
    color.setMode(OImageColor::Convert);
    QImage rgb(2,1,QImage::Format_RGB32);
    rgb.fill(QColor(20,40,60));
    imageParameter(&color)->setValue(rgb);
    QCOMPARE(color.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(imageResult(&color)->value().format(),QImage::Format_Grayscale8);

    OImageThreshold threshold;
    threshold.setMode(OImageThreshold::Threshold);
    imageParameter(&threshold)->setValue(grayImage(2,1,200));
    QCOMPARE(threshold.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(qGray(imageResult(&threshold)->value().pixel(0,0)),255);

    OImageTransform transform;
    transform.setMode(OImageTransform::Transpose);
    imageParameter(&transform)->setValue(grayImage(2,3,90));
    QCOMPARE(transform.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(imageResult(&transform)->value().size(),QSize(3,2));

    OImageAnalysis analysis;
    analysis.setMode(OImageAnalysis::Histogram);
    imageParameter(&analysis)->setValue(grayImage(8,8,64));
    QCOMPARE(analysis.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(imageResult(&analysis)->value().size(),QSize(256,128));
    auto descriptor=dynamic_cast<XTensor*>(analysis.getResultsByName("descriptor"));
    QVERIFY(descriptor);
    QCOMPARE(descriptor->dimensions(),QVector<qint64>({256}));

    OImageModel model;
    model.setMode(OImageModel::BackgroundSubtraction);
    imageParameter(&model)->setValue(grayImage(8,8,20));
    QCOMPARE(model.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(imageResult(&model)->value().size(),QSize(8,8));

    OImageComposition composition;
    composition.setMode(OImageComposition::Background);
    const QImage foreground=grayImage(3,2,220);
    const QImage background=grayImage(3,2,10);
    const QImage mask=grayImage(3,2,255);
    imageParameter(&composition)->setValue(foreground);
    imageParameter(&composition,"backgroundImage")->setValue(background);
    imageParameter(&composition,"maskImage")->setValue(mask);
    QCOMPARE(composition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(qGray(imageResult(&composition)->value().pixel(1,1)),220);
#endif
}

void OpenCvImageOperatorTests::invalidInputsPreserveExistingResults()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QImage previous=grayImage(1,1,73);

    OImageFilter filter;
    filter.setMode(OImageFilter::BoxBlur);
    imageParameter(&filter)->setValue(grayImage(3,3,10));
    imageResult(&filter)->setValue(previous);
    dynamic_cast<XInt*>(filter.getParamsByName("kernelWidth"))->setValue(2);
    QCOMPARE(filter.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(!filter.getXvFuncRunMsg().isEmpty());
    QCOMPARE(imageResult(&filter)->value(),previous);

    OImageTransform transform;
    transform.setMode(OImageTransform::WarpAffine);
    imageParameter(&transform)->setValue(grayImage(3,3,10));
    imageResult(&transform)->setValue(previous);
    dynamic_cast<XReal*>(transform.getParamsByName("sourceX1"))->setValue(0.0);
    dynamic_cast<XReal*>(transform.getParamsByName("sourceY1"))->setValue(0.0);
    dynamic_cast<XReal*>(transform.getParamsByName("sourceX2"))->setValue(0.0);
    dynamic_cast<XReal*>(transform.getParamsByName("sourceY2"))->setValue(0.0);
    QCOMPARE(transform.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(transform.getXvFuncRunMsg().contains("degenerate"));
    QCOMPARE(imageResult(&transform)->value(),previous);

    OImageModel model;
    model.setMode(OImageModel::SuperResolution);
    imageParameter(&model)->setValue(grayImage(2,2,10));
    imageResult(&model)->setValue(previous);
    QCOMPARE(model.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(model.getXvFuncRunMsg().contains("absolute readable file"));
    QCOMPARE(imageResult(&model)->value(),previous);

    OImageComposition composition;
    composition.setMode(OImageComposition::Stitching);
    imageParameter(&composition)->setValue(grayImage(2,2,10));
    imageResult(&composition)->setValue(previous);
    QCOMPARE(composition.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(composition.getXvFuncRunMsg().contains("at least two"));
    QCOMPARE(imageResult(&composition)->value(),previous);
#endif
}

QTEST_MAIN(OpenCvImageOperatorTests)
#include "tst_opencvimageoperators.moc"
