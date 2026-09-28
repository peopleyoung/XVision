#include <QtTest>

#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QSet>

#include <array>
#include <cmath>
#include <cstring>

#include "OpenCvTemplateRectificationUtils.h"
#include "ORectification.h"
#include "OTemplateMatch.h"
#include "XImage.h"
#include "XInt.h"
#include "XMatchResult.h"
#include "XObjectList.h"
#include "XReal.h"
#include "XRotateRectRoi.h"
#include "XVisionRuntimeData.h"
#include "XVisionSharedData.h"

using namespace XvCore;

namespace
{
QImage asymmetricTemplate(int width=48,int height=36)
{
    QImage image(width,height,QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing,false);
    painter.setPen(QPen(Qt::white,3));
    painter.drawRect(3,3,width-8,height-8);
    painter.drawLine(5,height-7,width-8,7);
    painter.setBrush(Qt::red);
    painter.drawEllipse(QPoint(width*3/4,height*2/3),4,4);
    return image;
}

QImage sceneWithTemplate(const QImage &pattern,const QPoint &topLeft,
                         const QSize &size=QSize(180,130))
{
    QImage image(size,QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.drawImage(topLeft,pattern);
    return image;
}

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

void configureFind(OTemplateMatch &matcher,const QImage &pattern,
                   const QImage &scene,double minimumScore=0.7)
{
    parameter<XImage>(matcher,"inputImage")->setValue(pattern);
    parameter<XInt>(matcher,"operation")->setValue(OTemplateMatch::CreateTemplate);
    QCOMPARE(matcher.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(matcher.hasTemplate());
    QCOMPARE(matcher.templateSize(),pattern.size());
    parameter<XImage>(matcher,"inputImage")->setValue(scene);
    parameter<XInt>(matcher,"operation")->setValue(OTemplateMatch::FindTemplate);
    parameter<XReal>(matcher,"minScore")->setValue(minimumScore);
    parameter<XInt>(matcher,"maxMatches")->setValue(1);
}
}

class OpenCvTemplatingTests:public QObject
{
    Q_OBJECT
private slots:
    void pngGeometryAndTransformContracts();
    void canonicalRolesExposeCompleteModes();
    void disabledBackendClearsTransientResults();
    void correlationShapeAndHsvMatchFixedImages();
    void featureMatchingFindsTranslatedTemplate();
    void rectificationModesPublishStructuredResults();
};

void OpenCvTemplatingTests::pngGeometryAndTransformContracts()
{
    const QImage source=asymmetricTemplate();
    QByteArray asset,digest;
    QSize size;
    QString error;
    QVERIFY2(OpenCvTemplateRectificationUtils::encodeTemplatePng(
                 source,asset,size,digest,error),qPrintable(error));
    QCOMPARE(size,source.size());
    QCOMPARE(digest.size(),64);
    QVERIFY(asset.size()<=OpenCvTemplateRectificationUtils::MaxTemplateAssetBytes);

    QImage decoded;
    QSize decodedSize;
    QByteArray decodedDigest;
    QVERIFY2(OpenCvTemplateRectificationUtils::decodeTemplatePng(
                 asset,decoded,decodedSize,decodedDigest,error),qPrintable(error));
    QCOMPARE(decodedSize,size);
    QCOMPARE(decodedDigest,digest);
    QCOMPARE(decoded.convertToFormat(QImage::Format_ARGB32),
             source.convertToFormat(QImage::Format_ARGB32));

    const QImage previous=decoded;
    const QSize previousSize=decodedSize;
    const QByteArray previousDigest=decodedDigest;
    QByteArray damaged=asset;
    damaged[0]='X';
    QVERIFY(!OpenCvTemplateRectificationUtils::decodeTemplatePng(
                damaged,decoded,decodedSize,decodedDigest,error));
    QCOMPARE(decoded,previous);
    QCOMPARE(decodedSize,previousSize);
    QCOMPARE(decodedDigest,previousDigest);

    XRotateRectRoi roi(10.0,20.0,4.0,2.0,1.5707963267948966);
    QVector<QPointF> corners;
    QVERIFY2(OpenCvTemplateRectificationUtils::roiCorners(roi,corners,error),
             qPrintable(error));
    QCOMPARE(corners.count(),4);
    QVERIFY(qAbs(corners[0].x()-8.0)<1e-9);
    QVERIFY(qAbs(corners[0].y()-24.0)<1e-9);
    QVERIFY(qAbs(corners[1].x()-8.0)<1e-9);
    QVERIFY(qAbs(corners[1].y()-16.0)<1e-9);
    QSize outputSize;
    QVERIFY(OpenCvTemplateRectificationUtils::roiOutputSize(
                roi,0,0,outputSize,error));
    QCOMPARE(outputSize,QSize(8,4));
    QVERIFY(!OpenCvTemplateRectificationUtils::roiOutputSize(
                roi,12,0,outputSize,error));

    const std::array<double,9> matrix={1.0,0.0,3.0,0.0,1.0,4.0,0.0,0.0,1.0};
    XTensor tensor;
    QVERIFY(OpenCvTemplateRectificationUtils::setTransformTensor(matrix,tensor,error));
    QCOMPARE(tensor.elementType(),QString("float64"));
    QCOMPARE(tensor.dimensions(),QVector<qint64>({3,3}));
    QCOMPARE(tensor.bytes().size(),72);
    std::array<double,9> restored{};
    std::memcpy(restored.data(),tensor.bytes().constData(),size_t(tensor.bytes().size()));
    QVERIFY(restored==matrix);
    std::array<double,9> invalid=matrix;
    invalid[4]=qQNaN();
    const QByteArray priorBytes=tensor.bytes();
    QVERIFY(!OpenCvTemplateRectificationUtils::setTransformTensor(invalid,tensor,error));
    QCOMPARE(tensor.bytes(),priorBytes);

    OTemplateMatch matcher;
    QVERIFY2(matcher.configureTemplateImage(source,error),qPrintable(error));
    const QByteArray acceptedAsset=matcher.templateAsset();
    QVERIFY(!matcher.configureTemplateImage(QImage(),error));
    QCOMPARE(matcher.templateAsset(),acceptedAsset);
    QImage oversizedDimension(
                OpenCvTemplateRectificationUtils::MaxTemplateDimension+1,1,
                QImage::Format_Grayscale8);
    QVERIFY(!oversizedDimension.isNull());
    QVERIFY(!matcher.configureTemplateImage(oversizedDimension,error));
    QCOMPARE(matcher.templateAsset(),acceptedAsset);
}

void OpenCvTemplatingTests::canonicalRolesExposeCompleteModes()
{
    OTemplateMatch matcher;
    ORectification rectification;
    const QList<QPair<OpenCvImageOperatorBase*,int>> roles={{&matcher,4},
                                                            {&rectification,3}};
    for(const auto &entry:roles)
    {
        QCOMPARE(entry.first->persistentPropertyNames(),QStringList({"mode"}));
        const int propertyIndex=entry.first->metaObject()->indexOfProperty("mode");
        QVERIFY(propertyIndex>=0);
        const QMetaProperty property=entry.first->metaObject()->property(propertyIndex);
        QVERIFY(property.isEnumType());
        QCOMPARE(property.enumerator().keyCount(),entry.second);
        for(int index=0;index<property.enumerator().keyCount();++index)
        {
            QVERIFY(property.write(entry.first,property.enumerator().value(index)));
            const QStringList names=entry.first->activeParameterNames();
            QVERIFY(names.contains("inputImage"));
            QSet<QString> unique;
            for(const QString &name:names)
            {
                QVERIFY2(!unique.contains(name),qPrintable(name));
                unique.insert(name);
                QVERIFY2(entry.first->getParamsByName(name),qPrintable(name));
            }
        }
    }
    QCOMPARE(matcher.funcRole(),QString("OTemplateMatch"));
    QCOMPARE(rectification.funcRole(),QString("ORectification"));
    QCOMPARE(result<XObjectList>(matcher,"matches")->valueType(),XMatchResult::type());
    QVERIFY(result<XRegion>(rectification,"foregroundRegion"));
    QVERIFY(result<XTensor>(rectification,"transform"));
}

void OpenCvTemplatingTests::disabledBackendClearsTransientResults()
{
#if defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is enabled");
#else
    OTemplateMatch matcher;
    QString error;
    QVERIFY(matcher.configureTemplateImage(asymmetricTemplate(),error));
    parameter<XImage>(matcher,"inputImage")->setValue(asymmetricTemplate());
    parameter<XInt>(matcher,"operation")->setValue(OTemplateMatch::FindTemplate);
    result<XImage>(matcher,"outputImage")->setValue(asymmetricTemplate());
    result<XObjectList>(matcher,"matches")->addValue(
                new XMatchResult(10.0,10.0,0.0,0.9));
    result<XRotateRectRoi>(matcher,"templateRoi")->setValue(
                10.0,10.0,8.0,6.0,0.2);
    QCOMPARE(matcher.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(matcher.getXvFuncRunMsg().contains("XVISION_ENABLE_OPENCV=ON"));
    QVERIFY(result<XImage>(matcher,"outputImage")->value().isNull());
    QCOMPARE(result<XObjectList>(matcher,"matches")->count(),qsizetype(0));
    QCOMPARE(result<XRotateRectRoi>(matcher,"templateRoi")->length1(),1.0);
    QCOMPARE(result<XRotateRectRoi>(matcher,"templateRoi")->length2(),1.0);
    QVERIFY(matcher.hasTemplate());

    ORectification rectification;
    parameter<XImage>(rectification,"inputImage")->setValue(asymmetricTemplate());
    result<XImage>(rectification,"outputImage")->setValue(asymmetricTemplate());
    QCOMPARE(rectification.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(rectification.getXvFuncRunMsg().contains("XVISION_ENABLE_OPENCV=ON"));
    QVERIFY(result<XImage>(rectification,"outputImage")->value().isNull());
    QVERIFY(result<XImage>(rectification,"foregroundMask")->value().isNull());
#endif
}

void OpenCvTemplatingTests::correlationShapeAndHsvMatchFixedImages()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    const QImage pattern=asymmetricTemplate();
    const QPoint position(57,42);
    const QImage scene=sceneWithTemplate(pattern,position);

    OTemplateMatch roiMatcher;
    parameter<XImage>(roiMatcher,"inputImage")->setValue(scene);
    parameter<XBool>(roiMatcher,"useTemplateRoi")->setValue(true);
    parameter<XRotateRectRoi>(roiMatcher,"templateRoi")->setValue(
                position.x()+(pattern.width()-1)*0.5,
                position.y()+(pattern.height()-1)*0.5,
                pattern.width()*0.5,pattern.height()*0.5,0.0);
    QCOMPARE(roiMatcher.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(roiMatcher.templateSize(),pattern.size());

    for(OTemplateMatch::Mode mode:{OTemplateMatch::Base64,
                                   OTemplateMatch::Shape,
                                   OTemplateMatch::Hsv})
    {
        OTemplateMatch matcher;
        matcher.setMode(mode);
        configureFind(matcher,pattern,scene,mode==OTemplateMatch::Shape?0.5:0.7);
        if(mode==OTemplateMatch::Shape)
        {
            parameter<XReal>(matcher,"angleStart")->setValue(0.0);
            parameter<XReal>(matcher,"angleExtent")->setValue(0.0);
            parameter<XReal>(matcher,"angleStep")->setValue(0.05);
        }
        QCOMPARE(matcher.runXvFunc(),EXvFuncRunStatus::Ok);
        auto matches=result<XObjectList>(matcher,"matches");
        QCOMPARE(matches->count(),qsizetype(1));
        auto match=dynamic_cast<XMatchResult*>(matches->value(0));
        QVERIFY(match);
        QVERIFY(qAbs(match->x()-(position.x()+pattern.width()*0.5))<2.0);
        QVERIFY(qAbs(match->y()-(position.y()+pattern.height()*0.5))<2.0);
        QVERIFY(match->score()>=parameter<XReal>(matcher,"minScore")->value());
        if(mode==OTemplateMatch::Base64)
        {
            parameter<XReal>(matcher,"minScore")->setValue(2.0);
            QCOMPARE(matcher.runXvFunc(),EXvFuncRunStatus::Error);
            QVERIFY(result<XImage>(matcher,"outputImage")->value().isNull());
            QCOMPARE(result<XObjectList>(matcher,"matches")->count(),qsizetype(0));
            QVERIFY(matcher.hasTemplate());
        }
    }
#endif
}

void OpenCvTemplatingTests::featureMatchingFindsTranslatedTemplate()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    QImage pattern(100,100,QImage::Format_RGB32);
    pattern.fill(Qt::black);
    QPainter painter(&pattern);
    painter.setRenderHint(QPainter::Antialiasing,false);
    for(int row=0;row<10;++row)
        for(int column=0;column<10;++column)
        {
            const QColor color((row*47+column*13)%255,
                               (row*17+column*61)%255,
                               (row*83+column*29)%255);
            painter.fillRect(column*10,row*10,8,8,color);
        }
    painter.setPen(QPen(Qt::white,2));
    painter.drawLine(2,80,90,7);
    painter.drawEllipse(QPoint(66,67),15,9);
    painter.end();

    const QPoint position(63,38);
    OTemplateMatch matcher;
    matcher.setMode(OTemplateMatch::Feature);
    configureFind(matcher,pattern,sceneWithTemplate(pattern,position,QSize(240,190)),0.2);
    parameter<XInt>(matcher,"minimumFeatureMatches")->setValue(4);
    parameter<XInt>(matcher,"minimumInliers")->setValue(4);
    QCOMPARE(matcher.runXvFunc(),EXvFuncRunStatus::Ok);
    auto matches=result<XObjectList>(matcher,"matches");
    QCOMPARE(matches->count(),qsizetype(1));
    auto match=dynamic_cast<XMatchResult*>(matches->value(0));
    QVERIFY(match);
    QVERIFY(qAbs(match->x()-(position.x()+pattern.width()*0.5))<4.0);
    QVERIFY(qAbs(match->y()-(position.y()+pattern.height()*0.5))<4.0);
#endif
}

void OpenCvTemplatingTests::rectificationModesPublishStructuredResults()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    QImage scene(160,120,QImage::Format_RGB32);
    scene.fill(Qt::black);
    QPainter painter(&scene);
    painter.fillRect(QRect(45,30,70,50),Qt::white);
    painter.setPen(QPen(Qt::red,3));
    painter.drawLine(48,33,108,75);
    painter.end();

    ORectification supplied;
    supplied.setMode(ORectification::RotatedRect);
    parameter<XImage>(supplied,"inputImage")->setValue(scene);
    parameter<XRotateRectRoi>(supplied,"rotatedRect")->setValue(
                80.0,55.0,35.0,25.0,0.0);
    parameter<XInt>(supplied,"outputWidth")->setValue(70);
    parameter<XInt>(supplied,"outputHeight")->setValue(50);
    QCOMPARE(supplied.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XImage>(supplied,"outputImage")->value().size(),QSize(70,50));
    QCOMPARE(result<XTensor>(supplied,"transform")->dimensions(),
             QVector<qint64>({3,3}));
    std::array<double,9> suppliedTransform{};
    std::memcpy(suppliedTransform.data(),
                result<XTensor>(supplied,"transform")->bytes().constData(),
                sizeof(suppliedTransform));
    for(double value:suppliedTransform) QVERIFY(qIsFinite(value));

    ORectification extracted;
    extracted.setMode(ORectification::ForegroundExtract);
    parameter<XImage>(extracted,"inputImage")->setValue(scene);
    parameter<XInt>(extracted,"foregroundThreshold")->setValue(100);
    QCOMPARE(extracted.runXvFunc(),EXvFuncRunStatus::Ok);
    const QImage mask=result<XImage>(extracted,"foregroundMask")->value();
    QCOMPARE(mask.size(),scene.size());
    QCOMPARE(mask.format(),QImage::Format_Grayscale8);
    QCOMPARE(mask.pixelColor(80,55).red(),255);
    QCOMPARE(mask.pixelColor(10,10).red(),0);
    QVERIFY(!result<XRegion>(extracted,"foregroundRegion")->mask().isNull());
    std::array<double,9> extractTransform{};
    std::memcpy(extractTransform.data(),
                result<XTensor>(extracted,"transform")->bytes().constData(),
                sizeof(extractTransform));
    const std::array<double,9> identity={1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0};
    QVERIFY(extractTransform==identity);

    ORectification detected;
    detected.setMode(ORectification::ForegroundRotatedRect);
    parameter<XImage>(detected,"inputImage")->setValue(scene);
    parameter<XInt>(detected,"foregroundThreshold")->setValue(100);
    QCOMPARE(detected.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(!result<XImage>(detected,"outputImage")->value().isNull());
    QVERIFY(result<XRotateRectRoi>(detected,"rectifiedRoi")->length1()>20.0);
    QVERIFY(result<XRotateRectRoi>(detected,"rectifiedRoi")->length2()>20.0);
    QImage emptyScene(scene.size(),QImage::Format_RGB32);
    emptyScene.fill(Qt::black);
    parameter<XImage>(detected,"inputImage")->setValue(emptyScene);
    QCOMPARE(detected.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(result<XImage>(detected,"outputImage")->value().isNull());
    QVERIFY(result<XImage>(detected,"foregroundMask")->value().isNull());
    QVERIFY(result<XRegion>(detected,"foregroundRegion")->mask().isNull());
    QCOMPARE(result<XRotateRectRoi>(detected,"rectifiedRoi")->length1(),1.0);
#endif
}

QTEST_MAIN(OpenCvTemplatingTests)
#include "tst_opencvtemplating.moc"
