#include <QtTest>

#include <QGuiApplication>
#include <QFile>
#include <QMap>
#include <QPluginLoader>
#include <QSet>
#include <QRegularExpression>

#include <memory>

#include "IXvFactoryPlugin.h"
#include "XvFunc.h"
#include "XvFuncAssembly.h"

using namespace XvCore;

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
        QCOMPARE(presets.count(),126);
        const QRegularExpression chinese(QStringLiteral("[\\x{4e00}-\\x{9fff}]"));
        for(const XvFuncPreset &preset:presets)
        {
            QVERIFY2(chinese.match(preset.displayName).hasMatch(),qPrintable(preset.alias));
            QVERIFY(!preset.alias.contains(chinese));
        }

        const QMap<QString,QString> expectedRolePresets={
            {"CreateShapeNodeData","GeometryCreate"},
            {"OpenCVConditionNodeData","ConditionalFlow"},
            {"QRCode","OCodeDetector"}
        };
        QMap<QString,int> expectedAcquisitionPresets;
        const QStringList directoryAliases={
            "SrcImageFilesNodeData",
            "OpenCVSrcImageFilesNodeData",
            "OpenCVBitholderSrcImageFilesNodeData",
            "OpenCVBoardSrcImageFilesNodeData",
            "OpenCVCardoorSrcImageFilesNodeData",
            "OpenCVHalconSrcImageFilesNodeData",
            "OpenCVPillbagSrcImageFilesNodeData",
            "OpenCVPillMagnesiumSrcImageFilesNodeData",
            "OpenCVPipeJointsSrcImageFilesNodeData",
            "OpenCVRadiusGaugesSrcImageFilesNodeData",
            "OpenCVWoodSrcImageFilesNodeData",
            "PersonSrcImageFilesNodeData"
        };
        for(const QString &alias:directoryAliases)
            expectedAcquisitionPresets.insert(alias,1);
        expectedAcquisitionPresets.insert("CameraCaptureNodeData",2);
        expectedAcquisitionPresets.insert("CameraNodeData",2);
        expectedAcquisitionPresets.insert("SrcVideoFilesNodeData",3);
        const QMap<QString,QPair<QString,int>> expectedModePresets={
            {"AddSutract",{"OImageArithmetic",0}},
            {"BitwiseNot",{"OImageArithmetic",1}},
            {"MultiplayDivide",{"OImageArithmetic",2}},
            {"Pow",{"OImageArithmetic",3}},
            {"Blur",{"OImageFilter",0}},
            {"GaussianBlur",{"OImageFilter",1}},
            {"DetailEnhance",{"OImageFilter",2}},
            {"EdgePreservingFilter",{"OImageFilter",3}},
            {"PencilSketch",{"OImageFilter",4}},
            {"Stylization",{"OImageFilter",5}},
            {"CvtColor",{"OImageColor",0}},
            {"HSVInRange",{"OImageColor",1}},
            {"Normalize",{"OImageColor",2}},
            {"SplitBGR",{"OImageColor",3}},
            {"Threshold",{"OImageThreshold",0}},
            {"PixelThresholdIfConditionNodeData",{"OImageThreshold",1}},
            {"Flip",{"OImageTransform",0}},
            {"HomographyTransform",{"OImageTransform",1}},
            {"Repeat",{"OImageTransform",2}},
            {"Resize",{"OImageTransform",3}},
            {"Rotate",{"OImageTransform",4}},
            {"Transpose",{"OImageTransform",5}},
            {"WarpAffineTransform",{"OImageTransform",6}},
            {"WarpPerspectiveTransform",{"OImageTransform",7}},
            {"Canny",{"OImageAnalysis",0}},
            {"Hist",{"OImageAnalysis",1}},
            {"Hog",{"OImageAnalysis",2}},
            {"Subdiv2D",{"OImageAnalysis",3}},
            {"DnnSuperres",{"OImageModel",0}},
            {"MOG",{"OImageModel",1}},
            {"SVM",{"OImageModel",2}},
            {"SeamlessCloneBackground",{"OImageComposition",0}},
            {"SeamlessClone",{"OImageComposition",1}},
            {"Stitching",{"OImageComposition",2}},
            {"BlackHat",{"OMorphology",0}},
            {"Close",{"OMorphology",1}},
            {"Dilate",{"OMorphology",2}},
            {"Erode",{"OMorphology",3}},
            {"Gradient",{"OMorphology",4}},
            {"Open",{"OMorphology",5}},
            {"TopHat",{"OMorphology",6}},
            {"BlobDetector",{"ORegionDetector",0}},
            {"FindContours",{"ORegionDetector",1}},
            {"HoughCircles",{"ORegionDetector",2}},
            {"RenderBlobs",{"ORegionDetector",3}},
            {"HoughLines",{"ORegionDetector",4}},
            {"HoughLinesP",{"ORegionDetector",5}},
            {"CornerHarris",{"OPointFeature",0}},
            {"CornerSubPix",{"OPointFeature",1}},
            {"AKazeFeatureDetector",{"OPointFeature",2}},
            {"BriskFeatureDetector",{"OPointFeature",3}},
            {"FastFeatureDetector",{"OPointFeature",4}},
            {"FreakFeatureDetector",{"OPointFeature",5}},
            {"KazeFeatureDetector",{"OPointFeature",6}},
            {"MserFeatureDetector",{"OPointFeature",7}},
            {"StarFeatureDetector",{"OPointFeature",8}},
            {"HaarCascade",{"OCascadeDetector",0}},
            {"LbpCascade",{"OCascadeDetector",1}},
            {"OKOutputNodeData",{"NotificationOutput",0}},
            {"NGOutputNodeData",{"NotificationOutput",1}},
            {"ShowInfoNotifyMessageOutputNodeData",{"NotificationOutput",2}},
            {"ShowSuccessNotifyMessageOutputNodeData",{"NotificationOutput",3}},
            {"ShowWarnNotifyMessageOutputNodeData",{"NotificationOutput",4}},
            {"ShowErrorNotifyMessageOutputNodeData",{"NotificationOutput",5}},
            {"ShowFatalNotifyMessageOutputNodeData",{"NotificationOutput",6}},
            {"ShowDialogNotifyMessageOutputNodeData",{"NotificationOutput",7}},
            {"ForNodeData",{"LoopFlow",0}},
            {"ForeachSplitResultImageNodeData",{"LoopFlow",1}},
            {"InferOnnxNodeData",{"NInference",0}},
            {"AgeInferOnnxNodeData",{"NInference",1}},
            {"ClsOnnxNodeData",{"NClassification",0}},
            {"GenderClsOnnxNodeData",{"NClassification",1}},
            {"ObjDetectOnnxNodeData",{"NObjectDetection",0}},
            {"Yolov3",{"NObjectDetection",1}},
            {"Yolov5OnnxNodeData",{"NObjectDetection",2}},
            {"Yolov5FaceOnnxNodeData",{"NObjectDetection",3}},
            {"SemSegOnnxNodeData",{"NSemanticSegmentation",0}},
            {"HumanSemSegOnnxNodeData",{"NSemanticSegmentation",1}},
            {"Base64TemplateMatchNodeData",{"OTemplateMatch",0}},
            {"FeaturePointTemplateMatch",{"OTemplateMatch",1}},
            {"ShapeTemplateMatch",{"OTemplateMatch",2}},
            {"HSVTemplateMatch",{"OTemplateMatch",3}},
            {"ForegroundRotatedRectRectification",{"ORectification",0}},
            {"TakeoffForegroundInfo",{"ORectification",1}},
            {"RotatedRectRectification",{"ORectification",2}},
            {"CircleToCircleMesauseNodeData",{"GeometryMeasure",0}},
            {"LineToCircleMesauseNodeData",{"GeometryMeasure",1}},
            {"LineToLineAngleMesauseNodeData",{"GeometryMeasure",2}},
            {"LineToLineMesauseNodeData",{"GeometryMeasure",3}},
            {"PointToCircleMesauseNodeData",{"GeometryMeasure",4}},
            {"PointToLineMesauseNodeData",{"GeometryMeasure",5}},
            {"PointToPointMesauseNodeData",{"GeometryMeasure",6}},
            {"DetectRecordNodeData",{"DetectRecord",0}},
            {"ClassDetectRecordNodeData",{"DetectRecord",1}},
            {"ObjectDetectRecordNodeData",{"DetectRecord",2}},
            {"HasDetectRecordNodeData",{"DetectRecord",3}},
            {"HttpReadJsonNodeData",{"HttpJson",0}},
            {"HttpWriteJsonNodeData",{"HttpJson",1}},
            {"TcpReadStringNodeData",{"TcpText",0}},
            {"TcpWriteStringNodeData",{"TcpText",1}},
            {"UdpReadStringNodeData",{"UdpText",0}},
            {"UdpWriteStringNodeData",{"UdpText",1}},
            {"SerialReadByteNodeData",{"SerialData",0}},
            {"SerialReadStringNodeData",{"SerialData",1}},
            {"SerialWriteByteNodeData",{"SerialData",2}},
            {"SerialWriteStringNodeData",{"SerialData",3}},
            {"IntReadableModbusNodeData",{"ModbusRegister",0}},
            {"ShortWriteableModbusNodeData",{"ModbusRegister",1}}
        };
        QSet<QString> aliases;
        int acquisitionPresetCount=0;
        int modePresetCount=0;
        for(const XvFuncPreset &preset:presets)
        {
            QVERIFY(preset.isValid());
            QVERIFY(!aliases.contains(preset.alias));
            aliases.insert(preset.alias);
            QVERIFY(preset.parameters.isEmpty());
            if(expectedRolePresets.contains(preset.alias))
            {
                QCOMPARE(preset.canonicalRole,expectedRolePresets.value(preset.alias));
                QVERIFY(preset.properties.isEmpty());
            }
            else
            {
                QCOMPARE(preset.properties.size(),1);
                QVERIFY(preset.properties.contains("acqType")
                        || preset.properties.contains("mode"));
            }
            if(preset.properties.contains("mode"))
            {
                QVERIFY2(expectedModePresets.contains(preset.alias),qPrintable(preset.alias));
                const QPair<QString,int> expected=expectedModePresets.value(preset.alias);
                QCOMPARE(preset.canonicalRole,expected.first);
                QCOMPARE(preset.properties.value("mode").toInt(),expected.second);
                ++modePresetCount;
            }
            if(preset.properties.contains("acqType"))
            {
                QVERIFY2(expectedAcquisitionPresets.contains(preset.alias),
                         qPrintable(preset.alias));
                QCOMPARE(preset.canonicalRole,QString("ImageAcquisition"));
                QCOMPARE(preset.properties.value("acqType").toInt(),
                         expectedAcquisitionPresets.value(preset.alias));
                ++acquisitionPresetCount;
            }
        }
        QCOMPARE(acquisitionPresetCount,expectedAcquisitionPresets.size());
        QCOMPARE(modePresetCount,expectedModePresets.size());
        QVERIFY(aliases.contains("SrcImageFilesNodeData"));
        QVERIFY(aliases.contains("CameraCaptureNodeData"));
        QVERIFY(aliases.contains("CameraNodeData"));
        QVERIFY(aliases.contains("SrcVideoFilesNodeData"));
        QVERIFY(aliases.contains("OKOutputNodeData"));
        QVERIFY(aliases.contains("ShowDialogNotifyMessageOutputNodeData"));
        QVERIFY(aliases.contains("OpenCVConditionNodeData"));
        QVERIFY(aliases.contains("ForNodeData"));
        QVERIFY(aliases.contains("ForeachSplitResultImageNodeData"));
        QVERIFY(aliases.contains("InferOnnxNodeData"));
        QVERIFY(aliases.contains("AgeInferOnnxNodeData"));
        QVERIFY(aliases.contains("ClsOnnxNodeData"));
        QVERIFY(aliases.contains("GenderClsOnnxNodeData"));
        QVERIFY(aliases.contains("ObjDetectOnnxNodeData"));
        QVERIFY(aliases.contains("Yolov3"));
        QVERIFY(aliases.contains("Yolov5OnnxNodeData"));
        QVERIFY(aliases.contains("Yolov5FaceOnnxNodeData"));
        QVERIFY(aliases.contains("SemSegOnnxNodeData"));
        QVERIFY(aliases.contains("HumanSemSegOnnxNodeData"));
        QVERIFY(aliases.contains("Base64TemplateMatchNodeData"));
        QVERIFY(aliases.contains("FeaturePointTemplateMatch"));
        QVERIFY(aliases.contains("ShapeTemplateMatch"));
        QVERIFY(aliases.contains("HSVTemplateMatch"));
        QVERIFY(aliases.contains("ForegroundRotatedRectRectification"));
        QVERIFY(aliases.contains("TakeoffForegroundInfo"));
        QVERIFY(aliases.contains("RotatedRectRectification"));
        QVERIFY(aliases.contains("AddSutract"));
        QVERIFY(aliases.contains("WarpPerspectiveTransform"));
        QVERIFY(aliases.contains("DnnSuperres"));
        QVERIFY(aliases.contains("SeamlessClone"));
        QVERIFY(aliases.contains("QRCode"));
        QVERIFY(aliases.contains("StarFeatureDetector"));
        QVERIFY(aliases.contains("LbpCascade"));
        QVERIFY(aliases.contains("CreateShapeNodeData"));
        QVERIFY(aliases.contains("CircleToCircleMesauseNodeData"));
        QVERIFY(aliases.contains("PointToPointMesauseNodeData"));
        QVERIFY(aliases.contains("DetectRecordNodeData"));
        QVERIFY(aliases.contains("ClassDetectRecordNodeData"));
        QVERIFY(aliases.contains("ObjectDetectRecordNodeData"));
        QVERIFY(aliases.contains("HasDetectRecordNodeData"));
        QVERIFY(aliases.contains("HttpReadJsonNodeData"));
        QVERIFY(aliases.contains("HttpWriteJsonNodeData"));
        QVERIFY(aliases.contains("TcpReadStringNodeData"));
        QVERIFY(aliases.contains("TcpWriteStringNodeData"));
        QVERIFY(aliases.contains("UdpReadStringNodeData"));
        QVERIFY(aliases.contains("UdpWriteStringNodeData"));
        QVERIFY(aliases.contains("SerialReadByteNodeData"));
        QVERIFY(aliases.contains("SerialReadStringNodeData"));
        QVERIFY(aliases.contains("SerialWriteByteNodeData"));
        QVERIFY(aliases.contains("SerialWriteStringNodeData"));
        QVERIFY(aliases.contains("IntReadableModbusNodeData"));
        QVERIFY(aliases.contains("ShortWriteableModbusNodeData"));

        QFile matrixFile(arguments.at(2));
        QVERIFY2(matrixFile.open(QIODevice::ReadOnly|QIODevice::Text),
                 qPrintable(matrixFile.errorString()));
        QCOMPARE(matrixFile.readLine().trimmed(),
                 QByteArray("vm_node,domain,xvision_role,mode,mapping,status"));
        QMap<QString,QString> matrixRoles;
        while(!matrixFile.atEnd())
        {
            const QByteArray line=matrixFile.readLine().trimmed();
            QVERIFY2(!line.isEmpty(),"VisionMaster matrix contains a blank row");
            const QList<QByteArray> fields=line.split(',');
            QCOMPARE(fields.size(),6);
            const QString alias=QString::fromUtf8(fields.at(0));
            QVERIFY2(!matrixRoles.contains(alias),qPrintable(alias));
            matrixRoles.insert(alias,QString::fromUtf8(fields.at(2)));
        }
        QCOMPARE(matrixRoles.size(),126);
        QSet<QString> matrixAliases;
        for(auto it=matrixRoles.cbegin();it!=matrixRoles.cend();++it)
            matrixAliases.insert(it.key());
        QVERIFY(aliases==matrixAliases);
        for(const XvFuncPreset &preset:presets)
            QCOMPARE(preset.canonicalRole,matrixRoles.value(preset.alias));

        XvFuncAssembly *assembly=XvFuncAssembly::getInstance();
        QString registrationError;
        QVERIFY2(assembly->registerPlugin(metaObjects,presets,&registrationError),
                 qPrintable(registrationError));
        QCOMPARE(assembly->getXvFuncPresets().count(),presets.count());
        QCOMPARE(assembly->getXvFuncInfos().count(),expectedRoles.count()+presets.count());
        for(const XvFuncPreset &preset:presets)
        {
            const XvFuncInfo info=assembly->getXvFuncInfo(preset.alias);
            QVERIFY(info.isValid());
            QVERIFY(info.preset);
            QCOMPARE(info.canonicalRole,preset.canonicalRole);
            std::unique_ptr<XvFunc> created(assembly->createNewXvFunc(preset.alias));
            QVERIFY2(created,qPrintable(assembly->lastErrorMsg()));
            QCOMPARE(created->funcRole(),preset.canonicalRole);
            if(preset.properties.isEmpty())
            {
                if(preset.alias=="QRCode")
                    QCOMPARE(created->property("mode").toInt(),0);
                else
                    QVERIFY(!created->property("mode").isValid());
            }
            else
            {
                const QString propertyName=preset.properties.firstKey();
                QCOMPARE(created->property(propertyName.toUtf8().constData()).toInt(),
                         preset.properties.value(propertyName).toInt());
            }
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
