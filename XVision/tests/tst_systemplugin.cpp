#include <QtTest>

#include <QGuiApplication>
#include <QFile>
#include <QMap>
#include <QPluginLoader>
#include <QSet>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <memory>

#include "IXvFactoryPlugin.h"
#include "XvFunc.h"
#include "XvFuncAssembly.h"

using namespace XvCore;

namespace
{
QJsonObject configurationNode(const QDomElement &element)
{
    QJsonObject attributes;
    const auto values=element.attributes();
    for(int i=0;i<values.count();++i)
    {
        const auto attribute=values.item(i).toAttr();
        if(element.tagName()=="Function"
                && QStringList{"id","name","x","y"}.contains(attribute.name())) continue;
        attributes.insert(attribute.name(),attribute.value());
    }
    QJsonArray children;
    for(auto child=element.firstChildElement();!child.isNull();child=child.nextSiblingElement())
        children.append(configurationNode(child));
    return {{"tag",element.tagName()},{"attributes",attributes},
            {"text",element.firstChildElement().isNull()?element.text():QString()},
            {"children",children}};
}
}

class SystemPluginTest : public QObject
{
    Q_OBJECT

private slots:
    void pluginLoadsAndExposesCurrentRoles()
    {
        const QStringList arguments=QCoreApplication::arguments();
        QVERIFY(arguments.count()>=3);

        QPluginLoader loader(arguments.at(1));
        QObject *instance=loader.instance();
        QVERIFY2(instance,qPrintable(loader.errorString()));

        auto plugin=qobject_cast<IXvFactoryPlugin*>(instance);
        QVERIFY2(plugin,"The system plugin does not implement IXvFactoryPlugin");
        QCOMPARE(plugin->name(),QString("SystemXvFactoryPlugin"));
        QVERIFY(plugin->init());

        const QStringList expectedRoles={
            "BaseDataBoolCalc",
            "BaseDataIntCalc",
            "BaseDataRealCalc",
            "BaseDataStringProcess",
            "BaseDataWriter",
            "ConditionalFlow",
            "Delayer",
            "DetectRecord",
            "ElapsedTimer",
            "GeometryCreate",
            "GeometryMeasure",
            "HttpJson",
            "ImageAcquisition",
            "LogOutput",
            "LoopFlow",
            "ModbusRegister",
            "NClassification",
            "NInference",
            "NObjectDetection",
            "NSemanticSegmentation",
            "NotificationOutput",
            "OCascadeDetector",
            "OCodeDetector",
            "OImageAnalysis",
            "OImageArithmetic",
            "OImageColor",
            "OImageComposition",
            "OImageFilter",
            "OImageModel",
            "OImageThreshold",
            "OImageTransform",
            "OMorphology",
            "OPointFeature",
            "ORectification",
            "ORegionDetector",
            "OTemplateMatch",
            "SerialData",
            "TcpText",
            "UdpText"
        };
        const QList<QMetaObject> metaObjects=plugin->getPlgXvFunc();
        QCOMPARE(metaObjects.count(),expectedRoles.count());

        const QMap<QString,EXvFuncType> expectedCategories={
            {"HttpJson",EXvFuncType::Communication},
            {"TcpText",EXvFuncType::Communication},
            {"UdpText",EXvFuncType::Communication},
            {"SerialData",EXvFuncType::Communication},
            {"ModbusRegister",EXvFuncType::Communication},
            {"NInference",EXvFuncType::MachineLearning},
            {"NClassification",EXvFuncType::MachineLearning},
            {"NObjectDetection",EXvFuncType::MachineLearning},
            {"NSemanticSegmentation",EXvFuncType::MachineLearning},
            {"OCodeDetector",EXvFuncType::Recognition},
            {"OCascadeDetector",EXvFuncType::DefectDetection},
            {"ORegionDetector",EXvFuncType::DefectDetection},
            {"OPointFeature",EXvFuncType::Location},
            {"ORectification",EXvFuncType::Calibration}
        };
        QSet<QString> roles;
        QStringList missingIcons;
        for(const QMetaObject &metaObject:metaObjects)
        {
            QVERIFY2(metaObject.inherits(&XvFunc::staticMetaObject),
                     metaObject.className());
            QObject *parent=nullptr;
            std::unique_ptr<QObject> object(
                        metaObject.newInstance(Q_ARG(QObject*,parent)));
            QVERIFY2(object,metaObject.className());
            auto function=qobject_cast<XvFunc*>(object.get());
            QVERIFY2(function,metaObject.className());
            QVERIFY2(!function->funcRole().isEmpty(),metaObject.className());
            QVERIFY2(!roles.contains(function->funcRole()),
                     qPrintable(function->funcRole()));
            roles.insert(function->funcRole());
            if(function->funcIcon().isNull()) missingIcons.append(function->funcRole());
            if(expectedCategories.contains(function->funcRole()))
                QCOMPARE(function->funcType(),expectedCategories.value(function->funcRole()));
        }

        QVERIFY2(missingIcons.isEmpty(),qPrintable(missingIcons.join(", ")));
        QStringList actualRoles=roles.values();
        actualRoles.sort();
        QCOMPARE(actualRoles,expectedRoles);
        for(const QString &removedRole:QStringList({"HModelMatch","HObjectDetection",
                                                    "HSemanticSegmentation"}))
            QVERIFY2(!roles.contains(removedRole),qPrintable(removedRole));

        const QList<XvFuncPreset> presets=plugin->getPlgXvFuncPresets();
        QCOMPARE(presets.count(),84);
        const QRegularExpression chinese(QStringLiteral("[\\x{4e00}-\\x{9fff}]"));
        QSet<QString> aliases;
        for(const auto &preset:presets)
        {
            QVERIFY(preset.isValid());
            QVERIFY(!aliases.contains(preset.alias));
            QVERIFY(preset.alias.startsWith(preset.canonicalRole+"."));
            QVERIFY(chinese.match(preset.displayName).hasMatch());
            QVERIFY(preset.parameters.isEmpty());
            QCOMPARE(preset.properties.size(),1);
            aliases.insert(preset.alias);
        }
        QFile catalogFile(arguments.at(2));
        QVERIFY2(catalogFile.open(QIODevice::ReadOnly|QIODevice::Text),qPrintable(catalogFile.errorString()));
        QCOMPARE(catalogFile.readLine().trimmed(),QByteArray("entry_id,canonical_role,property,value,display_name,category"));
        QMap<QString,QStringList> catalog;
        while(!catalogFile.atEnd())
        {
            const QString line=QString::fromUtf8(catalogFile.readLine()).trimmed();
            QVERIFY(!line.isEmpty());
            const QStringList fields=line.split(',');
            QCOMPARE(fields.size(),6);
            QVERIFY(!catalog.contains(fields.first()));
            catalog.insert(fields.first(),fields);
        }
        QCOMPARE(catalog.size(),123);

        XvFuncAssembly *assembly=XvFuncAssembly::getInstance();
        QString registrationError;
        QVERIFY2(assembly->registerPlugin(metaObjects,presets,&registrationError),
                 qPrintable(registrationError));
        QCOMPARE(assembly->getXvFuncPresets().count(),presets.count());
        QCOMPARE(assembly->getXvFuncInfos().count(),expectedRoles.count()+presets.count());
        QSet<QByteArray> configurations;
        QSet<QString> registeredIds;
        for(const auto &info:assembly->getXvFuncInfos())
        {
            QVERIFY2(catalog.contains(info.role),qPrintable(info.role));
            registeredIds.insert(info.role);
            const auto expected=catalog.value(info.role);
            QCOMPARE(info.canonicalRole,expected.at(1));
            QCOMPARE(info.name,expected.at(4));
            std::unique_ptr<XvFunc> created(assembly->createNewXvFunc(info.role));
            QVERIFY2(created,qPrintable(assembly->lastErrorMsg()));
            QCOMPARE(created->funcRole(),expected.at(1));
            if(expected.at(2)!="-")
                QCOMPARE(created->property(expected.at(2).toUtf8().constData()).toInt(),expected.at(3).toInt());
            QDomDocument document;
            const auto element=created->toXmlElement(document);
            QVERIFY2(!element.isNull(),qPrintable(created->lastErrorMsg()));
            const QByteArray configuration=QJsonDocument(configurationNode(element)).toJson(QJsonDocument::Compact);
            QVERIFY2(!configurations.contains(configuration),qPrintable("Duplicate initial configuration: "+info.role));
            configurations.insert(configuration);
        }
        QCOMPARE(configurations.count(),123);
        QCOMPARE(registeredIds.count(),catalog.size());
        const QStringList retiredIds={
            "AKazeFeatureDetector",
            "AddSutract",
            "AgeInferOnnxNodeData",
            "Base64TemplateMatchNodeData",
            "BitwiseNot",
            "BlackHat",
            "BlobDetector",
            "Blur",
            "BriskFeatureDetector",
            "CameraCaptureNodeData",
            "CameraNodeData",
            "Canny",
            "CircleToCircleMesauseNodeData",
            "ClassDetectRecordNodeData",
            "Close",
            "ClsOnnxNodeData",
            "CornerHarris",
            "CornerSubPix",
            "CreateShapeNodeData",
            "CvtColor",
            "DetailEnhance",
            "DetectRecordNodeData",
            "Dilate",
            "DnnSuperres",
            "EdgePreservingFilter",
            "Erode",
            "FastFeatureDetector",
            "FeaturePointTemplateMatch",
            "FindContours",
            "Flip",
            "ForNodeData",
            "ForeachSplitResultImageNodeData",
            "ForegroundRotatedRectRectification",
            "FreakFeatureDetector",
            "GaussianBlur",
            "GenderClsOnnxNodeData",
            "Gradient",
            "HSVInRange",
            "HSVTemplateMatch",
            "HaarCascade",
            "HasDetectRecordNodeData",
            "Hist",
            "Hog",
            "HomographyTransform",
            "HoughCircles",
            "HoughLines",
            "HoughLinesP",
            "HttpReadJsonNodeData",
            "HttpWriteJsonNodeData",
            "HumanSemSegOnnxNodeData",
            "InferOnnxNodeData",
            "IntReadableModbusNodeData",
            "KazeFeatureDetector",
            "LbpCascade",
            "LineToCircleMesauseNodeData",
            "LineToLineAngleMesauseNodeData",
            "LineToLineMesauseNodeData",
            "MOG",
            "MserFeatureDetector",
            "MultiplayDivide",
            "NGOutputNodeData",
            "Normalize",
            "OKOutputNodeData",
            "ObjDetectOnnxNodeData",
            "ObjectDetectRecordNodeData",
            "Open",
            "OpenCVBitholderSrcImageFilesNodeData",
            "OpenCVBoardSrcImageFilesNodeData",
            "OpenCVCardoorSrcImageFilesNodeData",
            "OpenCVConditionNodeData",
            "OpenCVHalconSrcImageFilesNodeData",
            "OpenCVPillMagnesiumSrcImageFilesNodeData",
            "OpenCVPillbagSrcImageFilesNodeData",
            "OpenCVPipeJointsSrcImageFilesNodeData",
            "OpenCVRadiusGaugesSrcImageFilesNodeData",
            "OpenCVSrcImageFilesNodeData",
            "OpenCVWoodSrcImageFilesNodeData",
            "PencilSketch",
            "PersonSrcImageFilesNodeData",
            "PixelThresholdIfConditionNodeData",
            "PointToCircleMesauseNodeData",
            "PointToLineMesauseNodeData",
            "PointToPointMesauseNodeData",
            "Pow",
            "QRCode",
            "RenderBlobs",
            "Repeat",
            "Resize",
            "Rotate",
            "RotatedRectRectification",
            "SVM",
            "SeamlessClone",
            "SeamlessCloneBackground",
            "SemSegOnnxNodeData",
            "SerialReadByteNodeData",
            "SerialReadStringNodeData",
            "SerialWriteByteNodeData",
            "SerialWriteStringNodeData",
            "ShapeTemplateMatch",
            "ShortWriteableModbusNodeData",
            "ShowDialogNotifyMessageOutputNodeData",
            "ShowErrorNotifyMessageOutputNodeData",
            "ShowFatalNotifyMessageOutputNodeData",
            "ShowInfoNotifyMessageOutputNodeData",
            "ShowSuccessNotifyMessageOutputNodeData",
            "ShowWarnNotifyMessageOutputNodeData",
            "SplitBGR",
            "SrcImageFilesNodeData",
            "SrcVideoFilesNodeData",
            "StarFeatureDetector",
            "Stitching",
            "Stylization",
            "Subdiv2D",
            "TakeoffForegroundInfo",
            "TcpReadStringNodeData",
            "TcpWriteStringNodeData",
            "Threshold",
            "TopHat",
            "Transpose",
            "UdpReadStringNodeData",
            "UdpWriteStringNodeData",
            "WarpAffineTransform",
            "WarpPerspectiveTransform",
            "Yolov3",
            "Yolov5FaceOnnxNodeData",
            "Yolov5OnnxNodeData",
        };
        for(const auto &retired:retiredIds)
        {
            QVERIFY2(!assembly->getXvFuncInfo(retired).isValid(),qPrintable(retired));
            QVERIFY(!assembly->createNewXvFunc(retired));
            QVERIFY(assembly->canonicalRole(retired).isEmpty());
        }

        const int roleCount=assembly->getXvFuncInfos().count();
        XvFuncPreset invalidPreset;
        invalidPreset.alias="InvalidPreset";
        invalidPreset.canonicalRole="ImageAcquisition";
        invalidPreset.properties.insert("missingProperty",1);
        QVERIFY(!assembly->registerPlugin({}, {invalidPreset},&registrationError));
        QVERIFY(!registrationError.isEmpty());
        QCOMPARE(assembly->getXvFuncInfos().count(),roleCount);
        QVERIFY(!assembly->getXvFuncInfo(invalidPreset.alias).isValid());

        XvFuncPreset validBeforeFailure;
        validBeforeFailure.alias="ValidBeforeFailure";
        validBeforeFailure.canonicalRole="ImageAcquisition";
        validBeforeFailure.properties.insert("acqType",1);
        XvFuncPreset unknownTarget;
        unknownTarget.alias="UnknownTarget";
        unknownTarget.canonicalRole="MissingRole";
        QVERIFY(!assembly->registerPlugin({}, {validBeforeFailure,unknownTarget},
                                          &registrationError));
        QVERIFY(!registrationError.isEmpty());
        QCOMPARE(assembly->getXvFuncInfos().count(),roleCount);
        QVERIFY(!assembly->getXvFuncInfo(validBeforeFailure.alias).isValid());

        XvFuncPreset wrongType;
        wrongType.alias="WrongType";
        wrongType.canonicalRole="ImageAcquisition";
        wrongType.properties.insert("acqType",QString("Dir"));
        QVERIFY(!assembly->registerPlugin({}, {wrongType},&registrationError));
        QVERIFY(!registrationError.isEmpty());
        QCOMPARE(assembly->getXvFuncInfos().count(),roleCount);

        QVERIFY(!assembly->registerPlugin({metaObjects.first()},{},
                                          &registrationError));
        QVERIFY(!registrationError.isEmpty());
        QCOMPARE(assembly->getXvFuncInfos().count(),roleCount);
        QVERIFY(plugin->uninit());
    }
};

int main(int argc,char *argv[])
{
    // Registering operator metadata creates QPixmap icons.
    QGuiApplication application(argc,argv);
    if(application.arguments().size()<3)
    {
        qCritical("Usage: XvSystemPluginTests <plugin-path> <compatibility-matrix>");
        return 1;
    }
    SystemPluginTest test;
    // Keep the two application inputs available through QCoreApplication,
    // but do not let Qt Test interpret them as test-function selectors.
    QStringList testArguments{application.arguments().first()};
    testArguments.append(application.arguments().mid(3));
    return QTest::qExec(&test,testArguments);
}
#include "tst_systemplugin.moc"
