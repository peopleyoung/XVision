#include <QtTest>

#include <QDomDocument>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEvent>
#include <QFile>
#include <QLocale>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QSet>
#include <QTemporaryDir>

#include <atomic>
#include <functional>
#include <limits>
#include <memory>

#include "BaseDataBoolCalc.h"
#include "BaseDataIntCalc.h"
#include "BaseDataRealCalc.h"
#include "BaseDataStringProcess.h"
#include "BaseDataWriter.h"
#include "ConditionalFlow.h"
#include "Delayer.h"
#include "DetectRecord.h"
#include "ElapsedTimer.h"
#include "GeometryCreate.h"
#include "GeometryMeasure.h"
#include "HttpJson.h"
#include "HObjectDetection.h"
#include "HModelMatch.h"
#include "HSemanticSegmentation.h"
#include "HalconImageInterop.h"
#include "ImageAcquisition.h"
#include "LogOutput.h"
#include "NotificationOutput.h"
#include "NClassification.h"
#include "NInference.h"
#include "NObjectDetection.h"
#include "NSemanticSegmentation.h"
#include "LoopFlow.h"
#include "ModbusRegister.h"
#include "OImageAnalysis.h"
#include "OImageArithmetic.h"
#include "OImageColor.h"
#include "OImageComposition.h"
#include "OImageFilter.h"
#include "OImageModel.h"
#include "OImageThreshold.h"
#include "OImageTransform.h"
#include "OCascadeDetector.h"
#include "OCodeDetector.h"
#include "OMorphology.h"
#include "OPointFeature.h"
#include "ORegionDetector.h"
#include "ORectification.h"
#include "OTemplateMatch.h"
#include "SerialData.h"
#include "TcpText.h"
#include "UdpText.h"
#include "XBool.h"
#include "XDetectionResult.h"
#include "XImage.h"
#include "XInt.h"
#include "XMatchResult.h"
#include "XObjectList.h"
#include "XPoint2D.h"
#include "XReal.h"
#include "XRotateRectRoi.h"
#include "XSegmentationResult.h"
#include "XString.h"
#include "XVisionSharedData.h"
#include "XVisionRuntimeData.h"
#include "XvDisplayRotateRectRoi.h"
#include "XvCoreManager.h"
#include "IXvCamera.h"
#include "XvCameraManager.h"
#include "XvDirectoryCamera.h"
#include "XvFlow.h"
#include "XvFunc.h"
#include "XvFuncAssembly.h"
#include "XvProject.h"
#include "XvTokenMsgManager.h"

using namespace XvCore;

class DefaultLocaleGuard
{
public:
    explicit DefaultLocaleGuard(const QLocale &locale):m_previous(QLocale())
    {
        QLocale::setDefault(locale);
    }
    ~DefaultLocaleGuard()
    {
        QLocale::setDefault(m_previous);
    }
private:
    QLocale m_previous;
};

class BrokenPersistentFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit BrokenPersistentFunc(QObject *parent=nullptr)
        :XvFunc(parent)
    {
        _funcRole="BrokenPersistentFunc";
        _funcName="Broken persistence fixture";
        _funcType=EXvFuncType::Other;
    }

    QStringList persistentPropertyNames() const override
    {
        return {"missingProperty"};
    }
};

class AlwaysErrorFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit AlwaysErrorFunc(QObject *parent=nullptr)
        :XvFunc(parent)
    {
        _funcRole="AlwaysErrorFunc";
        _funcName="Always error fixture";
        _funcType=EXvFuncType::Other;
    }

protected:
    EXvFuncRunStatus run() override
    {
        setRunMsg("intentional test error");
        return EXvFuncRunStatus::Error;
    }
};

class StopFlowFixtureFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit StopFlowFixtureFunc(QObject *parent=nullptr)
        :XvFunc(parent)
    {
        _funcRole="StopFlowFixtureFunc";
        _funcName="Stop flow fixture";
        _funcType=EXvFuncType::Other;
    }

protected:
    EXvFuncRunStatus run() override
    {
        if(!parFlow() || parFlow()->stop()!=Ret_Xv_Success)
            return EXvFuncRunStatus::Error;
        return EXvFuncRunStatus::Ok;
    }
};

class IterationCaptureParam:public XvBaseParam
{
public:
    IterationCaptureParam()
    {
        value=new XInt("value",0,this,"Value");
    }

    XInt *value=nullptr;
};

class IterationCaptureFunc:public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit IterationCaptureFunc(QObject *parent=nullptr)
        :XvFunc(parent),m_param(new IterationCaptureParam())
    {
        _funcRole="IterationCaptureFunc";
        _funcName="Iteration capture fixture";
        _funcType=EXvFuncType::Other;
    }

    ~IterationCaptureFunc() override { delete m_param; }
    QList<int> observedValues() const { return m_observedValues; }

protected:
    XvBaseParam *getParam() const override { return m_param; }
    EXvFuncRunStatus run() override
    {
        m_observedValues.append(m_param->value->value());
        return EXvFuncRunStatus::Ok;
    }

private:
    IterationCaptureParam *m_param=nullptr;
    QList<int> m_observedValues;
};

class SegmentationFixtureParam : public XvBaseParam
{
public:
    SegmentationFixtureParam()
    {
        value=new XSegmentationResult("value",this);
    }

    XSegmentationResult *value=nullptr;
};

class SegmentationFixtureResult : public XvBaseResult
{
public:
    SegmentationFixtureResult()
    {
        value=new XSegmentationResult("value",this);
    }

    XSegmentationResult *value=nullptr;
};

class SegmentationFixtureFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SegmentationFixtureFunc(QObject *parent=nullptr)
        :XvFunc(parent),m_param(new SegmentationFixtureParam),
          m_result(new SegmentationFixtureResult)
    {
        _funcRole="SegmentationFixtureFunc";
        _funcName="Segmentation fixture";
        _funcType=EXvFuncType::Other;
    }

    ~SegmentationFixtureFunc() override
    {
        delete m_param;
        delete m_result;
    }

    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }

private:
    SegmentationFixtureParam *m_param=nullptr;
    SegmentationFixtureResult *m_result=nullptr;
};

class GeometryPersistentParam : public XvBaseParam
{
public:
    GeometryPersistentParam()
    {
        point=new XPoint2D("point",0.0,0.0,this);
        roi=new XRotateRectRoi("roi",0.0,0.0,1.0,1.0,0.0,this);
        matches=new XObjectList("matches",XMatchResult::type(),this);
        detections=new XObjectList("detections",XDetectionResult::type(),this);
    }

    XPoint2D *point=nullptr;
    XRotateRectRoi *roi=nullptr;
    XObjectList *matches=nullptr;
    XObjectList *detections=nullptr;
};

class GeometryPersistentResult : public XvBaseResult
{
public:
    GeometryPersistentResult()
    {
        match=new XMatchResult("match",0.0,0.0,0.0,0.0,this);
        matches=new XObjectList("resultMatches",XMatchResult::type(),this);
        points=new XObjectList("resultPoints",XPoint2D::type(),this);
        detections=new XObjectList(
                    "resultDetections",XDetectionResult::type(),this);
    }

    XMatchResult *match=nullptr;
    XObjectList *matches=nullptr;
    XObjectList *points=nullptr;
    XObjectList *detections=nullptr;
};

class GeometryPersistentFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit GeometryPersistentFunc(QObject *parent=nullptr)
        :XvFunc(parent),m_param(new GeometryPersistentParam()),
          m_result(new GeometryPersistentResult())
    {
        _funcRole="GeometryPersistentFunc";
        _funcName="Geometry persistence fixture";
        _funcType=EXvFuncType::Other;
    }

    ~GeometryPersistentFunc() override
    {
        delete m_param;
        delete m_result;
    }

    QStringList persistentResultNames() const override
    {
        return {"match","resultMatches","resultDetections"};
    }

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }

private:
    GeometryPersistentParam *m_param=nullptr;
    GeometryPersistentResult *m_result=nullptr;
};

class PortFixtureFunc : public XvFunc
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit PortFixtureFunc(QObject *parent=nullptr)
        :XvFunc(parent)
    {
        _funcRole="PortFixtureFunc";
        _funcName="Port fixture";
        _funcType=EXvFuncType::Logic;
    }

    QStringList outputPorts() const override
    {
        return {"true","false"};
    }

    void selectPorts(const QStringList &ports)
    {
        m_selectPorts=true;
        m_selectedPorts=ports;
    }

    XvExecutionDirective executionDirective() const override
    {
        XvExecutionDirective directive;
        if(m_selectPorts)
        {
            directive.kind=XvExecutionDirective::SelectPorts;
            directive.selectedPorts=m_selectedPorts;
        }
        return directive;
    }

protected:
    EXvFuncRunStatus run() override
    {
        return EXvFuncRunStatus::Ok;
    }

private:
    bool m_selectPorts=false;
    QStringList m_selectedPorts;
};

class BrokenCloneMatchResult : public XMatchResult
{
public:
    using XMatchResult::XMatchResult;
    XObject *clone() override { return nullptr; }
};

class TestRotateRectRoi : public XvDisplayRotateRectRoi
{
public:
    using XvDisplayRotateRectRoi::XvDisplayRotateRectRoi;

    bool dragLength1Handle(const QPointF &scenePosition)
    {
        m_controlItem2->updatePos(mapFromScene(scenePosition));
        return updateRoi(m_controlItem2);
    }

    bool moveForTest(const QPointF &offset)
    {
        return moveRoiByOffset(offset);
    }
};

class ProjectXmlTest : public QObject
{
    Q_OBJECT
private:
    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) return QByteArray();
        return file.readAll();
    }

    static bool writeDocument(const QString &path,const QDomDocument &document)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly|QIODevice::Truncate)
                && file.write(document.toByteArray(2))>=0;
    }

    static QDomDocument parseDocument(const QString &path)
    {
        QDomDocument document;
        QFile file(path);
        if(file.open(QIODevice::ReadOnly)) document.setContent(&file);
        return document;
    }

    static QDomElement findFunctionElementByRole(QDomDocument &document,
                                                  const QString &role)
    {
        const QDomNodeList functions=document.elementsByTagName("Function");
        for(int index=0;index<functions.count();++index)
        {
            const QDomElement function=functions.at(index).toElement();
            if(function.attribute("role")==role) return function;
        }
        return QDomElement();
    }

    static bool writeSolidImage(const QString &path,const QColor &color)
    {
        QImage image(5,4,QImage::Format_RGB32);
        image.fill(color);
        return image.save(path);
    }

    static QImage makeIntegrationImage()
    {
        QImage image(180,140,QImage::Format_RGB888);
        image.fill(Qt::black);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing,false);
        painter.setPen(QPen(Qt::white,5));
        painter.drawRect(QRect(52,35,70,54));
        painter.drawLine(QPoint(52,35),QPoint(122,89));
        painter.drawLine(QPoint(72,89),QPoint(122,60));
        painter.setBrush(Qt::white);
        painter.drawEllipse(QPoint(100,52),7,7);
        painter.setPen(QPen(QColor(180,180,180),3));
        painter.drawLine(QPoint(20,115),QPoint(155,115));
        return image;
    }

    void expectLoadRejected(const QString &path,XvProject *expectedProject)
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        QVERIFY(!manager->loadXvProject(path));
        QCOMPARE(manager->getXvProject(),expectedProject);
        QVERIFY(!manager->lastErrorMsg().isEmpty());
    }

    void expectFlowImportRejected(const QString &path,XvProject *expectedProject,
                                  int expectedFlowCount)
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        QVERIFY(!manager->importXvFlow(path));
        QCOMPARE(manager->getXvProject(),expectedProject);
        QCOMPARE(expectedProject->xvFlowCount(),expectedFlowCount);
        QVERIFY(!manager->lastErrorMsg().isEmpty());
    }

    static XvFunc *findFunctionByRole(XvFlow *flow,const QString &role)
    {
        if(!flow) return nullptr;
        for(XvFunc *function:flow->getXvFuncs())
        {
            if(function && function->funcRole()==role) return function;
        }
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        XvFuncAssembly *assembly=XvFuncAssembly::getInstance();
        QVERIFY(assembly->registerXvFunc(BaseDataWriter::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(BaseDataBoolCalc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(BaseDataIntCalc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(BaseDataRealCalc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(BaseDataStringProcess::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ImageAcquisition::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ConditionalFlow::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(LoopFlow::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(HModelMatch::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(HObjectDetection::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(HSemanticSegmentation::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(Delayer::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ElapsedTimer::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(LogOutput::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(NotificationOutput::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(NInference::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(NClassification::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(NObjectDetection::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(NSemanticSegmentation::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageArithmetic::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageFilter::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageColor::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageThreshold::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageTransform::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageAnalysis::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageModel::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OImageComposition::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OMorphology::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ORegionDetector::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OCodeDetector::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OPointFeature::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OCascadeDetector::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(OTemplateMatch::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ORectification::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(GeometryCreate::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(GeometryMeasure::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(DetectRecord::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(HttpJson::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(TcpText::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(UdpText::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(SerialData::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(ModbusRegister::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(BrokenPersistentFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(AlwaysErrorFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(StopFlowFixtureFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(IterationCaptureFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(SegmentationFixtureFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(GeometryPersistentFunc::staticMetaObject));
        QVERIFY(assembly->registerXvFunc(PortFixtureFunc::staticMetaObject));

        XvFuncPreset directoryPreset;
        directoryPreset.alias="SrcImageFilesNodeData";
        directoryPreset.displayName="SrcImageFilesNodeData";
        directoryPreset.canonicalRole="ImageAcquisition";
        directoryPreset.properties.insert("acqType",
                                          static_cast<int>(ImageAcquisition::Dir));
        XvFuncPreset cannyPreset;
        cannyPreset.alias="Canny";
        cannyPreset.displayName="Canny";
        cannyPreset.canonicalRole="OImageAnalysis";
        cannyPreset.properties.insert("mode",static_cast<int>(OImageAnalysis::Canny));
        XvFuncPreset qrPreset;
        qrPreset.alias="QRCode";
        qrPreset.displayName="QRCode";
        qrPreset.canonicalRole="OCodeDetector";
        XvFuncPreset blackHatPreset;
        blackHatPreset.alias="BlackHat";
        blackHatPreset.displayName="BlackHat";
        blackHatPreset.canonicalRole="OMorphology";
        blackHatPreset.properties.insert("mode",static_cast<int>(OMorphology::BlackHat));
        XvFuncPreset houghLinesPreset;
        houghLinesPreset.alias="HoughLinesP";
        houghLinesPreset.displayName="HoughLinesP";
        houghLinesPreset.canonicalRole="ORegionDetector";
        houghLinesPreset.properties.insert("mode",
                                          static_cast<int>(ORegionDetector::HoughLinesP));
        XvFuncPreset starPreset;
        starPreset.alias="StarFeatureDetector";
        starPreset.displayName="StarFeatureDetector";
        starPreset.canonicalRole="OPointFeature";
        starPreset.properties.insert("mode",static_cast<int>(OPointFeature::Star));
        XvFuncPreset lbpPreset;
        lbpPreset.alias="LbpCascade";
        lbpPreset.displayName="LbpCascade";
        lbpPreset.canonicalRole="OCascadeDetector";
        lbpPreset.properties.insert("mode",static_cast<int>(OCascadeDetector::Lbp));
        XvFuncPreset videoPreset;
        videoPreset.alias="SrcVideoFilesNodeData";
        videoPreset.displayName="SrcVideoFilesNodeData";
        videoPreset.canonicalRole="ImageAcquisition";
        videoPreset.properties.insert("acqType",
                                      static_cast<int>(ImageAcquisition::Video));
        XvFuncPreset dialogPreset;
        dialogPreset.alias="ShowDialogNotifyMessageOutputNodeData";
        dialogPreset.displayName="ShowDialogNotifyMessageOutputNodeData";
        dialogPreset.canonicalRole="NotificationOutput";
        dialogPreset.properties.insert("mode",static_cast<int>(NotificationOutput::Dialog));
        QList<XvFuncPreset> onnxPresets;
        const auto addOnnxPreset=[&](const QString &alias,const QString &role,int mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole=role;
            preset.properties.insert("mode",mode);
            onnxPresets.append(preset);
        };
        addOnnxPreset("InferOnnxNodeData","NInference",NInference::Generic);
        addOnnxPreset("AgeInferOnnxNodeData","NInference",NInference::Age);
        addOnnxPreset("ClsOnnxNodeData","NClassification",NClassification::Generic);
        addOnnxPreset("GenderClsOnnxNodeData","NClassification",NClassification::Gender);
        addOnnxPreset("ObjDetectOnnxNodeData","NObjectDetection",NObjectDetection::Generic);
        addOnnxPreset("Yolov3","NObjectDetection",NObjectDetection::Yolov3);
        addOnnxPreset("Yolov5OnnxNodeData","NObjectDetection",NObjectDetection::Yolov5);
        addOnnxPreset("Yolov5FaceOnnxNodeData","NObjectDetection",NObjectDetection::Yolov5Face);
        addOnnxPreset("SemSegOnnxNodeData","NSemanticSegmentation",
                      NSemanticSegmentation::Generic);
        addOnnxPreset("HumanSemSegOnnxNodeData","NSemanticSegmentation",
                      NSemanticSegmentation::Human);
        QList<XvFuncPreset> templatePresets;
        const auto addTemplatePreset=[&](const QString &alias,const QString &role,int mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole=role;
            preset.properties.insert("mode",mode);
            templatePresets.append(preset);
        };
        addTemplatePreset("Base64TemplateMatchNodeData","OTemplateMatch",
                          OTemplateMatch::Base64);
        addTemplatePreset("FeaturePointTemplateMatch","OTemplateMatch",
                          OTemplateMatch::Feature);
        addTemplatePreset("ShapeTemplateMatch","OTemplateMatch",
                          OTemplateMatch::Shape);
        addTemplatePreset("HSVTemplateMatch","OTemplateMatch",OTemplateMatch::Hsv);
        addTemplatePreset("ForegroundRotatedRectRectification","ORectification",
                          ORectification::ForegroundRotatedRect);
        addTemplatePreset("TakeoffForegroundInfo","ORectification",
                          ORectification::ForegroundExtract);
        addTemplatePreset("RotatedRectRectification","ORectification",
                          ORectification::RotatedRect);
        QList<XvFuncPreset> geometryPresets;
        XvFuncPreset createShapePreset;
        createShapePreset.alias="CreateShapeNodeData";
        createShapePreset.displayName="CreateShapeNodeData";
        createShapePreset.canonicalRole="GeometryCreate";
        geometryPresets.append(createShapePreset);
        const auto addGeometryPreset=[&](const QString &alias,GeometryMeasure::Mode mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole="GeometryMeasure";
            preset.properties.insert("mode",static_cast<int>(mode));
            geometryPresets.append(preset);
        };
        addGeometryPreset("CircleToCircleMesauseNodeData",GeometryMeasure::CircleCircle);
        addGeometryPreset("LineToCircleMesauseNodeData",GeometryMeasure::LineCircle);
        addGeometryPreset("LineToLineAngleMesauseNodeData",GeometryMeasure::LineLineAngle);
        addGeometryPreset("LineToLineMesauseNodeData",GeometryMeasure::LineLine);
        addGeometryPreset("PointToCircleMesauseNodeData",GeometryMeasure::PointCircle);
        addGeometryPreset("PointToLineMesauseNodeData",GeometryMeasure::PointLine);
        addGeometryPreset("PointToPointMesauseNodeData",GeometryMeasure::PointPoint);
        QList<XvFuncPreset> recordPresets;
        const auto addRecordPreset=[&](const QString &alias,DetectRecord::Mode mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole="DetectRecord";
            preset.properties.insert("mode",static_cast<int>(mode));
            recordPresets.append(preset);
        };
        addRecordPreset("DetectRecordNodeData",DetectRecord::Record);
        addRecordPreset("ClassDetectRecordNodeData",DetectRecord::ClassRecord);
        addRecordPreset("ObjectDetectRecordNodeData",DetectRecord::ObjectRecord);
        addRecordPreset("HasDetectRecordNodeData",DetectRecord::HasRecord);
        QList<XvFuncPreset> communicationPresets;
        const auto addCommunicationPreset=[&](const QString &alias,const QString &role,int mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole=role;
            preset.properties.insert("mode",mode);
            communicationPresets.append(preset);
        };
        addCommunicationPreset("HttpReadJsonNodeData","HttpJson",HttpJson::Read);
        addCommunicationPreset("HttpWriteJsonNodeData","HttpJson",HttpJson::Write);
        addCommunicationPreset("TcpReadStringNodeData","TcpText",TcpText::Read);
        addCommunicationPreset("TcpWriteStringNodeData","TcpText",TcpText::Write);
        addCommunicationPreset("UdpReadStringNodeData","UdpText",UdpText::Read);
        addCommunicationPreset("UdpWriteStringNodeData","UdpText",UdpText::Write);
        addCommunicationPreset("SerialReadByteNodeData","SerialData",SerialData::ReadBytes);
        addCommunicationPreset("SerialReadStringNodeData","SerialData",SerialData::ReadText);
        addCommunicationPreset("SerialWriteByteNodeData","SerialData",SerialData::WriteBytes);
        addCommunicationPreset("SerialWriteStringNodeData","SerialData",SerialData::WriteText);
        addCommunicationPreset("IntReadableModbusNodeData","ModbusRegister",
                               ModbusRegister::ReadInt32);
        addCommunicationPreset("ShortWriteableModbusNodeData","ModbusRegister",
                               ModbusRegister::WriteInt16);
        QList<XvFuncPreset> flowPresets;
        XvFuncPreset conditionPreset;
        conditionPreset.alias="OpenCVConditionNodeData";
        conditionPreset.displayName=conditionPreset.alias;
        conditionPreset.canonicalRole="ConditionalFlow";
        flowPresets.append(conditionPreset);
        const auto addLoopPreset=[&](const QString &alias,LoopFlow::Mode mode)
        {
            XvFuncPreset preset;
            preset.alias=alias;
            preset.displayName=alias;
            preset.canonicalRole="LoopFlow";
            preset.properties.insert("mode",static_cast<int>(mode));
            flowPresets.append(preset);
        };
        addLoopPreset("ForNodeData",LoopFlow::For);
        addLoopPreset("ForeachSplitResultImageNodeData",LoopFlow::ForeachImages);
        QList<XvFuncPreset> presets={directoryPreset,cannyPreset,qrPreset,
                                     blackHatPreset,houghLinesPreset,
                                     starPreset,lbpPreset,videoPreset,
                                     dialogPreset};
        presets.append(onnxPresets);
        presets.append(templatePresets);
        presets.append(geometryPresets);
        presets.append(recordPresets);
        presets.append(communicationPresets);
        presets.append(flowPresets);
        QString error;
        QVERIFY2(assembly->registerPlugin({},presets,&error),
                 qPrintable(error));
    }

    void cleanup()
    {
        XvCameraMgr->shutdown();
        if(XvCameraMgr->directoryProvider())
            XvCameraMgr->directoryProvider()->clearDevices();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    }

    void geometryValuesValidateAndClone()
    {
        XPoint2D point("point",12.5,-8.25);
        QVERIFY(!point.setValue(qQNaN(),4.0));
        QCOMPARE(point.x(),12.5);
        QCOMPARE(point.y(),-8.25);
        std::unique_ptr<XObject> pointClone(point.clone());
        auto clonedPoint=dynamic_cast<XPoint2D*>(pointClone.get());
        QVERIFY(clonedPoint);
        QCOMPARE(clonedPoint->x(),point.x());
        QCOMPARE(clonedPoint->parObjectSet(),nullptr);

        XRotateRectRoi roi("roi",10.0,20.0,30.0,15.0,0.25);
        QVERIFY(!roi.setValue(1.0,2.0,0.0,3.0,0.0));
        QVERIFY(!roi.setValue(1.0,2.0,3.0,4.0,qInf()));
        QCOMPARE(roi.centerX(),10.0);
        QCOMPARE(roi.length1(),30.0);
        QCOMPARE(roi.angle(),0.25);

        XMatchResult match("match",3.0,4.0,-0.5,0.75);
        QVERIFY(!match.setValue(3.0,4.0,-0.5,1.01));
        QVERIFY(!match.setValue(3.0,qQNaN(),-0.5,0.5));
        QCOMPARE(match.score(),0.75);
        QVERIFY(!match.setData(&roi));
        QCOMPARE(match.x(),3.0);

        XDetectionResult detection("detection",10.0,20.0,30.0,40.0,2,
                                   "defect",0.85);
        QVERIFY(!detection.setValue(10.0,20.0,0.0,40.0,2,"defect",0.85));
        QVERIFY(!detection.setValue(10.0,20.0,30.0,40.0,-1,"defect",0.85));
        QVERIFY(!detection.setValue(10.0,20.0,30.0,40.0,2," ",0.85));
        QVERIFY(!detection.setValue(10.0,20.0,30.0,40.0,2,"defect",1.01));
        QCOMPARE(detection.width(),30.0);
        QCOMPARE(detection.className(),QString("defect"));
        std::unique_ptr<XObject> detectionClone(detection.clone());
        auto clonedDetection=dynamic_cast<XDetectionResult*>(detectionClone.get());
        QVERIFY(clonedDetection);
        QCOMPARE(clonedDetection->classId(),2);
        QCOMPARE(clonedDetection->score(),0.85);
        QCOMPARE(clonedDetection->parObjectSet(),nullptr);
    }

    void objectListDeepCopyIsTransactional()
    {
        XObjectList source("source",XMatchResult::type());
        QVERIFY(source.addValue(new XMatchResult("first",1.0,2.0,0.1,0.9)));
        QVERIFY(source.addValue(new XMatchResult("second",3.0,4.0,0.2,0.8)));
        XObjectList target("target",XMatchResult::type());
        QVERIFY(target.addValue(new XMatchResult("old",9.0,9.0,0.0,0.1)));

        QVERIFY(target.setData(&source));
        QCOMPARE(target.count(),qsizetype(2));
        auto sourceFirst=dynamic_cast<XMatchResult*>(source.value(0));
        auto targetFirst=dynamic_cast<XMatchResult*>(target.value(0));
        QVERIFY(sourceFirst);
        QVERIFY(targetFirst);
        QVERIFY(sourceFirst!=targetFirst);
        QVERIFY(sourceFirst->setValue(20.0,30.0,0.5,0.7));
        QCOMPARE(targetFirst->x(),1.0);
        QCOMPARE(targetFirst->parObjectSet(),static_cast<XObjectSet*>(&target));

        std::unique_ptr<XObject> clone(source.clone());
        auto clonedList=dynamic_cast<XObjectList*>(clone.get());
        QVERIFY(clonedList);
        QCOMPARE(clonedList->count(),source.count());
        QVERIFY(clonedList->value(0)!=source.value(0));
        QCOMPARE(clonedList->parObjectSet(),nullptr);

        XObjectList broken("broken",XMatchResult::type());
        QVERIFY(broken.addValue(new XMatchResult("ok",5.0,6.0,0.0,0.5)));
        QVERIFY(broken.addValue(new BrokenCloneMatchResult("bad",7.0,8.0,0.0,0.6)));
        const double previousX=targetFirst->x();
        QVERIFY(!target.setData(&broken));
        QCOMPARE(target.count(),qsizetype(2));
        QCOMPARE(dynamic_cast<XMatchResult*>(target.value(0))->x(),previousX);

        XObjectList lifetimeTarget("lifetime",XMatchResult::type());
        {
            XObjectList temporary("temporary",XMatchResult::type());
            QVERIFY(temporary.addValue(new XMatchResult("",2.0,3.0,0.4,0.6)));
            QVERIFY(temporary.getData(&lifetimeTarget));
        }
        QCOMPARE(lifetimeTarget.count(),qsizetype(1));
        QCOMPARE(dynamic_cast<XMatchResult*>(lifetimeTarget.value(0))->score(),0.6);

        XObjectList detectionSource("detections",XDetectionResult::type());
        QVERIFY(detectionSource.addValue(new XDetectionResult(
                    "box",1.0,2.0,3.0,4.0,5,"part",0.7)));
        XObjectList detectionTarget("targetDetections",XDetectionResult::type());
        QVERIFY(detectionTarget.setData(&detectionSource));
        QCOMPARE(detectionTarget.count(),qsizetype(1));
        auto detection=dynamic_cast<XDetectionResult*>(detectionTarget.value(0));
        QVERIFY(detection);
        QCOMPARE(detection->className(),QString("part"));
    }

    void segmentationResultValidatesAndCopiesTransactionally()
    {
        const QVector<qint32> labels={0,1,1,0,2,2};
        const QVector<float> confidences={0.9f,0.8f,0.7f,0.6f,0.5f,0.4f};
        const QVector<qint32> classIds={0,1,2};
        const QStringList classNames={"background","part","defect"};
        const QVector<QRgb> classColors={qRgb(0,0,0),qRgb(20,180,80),
                                         qRgb(220,40,40)};

        XSegmentationResult source("segmentation");
        QVERIFY(source.setValue(3,2,labels,confidences,classIds,
                                classNames,classColors));
        QCOMPARE(source.width(),3);
        QCOMPARE(source.height(),2);
        QCOMPARE(source.labels(),labels);

        XSegmentationResult target("target");
        QVERIFY(source.getData(&target));
        QCOMPARE(target.confidences(),confidences);
        QCOMPARE(target.classColors(),classColors);

        std::unique_ptr<XObject> clone(source.clone());
        auto clonedResult=dynamic_cast<XSegmentationResult*>(clone.get());
        QVERIFY(clonedResult);
        QCOMPARE(clonedResult->labels(),labels);
        QCOMPARE(clonedResult->parObjectSet(),nullptr);

        QVector<qint32> invalidLabels=labels;
        invalidLabels[0]=99;
        QVERIFY(!target.setValue(3,2,invalidLabels,confidences,classIds,
                                 classNames,classColors));
        QCOMPARE(target.labels(),labels);

        QVector<float> invalidConfidence=confidences;
        invalidConfidence[1]=qQNaN();
        QVERIFY(!target.setValue(3,2,labels,invalidConfidence,classIds,
                                 classNames,classColors));
        QCOMPARE(target.confidences(),confidences);

        invalidConfidence=confidences;
        invalidConfidence[1]=1.01f;
        QVERIFY(!target.setValue(3,2,labels,invalidConfidence,classIds,
                                 classNames,classColors));
        invalidConfidence[1]=-0.01f;
        QVERIFY(!target.setValue(3,2,labels,invalidConfidence,classIds,
                                 classNames,classColors));

        QVERIFY(!target.setValue(3,2,labels.mid(1),confidences,classIds,
                                 classNames,classColors));
        QVERIFY(!target.setValue(3,2,labels,confidences,{0,1,1},
                                 classNames,classColors));
        QVERIFY(!target.setValue(3,2,labels,confidences,classIds,
                                 {"background","","defect"},classColors));
        QVERIFY(!target.setValue(3,2,labels,confidences,classIds,
                                 classNames,classColors.mid(1)));
        XInt wrongType("wrong",42);
        QVERIFY(!target.setData(&wrongType));
        QCOMPARE(target.width(),3);

        target.clear();
        QVERIFY(target.isEmpty());
        QVERIFY(target.labels().isEmpty());
        QVERIFY(target.setValue(0,0,{}, {}, {}, {}, {}));
        QVERIFY(!target.setValue(0,1,{}, {}, {}, {}, {}));

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Segmentation binding");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Binding");
        QVERIFY(flow);
        XvFunc *publisher=flow->createXvFunc("SegmentationFixtureFunc");
        XvFunc *subscriber=flow->createXvFunc("SegmentationFixtureFunc");
        QVERIFY(publisher && subscriber);
        QVERIFY(publisher->addSonFunc(subscriber));
        auto published=dynamic_cast<XSegmentationResult*>(
                    publisher->getResultsByName("value"));
        auto subscribed=dynamic_cast<XSegmentationResult*>(
                    subscriber->getParamsByName("value"));
        QVERIFY(published && subscribed);
        QVERIFY(published->setValue(3,2,labels,confidences,classIds,
                                    classNames,classColors));
        QVERIFY(subscriber->paramSubscribe("value",publisher,"value"));
        QVERIFY(subscriber->updataParam("value",published));
        QCOMPARE(subscribed->labels(),labels);
        QVector<qint32> changedLabels=labels;
        changedLabels[0]=1;
        QVERIFY(published->setValue(3,2,changedLabels,confidences,classIds,
                                    classNames,classColors));
        QCOMPARE(subscribed->labels(),labels);
    }

    void semanticSegmentationUnconfiguredStateIsStrictAndTransactional()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("segmentation.xvproj");
        const QString invalidPath=directory.filePath("segmentation-invalid.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Segmentation");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Inference");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto function=qobject_cast<HSemanticSegmentation*>(
                    flow->createXvFunc("HSemanticSegmentation"));
        QVERIFY(function);
        QCOMPARE(function->runtime(),HSemanticSegmentation::Cpu);
        QCOMPARE(function->overlayOpacity(),0.45);
        QVERIFY(!function->hasConfiguredAssets());

        QImage input(3,2,QImage::Format_RGB888);
        input.fill(Qt::red);
        auto inputValue=dynamic_cast<XImage*>(
                    function->getParamsByName("inputImage"));
        auto segmentation=dynamic_cast<XSegmentationResult*>(
                    function->getResultsByName("segmentation"));
        auto colorMask=dynamic_cast<XImage*>(
                    function->getResultsByName("colorMask"));
        auto overlay=dynamic_cast<XImage*>(
                    function->getResultsByName("overlayImage"));
        auto confidence=dynamic_cast<XImage*>(
                    function->getResultsByName("confidenceImage"));
        QVERIFY(inputValue && segmentation && colorMask && overlay && confidence);
        QVERIFY(segmentation->setValue(1,1,{0},{1.0f},{0},{"background"},
                                         {qRgb(0,0,0)}));
        QImage stale(1,1,QImage::Format_RGB888);
        stale.fill(Qt::green);
        colorMask->setValue(stale);
        overlay->setValue(stale);
        confidence->setValue(stale);
        inputValue->setValue(input);
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!function->getXvFuncRunMsg().isEmpty());
        QVERIFY(segmentation->isEmpty());
        QVERIFY(colorMask->value().isNull());
        QVERIFY(overlay->value().isNull());
        QVERIFY(confidence->value().isNull());

        function->setOverlayOpacity(-0.1);
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        function->setOverlayOpacity(0.35);
        function->setRuntime(static_cast<HSemanticSegmentation::Runtime>(99));
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        function->setRuntime(HSemanticSegmentation::Cpu);
        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(!xml.contains("SemanticSegmentationAssets"));
        QVERIFY(!xml.contains("colorMask"));
        QVERIFY(manager->loadXvProject(path));

        XvProject *restoredProject=manager->getXvProject();
        QVERIFY(restoredProject);
        auto restored=qobject_cast<HSemanticSegmentation*>(findFunctionByRole(
                    restoredProject->getXvFlow(flowId),
                    "HSemanticSegmentation"));
        QVERIFY(restored);
        QCOMPARE(restored->overlayOpacity(),0.35);
        QVERIFY(!restored->hasConfiguredAssets());

        QDomDocument invalid=parseDocument(path);
        QDomNodeList functions=invalid.elementsByTagName("Function");
        QDomElement segmentationFunction;
        for(int index=0;index<functions.count();++index)
        {
            QDomElement candidate=functions.at(index).toElement();
            if(candidate.attribute("role")=="HSemanticSegmentation")
            {
                segmentationFunction=candidate;
                break;
            }
        }
        QVERIFY(!segmentationFunction.isNull());
        QDomElement data=invalid.createElement("PersistentData");
        QDomElement assets=invalid.createElement("SemanticSegmentationAssets");
        assets.setAttribute("format","halcon-dl-segmentation");
        assets.setAttribute("version","1");
        data.appendChild(assets);
        segmentationFunction.appendChild(data);
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,restoredProject);
    }

    void semanticSegmentationFixtureInferenceAndPersistence()
    {
        const QString modelPath=qEnvironmentVariable(
                    "XVISION_SEGMENTATION_MODEL");
        const QString preprocessPath=qEnvironmentVariable(
                    "XVISION_SEGMENTATION_PREPROCESS");
        const QString inputPath=qEnvironmentVariable(
                    "XVISION_SEGMENTATION_INPUT");
        const QString expectedPath=qEnvironmentVariable(
                    "XVISION_SEGMENTATION_EXPECTED_LABELS");
        if(modelPath.isEmpty() && preprocessPath.isEmpty()
                && inputPath.isEmpty() && expectedPath.isEmpty())
        {
            QSKIP("Set XVISION_SEGMENTATION_MODEL, "
                  "XVISION_SEGMENTATION_PREPROCESS, XVISION_SEGMENTATION_INPUT, "
                  "and XVISION_SEGMENTATION_EXPECTED_LABELS to run inference");
        }
        QVERIFY2(!modelPath.isEmpty() && !preprocessPath.isEmpty()
                 && !inputPath.isEmpty() && !expectedPath.isEmpty(),
                 "All semantic-segmentation fixture variables are required");

        const QImage input(inputPath);
        const QImage expectedSource(expectedPath);
        QVERIFY2(!input.isNull(),qPrintable(inputPath));
        QVERIFY2(!expectedSource.isNull(),qPrintable(expectedPath));
        QCOMPARE(expectedSource.size(),input.size());
        const QImage expected=expectedSource.convertToFormat(
                    QImage::Format_Grayscale8);
        QVERIFY(!expected.isNull());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("segmentation-fixture.xvproj");
        const QString invalidPath=directory.filePath(
                    "segmentation-fixture-invalid.xvproj");
        const QString fixtureModel=directory.filePath("fixture-model.hdl");
        const QString fixturePreprocess=directory.filePath(
                    "fixture-preprocess.hdict");
        QVERIFY2(QFile::copy(modelPath,fixtureModel),qPrintable(modelPath));
        QVERIFY2(QFile::copy(preprocessPath,fixturePreprocess),
                 qPrintable(preprocessPath));
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Segmentation fixture");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Inference");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto function=qobject_cast<HSemanticSegmentation*>(
                    flow->createXvFunc("HSemanticSegmentation"));
        QVERIFY(function);
        QString error;
        QVERIFY2(function->configureAssets(fixtureModel,fixturePreprocess,error),
                 qPrintable(error));
        const QString acceptedModel=function->modelPath();
        const QString acceptedPreprocess=function->preprocessPath();
        QVERIFY(!function->configureAssets(
                    directory.filePath("missing.hdl"),fixturePreprocess,error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(function->modelPath(),acceptedModel);
        QCOMPARE(function->preprocessPath(),acceptedPreprocess);
        auto inputValue=dynamic_cast<XImage*>(
                    function->getParamsByName("inputImage"));
        QVERIFY(inputValue);
        inputValue->setValue(input);
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Ok);

        auto segmentation=dynamic_cast<XSegmentationResult*>(
                    function->getResultsByName("segmentation"));
        auto colorMask=dynamic_cast<XImage*>(
                    function->getResultsByName("colorMask"));
        auto overlay=dynamic_cast<XImage*>(
                    function->getResultsByName("overlayImage"));
        auto confidence=dynamic_cast<XImage*>(
                    function->getResultsByName("confidenceImage"));
        QVERIFY(segmentation && colorMask && overlay && confidence);
        QCOMPARE(segmentation->width(),input.width());
        QCOMPARE(segmentation->height(),input.height());
        QCOMPARE(colorMask->value().size(),input.size());
        QCOMPARE(overlay->value().size(),input.size());
        QCOMPARE(confidence->value().size(),input.size());
        for(int row=0;row<expected.height();++row)
        {
            const uchar *line=expected.constScanLine(row);
            for(int column=0;column<expected.width();++column)
            {
                QCOMPARE(segmentation->labels().at(
                             row*expected.width()+column),qint32(line[column]));
            }
        }
        const QVector<qint32> baselineLabels=segmentation->labels();
        const QVector<float> baselineConfidences=segmentation->confidences();

        QVERIFY(manager->saveXvProject(path));
        QDomDocument invalid=parseDocument(path);
        QDomElement model=invalid.elementsByTagName("Model").at(0).toElement();
        QVERIFY(!model.isNull());
        model.setAttribute("sha256",QString(64,QLatin1Char('0')));
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,project);

        invalid=parseDocument(path);
        model=invalid.elementsByTagName("Model").at(0).toElement();
        QVERIFY(!model.isNull());
        model.setAttribute("length","01");
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,project);

        invalid=parseDocument(path);
        QDomElement assets=invalid.elementsByTagName(
                    "SemanticSegmentationAssets").at(0).toElement();
        QDomElement preprocess=assets.firstChildElement("Preprocess");
        QVERIFY(!assets.isNull() && !preprocess.isNull());
        assets.appendChild(preprocess.cloneNode(true));
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,project);

        QFile changedPreprocess(fixturePreprocess);
        QVERIFY(changedPreprocess.open(QIODevice::Append));
        QCOMPARE(changedPreprocess.write("x",1),qint64(1));
        changedPreprocess.close();
        expectLoadRejected(path,project);
        QVERIFY(QFile::remove(fixturePreprocess));
        QVERIFY2(QFile::copy(preprocessPath,fixturePreprocess),
                 qPrintable(preprocessPath));

        QVERIFY(manager->loadXvProject(path));
        auto restored=qobject_cast<HSemanticSegmentation*>(findFunctionByRole(
                    manager->getXvProject()->getXvFlow(flowId),
                    "HSemanticSegmentation"));
        QVERIFY(restored);
        auto restoredInput=dynamic_cast<XImage*>(
                    restored->getParamsByName("inputImage"));
        QVERIFY(restoredInput);
        restoredInput->setValue(input);
        QCOMPARE(restored->runXvFunc(),EXvFuncRunStatus::Ok);
        auto restoredSegmentation=dynamic_cast<XSegmentationResult*>(
                    restored->getResultsByName("segmentation"));
        QVERIFY(restoredSegmentation);
        QCOMPARE(restoredSegmentation->labels(),baselineLabels);
        QCOMPARE(restoredSegmentation->confidences(),baselineConfidences);
    }

    void objectDetectionUnconfiguredStateIsStrictAndClearsResults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("detection.xvproj");
        const QString invalidPath=directory.filePath("detection-invalid.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Detection");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Inference");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto function=qobject_cast<HObjectDetection*>(
                    flow->createXvFunc("HObjectDetection"));
        QVERIFY(function);
        QCOMPARE(function->runtime(),HObjectDetection::Cpu);
        QVERIFY(!function->hasConfiguredAssets());

        auto input=dynamic_cast<XImage*>(function->getParamsByName("inputImage"));
        auto minConfidence=dynamic_cast<XReal*>(
                    function->getParamsByName("minConfidence"));
        auto count=dynamic_cast<XInt*>(
                    function->getResultsByName("detectionCount"));
        auto output=dynamic_cast<XImage*>(
                    function->getResultsByName("outputImage"));
        auto detections=dynamic_cast<XObjectList*>(
                    function->getResultsByName("detections"));
        QVERIFY(input && minConfidence && count && output && detections);
        QCOMPARE(detections->valueType(),XDetectionResult::type());
        QImage image(4,3,QImage::Format_RGB888);
        image.fill(Qt::red);
        input->setValue(image);
        count->setValue(1);
        output->setValue(image);
        QVERIFY(detections->addValue(new XDetectionResult(
                    "stale",0.0,0.0,1.0,1.0,0,"stale",0.9)));
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!function->getXvFuncRunMsg().isEmpty());
        QCOMPARE(count->value(),0);
        QVERIFY(output->value().isNull());
        QCOMPARE(detections->count(),qsizetype(0));

        minConfidence->setValue(1.1);
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        minConfidence->setValue(0.5);
        function->setRuntime(static_cast<HObjectDetection::Runtime>(99));
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        function->setRuntime(HObjectDetection::Cpu);
        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(!xml.contains("ObjectDetectionAssets"));
        QVERIFY(!xml.contains("detectionCount"));
        QVERIFY(manager->loadXvProject(path));

        XvProject *restoredProject=manager->getXvProject();
        QVERIFY(restoredProject);
        auto restored=qobject_cast<HObjectDetection*>(findFunctionByRole(
                    restoredProject->getXvFlow(flowId),"HObjectDetection"));
        QVERIFY(restored);
        QVERIFY(!restored->hasConfiguredAssets());

        QDomDocument invalid=parseDocument(path);
        QDomElement detectionFunction=findFunctionElementByRole(
                    invalid,"HObjectDetection");
        QVERIFY(!detectionFunction.isNull());
        QDomElement data=invalid.createElement("PersistentData");
        QDomElement assets=invalid.createElement("ObjectDetectionAssets");
        assets.setAttribute("format","halcon-dl-detection");
        assets.setAttribute("version","1");
        data.appendChild(assets);
        detectionFunction.appendChild(data);
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,restoredProject);
    }

    void objectDetectionFixtureInferenceAndPersistence()
    {
        const QString modelPath=qEnvironmentVariable("XVISION_DETECTION_MODEL");
        const QString preprocessPath=qEnvironmentVariable(
                    "XVISION_DETECTION_PREPROCESS");
        const QString inputPath=qEnvironmentVariable("XVISION_DETECTION_INPUT");
        const QString expectedPath=qEnvironmentVariable(
                    "XVISION_DETECTION_EXPECTED");
        if(modelPath.isEmpty() && preprocessPath.isEmpty()
                && inputPath.isEmpty() && expectedPath.isEmpty())
        {
            QSKIP("Set XVISION_DETECTION_MODEL, XVISION_DETECTION_PREPROCESS, "
                  "XVISION_DETECTION_INPUT, and XVISION_DETECTION_EXPECTED "
                  "to run inference");
        }
        QVERIFY2(!modelPath.isEmpty() && !preprocessPath.isEmpty()
                 && !inputPath.isEmpty() && !expectedPath.isEmpty(),
                 "All object-detection fixture variables are required");

        struct ExpectedDetection
        {
            int classId=0;
            double x=0.0;
            double y=0.0;
            double width=0.0;
            double height=0.0;
            double score=0.0;
        };
        QVector<ExpectedDetection> expected;
        QFile expectedFile(expectedPath);
        QVERIFY2(expectedFile.open(QIODevice::ReadOnly|QIODevice::Text),
                 qPrintable(expectedPath));
        int lineNumber=0;
        while(!expectedFile.atEnd())
        {
            ++lineNumber;
            const QString line=QString::fromUtf8(expectedFile.readLine()).trimmed();
            if(line.isEmpty() || line.startsWith('#')) continue;
            const QStringList fields=line.split(',');
            QVERIFY2(fields.size()==6,
                     qPrintable(QString("invalid expected detection at line %1")
                                .arg(lineNumber)));
            ExpectedDetection item;
            bool ok[6]={false,false,false,false,false,false};
            item.classId=fields.at(0).trimmed().toInt(&ok[0]);
            item.x=fields.at(1).trimmed().toDouble(&ok[1]);
            item.y=fields.at(2).trimmed().toDouble(&ok[2]);
            item.width=fields.at(3).trimmed().toDouble(&ok[3]);
            item.height=fields.at(4).trimmed().toDouble(&ok[4]);
            item.score=fields.at(5).trimmed().toDouble(&ok[5]);
            QVERIFY2(ok[0] && ok[1] && ok[2] && ok[3] && ok[4] && ok[5],
                     qPrintable(QString("non-numeric expected detection at line %1")
                           .arg(lineNumber)));
            expected.append(item);
        }

        const QImage inputImage(inputPath);
        QVERIFY2(!inputImage.isNull(),qPrintable(inputPath));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("detection-fixture.xvproj");
        const QString fixtureModel=directory.filePath("fixture-model.hdl");
        const QString fixturePreprocess=directory.filePath(
                    "fixture-preprocess.hdict");
        QVERIFY2(QFile::copy(modelPath,fixtureModel),qPrintable(modelPath));
        QVERIFY2(QFile::copy(preprocessPath,fixturePreprocess),
                 qPrintable(preprocessPath));

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Detection fixture");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Inference");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto function=qobject_cast<HObjectDetection*>(
                    flow->createXvFunc("HObjectDetection"));
        QVERIFY(function);
        QString error;
        QVERIFY2(function->configureAssets(fixtureModel,fixturePreprocess,error),
                 qPrintable(error));
        const QString acceptedModel=function->modelPath();
        const QString acceptedPreprocess=function->preprocessPath();
        QVERIFY(!function->configureAssets(
                    directory.filePath("missing.hdl"),fixturePreprocess,error));
        QCOMPARE(function->modelPath(),acceptedModel);
        QCOMPARE(function->preprocessPath(),acceptedPreprocess);

        auto input=dynamic_cast<XImage*>(function->getParamsByName("inputImage"));
        auto count=dynamic_cast<XInt*>(
                    function->getResultsByName("detectionCount"));
        auto output=dynamic_cast<XImage*>(
                    function->getResultsByName("outputImage"));
        auto detections=dynamic_cast<XObjectList*>(
                    function->getResultsByName("detections"));
        QVERIFY(input && count && output && detections);
        input->setValue(inputImage);
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(count->value(),int(expected.size()));
        QCOMPARE(output->value().size(),inputImage.size());
        QCOMPARE(detections->count(),qsizetype(expected.size()));
        for(qsizetype index=0;index<detections->count();++index)
        {
            auto actual=dynamic_cast<XDetectionResult*>(detections->value(index));
            QVERIFY(actual);
            const ExpectedDetection &wanted=expected.at(index);
            QCOMPARE(actual->classId(),wanted.classId);
            QVERIFY(qAbs(actual->x()-wanted.x)<=1e-3);
            QVERIFY(qAbs(actual->y()-wanted.y)<=1e-3);
            QVERIFY(qAbs(actual->width()-wanted.width)<=1e-3);
            QVERIFY(qAbs(actual->height()-wanted.height)<=1e-3);
            QVERIFY(qAbs(actual->score()-wanted.score)<=1e-4);
        }

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("ObjectDetectionAssets"));
        QVERIFY(!xml.contains("detectionCount"));

        const QString invalidPath=directory.filePath(
                    "detection-fixture-invalid.xvproj");
        QDomDocument invalid=parseDocument(projectPath);
        QDomElement model=invalid.elementsByTagName("Model").at(0).toElement();
        QVERIFY(!model.isNull());
        model.setAttribute("sha256",QString(64,QLatin1Char('0')));
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,project);

        invalid=parseDocument(projectPath);
        QDomElement assets=invalid.elementsByTagName(
                    "ObjectDetectionAssets").at(0).toElement();
        QDomElement preprocess=assets.firstChildElement("Preprocess");
        QVERIFY(!assets.isNull() && !preprocess.isNull());
        assets.appendChild(preprocess.cloneNode(true));
        QVERIFY(writeDocument(invalidPath,invalid));
        expectLoadRejected(invalidPath,project);

        QFile changedPreprocess(fixturePreprocess);
        QVERIFY(changedPreprocess.open(QIODevice::Append));
        QCOMPARE(changedPreprocess.write("x",1),qint64(1));
        changedPreprocess.close();
        expectLoadRejected(projectPath,project);
        QVERIFY(QFile::remove(fixturePreprocess));
        QVERIFY2(QFile::copy(preprocessPath,fixturePreprocess),
                 qPrintable(preprocessPath));

        QVERIFY(manager->loadXvProject(projectPath));
        auto restored=qobject_cast<HObjectDetection*>(findFunctionByRole(
                    manager->getXvProject()->getXvFlow(flowId),
                    "HObjectDetection"));
        QVERIFY(restored);
        auto restoredInput=dynamic_cast<XImage*>(
                    restored->getParamsByName("inputImage"));
        QVERIFY(restoredInput);
        restoredInput->setValue(inputImage);
        QCOMPARE(restored->runXvFunc(),EXvFuncRunStatus::Ok);
        auto restoredDetections=dynamic_cast<XObjectList*>(
                    restored->getResultsByName("detections"));
        QVERIFY(restoredDetections);
        QCOMPARE(restoredDetections->count(),qsizetype(expected.size()));
        for(qsizetype index=0;index<restoredDetections->count();++index)
        {
            auto actual=dynamic_cast<XDetectionResult*>(
                        restoredDetections->value(index));
            const ExpectedDetection &wanted=expected.at(index);
            QVERIFY(actual);
            QCOMPARE(actual->classId(),wanted.classId);
            QVERIFY(qAbs(actual->score()-wanted.score)<=1e-4);
        }
    }

    void structuredValuesRoundTripAndRejectInvalidXml()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("geometry.xvproj");
        const QString secondPath=directory.filePath("geometry-second.xvproj");
        const QString invalidPath=directory.filePath("geometry-invalid.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Geometry");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Geometry flow");
        QVERIFY(flow);
        XvFunc *function=flow->createXvFunc("GeometryPersistentFunc");
        XvFunc *subscriber=flow->createXvFunc("GeometryPersistentFunc");
        QVERIFY(function);
        QVERIFY(subscriber);
        QVERIFY(function->addSonFunc(subscriber));
        QVERIFY(!subscriber->paramSubscribe("matches",function,"resultPoints"));
        QVERIFY(subscriber->paramSubscribe("matches",function,"resultMatches"));
        QVERIFY(!subscriber->paramSubscribe("matches",function,"resultPoints"));
        QVERIFY(subscriber->paramSubscribe(
                    "detections",function,"resultDetections"));
        QVERIFY(!subscriber->paramSubscribe(
                    "detections",function,"resultMatches"));
        XvFunc::SubscribeInfo originalSubscribeInfo;
        QVERIFY(subscriber->getParamSubscribe("matches",originalSubscribeInfo));
        QCOMPARE(originalSubscribeInfo.first,function);
        QCOMPARE(originalSubscribeInfo.second,QString("resultMatches"));
        const QString flowId=flow->flowId();
        const QString functionId=function->funcId();
        const QString subscriberId=subscriber->funcId();
        auto point=dynamic_cast<XPoint2D*>(function->getParamsByName("point"));
        auto roi=dynamic_cast<XRotateRectRoi*>(function->getParamsByName("roi"));
        auto matches=dynamic_cast<XObjectList*>(function->getParamsByName("matches"));
        auto detections=dynamic_cast<XObjectList*>(
                    function->getParamsByName("detections"));
        auto match=dynamic_cast<XMatchResult*>(function->getResultsByName("match"));
        auto resultMatches=dynamic_cast<XObjectList*>(function->getResultsByName("resultMatches"));
        auto resultDetections=dynamic_cast<XObjectList*>(
                    function->getResultsByName("resultDetections"));
        QVERIFY(point && roi && matches && detections && match && resultMatches
                && resultDetections);
        QVERIFY(point->setValue(1.25,-2.5));
        QVERIFY(roi->setValue(10.0,20.0,30.0,15.0,0.2));
        QVERIFY(matches->addValue(new XMatchResult("",11.0,21.0,0.3,0.95)));
        QVERIFY(matches->addValue(new XMatchResult("",12.0,22.0,0.4,0.85)));
        QVERIFY(match->setValue(13.0,23.0,0.5,0.75));
        QVERIFY(resultMatches->addValue(new XMatchResult("",14.0,24.0,0.6,0.65)));
        QVERIFY(detections->addValue(new XDetectionResult(
                    "",3.0,4.0,20.0,10.0,7,"scratch",0.91)));
        QVERIFY(resultDetections->addValue(new XDetectionResult(
                    "",8.0,9.0,15.0,12.0,8,"dent",0.81)));

        QVERIFY(manager->saveXvProject(path));
        const QByteArray original=readFile(path);
        QVERIFY(original.contains("type=\"XRotateRectRoi\""));
        QVERIFY(original.contains("valueType=\"XMatchResult\""));
        QVERIFY(original.contains("valueType=\"XDetectionResult\""));
        QVERIFY(original.contains("className=\"dent\""));
        // Persisted doubles retain round-trip precision, not a fixed decimal spelling.
        const QDomDocument serializedDocument=parseDocument(path);
        const QDomNodeList serializedItems=serializedDocument.elementsByTagName("Item");
        bool foundExpectedScore=false;
        for(int index=0;index<serializedItems.count();++index)
        {
            bool validScore=false;
            const double score=serializedItems.at(index).toElement()
                    .attribute("score").toDouble(&validScore);
            if(validScore && score==0.95) foundExpectedScore=true;
        }
        QVERIFY(foundExpectedScore);

        QVERIFY(manager->loadXvProject(path));
        XvProject *restoredProject=manager->getXvProject();
        QVERIFY(restoredProject);
        XvFlow *restoredFlow=restoredProject->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        XvFunc *restored=restoredFlow->getXvFunc(functionId);
        XvFunc *restoredSubscriber=restoredFlow->getXvFunc(subscriberId);
        QVERIFY(restored);
        QVERIFY(restoredSubscriber);
        auto restoredRoi=dynamic_cast<XRotateRectRoi*>(restored->getParamsByName("roi"));
        auto restoredMatches=dynamic_cast<XObjectList*>(restored->getParamsByName("matches"));
        auto restoredMatch=dynamic_cast<XMatchResult*>(restored->getResultsByName("match"));
        auto restoredDetections=dynamic_cast<XObjectList*>(
                    restored->getResultsByName("resultDetections"));
        QVERIFY(restoredRoi && restoredMatches && restoredMatch
                && restoredDetections);
        QCOMPARE(restoredRoi->centerY(),20.0);
        QCOMPARE(restoredRoi->length2(),15.0);
        QCOMPARE(restoredMatches->count(),qsizetype(2));
        QCOMPARE(dynamic_cast<XMatchResult*>(restoredMatches->value(0))->score(),0.95);
        QCOMPARE(restoredMatch->angle(),0.5);
        QCOMPARE(restoredDetections->count(),qsizetype(1));
        auto restoredDetection=dynamic_cast<XDetectionResult*>(
                    restoredDetections->value(0));
        QVERIFY(restoredDetection);
        QCOMPARE(restoredDetection->classId(),8);
        QCOMPARE(restoredDetection->className(),QString("dent"));
        XvFunc::SubscribeInfo subscribeInfo;
        QVERIFY(restoredSubscriber->getParamSubscribe("matches",subscribeInfo));
        QCOMPARE(subscribeInfo.first,restored);
        QCOMPARE(subscribeInfo.second,QString("resultMatches"));
        QVERIFY(restoredSubscriber->getParamSubscribe("detections",subscribeInfo));
        QCOMPARE(subscribeInfo.first,restored);
        QCOMPARE(subscribeInfo.second,QString("resultDetections"));
        QVERIFY(restoredSubscriber->updataParam(
                    "matches",restored->getResultsByName("resultMatches")));
        auto subscribedMatches=dynamic_cast<XObjectList*>(
                    restoredSubscriber->getParamsByName("matches"));
        QVERIFY(subscribedMatches);
        QCOMPARE(subscribedMatches->count(),qsizetype(1));
        QVERIFY(subscribedMatches->value(0)
                !=dynamic_cast<XObjectList*>(restored->getResultsByName(
                                                "resultMatches"))->value(0));
        QVERIFY(manager->saveXvProject(secondPath));
        QCOMPARE(readFile(secondPath),original);

        const auto rejectMutation=[&](const std::function<void(QDomDocument&)> &mutate)
        {
            QDomDocument document=parseDocument(path);
            mutate(document);
            QVERIFY(writeDocument(invalidPath,document));
            expectLoadRejected(invalidPath,restoredProject);
        };
        rejectMutation([](QDomDocument &document) {
            const QDomNodeList values=document.elementsByTagName("Value");
            for(int i=0;i<values.count();++i)
            {
                QDomElement value=values.at(i).toElement();
                if(value.attribute("name")=="point") { value.setAttribute("extra","1"); return; }
            }
        });
        rejectMutation([](QDomDocument &document) {
            const QDomNodeList values=document.elementsByTagName("Value");
            for(int i=0;i<values.count();++i)
            {
                QDomElement value=values.at(i).toElement();
                if(value.attribute("name")=="roi") { value.setAttribute("length1","0"); return; }
            }
        });
        rejectMutation([](QDomDocument &document) {
            const QDomNodeList values=document.elementsByTagName("Value");
            for(int i=0;i<values.count();++i)
            {
                QDomElement value=values.at(i).toElement();
                if(value.attribute("name")=="roi") { value.setAttribute("angle","inf"); return; }
            }
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement item=document.elementsByTagName("Item").at(0).toElement();
            item.setAttribute("score","nan");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement item=document.elementsByTagName("Item").at(0).toElement();
            item.setAttribute("score","1.1");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement item=document.elementsByTagName("Item").at(0).toElement();
            item.setAttribute("type","XPoint2D");
        });
        rejectMutation([](QDomDocument &document) {
            const QDomNodeList items=document.elementsByTagName("Item");
            for(int index=0;index<items.count();++index)
            {
                QDomElement item=items.at(index).toElement();
                if(item.attribute("type")==XDetectionResult::type())
                {
                    item.setAttribute("width","0");
                    return;
                }
            }
        });
        rejectMutation([](QDomDocument &document) {
            const QDomNodeList items=document.elementsByTagName("Item");
            for(int index=0;index<items.count();++index)
            {
                QDomElement item=items.at(index).toElement();
                if(item.attribute("type")==XDetectionResult::type())
                {
                    item.setAttribute("classId","08");
                    return;
                }
            }
        });
    }

    void rotateRectDisplayGeometryStaysSynchronized()
    {
        TestRotateRectRoi item(QPointF(0.0,0.0),2.0,3.0,0.0);
        XRotateRectRoi expected(12.0,-3.0,40.0,20.0,0.35);
        QVERIFY(item.setGeometry(expected));
        XRotateRectRoi actual=item.geometry();
        QVERIFY(qAbs(actual.centerX()-expected.centerX())<1e-12);
        QVERIFY(qAbs(actual.centerY()-expected.centerY())<1e-12);
        QVERIFY(qAbs(actual.length1()-expected.length1())<1e-12);
        QVERIFY(qAbs(actual.angle()-expected.angle())<1e-12);
        QVERIFY(item.boundingRect().width()>expected.length1()*2.0);
        QVERIFY(item.boundingRect().height()>expected.length2()*2.0);

        QVERIFY(item.moveForTest(QPointF(5.0,7.0)));
        actual=item.geometry();
        QVERIFY(qAbs(actual.centerX()-17.0)<1e-12);
        QVERIFY(qAbs(actual.centerY()-4.0)<1e-12);

        QVERIFY(item.dragLength1Handle(QPointF(17.0,-21.0)));
        actual=item.geometry();
        QVERIFY(qAbs(actual.length1()-25.0)<1e-12);
        QVERIFY(qAbs(actual.angle()-1.5707963267948966)<1e-12);
    }

    void halconImageInteropRoundTripsPixelsAndOwnsMemory()
    {
        HalconCpp::HImage grayHalcon;
        {
            QByteArray storage(10,'\0');
            storage[0]=char(1); storage[1]=char(2); storage[2]=char(3);
            storage[5]=char(4); storage[6]=char(5); storage[7]=char(6);
            QImage gray(reinterpret_cast<uchar*>(storage.data()),3,2,5,
                        QImage::Format_Grayscale8);
            QString error;
            QVERIFY2(XvHalconImageInterop::toHalcon(gray,grayHalcon,&error),
                     qPrintable(error));
        }
        QImage grayResult;
        QString error;
        QVERIFY2(XvHalconImageInterop::toQImage(grayHalcon,grayResult,&error),
                 qPrintable(error));
        QCOMPARE(grayResult.format(),QImage::Format_Grayscale8);
        QCOMPARE(grayResult.constScanLine(0)[0],uchar(1));
        QCOMPARE(grayResult.constScanLine(1)[2],uchar(6));

        const QList<QImage::Format> formats={QImage::Format_RGB888,QImage::Format_RGB32,
                                             QImage::Format_RGBA8888,
                                             QImage::Format_ARGB32};
        for(QImage::Format format:formats)
        {
            HalconCpp::HImage halcon;
            {
                QImage source(3,2,format);
                QVERIFY(!source.isNull());
                source.setPixelColor(0,0,QColor(10,20,30,40));
                source.setPixelColor(1,0,QColor(50,60,70,80));
                source.setPixelColor(2,1,QColor(90,100,110,120));
                QVERIFY2(XvHalconImageInterop::toHalcon(source,halcon,&error),
                         qPrintable(error));
            }
            QImage result;
            QVERIFY2(XvHalconImageInterop::toQImage(halcon,result,&error),
                     qPrintable(error));
            QCOMPARE(result.format(),QImage::Format_RGB888);
            QCOMPARE(result.pixelColor(0,0),QColor(10,20,30));
            QCOMPARE(result.pixelColor(1,0),QColor(50,60,70));
            QCOMPARE(result.pixelColor(2,1),QColor(90,100,110));
        }

        QByteArray rgbStorage(16,'\0');
        uchar *rgb=reinterpret_cast<uchar*>(rgbStorage.data());
        rgb[0]=1; rgb[1]=2; rgb[2]=3; rgb[3]=4; rgb[4]=5; rgb[5]=6;
        rgb[8]=7; rgb[9]=8; rgb[10]=9; rgb[11]=10; rgb[12]=11; rgb[13]=12;
        QImage strided(rgb,2,2,8,QImage::Format_RGB888);
        HalconCpp::HImage stridedHalcon;
        QVERIFY2(XvHalconImageInterop::toHalcon(strided,stridedHalcon,&error),
                 qPrintable(error));
        QImage stridedResult;
        QVERIFY2(XvHalconImageInterop::toQImage(stridedHalcon,stridedResult,&error),
                 qPrintable(error));
        QCOMPARE(stridedResult.pixelColor(1,1),QColor(10,11,12));

        QImage unsupported(8,8,QImage::Format_Mono);
        QVERIFY(!XvHalconImageInterop::toHalcon(unsupported,stridedHalcon,&error));
        QVERIFY(!error.isEmpty());
        HalconCpp::HImage empty;
        QVERIFY(!XvHalconImageInterop::toQImage(empty,stridedResult,&error));
        QVERIFY(!error.isEmpty());

        QVector<qint32> labelStorage={0,1,2,3,-1,65536};
        HalconCpp::HImage labelImage("int4",3,2,labelStorage.data());
        QVector<qint32> labels={99};
        int width=-1;
        int height=-1;
        QVERIFY2(XvHalconImageInterop::toInt32Pixels(
                     labelImage,labels,width,height,&error),qPrintable(error));
        QCOMPARE(width,3);
        QCOMPARE(height,2);
        QCOMPARE(labels,labelStorage);

        QVector<float> confidenceStorage={0.0f,0.2f,0.4f,0.6f,0.8f,1.0f};
        HalconCpp::HImage confidenceImage("real",3,2,confidenceStorage.data());
        QVector<float> confidences={9.0f};
        QVERIFY2(XvHalconImageInterop::toFloatPixels(
                     confidenceImage,confidences,width,height,&error),qPrintable(error));
        QCOMPARE(confidences,confidenceStorage);

        QVector<float> invalidConfidence=confidenceStorage;
        invalidConfidence[2]=qQNaN();
        HalconCpp::HImage invalidConfidenceImage(
                    "real",3,2,invalidConfidence.data());
        const QVector<float> previousConfidences=confidences;
        QVERIFY(!XvHalconImageInterop::toFloatPixels(
                    invalidConfidenceImage,confidences,width,height,&error));
        QCOMPARE(confidences,previousConfidences);
        QCOMPARE(width,3);
        QCOMPARE(height,2);

        QVERIFY(!XvHalconImageInterop::toInt32Pixels(
                    confidenceImage,labels,width,height,&error));
        QCOMPARE(labels,labelStorage);

        QVector<qint64> overflowStorage={
            std::numeric_limits<qint64>::max()
        };
        HalconCpp::HImage overflowImage("int8",1,1,overflowStorage.data());
        QVERIFY(!XvHalconImageInterop::toInt32Pixels(
                    overflowImage,labels,width,height,&error));
        QCOMPARE(labels,labelStorage);
        QCOMPARE(width,3);
        QCOMPARE(height,2);
    }

    void halconModelMatchCreatesFindsAndPersistsAsset()
    {
        QImage image(240,180,QImage::Format_RGB888);
        image.fill(Qt::black);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing,false);
            painter.setPen(QPen(Qt::white,6));
            painter.drawRect(QRect(70,45,72,58));
            painter.drawLine(QPoint(70,45),QPoint(142,103));
            painter.drawLine(QPoint(92,103),QPoint(142,72));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QPoint(118,63),7,7);
        }

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("model.xvproj");
        const QString flowPath=directory.filePath("model.xvflow");
        const QString invalidPath=directory.filePath("model-invalid.xvproj");
        const QString imagePath=directory.filePath("model-source.png");
        QVERIFY(image.save(imagePath));

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Model match");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Match flow");
        QVERIFY(flow);
        auto acquisition=qobject_cast<ImageAcquisition*>(
                    flow->createXvFunc("ImageAcquisition"));
        auto matcher=qobject_cast<HModelMatch*>(flow->createXvFunc("HModelMatch"));
        auto logOutput=qobject_cast<LogOutput*>(flow->createXvFunc("LogOutput"));
        QVERIFY(acquisition && matcher && logOutput);
        acquisition->setAcqType(ImageAcquisition::File);
        acquisition->setLocalFile(imagePath);
        acquisition->setCanvasPosition(QPointF(20.0,40.0));
        matcher->setCanvasPosition(QPointF(260.0,40.0));
        logOutput->setCanvasPosition(QPointF(500.0,40.0));
        dynamic_cast<XString*>(logOutput->getParamsByName("outputMsg"))
                ->setValue("M1 offline match completed");
        flow->getFlowConfig()->funcErrorInterruptRun=true;
        const QString flowId=flow->flowId();
        const QString matcherId=matcher->funcId();

        auto input=dynamic_cast<XImage*>(matcher->getParamsByName("inputImage"));
        auto roi=dynamic_cast<XRotateRectRoi*>(matcher->getParamsByName("templateRoi"));
        auto useRoi=dynamic_cast<XBool*>(matcher->getParamsByName("useTemplateRoi"));
        auto mode=dynamic_cast<XInt*>(matcher->getParamsByName("mode"));
        auto minScore=dynamic_cast<XReal*>(matcher->getParamsByName("minScore"));
        auto count=dynamic_cast<XInt*>(matcher->getResultsByName("matchCount"));
        auto matches=dynamic_cast<XObjectList*>(matcher->getResultsByName("matches"));
        QVERIFY(input && roi && useRoi && mode && minScore && count && matches);
        input->setValue(image);
        QVERIFY(roi->setValue(106.0,74.0,45.0,38.0,0.0));
        minScore->setValue(0.2);
        mode->setValue(HModelMatch::CreateTemplate);
        useRoi->setValue(false);
        QCOMPARE(matcher->runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY(matcher->hasTemplateModel());
        useRoi->setValue(true);
        QCOMPARE(matcher->runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY2(matcher->hasTemplateModel(),qPrintable(matcher->getXvFuncRunMsg()));
        const QByteArray modelAsset=matcher->templateModelAsset();
        QVERIFY(!modelAsset.isEmpty());

        mode->setValue(HModelMatch::FindTemplate);
        QCOMPARE(matcher->runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY2(count->value()>=1,qPrintable(matcher->getXvFuncRunMsg()));
        QCOMPARE(matches->count(),qsizetype(count->value()));
        auto firstMatch=dynamic_cast<XMatchResult*>(matches->value(0));
        QVERIFY(firstMatch);
        QVERIFY(firstMatch->x()>=0.0 && firstMatch->x()<image.width());
        QVERIFY(firstMatch->y()>=0.0 && firstMatch->y()<image.height());
        QVERIFY(firstMatch->score()>=minScore->value());

        const int previousCount=count->value();
        const double previousScore=firstMatch->score();
        minScore->setValue(2.0);
        QCOMPARE(matcher->runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(count->value(),previousCount);
        QCOMPARE(dynamic_cast<XMatchResult*>(matches->value(0))->score(),previousScore);
        QCOMPARE(matcher->templateModelAsset(),modelAsset);
        minScore->setValue(0.2);

        QVERIFY(acquisition->addSonFunc(matcher));
        QVERIFY(matcher->addSonFunc(logOutput));
        QVERIFY(matcher->paramSubscribe("inputImage",acquisition,"outputImage"));
        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QVERIFY2(flow->getXvFuncRunStatus()==EXvFlowRunStatus::Ok,
                 qPrintable(QString("acquisition: %1; matcher: %2; log: %3")
                            .arg(acquisition->getXvFuncRunMsg(),matcher->getXvFuncRunMsg(),
                                 logOutput->getXvFuncRunMsg())));
        QVERIFY(count->value()>=1);
        QCOMPARE(logOutput->getXvFuncRunStatus(),EXvFuncRunStatus::Ok);

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("<PersistentData>"));
        QVERIFY(xml.contains("format=\"halcon-shape-model\""));
        QVERIFY(xml.contains("encoding=\"base64\""));
        QVERIFY(manager->loadXvProject(projectPath));
        XvProject *restoredProject=manager->getXvProject();
        QVERIFY(restoredProject);
        XvFlow *restoredFlow=restoredProject->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        auto restoredMatcher=qobject_cast<HModelMatch*>(
                    restoredFlow->getXvFunc(matcherId));
        auto restoredAcquisition=qobject_cast<ImageAcquisition*>(
                    findFunctionByRole(restoredFlow,"ImageAcquisition"));
        QVERIFY(restoredMatcher);
        QVERIFY(restoredAcquisition);
        QVERIFY(restoredMatcher->hasTemplateModel());
        QCOMPARE(restoredMatcher->templateModelAsset(),modelAsset);
        QCOMPARE(restoredAcquisition->localFile(),imagePath);
        QCOMPARE(restoredAcquisition->canvasPosition(),QPointF(20.0,40.0));
        QCOMPARE(restoredMatcher->canvasPosition(),QPointF(260.0,40.0));
        auto restoredLog=qobject_cast<LogOutput*>(
                    findFunctionByRole(restoredFlow,"LogOutput"));
        QVERIFY(restoredLog);
        QCOMPARE(restoredLog->canvasPosition(),QPointF(500.0,40.0));
        XvFunc::SubscribeInfo restoredSubscription;
        QVERIFY(restoredMatcher->getParamSubscribe("inputImage",
                                                   restoredSubscription));
        QCOMPARE(restoredSubscription.first,
                 static_cast<XvFunc*>(restoredAcquisition));
        QCOMPARE(restoredFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!restoredFlow->isRunning(),5000);
        QVERIFY(dynamic_cast<XInt*>(restoredMatcher->getResultsByName("matchCount"))->value()>=1);

        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        XvFlow *firstImported=manager->importXvFlow(flowPath);
        XvFlow *secondImported=manager->importXvFlow(flowPath);
        QVERIFY(firstImported && secondImported);
        QVERIFY(firstImported->flowId()!=secondImported->flowId());
        for(XvFlow *imported:{firstImported,secondImported})
        {
            auto importedMatcher=qobject_cast<HModelMatch*>(
                        findFunctionByRole(imported,"HModelMatch"));
            auto importedAcquisition=qobject_cast<ImageAcquisition*>(
                        findFunctionByRole(imported,"ImageAcquisition"));
            QVERIFY(importedMatcher && importedAcquisition);
            QVERIFY(importedMatcher->hasTemplateModel());
            QCOMPARE(importedMatcher->templateModelAsset(),modelAsset);
            QCOMPARE(importedAcquisition->localFile(),imagePath);
            QCOMPARE(importedAcquisition->canvasPosition(),QPointF(20.0,40.0));
            QCOMPARE(importedMatcher->canvasPosition(),QPointF(260.0,40.0));
            QCOMPARE(imported->runOnce(),Ret_Xv_Success);
            QTRY_VERIFY_WITH_TIMEOUT(!imported->isRunning(),5000);
            QCOMPARE(imported->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
            QVERIFY(dynamic_cast<XInt*>(
                        importedMatcher->getResultsByName("matchCount"))->value()>=1);
        }

        const auto rejectMutation=[&](const std::function<void(QDomDocument&)> &mutate)
        {
            QDomDocument document=parseDocument(projectPath);
            mutate(document);
            QVERIFY(writeDocument(invalidPath,document));
            expectLoadRejected(invalidPath,restoredProject);
        };
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("ShapeModel").at(0).toElement()
                    .setAttribute("version","2");
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("ShapeModel").at(0).toElement()
                    .setAttribute("encoding","hex");
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("ShapeModel").at(0).toElement()
                    .setAttribute("length","1");
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("ShapeModel").at(0).toElement()
                    .setAttribute("length","16777217");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement model=document.elementsByTagName("ShapeModel").at(0).toElement();
            model.firstChild().setNodeValue("@@@@");
            model.setAttribute("length","3");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement model=document.elementsByTagName("ShapeModel").at(0).toElement();
            const QByteArray encoded=model.text().toLatin1();
            QByteArray damaged(QByteArray::fromBase64(encoded).size(),'\0');
            model.firstChild().setNodeValue(QString::fromLatin1(damaged.toBase64()));
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement data=document.elementsByTagName("PersistentData").at(0).toElement();
            data.removeChild(data.firstChildElement("ShapeModel"));
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement function=ProjectXmlTest::findFunctionElementByRole(
                        document,"HModelMatch");
            QDomElement data=function.firstChildElement("PersistentData");
            function.appendChild(data.cloneNode(true));
        });

        QDomDocument legacy=parseDocument(projectPath);
        QDomElement legacyFunction=findFunctionElementByRole(legacy,"HModelMatch");
        QVERIFY(!legacyFunction.isNull());
        legacyFunction.removeChild(legacyFunction.firstChildElement("PersistentData"));
        QDomElement parameters=legacyFunction.firstChildElement("Parameters");
        while(!parameters.firstChild().isNull()) parameters.removeChild(parameters.firstChild());
        QVERIFY(writeDocument(invalidPath,legacy));
        QVERIFY(manager->loadXvProject(invalidPath));
        auto legacyMatcher=qobject_cast<HModelMatch*>(findFunctionByRole(
                    manager->getXvProject()->getXvFlow(flowId),"HModelMatch"));
        QVERIFY(legacyMatcher);
        QVERIFY(!legacyMatcher->hasTemplateModel());

        HModelMatch emptyMatcher;
        QCOMPARE(emptyMatcher.runXvFunc(),EXvFuncRunStatus::Error);
        dynamic_cast<XImage*>(emptyMatcher.getParamsByName("inputImage"))->setValue(image);
        dynamic_cast<XInt*>(emptyMatcher.getParamsByName("mode"))->setValue(
                    HModelMatch::FindTemplate);
        QCOMPARE(emptyMatcher.runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(!emptyMatcher.getXvFuncRunMsg().isEmpty());
    }

    void allCurrentRolesCanRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("all-roles.xvproj");
        const QStringList roles={
            "ImageAcquisition","BaseDataWriter","BaseDataBoolCalc",
            "BaseDataIntCalc","BaseDataRealCalc","BaseDataStringProcess",
            "HModelMatch","HObjectDetection","HSemanticSegmentation",
            "NInference","NClassification","NObjectDetection",
            "NSemanticSegmentation",
            "ConditionalFlow","LoopFlow","Delayer","ElapsedTimer",
            "LogOutput","NotificationOutput","OImageArithmetic","OImageFilter","OImageColor",
            "OImageThreshold","OImageTransform","OImageAnalysis",
            "OImageModel","OImageComposition","OMorphology","ORegionDetector",
            "OCodeDetector","OPointFeature","OCascadeDetector",
            "OTemplateMatch","ORectification","GeometryCreate","GeometryMeasure",
            "DetectRecord","HttpJson","TcpText","UdpText","SerialData","ModbusRegister"
        };

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("All roles");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Operators");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        QMap<QString,QString> ids;
        QMap<QString,XvFunc*> functions;
        QMap<QString,int> modes;
        for(const QString &role:roles)
        {
            XvFunc *function=flow->createXvFunc(role);
            QVERIFY2(function,qPrintable(role));
            ids.insert(role,function->funcId());
            functions.insert(role,function);
            const int modeIndex=function->metaObject()->indexOfProperty("mode");
            if(modeIndex>=0)
            {
                const QMetaProperty property=function->metaObject()->property(modeIndex);
                const QMetaEnum values=property.enumerator();
                QVERIFY(values.keyCount()>0);
                QVERIFY(property.write(function,values.value(values.keyCount()-1)));
                modes.insert(role,property.read(function).toInt());
            }
        }
        dynamic_cast<XInt*>(functions.value("OMorphology")->getParamsByName(
                                "kernelWidth"))->setValue(5);
        dynamic_cast<XReal*>(functions.value("ORegionDetector")->getParamsByName(
                                 "targetAngle"))->setValue(37.5);
        dynamic_cast<XInt*>(functions.value("OPointFeature")->getParamsByName(
                                "starMaxSize"))->setValue(65);
        dynamic_cast<XString*>(functions.value("OCascadeDetector")->getParamsByName(
                                   "cascadePath"))->setValue("/tmp/lbp-cascade.xml");
        auto acquisition=qobject_cast<ImageAcquisition*>(functions.value("ImageAcquisition"));
        QVERIFY(acquisition);
        acquisition->setVideoPath("/tmp/source.avi");
        acquisition->setVideoStartFrame(3);
        acquisition->setVideoEndFrame(17);
        acquisition->setVideoFrameStep(2);
        acquisition->setVideoLoop(true);
        dynamic_cast<XString*>(functions.value("NotificationOutput")->getParamsByName(
                                   "message"))->setValue("inspection complete");
        QVERIFY(functions.value("ElapsedTimer")->addSonFunc(
                    functions.value("LogOutput")));

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(!xml.contains("outputImage"));
        QVERIFY(!xml.contains("inputImage"));
        QVERIFY(!xml.contains("elapsedResult"));
        QVERIFY(!xml.contains("runStatus"));
        QVERIFY(!xml.contains("name=\"keyPoints\""));
        QVERIFY(!xml.contains("name=\"contours\""));
        QVERIFY(!xml.contains("name=\"circles\""));
        QVERIFY(!xml.contains("name=\"regions\""));
        QVERIFY(!xml.contains("name=\"rectangles\""));
        QVERIFY(!xml.contains("name=\"lines\""));
        QVERIFY(!xml.contains("name=\"corners\""));
        QVERIFY(!xml.contains("name=\"text\""));
        QVERIFY(!xml.contains("name=\"count\""));
        QVERIFY(!xml.contains("name=\"published\""));
        QVERIFY(!xml.contains("name=\"accepted\""));
        QVERIFY(!xml.contains("name=\"publishedMessage\""));
        QVERIFY(!xml.contains("name=\"videoEndOfStream\""));
        QVERIFY(!xml.contains("name=\"outputs\""));
        QVERIFY(!xml.contains("name=\"outputCount\""));
        QVERIFY(!xml.contains("name=\"age\""));
        QVERIFY(!xml.contains("name=\"ageValid\""));
        QVERIFY(!xml.contains("name=\"classifications\""));
        QVERIFY(!xml.contains("name=\"classificationCount\""));
        QVERIFY(!xml.contains("name=\"detections\""));
        QVERIFY(!xml.contains("name=\"detectionCount\""));
        QVERIFY(!xml.contains("name=\"segmentation\""));
        QVERIFY(!xml.contains("name=\"latestRecord\""));
        QVERIFY(!xml.contains("name=\"recordCount\""));
        QVERIFY(!xml.contains("name=\"insertedRecordId\""));
        QVERIFY(!xml.contains("name=\"colorMask\""));
        QVERIFY(!xml.contains("name=\"overlayImage\""));
        QVERIFY(!xml.contains("name=\"confidenceImage\""));
        QVERIFY(!xml.contains("name=\"outputs\""));
        QVERIFY(!xml.contains("name=\"classifications\""));
        QVERIFY(!xml.contains("name=\"detections\""));
        QVERIFY(!xml.contains("name=\"segmentation\""));
        QVERIFY(manager->loadXvProject(path));
        XvFlow *restored=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restored);
        QCOMPARE(restored->xvFuncCount(),roles.count());
        for(const QString &role:roles)
        {
            XvFunc *function=restored->getXvFunc(ids.value(role));
            QVERIFY2(function,qPrintable(role));
            QCOMPARE(function->funcRole(),role);
            if(modes.contains(role)) QCOMPARE(function->property("mode").toInt(),modes.value(role));
        }
        QCOMPARE(dynamic_cast<XInt*>(restored->getXvFunc(ids.value("OMorphology"))
                                    ->getParamsByName("kernelWidth"))->value(),5);
        QCOMPARE(dynamic_cast<XReal*>(restored->getXvFunc(ids.value("ORegionDetector"))
                                     ->getParamsByName("targetAngle"))->value(),37.5);
        QCOMPARE(dynamic_cast<XInt*>(restored->getXvFunc(ids.value("OPointFeature"))
                                    ->getParamsByName("starMaxSize"))->value(),65);
        QCOMPARE(dynamic_cast<XString*>(restored->getXvFunc(ids.value("OCascadeDetector"))
                                       ->getParamsByName("cascadePath"))->value(),
                 QString("/tmp/lbp-cascade.xml"));
        auto restoredAcquisition=qobject_cast<ImageAcquisition*>(
                    restored->getXvFunc(ids.value("ImageAcquisition")));
        QVERIFY(restoredAcquisition);
        QCOMPARE(restoredAcquisition->videoPath(),QString("/tmp/source.avi"));
        QCOMPARE(restoredAcquisition->videoStartFrame(),3);
        QCOMPARE(restoredAcquisition->videoEndFrame(),17);
        QCOMPARE(restoredAcquisition->videoFrameStep(),2);
        QVERIFY(restoredAcquisition->videoLoop());
        QCOMPARE(dynamic_cast<XString*>(restored->getXvFunc(ids.value("NotificationOutput"))
                                       ->getParamsByName("message"))->value(),
                 QString("inspection complete"));
        QVERIFY(restored->getXvFunc(ids.value("ElapsedTimer"))->existSonFunc(
                    restored->getXvFunc(ids.value("LogOutput"))));
    }

    void presetAliasPersistsCanonicalRole()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("preset-alias.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Preset alias");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Acquisition");
        QVERIFY(flow);
        const QString flowId=flow->flowId();

        auto acquisition=qobject_cast<ImageAcquisition*>(
                    flow->createXvFunc("SrcImageFilesNodeData"));
        QVERIFY2(acquisition,qPrintable(flow->lastErrorMsg()));
        const QString functionId=acquisition->funcId();
        QCOMPARE(acquisition->funcRole(),QString("ImageAcquisition"));
        QCOMPARE(acquisition->acqType(),ImageAcquisition::Dir);

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(xml.contains("role=\"ImageAcquisition\""));
        QVERIFY(!xml.contains("SrcImageFilesNodeData"));

        QVERIFY(manager->loadXvProject(path));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        auto restored=qobject_cast<ImageAcquisition*>(
                    restoredFlow->getXvFunc(functionId));
        QVERIFY(restored);
        QCOMPARE(restored->funcRole(),QString("ImageAcquisition"));
        QCOMPARE(restored->acqType(),ImageAcquisition::Dir);
    }

    void videoAndNotificationPresetsPersistCanonicalContracts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("source-notification.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Source notification aliases");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Aliases");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto video=qobject_cast<ImageAcquisition*>(
                    flow->createXvFunc("SrcVideoFilesNodeData"));
        auto notification=qobject_cast<NotificationOutput*>(
                    flow->createXvFunc("ShowDialogNotifyMessageOutputNodeData"));
        QVERIFY2(video,qPrintable(flow->lastErrorMsg()));
        QVERIFY2(notification,qPrintable(flow->lastErrorMsg()));
        QCOMPARE(video->funcRole(),QString("ImageAcquisition"));
        QCOMPARE(video->acqType(),ImageAcquisition::Video);
        QCOMPARE(notification->funcRole(),QString("NotificationOutput"));
        QCOMPARE(notification->mode(),NotificationOutput::Dialog);
        video->setVideoPath("/tmp/alias-video.avi");
        video->setVideoStartFrame(4);
        video->setVideoEndFrame(14);
        video->setVideoFrameStep(3);
        video->setVideoLoop(true);
        dynamic_cast<XString*>(notification->getParamsByName("message"))
                ->setValue("show inspection summary");
        const QString videoId=video->funcId();
        const QString notificationId=notification->funcId();

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(xml.contains("role=\"ImageAcquisition\""));
        QVERIFY(xml.contains("role=\"NotificationOutput\""));
        QVERIFY(!xml.contains("SrcVideoFilesNodeData"));
        QVERIFY(!xml.contains("ShowDialogNotifyMessageOutputNodeData"));
        QVERIFY(!xml.contains("videoEndOfStream"));
        QVERIFY(!xml.contains("publishedMessage"));

        QVERIFY(manager->loadXvProject(path));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        auto restoredVideo=qobject_cast<ImageAcquisition*>(
                    restoredFlow->getXvFunc(videoId));
        auto restoredNotification=qobject_cast<NotificationOutput*>(
                    restoredFlow->getXvFunc(notificationId));
        QVERIFY(restoredVideo && restoredNotification);
        QCOMPARE(restoredVideo->acqType(),ImageAcquisition::Video);
        QCOMPARE(restoredVideo->videoPath(),QString("/tmp/alias-video.avi"));
        QCOMPARE(restoredVideo->videoStartFrame(),4);
        QCOMPARE(restoredVideo->videoEndFrame(),14);
        QCOMPARE(restoredVideo->videoFrameStep(),3);
        QVERIFY(restoredVideo->videoLoop());
        QCOMPARE(restoredNotification->mode(),NotificationOutput::Dialog);
        QCOMPARE(dynamic_cast<XString*>(restoredNotification->getParamsByName("message"))
                 ->value(),QString("show inspection summary"));
    }

    void onnxPresetsPersistCanonicalRolesAndModes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("onnx-aliases.xvproj");
        const QList<QPair<QString,QPair<QString,int>>> aliases={
            {"InferOnnxNodeData",{"NInference",NInference::Generic}},
            {"AgeInferOnnxNodeData",{"NInference",NInference::Age}},
            {"ClsOnnxNodeData",{"NClassification",NClassification::Generic}},
            {"GenderClsOnnxNodeData",{"NClassification",NClassification::Gender}},
            {"ObjDetectOnnxNodeData",{"NObjectDetection",NObjectDetection::Generic}},
            {"Yolov3",{"NObjectDetection",NObjectDetection::Yolov3}},
            {"Yolov5OnnxNodeData",{"NObjectDetection",NObjectDetection::Yolov5}},
            {"Yolov5FaceOnnxNodeData",{"NObjectDetection",NObjectDetection::Yolov5Face}},
            {"SemSegOnnxNodeData",{"NSemanticSegmentation",NSemanticSegmentation::Generic}},
            {"HumanSemSegOnnxNodeData",{"NSemanticSegmentation",NSemanticSegmentation::Human}}
        };
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("ONNX aliases");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Aliases");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        QMap<QString,QPair<QString,int>> expectedById;
        for(const auto &alias:aliases)
        {
            XvFunc *function=flow->createXvFunc(alias.first);
            QVERIFY2(function,qPrintable(flow->lastErrorMsg()));
            QCOMPARE(function->funcRole(),alias.second.first);
            QCOMPARE(function->property("mode").toInt(),alias.second.second);
            expectedById.insert(function->funcId(),alias.second);
        }
        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        for(const auto &alias:aliases) QVERIFY(!xml.contains(alias.first.toUtf8()));
        for(const QString &role:{QString("NInference"),QString("NClassification"),
                                 QString("NObjectDetection"),
                                 QString("NSemanticSegmentation")})
            QVERIFY(xml.contains(QString("role=\"%1\"").arg(role).toUtf8()));
        QVERIFY(manager->loadXvProject(path));
        XvFlow *restored=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restored);
        for(auto iterator=expectedById.cbegin();iterator!=expectedById.cend();++iterator)
        {
            XvFunc *function=restored->getXvFunc(iterator.key());
            QVERIFY(function);
            QCOMPARE(function->funcRole(),iterator.value().first);
            QCOMPARE(function->property("mode").toInt(),iterator.value().second);
        }
    }

    void geometryPresetsAndParametersPersist()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("geometry.xvproj");
        const QString flowPath=directory.filePath("geometry.xvflow");
        const QList<QPair<QString,int>> measureAliases={
            {"CircleToCircleMesauseNodeData",GeometryMeasure::CircleCircle},
            {"LineToCircleMesauseNodeData",GeometryMeasure::LineCircle},
            {"LineToLineAngleMesauseNodeData",GeometryMeasure::LineLineAngle},
            {"LineToLineMesauseNodeData",GeometryMeasure::LineLine},
            {"PointToCircleMesauseNodeData",GeometryMeasure::PointCircle},
            {"PointToLineMesauseNodeData",GeometryMeasure::PointLine},
            {"PointToPointMesauseNodeData",GeometryMeasure::PointPoint}
        };

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Geometry operators");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Measurement");
        QVERIFY(flow);
        const QString flowId=flow->flowId();

        auto creator=qobject_cast<GeometryCreate*>(flow->createXvFunc("CreateShapeNodeData"));
        QVERIFY2(creator,qPrintable(flow->lastErrorMsg()));
        QCOMPARE(creator->funcRole(),QString("GeometryCreate"));
        creator->setShapeType(GeometryCreate::Rectangle);
        dynamic_cast<XReal*>(creator->getParamsByName("rectangleX"))->setValue(4.0);
        dynamic_cast<XReal*>(creator->getParamsByName("rectangleY"))->setValue(7.0);
        dynamic_cast<XReal*>(creator->getParamsByName("rectangleWidth"))->setValue(32.0);
        dynamic_cast<XReal*>(creator->getParamsByName("rectangleHeight"))->setValue(18.0);
        QCOMPARE(creator->runXvFunc(),EXvFuncRunStatus::Ok);
        const QString creatorId=creator->funcId();

        QMap<QString,int> expectedModes;
        GeometryMeasure *pointLine=nullptr;
        for(const auto &entry:measureAliases)
        {
            auto measure=qobject_cast<GeometryMeasure*>(flow->createXvFunc(entry.first));
            QVERIFY2(measure,qPrintable(flow->lastErrorMsg()));
            QCOMPARE(measure->funcRole(),QString("GeometryMeasure"));
            QCOMPARE(measure->mode(),GeometryMeasure::Mode(entry.second));
            expectedModes.insert(measure->funcId(),entry.second);
            if(entry.second==GeometryMeasure::PointLine) pointLine=measure;
        }
        QVERIFY(pointLine);
        const QString pointLineId=pointLine->funcId();
        QVERIFY(dynamic_cast<XPoint2D*>(pointLine->getParamsByName("point1"))
                ->setValue(12.0,-6.0));
        QVERIFY(dynamic_cast<XLine2D*>(pointLine->getParamsByName("line1"))
                ->setValue(QPointF(0.0,0.0),QPointF(20.0,0.0)));
        dynamic_cast<XBool*>(pointLine->getParamsByName("absoluteValue"))->setValue(false);
        dynamic_cast<XReal*>(pointLine->getParamsByName("scale"))->setValue(0.25);
        dynamic_cast<XString*>(pointLine->getParamsByName("unit"))->setValue("mm");
        dynamic_cast<XReal*>(pointLine->getParamsByName("lowerLimit"))->setValue(-2.0);
        dynamic_cast<XReal*>(pointLine->getParamsByName("upperLimit"))->setValue(-1.0);
        QCOMPARE(pointLine->runXvFunc(),EXvFuncRunStatus::Ok);

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("role=\"GeometryCreate\""));
        QVERIFY(xml.contains("role=\"GeometryMeasure\""));
        QVERIFY(xml.contains("name=\"line1\""));
        QVERIFY(xml.contains("type=\"XLine2D\""));
        QVERIFY(xml.contains("name=\"point1\""));
        QVERIFY(xml.contains("type=\"XPoint2D\""));
        QVERIFY(!xml.contains("createdType"));
        QVERIFY(!xml.contains("annotationStart"));
        QVERIFY(!xml.contains("measurement\""));
        QVERIFY(!xml.contains("rawValue"));
        for(const auto &entry:measureAliases) QVERIFY(!xml.contains(entry.first.toUtf8()));
        QVERIFY(!xml.contains("CreateShapeNodeData"));

        QVERIFY(manager->loadXvProject(projectPath));
        XvFlow *restored=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restored);
        auto restoredCreator=qobject_cast<GeometryCreate*>(restored->getXvFunc(creatorId));
        QVERIFY(restoredCreator);
        QCOMPARE(restoredCreator->shapeType(),GeometryCreate::Rectangle);
        QCOMPARE(dynamic_cast<XReal*>(restoredCreator->getParamsByName("rectangleWidth"))
                 ->value(),32.0);
        for(auto iterator=expectedModes.cbegin();iterator!=expectedModes.cend();++iterator)
        {
            auto measure=qobject_cast<GeometryMeasure*>(restored->getXvFunc(iterator.key()));
            QVERIFY(measure);
            QCOMPARE(measure->mode(),GeometryMeasure::Mode(iterator.value()));
        }
        auto restoredPointLine=qobject_cast<GeometryMeasure*>(restored->getXvFunc(pointLineId));
        QVERIFY(restoredPointLine);
        QCOMPARE(dynamic_cast<XPoint2D*>(restoredPointLine->getParamsByName("point1"))->x(),
                 12.0);
        QCOMPARE(dynamic_cast<XLine2D*>(restoredPointLine->getParamsByName("line1"))->end(),
                 QPointF(20.0,0.0));
        QCOMPARE(dynamic_cast<XReal*>(restoredPointLine->getParamsByName("scale"))->value(),
                 0.25);
        QCOMPARE(dynamic_cast<XString*>(restoredPointLine->getParamsByName("unit"))->value(),
                 QString("mm"));

        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        XvFlow *imported=manager->importXvFlow(flowPath);
        QVERIFY(imported);
        bool importedPointLine=false;
        for(XvFunc *function:imported->getXvFuncs())
        {
            auto measure=qobject_cast<GeometryMeasure*>(function);
            if(measure && measure->mode()==GeometryMeasure::PointLine)
            {
                importedPointLine=true;
                QCOMPARE(dynamic_cast<XReal*>(measure->getParamsByName("scale"))->value(),
                         0.25);
                QCOMPARE(dynamic_cast<XPoint2D*>(measure->getParamsByName("point1"))->y(),
                         -6.0);
            }
        }
        QVERIFY(importedPointLine);
    }

    void detectRecordPresetsParametersAndSubscriptionPersist()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("detect-record.xvproj");
        const QString flowPath=directory.filePath("detect-record.xvflow");
        const QString databasePath=directory.filePath("records.sqlite");
        const QList<QPair<QString,int>> aliases={
            {"DetectRecordNodeData",DetectRecord::Record},
            {"ClassDetectRecordNodeData",DetectRecord::ClassRecord},
            {"ObjectDetectRecordNodeData",DetectRecord::ObjectRecord},
            {"HasDetectRecordNodeData",DetectRecord::HasRecord}
        };

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Detect record operators");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Records");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        QMap<QString,int> modesById;
        DetectRecord *writer=nullptr;
        DetectRecord *classQuery=nullptr;
        for(const auto &alias:aliases)
        {
            auto function=qobject_cast<DetectRecord*>(flow->createXvFunc(alias.first));
            QVERIFY2(function,qPrintable(flow->lastErrorMsg()));
            QCOMPARE(function->funcRole(),QString("DetectRecord"));
            QCOMPARE(function->mode(),DetectRecord::Mode(alias.second));
            modesById.insert(function->funcId(),alias.second);
            if(alias.second==DetectRecord::Record) writer=function;
            if(alias.second==DetectRecord::ClassRecord) classQuery=function;
        }
        QVERIFY(writer);
        QVERIFY(classQuery);
        const QString writerId=writer->funcId();
        const QString classQueryId=classQuery->funcId();
        dynamic_cast<XString*>(writer->getParamsByName("databasePath"))
                ->setValue(databasePath);
        dynamic_cast<XInt*>(writer->getParamsByName("busyTimeoutMs"))->setValue(4321);
        dynamic_cast<XString*>(writer->getParamsByName("category"))->setValue("weld");
        dynamic_cast<XString*>(writer->getParamsByName("objectId"))->setValue("part-7");
        dynamic_cast<XBool*>(writer->getParamsByName("useRecordInput"))->setValue(true);
        dynamic_cast<XString*>(writer->getParamsByName("recordId"))->setValue("fallback");
        dynamic_cast<XReal*>(writer->getParamsByName("timestampMs"))->setValue(987654.0);
        dynamic_cast<XString*>(writer->getParamsByName("outcome"))->setValue("ok");
        dynamic_cast<XString*>(writer->getParamsByName("detailsJson"))
                ->setValue("{\"score\":0.91}");
        dynamic_cast<XString*>(classQuery->getParamsByName("databasePath"))
                ->setValue(databasePath);
        dynamic_cast<XString*>(classQuery->getParamsByName("category"))->setValue("weld");
        dynamic_cast<XInt*>(classQuery->getParamsByName("limit"))->setValue(25);
        QVERIFY(classQuery->addSonFunc(writer));
        QVERIFY(writer->paramSubscribe("record",classQuery,"latestRecord"));

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("role=\"DetectRecord\""));
        QVERIFY(xml.contains("name=\"databasePath\""));
        QVERIFY(xml.contains("name=\"busyTimeoutMs\""));
        QVERIFY(xml.contains("name=\"detailsJson\""));
        QVERIFY(xml.contains("parameter=\"record\""));
        QVERIFY(xml.contains("sourceResult=\"latestRecord\""));
        QVERIFY(!xml.contains("<Value name=\"record\""));
        QVERIFY(!xml.contains("name=\"records\""));
        QVERIFY(!xml.contains("name=\"latestRecord\""));
        QVERIFY(!xml.contains("name=\"recordCount\""));
        QVERIFY(!xml.contains("name=\"insertedRecordId\""));
        for(const auto &alias:aliases) QVERIFY(!xml.contains(alias.first.toUtf8()));

        QVERIFY(manager->loadXvProject(projectPath));
        XvFlow *restored=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restored);
        for(auto iterator=modesById.cbegin();iterator!=modesById.cend();++iterator)
        {
            auto function=qobject_cast<DetectRecord*>(restored->getXvFunc(iterator.key()));
            QVERIFY(function);
            QCOMPARE(function->mode(),DetectRecord::Mode(iterator.value()));
        }
        auto restoredWriter=qobject_cast<DetectRecord*>(restored->getXvFunc(writerId));
        auto restoredQuery=qobject_cast<DetectRecord*>(restored->getXvFunc(classQueryId));
        QVERIFY(restoredWriter);
        QVERIFY(restoredQuery);
        QCOMPARE(dynamic_cast<XString*>(restoredWriter->getParamsByName("databasePath"))
                 ->value(),databasePath);
        QCOMPARE(dynamic_cast<XInt*>(restoredWriter->getParamsByName("busyTimeoutMs"))
                 ->value(),4321);
        QCOMPARE(dynamic_cast<XString*>(restoredWriter->getParamsByName("category"))
                 ->value(),QString("weld"));
        QVERIFY(dynamic_cast<XBool*>(restoredWriter->getParamsByName("useRecordInput"))
                ->value());
        QCOMPARE(dynamic_cast<XInt*>(restoredQuery->getParamsByName("limit"))->value(),25);
        XvFunc::SubscribeInfo subscription;
        QVERIFY(restoredWriter->getParamSubscribe("record",subscription));
        QCOMPARE(subscription.first,restoredQuery);
        QCOMPARE(subscription.second,QString("latestRecord"));

        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        XvFlow *imported=manager->importXvFlow(flowPath);
        QVERIFY(imported);
        int importedRecords=0;
        bool importedSubscription=false;
        for(XvFunc *function:imported->getXvFuncs())
        {
            auto record=qobject_cast<DetectRecord*>(function);
            if(!record) continue;
            ++importedRecords;
            if(record->mode()!=DetectRecord::Record) continue;
            XvFunc::SubscribeInfo importedInfo;
            QVERIFY(record->getParamSubscribe("record",importedInfo));
            QCOMPARE(importedInfo.first->funcRole(),QString("DetectRecord"));
            QCOMPARE(importedInfo.first->property("mode").toInt(),
                     int(DetectRecord::ClassRecord));
            QCOMPARE(importedInfo.second,QString("latestRecord"));
            importedSubscription=true;
        }
        QCOMPARE(importedRecords,4);
        QVERIFY(importedSubscription);
    }

    void communicationPresetsParametersAndSubscriptionsPersist()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("communication.xvproj");
        const QString flowPath=directory.filePath("communication.xvflow");
        const QList<QPair<QString,QPair<QString,int>>> aliases={
            {"HttpReadJsonNodeData",{"HttpJson",HttpJson::Read}},
            {"HttpWriteJsonNodeData",{"HttpJson",HttpJson::Write}},
            {"TcpReadStringNodeData",{"TcpText",TcpText::Read}},
            {"TcpWriteStringNodeData",{"TcpText",TcpText::Write}},
            {"UdpReadStringNodeData",{"UdpText",UdpText::Read}},
            {"UdpWriteStringNodeData",{"UdpText",UdpText::Write}},
            {"SerialReadByteNodeData",{"SerialData",SerialData::ReadBytes}},
            {"SerialReadStringNodeData",{"SerialData",SerialData::ReadText}},
            {"SerialWriteByteNodeData",{"SerialData",SerialData::WriteBytes}},
            {"SerialWriteStringNodeData",{"SerialData",SerialData::WriteText}},
            {"IntReadableModbusNodeData",{"ModbusRegister",ModbusRegister::ReadInt32}},
            {"ShortWriteableModbusNodeData",{"ModbusRegister",ModbusRegister::WriteInt16}}
        };

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Communication operators");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Communication");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        QMap<QString,QPair<QString,int>> expectedById;
        HttpJson *httpReader=nullptr;
        HttpJson *httpWriter=nullptr;
        SerialData *byteReader=nullptr;
        SerialData *byteWriter=nullptr;
        for(const auto &alias:aliases)
        {
            XvFunc *function=flow->createXvFunc(alias.first);
            QVERIFY2(function,qPrintable(flow->lastErrorMsg()));
            QCOMPARE(function->funcRole(),alias.second.first);
            QCOMPARE(function->property("mode").toInt(),alias.second.second);
            expectedById.insert(function->funcId(),alias.second);
            if(alias.first=="HttpReadJsonNodeData") httpReader=qobject_cast<HttpJson*>(function);
            if(alias.first=="HttpWriteJsonNodeData") httpWriter=qobject_cast<HttpJson*>(function);
            if(alias.first=="SerialReadByteNodeData") byteReader=qobject_cast<SerialData*>(function);
            if(alias.first=="SerialWriteByteNodeData") byteWriter=qobject_cast<SerialData*>(function);
        }
        QVERIFY(httpReader);
        QVERIFY(httpWriter);
        QVERIFY(byteReader);
        QVERIFY(byteWriter);
        const QString httpWriterId=httpWriter->funcId();
        const QString byteWriterId=byteWriter->funcId();
        dynamic_cast<XString*>(httpReader->getParamsByName("url"))
                ->setValue("http://127.0.0.1:18080/source");
        dynamic_cast<XInt*>(httpReader->getParamsByName("timeoutMs"))->setValue(4321);
        dynamic_cast<XString*>(httpWriter->getParamsByName("headersJson"))
                ->setValue("{\"X-Test\":\"roundtrip\"}");
        dynamic_cast<XBool*>(httpWriter->getParamsByName("useJsonInput"))->setValue(true);
        dynamic_cast<XString*>(byteReader->getParamsByName("portName"))->setValue("COM9");
        dynamic_cast<XInt*>(byteReader->getParamsByName("baudRate"))->setValue(57600);
        dynamic_cast<XBool*>(byteWriter->getParamsByName("useByteInput"))->setValue(true);
        dynamic_cast<XString*>(byteWriter->getParamsByName("payload"))->setValue("aa55");
        QVERIFY(httpReader->addSonFunc(httpWriter));
        QVERIFY(httpWriter->paramSubscribe("jsonInput",httpReader,"json"));
        QVERIFY(byteReader->addSonFunc(byteWriter));
        QVERIFY(byteWriter->paramSubscribe("byteInput",byteReader,"data"));

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        for(const QString &role:QStringList{"HttpJson","TcpText","UdpText","SerialData",
                                            "ModbusRegister"})
            QVERIFY(xml.contains(QString("role=\"%1\"").arg(role).toUtf8()));
        QVERIFY(xml.contains("name=\"url\""));
        QVERIFY(xml.contains("name=\"baudRate\""));
        QVERIFY(xml.contains("parameter=\"jsonInput\""));
        QVERIFY(xml.contains("sourceResult=\"json\""));
        QVERIFY(xml.contains("parameter=\"byteInput\""));
        QVERIFY(xml.contains("sourceResult=\"data\""));
        QVERIFY(!xml.contains("<Value name=\"jsonInput\""));
        QVERIFY(!xml.contains("<Value name=\"byteInput\""));
        QVERIFY(!xml.contains("name=\"statusCode\""));
        QVERIFY(!xml.contains("name=\"bytesTransferred\""));
        for(const auto &alias:aliases) QVERIFY(!xml.contains(alias.first.toUtf8()));

        QVERIFY(manager->loadXvProject(projectPath));
        XvFlow *restored=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restored);
        for(auto iterator=expectedById.cbegin();iterator!=expectedById.cend();++iterator)
        {
            XvFunc *function=restored->getXvFunc(iterator.key());
            QVERIFY(function);
            QCOMPARE(function->funcRole(),iterator.value().first);
            QCOMPARE(function->property("mode").toInt(),iterator.value().second);
        }
        auto restoredHttpWriter=qobject_cast<HttpJson*>(restored->getXvFunc(httpWriterId));
        auto restoredByteWriter=qobject_cast<SerialData*>(restored->getXvFunc(byteWriterId));
        QVERIFY(restoredHttpWriter);
        QVERIFY(restoredByteWriter);
        QCOMPARE(dynamic_cast<XString*>(restoredHttpWriter->getParamsByName("headersJson"))
                 ->value(),QString("{\"X-Test\":\"roundtrip\"}"));
        QVERIFY(dynamic_cast<XBool*>(restoredHttpWriter->getParamsByName("useJsonInput"))
                ->value());
        XvFunc::SubscribeInfo subscription;
        QVERIFY(restoredHttpWriter->getParamSubscribe("jsonInput",subscription));
        QCOMPARE(subscription.first->funcRole(),QString("HttpJson"));
        QCOMPARE(subscription.second,QString("json"));
        QVERIFY(restoredByteWriter->getParamSubscribe("byteInput",subscription));
        QCOMPARE(subscription.first->funcRole(),QString("SerialData"));
        QCOMPARE(subscription.second,QString("data"));

        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        XvFlow *imported=manager->importXvFlow(flowPath);
        QVERIFY(imported);
        int communicationCount=0;
        int restoredSubscriptions=0;
        for(XvFunc *function:imported->getXvFuncs())
        {
            if(!QStringList{"HttpJson","TcpText","UdpText","SerialData","ModbusRegister"}
                    .contains(function->funcRole())) continue;
            ++communicationCount;
            if(function->getParamSubscribe("jsonInput",subscription)
                    || function->getParamSubscribe("byteInput",subscription))
            {
                QVERIFY(subscription.first);
                ++restoredSubscriptions;
            }
        }
        QCOMPARE(communicationCount,12);
        QCOMPARE(restoredSubscriptions,2);
    }

    void templateRectificationPresetsAndAssetPersistence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("template-rectification.xvproj");
        const QString flowPath=directory.filePath("template-rectification.xvflow");
        const QString invalidPath=directory.filePath("template-rectification-invalid.xvproj");
        const QList<QPair<QString,QPair<QString,int>>> aliases={
            {"Base64TemplateMatchNodeData",{"OTemplateMatch",OTemplateMatch::Base64}},
            {"FeaturePointTemplateMatch",{"OTemplateMatch",OTemplateMatch::Feature}},
            {"ShapeTemplateMatch",{"OTemplateMatch",OTemplateMatch::Shape}},
            {"HSVTemplateMatch",{"OTemplateMatch",OTemplateMatch::Hsv}},
            {"ForegroundRotatedRectRectification",
             {"ORectification",ORectification::ForegroundRotatedRect}},
            {"TakeoffForegroundInfo",{"ORectification",ORectification::ForegroundExtract}},
            {"RotatedRectRectification",{"ORectification",ORectification::RotatedRect}}
        };

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Template and rectification");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Operators");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        QMap<QString,QPair<QString,int>> expectedById;
        OTemplateMatch *assetMatcher=nullptr;
        ORectification *configuredRectification=nullptr;
        for(const auto &alias:aliases)
        {
            XvFunc *function=flow->createXvFunc(alias.first);
            QVERIFY2(function,qPrintable(flow->lastErrorMsg()));
            QCOMPARE(function->funcRole(),alias.second.first);
            QCOMPARE(function->property("mode").toInt(),alias.second.second);
            expectedById.insert(function->funcId(),alias.second);
            if(alias.first=="Base64TemplateMatchNodeData")
                assetMatcher=qobject_cast<OTemplateMatch*>(function);
            if(alias.first=="RotatedRectRectification")
                configuredRectification=qobject_cast<ORectification*>(function);
        }
        QVERIFY(assetMatcher && configuredRectification);
        const QString matcherId=assetMatcher->funcId();
        QImage templateImage(43,31,QImage::Format_RGB32);
        templateImage.fill(Qt::black);
        {
            QPainter painter(&templateImage);
            painter.setPen(QPen(Qt::white,3));
            painter.drawRect(3,4,31,20);
            painter.drawLine(4,23,36,6);
            painter.fillRect(29,18,6,5,Qt::red);
        }
        QString error;
        QVERIFY2(assetMatcher->configureTemplateImage(templateImage,error),qPrintable(error));
        const QByteArray asset=assetMatcher->templateAsset();
        QVERIFY(!asset.isEmpty());
        dynamic_cast<XInt*>(assetMatcher->getParamsByName("operation"))
                ->setValue(OTemplateMatch::FindTemplate);
        dynamic_cast<XBool*>(assetMatcher->getParamsByName("useTemplateRoi"))->setValue(true);
        QVERIFY(dynamic_cast<XRotateRectRoi*>(assetMatcher->getParamsByName("templateRoi"))
                ->setValue(80.0,60.0,21.5,15.5,0.2));
        dynamic_cast<XReal*>(assetMatcher->getParamsByName("minScore"))->setValue(0.61);
        dynamic_cast<XInt*>(configuredRectification->getParamsByName("outputWidth"))
                ->setValue(96);
        dynamic_cast<XInt*>(configuredRectification->getParamsByName("outputHeight"))
                ->setValue(64);
        dynamic_cast<XReal*>(configuredRectification->getParamsByName("borderValue"))
                ->setValue(17.0);

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("role=\"OTemplateMatch\""));
        QVERIFY(xml.contains("role=\"ORectification\""));
        QVERIFY(xml.contains("<OpenCvTemplate"));
        QVERIFY(xml.contains("format=\"png\""));
        QVERIFY(xml.contains("encoding=\"base64\""));
        QVERIFY(xml.contains("sha256=\""));
        QVERIFY(!xml.contains("matchCount"));
        QVERIFY(!xml.contains("foregroundMask"));
        QVERIFY(!xml.contains("foregroundRegion"));
        QVERIFY(!xml.contains("rectifiedRoi"));
        QVERIFY(!xml.contains("transform"));
        for(const auto &alias:aliases) QVERIFY(!xml.contains(alias.first.toUtf8()));

        QVERIFY(manager->loadXvProject(projectPath));
        XvProject *restoredProject=manager->getXvProject();
        QVERIFY(restoredProject);
        XvFlow *restoredFlow=restoredProject->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        for(auto iterator=expectedById.cbegin();iterator!=expectedById.cend();++iterator)
        {
            XvFunc *function=restoredFlow->getXvFunc(iterator.key());
            QVERIFY(function);
            QCOMPARE(function->funcRole(),iterator.value().first);
            QCOMPARE(function->property("mode").toInt(),iterator.value().second);
        }
        auto restoredMatcher=qobject_cast<OTemplateMatch*>(
                    restoredFlow->getXvFunc(matcherId));
        QVERIFY(restoredMatcher);
        QCOMPARE(restoredMatcher->templateAsset(),asset);
        QCOMPARE(restoredMatcher->templateSize(),templateImage.size());
        QCOMPARE(dynamic_cast<XInt*>(restoredMatcher->getParamsByName("operation"))->value(),
                 int(OTemplateMatch::FindTemplate));
        QCOMPARE(dynamic_cast<XReal*>(restoredMatcher->getParamsByName("minScore"))->value(),
                 0.61);

        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        XvFlow *imported=manager->importXvFlow(flowPath);
        QVERIFY(imported);
        OTemplateMatch *importedMatcher=nullptr;
        int importedBase64Matchers=0;
        for(XvFunc *function:imported->getXvFuncs())
        {
            auto candidate=qobject_cast<OTemplateMatch*>(function);
            if(candidate && candidate->mode()==OTemplateMatch::Base64)
            {
                importedMatcher=candidate;
                ++importedBase64Matchers;
            }
        }
        QCOMPARE(importedBase64Matchers,1);
        QVERIFY(importedMatcher);
        QCOMPARE(importedMatcher->templateAsset(),asset);
        QCOMPARE(importedMatcher->templateSize(),templateImage.size());

        const auto rejectMutation=[&](const std::function<void(QDomDocument&)> &mutate)
        {
            QDomDocument document=parseDocument(projectPath);
            mutate(document);
            QVERIFY(writeDocument(invalidPath,document));
            expectLoadRejected(invalidPath,restoredProject);
        };
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("OpenCvTemplate").at(0).toElement()
                    .setAttribute("format","jpeg");
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("OpenCvTemplate").at(0).toElement()
                    .setAttribute("sha256",QString(64,'0'));
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("OpenCvTemplate").at(0).toElement()
                    .setAttribute("length","8388609");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement element=document.elementsByTagName("OpenCvTemplate").at(0).toElement();
            element.firstChild().setNodeValue("@@@@");
            element.setAttribute("length","3");
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement element=document.elementsByTagName("OpenCvTemplate").at(0).toElement();
            element.firstChild().setNodeValue(element.text()+"\n");
        });
        rejectMutation([](QDomDocument &document) {
            const QByteArray payload("not a png image");
            QDomElement element=document.elementsByTagName("OpenCvTemplate").at(0).toElement();
            element.firstChild().setNodeValue(QString::fromLatin1(payload.toBase64()));
            element.setAttribute("length",QString::number(payload.size()));
            element.setAttribute("sha256",QString::fromLatin1(
                                     QCryptographicHash::hash(
                                         payload,QCryptographicHash::Sha256).toHex()));
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement function=ProjectXmlTest::findFunctionElementByRole(
                        document,"OTemplateMatch");
            QDomElement data=function.firstChildElement("PersistentData");
            function.appendChild(data.cloneNode(true));
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement function=ProjectXmlTest::findFunctionElementByRole(
                        document,"OTemplateMatch");
            QDomElement data=function.firstChildElement("PersistentData");
            QDomElement value=data.firstChildElement("OpenCvTemplate");
            data.appendChild(value.cloneNode(true));
        });
        rejectMutation([](QDomDocument &document) {
            QDomElement function=ProjectXmlTest::findFunctionElementByRole(
                        document,"OTemplateMatch");
            QDomElement data=function.firstChildElement("PersistentData");
            data.appendChild(document.createElement("UnknownTemplateData"));
        });
        rejectMutation([](QDomDocument &document) {
            document.elementsByTagName("OpenCvTemplate").at(0).toElement()
                    .setAttribute("unexpected","1");
        });
    }

    void onnxModelMetadataRoundTripAndRejectsChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString modelPath=directory.filePath("fixture.onnx");
#if defined(XVISION_ENABLE_ONNXRUNTIME)
        const QString source=qEnvironmentVariable("XVISION_ONNX_TEST_MODEL");
        if(source.isEmpty())
            QSKIP("Set XVISION_ONNX_TEST_MODEL to validate configured ONNX persistence");
        QVERIFY(QFile::copy(source,modelPath));
#else
        QFile fixture(modelPath);
        QVERIFY(fixture.open(QIODevice::WriteOnly));
        QCOMPARE(fixture.write("disabled-backend-model"),qint64(22));
        fixture.close();
#endif
        const QString projectPath=directory.filePath("configured-onnx.xvproj");
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Configured ONNX");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Inference");
        QVERIFY(flow);
        auto function=qobject_cast<NClassification*>(
                    flow->createXvFunc("NClassification"));
        QVERIFY(function);
        QString error;
        QVERIFY2(function->configureModel(modelPath,error),qPrintable(error));
        function->setInputLayout(NOnnxBase::Nchw);
        function->setInputWidth(224);
        function->setInputHeight(224);
        function->setChannelOrder(NOnnxBase::Bgr);
        function->setResizeMode(NOnnxBase::Letterbox);
        function->setPixelScale(0.5);
        function->setMeanValues("1,2,3");
        function->setStdValues("4,5,6");
        function->setClassNames("A\nB");
        dynamic_cast<XInt*>(function->getParamsByName("topK"))->setValue(2);
        const QString functionId=function->funcId();
        const QString flowId=flow->flowId();
        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray xml=readFile(projectPath);
        QVERIFY(xml.contains("<OnnxModel"));
        QVERIFY(xml.contains("format=\"onnx-runtime\""));
        QVERIFY(xml.contains("sha256="));
        QVERIFY(!xml.contains("classificationCount"));
        QVERIFY(manager->loadXvProject(projectPath));
        auto restored=qobject_cast<NClassification*>(
                    manager->getXvProject()->getXvFlow(flowId)->getXvFunc(functionId));
        QVERIFY(restored);
        QCOMPARE(restored->modelPath(),QFileInfo(modelPath).canonicalFilePath());
        QCOMPARE(restored->inputLayout(),NOnnxBase::Nchw);
        QCOMPARE(restored->inputWidth(),224);
        QCOMPARE(restored->inputHeight(),224);
        QCOMPARE(restored->channelOrder(),NOnnxBase::Bgr);
        QCOMPARE(restored->resizeMode(),NOnnxBase::Letterbox);
        QCOMPARE(restored->pixelScale(),0.5);
        QCOMPARE(restored->classNames(),QString("A\nB"));
        QCOMPARE(dynamic_cast<XInt*>(restored->getParamsByName("topK"))->value(),2);

        const QString flowPath=directory.filePath("configured-onnx.xvflow");
        QVERIFY(manager->exportXvFlow(flowId,flowPath));
        const QByteArray flowXml=readFile(flowPath);
        QVERIFY(flowXml.contains("<OnnxModel"));
        QVERIFY(flowXml.contains("sha256="));
        XvFlow *importedFlow=manager->importXvFlow(flowPath);
        QVERIFY2(importedFlow,qPrintable(manager->lastErrorMsg()));
        NClassification *imported=nullptr;
        for(XvFunc *candidate:importedFlow->getXvFuncs())
            if(candidate->funcRole()=="NClassification")
            {
                QVERIFY(!imported);
                imported=qobject_cast<NClassification*>(candidate);
            }
        QVERIFY(imported);
        QVERIFY(imported->funcId()!=functionId);
        QCOMPARE(imported->modelPath(),QFileInfo(modelPath).canonicalFilePath());
        QCOMPARE(imported->resizeMode(),NOnnxBase::Letterbox);
        QCOMPARE(imported->meanValues(),QString("1,2,3"));
        QCOMPARE(imported->stdValues(),QString("4,5,6"));
        QCOMPARE(dynamic_cast<XInt*>(imported->getParamsByName("topK"))->value(),2);

        const QByteArray validProjectXml=readFile(projectPath);
        restored->setStdValues("-1");
        QVERIFY(!manager->saveXvProject(projectPath));
        QCOMPARE(readFile(projectPath),validProjectXml);
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        restored->setStdValues("4,5,6");

        XvProject *installed=manager->getXvProject();
        const QString invalidPath=directory.filePath("invalid-onnx.xvproj");
        const auto rejectMetadataMutation=[&](const std::function<void(QDomElement&)> &mutate)
        {
            QDomDocument document=parseDocument(projectPath);
            QDomElement storedFunction=findFunctionElementByRole(
                        document,"NClassification");
            QVERIFY(!storedFunction.isNull());
            QDomElement storedModel=storedFunction.firstChildElement("PersistentData")
                    .firstChildElement("OnnxModel");
            QVERIFY(!storedModel.isNull());
            mutate(storedModel);
            QVERIFY(writeDocument(invalidPath,document));
            QVERIFY(!manager->loadXvProject(invalidPath));
            QCOMPARE(manager->getXvProject(),installed);
            QVERIFY(!manager->lastErrorMsg().isEmpty());
        };
        rejectMetadataMutation([](QDomElement &model) {
            model.setAttribute("unknown","value");
        });
        rejectMetadataMutation([](QDomElement &model) {
            const QString digest=model.attribute("sha256");
            model.setAttribute("sha256",QString("A")+digest.mid(1));
        });
        rejectMetadataMutation([](QDomElement &model) {
            model.parentNode().appendChild(model.cloneNode(true));
        });

        QFile changed(modelPath);
        QVERIFY(changed.open(QIODevice::Append));
        QCOMPARE(changed.write("changed"),qint64(7));
        changed.close();
        QVERIFY(!manager->loadXvProject(projectPath));
        QCOMPARE(manager->getXvProject(),installed);
        QVERIFY(!manager->lastErrorMsg().isEmpty());
    }

    void imagePresetAliasPersistsCanonicalRoleAndMode()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("image-preset-alias.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Image preset alias");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Image processing");
        QVERIFY(flow);
        const QString flowId=flow->flowId();

        auto analysis=qobject_cast<OImageAnalysis*>(flow->createXvFunc("Canny"));
        auto morphology=qobject_cast<OMorphology*>(flow->createXvFunc("BlackHat"));
        auto region=qobject_cast<ORegionDetector*>(flow->createXvFunc("HoughLinesP"));
        auto code=qobject_cast<OCodeDetector*>(flow->createXvFunc("QRCode"));
        auto feature=qobject_cast<OPointFeature*>(flow->createXvFunc("StarFeatureDetector"));
        auto cascade=qobject_cast<OCascadeDetector*>(flow->createXvFunc("LbpCascade"));
        QVERIFY2(analysis && morphology && region && code && feature && cascade,
                 qPrintable(flow->lastErrorMsg()));
        const QString functionId=analysis->funcId();
        const QString morphologyId=morphology->funcId();
        const QString regionId=region->funcId();
        const QString codeId=code->funcId();
        const QString featureId=feature->funcId();
        const QString cascadeId=cascade->funcId();
        QCOMPARE(analysis->funcRole(),QString("OImageAnalysis"));
        QCOMPARE(analysis->mode(),OImageAnalysis::Canny);
        QCOMPARE(morphology->funcRole(),QString("OMorphology"));
        QCOMPARE(morphology->mode(),OMorphology::BlackHat);
        QCOMPARE(region->funcRole(),QString("ORegionDetector"));
        QCOMPARE(region->mode(),ORegionDetector::HoughLinesP);
        QCOMPARE(code->funcRole(),QString("OCodeDetector"));
        QCOMPARE(code->mode(),OCodeDetector::QrCode);
        QCOMPARE(feature->funcRole(),QString("OPointFeature"));
        QCOMPARE(feature->mode(),OPointFeature::Star);
        QCOMPARE(cascade->funcRole(),QString("OCascadeDetector"));
        QCOMPARE(cascade->mode(),OCascadeDetector::Lbp);

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(xml.contains("role=\"OImageAnalysis\""));
        QVERIFY(xml.contains("role=\"OMorphology\""));
        QVERIFY(xml.contains("role=\"ORegionDetector\""));
        QVERIFY(xml.contains("role=\"OCodeDetector\""));
        QVERIFY(xml.contains("role=\"OPointFeature\""));
        QVERIFY(xml.contains("role=\"OCascadeDetector\""));
        QVERIFY(!xml.contains("role=\"Canny\""));
        QVERIFY(!xml.contains("role=\"BlackHat\""));
        QVERIFY(!xml.contains("role=\"HoughLinesP\""));
        QVERIFY(!xml.contains("role=\"QRCode\""));
        QVERIFY(!xml.contains("role=\"StarFeatureDetector\""));
        QVERIFY(!xml.contains("role=\"LbpCascade\""));
        QVERIFY(!xml.contains("inputImage"));
        // descriptorSize/descriptorChannels are persistent configuration values.
        // Only descriptor payloads and runtime feature objects must be absent.
        QVERIFY(!xml.contains("name=\"descriptor\""));
        QVERIFY(!xml.contains("name=\"descriptors\""));
        QVERIFY(!xml.contains("type=\"XFeatureSet\""));

        QVERIFY(manager->loadXvProject(path));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        auto restored=qobject_cast<OImageAnalysis*>(restoredFlow->getXvFunc(functionId));
        auto restoredMorphology=qobject_cast<OMorphology*>(
                    restoredFlow->getXvFunc(morphologyId));
        auto restoredRegion=qobject_cast<ORegionDetector*>(restoredFlow->getXvFunc(regionId));
        auto restoredCode=qobject_cast<OCodeDetector*>(restoredFlow->getXvFunc(codeId));
        auto restoredFeature=qobject_cast<OPointFeature*>(restoredFlow->getXvFunc(featureId));
        auto restoredCascade=qobject_cast<OCascadeDetector*>(restoredFlow->getXvFunc(cascadeId));
        QVERIFY(restored && restoredMorphology && restoredRegion && restoredCode
                && restoredFeature && restoredCascade);
        QCOMPARE(restored->mode(),OImageAnalysis::Canny);
        QCOMPARE(restoredMorphology->mode(),OMorphology::BlackHat);
        QCOMPARE(restoredRegion->mode(),ORegionDetector::HoughLinesP);
        QCOMPARE(restoredCode->mode(),OCodeDetector::QrCode);
        QCOMPARE(restoredFeature->mode(),OPointFeature::Star);
        QCOMPARE(restoredCascade->mode(),OCascadeDetector::Lbp);
    }

    void outputPortsRoundTripAndRejectInvalidValues()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("ports.xvproj");
        const QString secondPath=directory.filePath("ports-second.xvproj");
        const QString invalidPath=directory.filePath("ports-invalid.xvproj");
        const QString missingPath=directory.filePath("ports-missing.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Ports");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Port flow");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        XvFunc *branch=flow->createXvFunc("PortFixtureFunc");
        XvFunc *writer=flow->createXvFunc("BaseDataWriter");
        XvFunc *calculator=flow->createXvFunc("BaseDataIntCalc");
        QVERIFY(branch && writer && calculator);
        const QString branchId=branch->funcId();
        const QString writerId=writer->funcId();
        const QString calculatorId=calculator->funcId();

        QCOMPARE(branch->outputPorts(),QStringList({"true","false"}));
        QVERIFY(branch->addSonFunc(writer,"true"));
        QVERIFY(branch->addSonFunc(calculator,"false"));
        QVERIFY(writer->addSonFunc(calculator));
        QCOMPARE(branch->sonFuncPort(writer),QString("true"));
        QCOMPARE(branch->sonFuncPort(calculator),QString("false"));
        QCOMPARE(writer->sonFuncPort(calculator),QString("default"));
        QVERIFY(!branch->addSonFunc(writer,"false"));
        QVERIFY(!branch->setSonFuncPort(writer,"missing"));
        QCOMPARE(branch->sonFuncPort(writer),QString("true"));

        QVERIFY(manager->saveXvProject(path));
        QDomDocument document=parseDocument(path);
        QDomNodeList links=document.elementsByTagName("Link");
        QCOMPARE(links.count(),3);
        QDomElement trueLink;
        QDomElement defaultLink;
        for(int index=0;index<links.count();++index)
        {
            const QDomElement link=links.at(index).toElement();
            if(link.attribute("from")==branchId
                    && link.attribute("to")==writerId)
                trueLink=link;
            if(link.attribute("from")==writerId
                    && link.attribute("to")==calculatorId)
                defaultLink=link;
        }
        QVERIFY(!trueLink.isNull());
        QCOMPARE(trueLink.attribute("fromPort"),QString("true"));
        QVERIFY(!defaultLink.isNull());
        QVERIFY(!defaultLink.hasAttribute("fromPort"));

        document=parseDocument(path);
        links=document.elementsByTagName("Link");
        for(int index=0;index<links.count();++index)
        {
            QDomElement link=links.at(index).toElement();
            if(link.attribute("from")==branchId)
            {
                link.setAttribute("fromPort","missing");
                break;
            }
        }
        QVERIFY(writeDocument(invalidPath,document));
        expectLoadRejected(invalidPath,project);

        document=parseDocument(path);
        links=document.elementsByTagName("Link");
        for(int index=0;index<links.count();++index)
        {
            QDomElement link=links.at(index).toElement();
            if(link.attribute("from")==branchId)
            {
                link.removeAttribute("fromPort");
                break;
            }
        }
        QVERIFY(writeDocument(missingPath,document));
        expectLoadRejected(missingPath,project);

        QVERIFY(manager->loadXvProject(path));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        XvFunc *restoredBranch=restoredFlow->getXvFunc(branchId);
        XvFunc *restoredWriter=restoredFlow->getXvFunc(writerId);
        XvFunc *restoredCalculator=restoredFlow->getXvFunc(calculatorId);
        QVERIFY(restoredBranch && restoredWriter && restoredCalculator);
        QCOMPARE(restoredBranch->sonFuncPort(restoredWriter),QString("true"));
        QCOMPARE(restoredBranch->sonFuncPort(restoredCalculator),QString("false"));
        QCOMPARE(restoredWriter->sonFuncPort(restoredCalculator),QString("default"));
        QVERIFY(manager->saveXvProject(secondPath));
        QCOMPARE(readFile(secondPath),readFile(path));
    }

    void dynamicSchedulerResolvesForkJoinAndSkippedBranches()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Scheduler");
        QVERIFY(project);

        XvFlow *forkFlow=project->createXvFlow("Fork join");
        QVERIFY(forkFlow);
        auto root=qobject_cast<PortFixtureFunc*>(
                    forkFlow->createXvFunc("PortFixtureFunc"));
        auto left=qobject_cast<PortFixtureFunc*>(
                    forkFlow->createXvFunc("PortFixtureFunc"));
        auto right=qobject_cast<PortFixtureFunc*>(
                    forkFlow->createXvFunc("PortFixtureFunc"));
        auto join=qobject_cast<PortFixtureFunc*>(
                    forkFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(root && left && right && join);
        root->setFuncName("root");
        left->setFuncName("left");
        right->setFuncName("right");
        join->setFuncName("join");
        QVERIFY(root->addSonFunc(left,"true"));
        QVERIFY(root->addSonFunc(right,"false"));
        QVERIFY(left->addSonFunc(join,"true"));
        QVERIFY(right->addSonFunc(join,"true"));

        QStringList order;
        const QList<XvFunc*> orderedFunctions={root,left,right,join};
        for(XvFunc *function:orderedFunctions)
        {
            connect(function,&XvFunc::sgFuncRunStart,this,
                    [&order](XvFunc *started)
            {
                order.append(started->funcName());
            },Qt::DirectConnection);
        }
        QCOMPARE(forkFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forkFlow->isRunning(),5000);
        QCOMPARE(forkFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QStringList branchOrder;
        if(left->funcId()<right->funcId()) branchOrder={"left","right"};
        else branchOrder={"right","left"};
        QCOMPARE(order,QStringList({"root",branchOrder.at(0),
                                    branchOrder.at(1),"join"}));
        QCOMPARE(root->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(left->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(right->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(join->getXvFuncRunInfo().runIdx,1U);

        XvFlow *selectedFlow=project->createXvFlow("Selected branch");
        QVERIFY(selectedFlow);
        auto selector=qobject_cast<PortFixtureFunc*>(
                    selectedFlow->createXvFunc("PortFixtureFunc"));
        auto selected=qobject_cast<PortFixtureFunc*>(
                    selectedFlow->createXvFunc("PortFixtureFunc"));
        auto skippedFirst=qobject_cast<PortFixtureFunc*>(
                    selectedFlow->createXvFunc("PortFixtureFunc"));
        auto skippedSecond=qobject_cast<PortFixtureFunc*>(
                    selectedFlow->createXvFunc("PortFixtureFunc"));
        auto selectedJoin=qobject_cast<PortFixtureFunc*>(
                    selectedFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(selector && selected && skippedFirst && skippedSecond
                && selectedJoin);
        selector->selectPorts({"true"});
        QVERIFY(selector->addSonFunc(selected,"true"));
        QVERIFY(selector->addSonFunc(skippedFirst,"false"));
        QVERIFY(selected->addSonFunc(selectedJoin,"true"));
        QVERIFY(skippedFirst->addSonFunc(skippedSecond,"true"));
        QVERIFY(skippedSecond->addSonFunc(selectedJoin,"true"));

        QCOMPARE(selectedFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!selectedFlow->isRunning(),5000);
        QCOMPARE(selectedFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QCOMPARE(selector->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(selected->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(skippedFirst->getXvFuncRunInfo().runIdx,0U);
        QCOMPARE(skippedSecond->getXvFuncRunInfo().runIdx,0U);
        QCOMPARE(selectedJoin->getXvFuncRunInfo().runIdx,1U);

        XvFlow *invalidFlow=project->createXvFlow("Invalid directive");
        QVERIFY(invalidFlow);
        auto invalid=qobject_cast<PortFixtureFunc*>(
                    invalidFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(invalid);
        invalid->selectPorts({"missing"});
        QCOMPARE(invalidFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!invalidFlow->isRunning(),5000);
        QCOMPARE(invalidFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
    }

    void conditionalFlowSelectsOneBranchAndPersistsPorts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("conditional.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Conditional");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Conditional flow");
        QVERIFY(flow);
        const QString flowId=flow->flowId();
        auto condition=qobject_cast<ConditionalFlow*>(
                    flow->createXvFunc("OpenCVConditionNodeData"));
        auto trueNode=qobject_cast<PortFixtureFunc*>(
                    flow->createXvFunc("PortFixtureFunc"));
        auto falseNode=qobject_cast<PortFixtureFunc*>(
                    flow->createXvFunc("PortFixtureFunc"));
        auto join=qobject_cast<PortFixtureFunc*>(
                    flow->createXvFunc("PortFixtureFunc"));
        QVERIFY(condition && trueNode && falseNode && join);
        const QString conditionId=condition->funcId();
        const QString trueId=trueNode->funcId();
        const QString falseId=falseNode->funcId();
        QVERIFY(condition->addSonFunc(trueNode,"true"));
        QVERIFY(condition->addSonFunc(falseNode,"false"));
        QVERIFY(trueNode->addSonFunc(join,"true"));
        QVERIFY(falseNode->addSonFunc(join,"true"));
        auto conditionValue=dynamic_cast<XBool*>(
                    condition->getParamsByName("condition"));
        QVERIFY(conditionValue);

        conditionValue->setValue(true);
        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(trueNode->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(falseNode->getXvFuncRunInfo().runIdx,0U);
        QCOMPARE(join->getXvFuncRunInfo().runIdx,1U);

        conditionValue->setValue(false);
        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(trueNode->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(falseNode->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(join->getXvFuncRunInfo().runIdx,2U);

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(xml.contains("role=\"ConditionalFlow\""));
        QVERIFY(xml.contains("fromPort=\"true\""));
        QVERIFY(xml.contains("fromPort=\"false\""));
        QVERIFY(manager->loadXvProject(path));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlow(flowId);
        QVERIFY(restoredFlow);
        auto restored=qobject_cast<ConditionalFlow*>(
                    restoredFlow->getXvFunc(conditionId));
        QVERIFY(restored);
        QCOMPARE(dynamic_cast<XBool*>(restored->getParamsByName("condition"))->value(),
                 false);
        QCOMPARE(restored->sonFuncPort(restoredFlow->getXvFunc(trueId)),
                 QString("true"));
        QCOMPARE(restored->sonFuncPort(restoredFlow->getXvFunc(falseId)),
                 QString("false"));
    }

    void loopFlowRunsBoundedForForeachZeroAndStops()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Loop controls");
        QVERIFY(project);

        XvFlow *forFlow=project->createXvFlow("For loop");
        auto loop=qobject_cast<LoopFlow*>(forFlow->createXvFunc("ForNodeData"));
        auto body=qobject_cast<IterationCaptureFunc*>(
                    forFlow->createXvFunc("IterationCaptureFunc"));
        auto done=qobject_cast<PortFixtureFunc*>(
                    forFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(loop && body && done);
        QVERIFY(loop->addSonFunc(body,"body"));
        QVERIFY(loop->addSonFunc(done,"done"));
        QVERIFY(body->addSonFunc(done));
        auto start=dynamic_cast<XInt*>(loop->getParamsByName("start"));
        auto end=dynamic_cast<XInt*>(loop->getParamsByName("end"));
        auto step=dynamic_cast<XInt*>(loop->getParamsByName("step"));
        auto iteration=dynamic_cast<XInt*>(loop->getResultsByName("iterationIndex"));
        auto currentValue=dynamic_cast<XInt*>(loop->getResultsByName("currentValue"));
        QVERIFY(start && end && step && iteration && currentValue);
        QVERIFY(body->paramSubscribe("value",loop,"currentValue"));
        start->setValue(1);
        end->setValue(7);
        step->setValue(2);
        QCOMPARE(forFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forFlow->isRunning(),5000);
        QCOMPARE(forFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,3U);
        QCOMPARE(done->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(iteration->value(),2);
        QCOMPARE(currentValue->value(),5);
        QCOMPARE(body->observedValues(),QList<int>({1,3,5}));

        start->setValue(4);
        end->setValue(4);
        QCOMPARE(forFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forFlow->isRunning(),5000);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,3U);
        QCOMPARE(done->getXvFuncRunInfo().runIdx,2U);
        QCOMPARE(iteration->value(),-1);

        start->setValue(5);
        end->setValue(-1);
        step->setValue(-2);
        QCOMPARE(forFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forFlow->isRunning(),5000);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,6U);
        QCOMPARE(done->getXvFuncRunInfo().runIdx,3U);
        QCOMPARE(body->observedValues(),QList<int>({1,3,5,5,3,1}));

        step->setValue(0);
        QCOMPARE(forFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forFlow->isRunning(),5000);
        QCOMPARE(forFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,6U);

        start->setValue(0);
        end->setValue(XvExecutionDirective::MaximumIterationCount+1);
        step->setValue(1);
        forFlow->getFlowConfig()->funcErrorInterruptRun=true;
        QCOMPARE(forFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!forFlow->isRunning(),5000);
        QCOMPARE(forFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,6U);

        XvFlow *foreachFlow=project->createXvFlow("Foreach images");
        auto foreachLoop=qobject_cast<LoopFlow*>(
                    foreachFlow->createXvFunc("ForeachSplitResultImageNodeData"));
        auto foreachBody=qobject_cast<PortFixtureFunc*>(
                    foreachFlow->createXvFunc("PortFixtureFunc"));
        auto foreachDone=qobject_cast<PortFixtureFunc*>(
                    foreachFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(foreachLoop && foreachBody && foreachDone);
        QCOMPARE(foreachLoop->mode(),LoopFlow::ForeachImages);
        QVERIFY(foreachLoop->addSonFunc(foreachBody,"body"));
        QVERIFY(foreachLoop->addSonFunc(foreachDone,"done"));
        QVERIFY(foreachBody->addSonFunc(foreachDone,"true"));
        auto images=dynamic_cast<XObjectList*>(
                    foreachLoop->getParamsByName("images"));
        QVERIFY(images);
        QImage red(2,2,QImage::Format_RGB32);
        red.fill(Qt::red);
        QImage blue(2,2,QImage::Format_RGB32);
        blue.fill(Qt::blue);
        QVERIFY(images->addValue(new XImage("red",red)));
        QVERIFY(images->addValue(new XImage("blue",blue)));
        QCOMPARE(foreachFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!foreachFlow->isRunning(),5000);
        QCOMPARE(foreachBody->getXvFuncRunInfo().runIdx,2U);
        QCOMPARE(foreachDone->getXvFuncRunInfo().runIdx,1U);
        auto currentImage=dynamic_cast<XImage*>(
                    foreachLoop->getResultsByName("currentImage"));
        QVERIFY(currentImage);
        QCOMPARE(currentImage->value(),blue);

        XvFlow *stopFlow=project->createXvFlow("Stopped body");
        auto stopLoop=qobject_cast<LoopFlow*>(stopFlow->createXvFunc("LoopFlow"));
        XvFunc *stopBody=stopFlow->createXvFunc("StopFlowFixtureFunc");
        auto stopDone=qobject_cast<PortFixtureFunc*>(
                    stopFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(stopLoop && stopBody && stopDone);
        dynamic_cast<XInt*>(stopLoop->getParamsByName("end"))->setValue(5);
        QVERIFY(stopLoop->addSonFunc(stopBody,"body"));
        QVERIFY(stopLoop->addSonFunc(stopDone,"done"));
        QVERIFY(stopBody->addSonFunc(stopDone));
        QCOMPARE(stopFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!stopFlow->isRunning(),5000);
        QCOMPARE(stopBody->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(stopDone->getXvFuncRunInfo().runIdx,0U);
    }

    void loopFlowConfigurationRoundTripsWithoutRuntimeImages()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("loop-config.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Loop persistence");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Loop");
        auto loop=qobject_cast<LoopFlow*>(
                    flow->createXvFunc("ForeachSplitResultImageNodeData"));
        QVERIFY(loop);
        const QString flowId=flow->flowId();
        const QString loopId=loop->funcId();
        QCOMPARE(loop->mode(),LoopFlow::ForeachImages);
        dynamic_cast<XInt*>(loop->getParamsByName("start"))->setValue(-4);
        dynamic_cast<XInt*>(loop->getParamsByName("end"))->setValue(12);
        dynamic_cast<XInt*>(loop->getParamsByName("step"))->setValue(3);
        auto images=dynamic_cast<XObjectList*>(loop->getParamsByName("images"));
        QVERIFY(images);
        QImage image(1,1,QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(images->addValue(new XImage("transient",image)));

        QVERIFY(manager->saveXvProject(path));
        const QByteArray xml=readFile(path);
        QVERIFY(xml.contains("role=\"LoopFlow\""));
        QVERIFY(!xml.contains("name=\"images\""));
        QVERIFY(!xml.contains("currentImage"));
        QVERIFY(manager->loadXvProject(path));
        auto restored=qobject_cast<LoopFlow*>(
                    manager->getXvProject()->getXvFlow(flowId)->getXvFunc(loopId));
        QVERIFY(restored);
        QCOMPARE(restored->mode(),LoopFlow::ForeachImages);
        QCOMPARE(dynamic_cast<XInt*>(restored->getParamsByName("start"))->value(),-4);
        QCOMPARE(dynamic_cast<XInt*>(restored->getParamsByName("end"))->value(),12);
        QCOMPARE(dynamic_cast<XInt*>(restored->getParamsByName("step"))->value(),3);
        QCOMPARE(dynamic_cast<XObjectList*>(restored->getParamsByName("images"))->count(),
                 qsizetype(0));
    }

    void loopFlowRejectsMissingAndUnboundedBodyLinks()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Invalid loops");
        QVERIFY(project);

        XvFlow *missingFlow=project->createXvFlow("Missing ports");
        QVERIFY(missingFlow->createXvFunc("LoopFlow"));
        QCOMPARE(missingFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!missingFlow->isRunning(),5000);
        QCOMPARE(missingFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);

        XvFlow *openFlow=project->createXvFlow("Open body");
        auto loop=qobject_cast<LoopFlow*>(openFlow->createXvFunc("LoopFlow"));
        auto body=qobject_cast<PortFixtureFunc*>(
                    openFlow->createXvFunc("PortFixtureFunc"));
        auto done=qobject_cast<PortFixtureFunc*>(
                    openFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(loop && body && done);
        QVERIFY(loop->addSonFunc(body,"body"));
        QVERIFY(loop->addSonFunc(done,"done"));
        QCOMPARE(openFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!openFlow->isRunning(),5000);
        QCOMPARE(openFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
        QCOMPARE(body->getXvFuncRunInfo().runIdx,0U);
        QCOMPARE(done->getXvFuncRunInfo().runIdx,0U);
    }

    void loopFlowPropagatesBodyErrorsAndSupportsNestedContexts()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Nested loops");
        QVERIFY(project);

        XvFlow *errorFlow=project->createXvFlow("Body error");
        auto errorLoop=qobject_cast<LoopFlow*>(
                    errorFlow->createXvFunc("LoopFlow"));
        XvFunc *errorBody=errorFlow->createXvFunc("AlwaysErrorFunc");
        auto errorDone=qobject_cast<PortFixtureFunc*>(
                    errorFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(errorLoop && errorBody && errorDone);
        dynamic_cast<XInt*>(errorLoop->getParamsByName("end"))->setValue(3);
        QVERIFY(errorLoop->addSonFunc(errorBody,"body"));
        QVERIFY(errorLoop->addSonFunc(errorDone,"done"));
        QVERIFY(errorBody->addSonFunc(errorDone));
        errorFlow->getFlowConfig()->funcErrorInterruptRun=true;
        QCOMPARE(errorFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!errorFlow->isRunning(),5000);
        QCOMPARE(errorFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
        QCOMPARE(errorBody->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(errorDone->getXvFuncRunInfo().runIdx,0U);

        XvFlow *nestedFlow=project->createXvFlow("Nested");
        auto outer=qobject_cast<LoopFlow*>(nestedFlow->createXvFunc("LoopFlow"));
        auto inner=qobject_cast<LoopFlow*>(nestedFlow->createXvFunc("LoopFlow"));
        auto leaf=qobject_cast<PortFixtureFunc*>(
                    nestedFlow->createXvFunc("PortFixtureFunc"));
        auto afterInner=qobject_cast<PortFixtureFunc*>(
                    nestedFlow->createXvFunc("PortFixtureFunc"));
        auto outerDone=qobject_cast<PortFixtureFunc*>(
                    nestedFlow->createXvFunc("PortFixtureFunc"));
        QVERIFY(outer && inner && leaf && afterInner && outerDone);
        dynamic_cast<XInt*>(outer->getParamsByName("end"))->setValue(2);
        dynamic_cast<XInt*>(inner->getParamsByName("end"))->setValue(2);
        QVERIFY(outer->addSonFunc(inner,"body"));
        QVERIFY(outer->addSonFunc(outerDone,"done"));
        QVERIFY(inner->addSonFunc(leaf,"body"));
        QVERIFY(inner->addSonFunc(afterInner,"done"));
        QVERIFY(leaf->addSonFunc(afterInner,"true"));
        QVERIFY(afterInner->addSonFunc(outerDone,"true"));

        QCOMPARE(nestedFlow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!nestedFlow->isRunning(),5000);
        QCOMPARE(nestedFlow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QCOMPARE(outer->getXvFuncRunInfo().runIdx,1U);
        QCOMPARE(inner->getXvFuncRunInfo().runIdx,2U);
        QCOMPARE(leaf->getXvFuncRunInfo().runIdx,4U);
        QCOMPARE(afterInner->getXvFuncRunInfo().runIdx,2U);
        QCOMPARE(outerDone->getXvFuncRunInfo().runIdx,1U);
    }

    void roundTripCurrentOperators()
    {
        DefaultLocaleGuard localeGuard{QLocale(QLocale::German)};
        Q_UNUSED(localeGuard);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath=directory.filePath("first.xvproj");
        const QString secondPath=directory.filePath("second.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Round <trip> & project");
        QVERIFY(project);
        XvFlow *dataFlow=project->createXvFlow("Data flow");
        XvFlow *imageFlow=project->createXvFlow("Image flow");
        QVERIFY(dataFlow);
        QVERIFY(imageFlow);
        dataFlow->getFlowConfig()->loopInterval=275;
        dataFlow->getFlowConfig()->funcErrorInterruptRun=true;

        auto writer=qobject_cast<BaseDataWriter*>(dataFlow->createXvFunc("BaseDataWriter"));
        auto calculator=qobject_cast<BaseDataIntCalc*>(dataFlow->createXvFunc("BaseDataIntCalc"));
        auto acquisition=qobject_cast<ImageAcquisition*>(imageFlow->createXvFunc("ImageAcquisition"));
        QVERIFY(writer);
        QVERIFY(calculator);
        QVERIFY(acquisition);

        writer->setFuncName("Constants & values");
        writer->setCanvasPosition(QPointF(-12.5,48.25));
        dynamic_cast<XBool*>(writer->getResultsByName("boolResult"))->setValue(true);
        dynamic_cast<XInt*>(writer->getResultsByName("intResult"))->setValue(37);
        dynamic_cast<XReal*>(writer->getResultsByName("realResult"))->setValue(3.125);
        dynamic_cast<XString*>(writer->getResultsByName("stringResult"))->setValue("<configured>&text");

        calculator->setIntCalaType(BaseDataIntCalc::Mul);
        calculator->setCanvasPosition(QPointF(320.0,96.0));
        dynamic_cast<XInt*>(calculator->getParamsByName("intParam1"))->setValue(5);
        QVERIFY(writer->addSonFunc(calculator));
        QVERIFY(calculator->paramSubscribe("intParam2",writer,"intResult"));

        acquisition->setAcqType(ImageAcquisition::Dir);
        acquisition->setLocalFile("C:/images/one & two.png");
        acquisition->setLocalDir("C:/images/<batch>");
        acquisition->setCameraDeviceId("sim-directory:persisted-camera");
        acquisition->setCameraTimeoutMs(2345);
        acquisition->setCanvasPosition(QPointF(16.0,-8.0));

        const QString projectId=project->projectId();
        const QString dataFlowId=dataFlow->flowId();
        const QString imageFlowId=imageFlow->flowId();
        const QString writerId=writer->funcId();
        const QString calculatorId=calculator->funcId();
        const QString acquisitionId=acquisition->funcId();

        QVERIFY(manager->saveXvProject(firstPath));
        const QByteArray firstXml=readFile(firstPath);
        QVERIFY(!firstXml.isEmpty());
        QVERIFY(!firstXml.contains("outputImage"));
        QVERIFY(firstXml.contains(">3.125<"));

        QVERIFY(manager->loadXvProject(firstPath));
        XvProject *restored=manager->getXvProject();
        QVERIFY(restored);
        QCOMPARE(restored->projectId(),projectId);
        QCOMPARE(restored->projectName(),QString("Round <trip> & project"));

        XvFlow *restoredData=restored->getXvFlow(dataFlowId);
        XvFlow *restoredImage=restored->getXvFlow(imageFlowId);
        QVERIFY(restoredData);
        QVERIFY(restoredImage);
        QCOMPARE(restoredData->getFlowConfig()->loopInterval,275U);
        QVERIFY(restoredData->getFlowConfig()->funcErrorInterruptRun);

        auto restoredWriter=qobject_cast<BaseDataWriter*>(restoredData->getXvFunc(writerId));
        auto restoredCalculator=qobject_cast<BaseDataIntCalc*>(restoredData->getXvFunc(calculatorId));
        auto restoredAcquisition=qobject_cast<ImageAcquisition*>(restoredImage->getXvFunc(acquisitionId));
        QVERIFY(restoredWriter);
        QVERIFY(restoredCalculator);
        QVERIFY(restoredAcquisition);
        const auto tokenObjects=XvTokenMsgManager::getInstance()->getTokenMsgAbles();
        QCOMPARE(tokenObjects.value(projectId),static_cast<IXvTokenMsgAble*>(restored));
        QCOMPARE(tokenObjects.value(dataFlowId),static_cast<IXvTokenMsgAble*>(restoredData));
        QCOMPARE(tokenObjects.value(writerId),static_cast<IXvTokenMsgAble*>(restoredWriter));
        QCOMPARE(restoredWriter->canvasPosition(),QPointF(-12.5,48.25));
        QCOMPARE(restoredCalculator->canvasPosition(),QPointF(320.0,96.0));
        QCOMPARE(dynamic_cast<XBool*>(restoredWriter->getResultsByName("boolResult"))->value(),true);
        QCOMPARE(dynamic_cast<XInt*>(restoredWriter->getResultsByName("intResult"))->value(),37);
        QCOMPARE(dynamic_cast<XReal*>(restoredWriter->getResultsByName("realResult"))->value(),3.125);
        QCOMPARE(dynamic_cast<XString*>(restoredWriter->getResultsByName("stringResult"))->value(),QString("<configured>&text"));
        QCOMPARE(restoredCalculator->intCalaType(),BaseDataIntCalc::Mul);
        QCOMPARE(dynamic_cast<XInt*>(restoredCalculator->getParamsByName("intParam1"))->value(),5);
        QVERIFY(restoredWriter->existSonFunc(restoredCalculator));
        XvFunc::SubscribeInfo subscription;
        QVERIFY(restoredCalculator->getParamSubscribe("intParam2",subscription));
        QCOMPARE(subscription.first,static_cast<XvFunc*>(restoredWriter));
        QCOMPARE(subscription.second,QString("intResult"));
        QCOMPARE(restoredAcquisition->acqType(),ImageAcquisition::Dir);
        QCOMPARE(restoredAcquisition->localFile(),QString("C:/images/one & two.png"));
        QCOMPARE(restoredAcquisition->localDir(),QString("C:/images/<batch>"));
        QCOMPARE(restoredAcquisition->cameraDeviceId(),
                 QString("sim-directory:persisted-camera"));
        QCOMPARE(restoredAcquisition->cameraTimeoutMs(),2345);

        QVERIFY(manager->saveXvProject(secondPath));
        QCOMPARE(readFile(secondPath),firstXml);
    }

    void imageAcquisitionCameraModeRunsAndRecovers()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeSolidImage(directory.filePath("01-green.png"),Qt::green));
        QVERIFY(writeSolidImage(directory.filePath("02-blue.bmp"),Qt::blue));

        XvCamera::XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider);
        QVERIFY(provider->addDevice("operator-camera",directory.path()));
        const QString deviceId=
                XvCamera::XvDirectoryCameraProvider::deviceIdForLocalId("operator-camera");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Camera operator project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Camera flow");
        QVERIFY(flow);
        flow->getFlowConfig()->funcErrorInterruptRun=true;
        auto acquisition=qobject_cast<ImageAcquisition*>(
                    flow->createXvFunc("ImageAcquisition"));
        QVERIFY(acquisition);
        acquisition->setAcqType(ImageAcquisition::Camera);
        acquisition->setCameraDeviceId(deviceId);
        acquisition->setCameraTimeoutMs(100);

        auto output=dynamic_cast<XImage*>(
                    acquisition->getResultsByName("outputImage"));
        QVERIFY(output);
        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(flow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QCOMPARE(output->value().pixelColor(0,0),QColor(Qt::green));
        QCOMPARE(output->value().text("DeviceId"),deviceId);

        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(output->value().pixelColor(0,0),QColor(Qt::blue));

        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(flow->getXvFuncRunStatus(),EXvFlowRunStatus::Error);
        QVERIFY(output->value().isNull());

        QCOMPARE(flow->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),5000);
        QCOMPARE(flow->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
        QCOMPARE(output->value().pixelColor(0,0),QColor(Qt::green));

        auto camera=qobject_cast<XvCamera::XvDirectoryCamera*>(XvCameraMgr->camera(deviceId));
        QVERIFY(camera);
        QCOMPARE(camera->setParameter("loop",true),XvCamera::EXvCameraError::None);
        QCOMPARE(camera->setParameter("frameIntervalMs",50),XvCamera::EXvCameraError::None);
        acquisition->setCameraTimeoutMs(1);
        QCOMPARE(acquisition->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(output->value().isNull());

        QCOMPARE(camera->setParameter("frameIntervalMs",1000),
                 XvCamera::EXvCameraError::None);
        QCOMPARE(camera->startContinuous(),XvCamera::EXvCameraError::None);
        acquisition->setCameraTimeoutMs(100);
        QCOMPARE(acquisition->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(output->value().isNull());
        QCOMPARE(camera->stopContinuous(),XvCamera::EXvCameraError::None);

        acquisition->setCameraDeviceId("missing-camera");
        QCOMPARE(acquisition->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(output->value().isNull());
        QVERIFY(!acquisition->getXvFuncRunInfo().runMsg.isEmpty());
    }

    void imageAcquisitionCameraConfigPersistsAndMigrates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString projectPath=directory.filePath("camera.xvproj");
        const QString flowPath=directory.filePath("camera.xvflow");
        const QString legacyPath=directory.filePath("camera-legacy.xvproj");
        const QString invalidPath=directory.filePath("camera-invalid.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Camera persistence");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Camera flow");
        QVERIFY(flow);
        auto acquisition=qobject_cast<ImageAcquisition*>(
                    flow->createXvFunc("ImageAcquisition"));
        QVERIFY(acquisition);
        acquisition->setAcqType(ImageAcquisition::Camera);
        acquisition->setCameraDeviceId("sim-directory:saved-device");
        acquisition->setCameraTimeoutMs(4321);

        QVERIFY(manager->saveXvProject(projectPath));
        QVERIFY(manager->exportXvFlow(flow->flowId(),flowPath));
        QVERIFY(manager->loadXvProject(projectPath));
        XvFlow *restoredFlow=manager->getXvProject()->getXvFlows("Camera flow").value(0);
        auto restored=qobject_cast<ImageAcquisition*>(
                    findFunctionByRole(restoredFlow,"ImageAcquisition"));
        QVERIFY(restored);
        QCOMPARE(restored->acqType(),ImageAcquisition::Camera);
        QCOMPARE(restored->cameraDeviceId(),QString("sim-directory:saved-device"));
        QCOMPARE(restored->cameraTimeoutMs(),4321);

        XvFlow *imported=manager->importXvFlow(flowPath);
        QVERIFY(imported);
        auto importedAcquisition=qobject_cast<ImageAcquisition*>(
                    findFunctionByRole(imported,"ImageAcquisition"));
        QVERIFY(importedAcquisition);
        QCOMPARE(importedAcquisition->cameraDeviceId(),
                 QString("sim-directory:saved-device"));
        QCOMPARE(importedAcquisition->cameraTimeoutMs(),4321);

        QDomDocument legacy=parseDocument(projectPath);
        QDomElement legacyFunction;
        const QDomNodeList functions=legacy.elementsByTagName("Function");
        for(int index=0;index<functions.count();++index)
        {
            QDomElement candidate=functions.at(index).toElement();
            if(candidate.attribute("role")=="ImageAcquisition")
            {
                legacyFunction=candidate;
                break;
            }
        }
        QVERIFY(!legacyFunction.isNull());
        QDomElement legacyParameters=legacyFunction.firstChildElement("Parameters");
        for(QDomElement value=legacyParameters.firstChildElement("Value");
            !value.isNull();)
        {
            QDomElement next=value.nextSiblingElement("Value");
            const QString name=value.attribute("name");
            if(name=="cameraDeviceId" || name=="cameraTimeoutMs"
                    || name=="videoPath" || name=="videoStartFrame"
                    || name=="videoEndFrame" || name=="videoFrameStep"
                    || name=="videoLoop")
                legacyParameters.removeChild(value);
            value=next;
        }
        QVERIFY(writeDocument(legacyPath,legacy));
        QVERIFY(manager->loadXvProject(legacyPath));
        restoredFlow=manager->getXvProject()->getXvFlows("Camera flow").value(0);
        restored=qobject_cast<ImageAcquisition*>(
                    findFunctionByRole(restoredFlow,"ImageAcquisition"));
        QVERIFY(restored);
        QVERIFY(restored->cameraDeviceId().isEmpty());
        QCOMPARE(restored->cameraTimeoutMs(),1000);
        QVERIFY(restored->videoPath().isEmpty());
        QCOMPARE(restored->videoStartFrame(),0);
        QCOMPARE(restored->videoEndFrame(),-1);
        QCOMPARE(restored->videoFrameStep(),1);
        QVERIFY(!restored->videoLoop());

        QDomDocument invalid=parseDocument(projectPath);
        const QDomNodeList values=invalid.elementsByTagName("Value");
        bool timeoutFound=false;
        for(int index=0;index<values.count();++index)
        {
            QDomElement value=values.at(index).toElement();
            if(value.attribute("name")!="cameraTimeoutMs") continue;
            value.firstChild().setNodeValue("60001");
            timeoutFound=true;
            break;
        }
        QVERIFY(timeoutFound);
        QVERIFY(writeDocument(invalidPath,invalid));
        XvProject *installedLegacy=manager->getXvProject();
        expectLoadRejected(invalidPath,installedLegacy);
    }

    void cameraAcquisitionFeedsTemplateMatchProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QImage trainingImage=makeIntegrationImage();
        QVERIFY(trainingImage.save(directory.filePath("integration.png")));
        const QString projectPath=directory.filePath("integration.xvproj");

        XvCamera::XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider);
        QVERIFY(provider->addDevice("m2-camera",directory.path(),
                                    "M2 simulator",true,0));
        const QString deviceId=
                XvCamera::XvDirectoryCameraProvider::deviceIdForLocalId("m2-camera");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("M2 acquisition project");
        QVERIFY(project);
        XvFlow *captureFlow=project->createXvFlow("Capture and match");
        XvFlow *afterFlow=project->createXvFlow("After match");
        QVERIFY(captureFlow);
        QVERIFY(afterFlow);
        captureFlow->getFlowConfig()->funcErrorInterruptRun=true;

        auto acquisition=qobject_cast<ImageAcquisition*>(
                    captureFlow->createXvFunc("ImageAcquisition"));
        auto matcher=qobject_cast<HModelMatch*>(
                    captureFlow->createXvFunc("HModelMatch"));
        auto writer=qobject_cast<BaseDataWriter*>(
                    afterFlow->createXvFunc("BaseDataWriter"));
        QVERIFY(acquisition && matcher && writer);
        acquisition->setAcqType(ImageAcquisition::Camera);
        acquisition->setCameraDeviceId(deviceId);
        acquisition->setCameraTimeoutMs(100);

        auto matcherInput=dynamic_cast<XImage*>(
                    matcher->getParamsByName("inputImage"));
        auto matcherMode=dynamic_cast<XInt*>(matcher->getParamsByName("mode"));
        auto matcherCount=dynamic_cast<XInt*>(
                    matcher->getResultsByName("matchCount"));
        QVERIFY(matcherInput && matcherMode && matcherCount);
        matcherInput->setValue(trainingImage);
        matcherMode->setValue(HModelMatch::CreateTemplate);
        QVERIFY2(matcher->runXvFunc()==EXvFuncRunStatus::Ok,
                 qPrintable(matcher->getXvFuncRunMsg()));
        QVERIFY(matcher->hasTemplateModel());
        const QByteArray modelAsset=matcher->templateModelAsset();
        matcherMode->setValue(HModelMatch::FindTemplate);
        QVERIFY(acquisition->addSonFunc(matcher));
        QVERIFY(matcher->paramSubscribe("inputImage",acquisition,
                                        "outputImage"));

        const QString captureFlowId=captureFlow->flowId();
        const QString afterFlowId=afterFlow->flowId();
        QStringList startedFlows;
        QMetaObject::Connection startConnection=connect(
                    project,&XvProject::sgProjectFlowRunStart,this,
                    [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!project->isRunning(),5000);
        disconnect(startConnection);
        QVERIFY2(captureFlow->getXvFuncRunStatus()==EXvFlowRunStatus::Ok,
                 qPrintable(QString("acquisition: %1; matcher: %2")
                            .arg(acquisition->getXvFuncRunMsg(),matcher->getXvFuncRunMsg())));
        QCOMPARE(startedFlows,QStringList({captureFlowId,afterFlowId}));
        QCOMPARE(project->projectRunInfo().runStatus,EXvProjectRunStatus::Ok);
        QVERIFY(matcherCount->value()>=1);
        const int firstMatchCount=matcherCount->value();

        QVERIFY(manager->saveXvProject(projectPath));
        const QByteArray savedXml=readFile(projectPath);
        QVERIFY(savedXml.contains("cameraDeviceId"));
        QVERIFY(savedXml.contains("halcon-shape-model"));
        QVERIFY(manager->loadXvProject(projectPath));
        XvProject *restored=manager->getXvProject();
        QVERIFY(restored);
        XvFlow *restoredCapture=restored->getXvFlow(captureFlowId);
        QVERIFY(restoredCapture);
        auto restoredAcquisition=qobject_cast<ImageAcquisition*>(
                    findFunctionByRole(restoredCapture,"ImageAcquisition"));
        auto restoredMatcher=qobject_cast<HModelMatch*>(
                    findFunctionByRole(restoredCapture,"HModelMatch"));
        QVERIFY(restoredAcquisition && restoredMatcher);
        QCOMPARE(restoredAcquisition->cameraDeviceId(),deviceId);
        QCOMPARE(restoredAcquisition->cameraTimeoutMs(),100);
        QCOMPARE(restoredMatcher->templateModelAsset(),modelAsset);
        XvFunc::SubscribeInfo subscription;
        QVERIFY(restoredMatcher->getParamSubscribe("inputImage",subscription));
        QCOMPARE(subscription.first->funcRole(),QString("ImageAcquisition"));
        QCOMPARE(subscription.second,QString("outputImage"));

        startedFlows.clear();
        startConnection=connect(restored,&XvProject::sgProjectFlowRunStart,this,
                                [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(restored->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!restored->isRunning(),5000);
        disconnect(startConnection);
        QCOMPARE(startedFlows,QStringList({captureFlowId,afterFlowId}));
        QCOMPARE(restored->projectRunInfo().runStatus,EXvProjectRunStatus::Ok);
        auto restoredCount=dynamic_cast<XInt*>(
                    restoredMatcher->getResultsByName("matchCount"));
        QVERIFY(restoredCount);
        QCOMPARE(restoredCount->value(),firstMatchCount);

        XvProjectConfig loopConfig=restored->projectConfig();
        loopConfig.loopInterval=1;
        QVERIFY(restored->setProjectConfig(loopConfig));
        std::atomic_int loopRunCount{0};
        const QMetaObject::Connection loopConnection=connect(
                    restoredMatcher,&XvFunc::sgFuncRunEnd,this,
                    [&loopRunCount](XvFunc*) { loopRunCount.fetch_add(1); },
                    Qt::DirectConnection);
        QCOMPARE(restored->runLoop(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(loopRunCount.load()>=20,15000);
        QCOMPARE(restored->stop(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!restored->isRunning(),5000);
        disconnect(loopConnection);
        QCOMPARE(restored->projectRunInfo().runStatus,EXvProjectRunStatus::Stopped);

        XvFlow *failureFlow=restored->createXvFlow("Missing camera");
        XvFlow *successFlow=restored->createXvFlow("Failure policy follow-up");
        QVERIFY(failureFlow && successFlow);
        failureFlow->getFlowConfig()->funcErrorInterruptRun=true;
        auto failureAcquisition=qobject_cast<ImageAcquisition*>(
                    failureFlow->createXvFunc("ImageAcquisition"));
        QVERIFY(failureAcquisition);
        failureAcquisition->setAcqType(ImageAcquisition::Camera);
        failureAcquisition->setCameraDeviceId("sim-directory:missing-for-m2");
        failureAcquisition->setCameraTimeoutMs(10);
        QVERIFY(successFlow->createXvFunc("BaseDataWriter"));

        XvProjectConfig failureConfig;
        failureConfig.mainFlows={XvProjectFlowEntry(failureFlow->flowId()),
                                 XvProjectFlowEntry(successFlow->flowId()),
                                 XvProjectFlowEntry(captureFlowId,false),
                                 XvProjectFlowEntry(afterFlowId,false)};
        failureConfig.flowErrorPolicy=EXvProjectFlowErrorPolicy::Stop;
        QVERIFY(restored->setProjectConfig(failureConfig));
        startedFlows.clear();
        startConnection=connect(restored,&XvProject::sgProjectFlowRunStart,this,
                                [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(restored->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!restored->isRunning(),5000);
        disconnect(startConnection);
        QCOMPARE(startedFlows,QStringList({failureFlow->flowId()}));
        XvProjectRunInfo failureInfo=restored->projectRunInfo();
        QCOMPARE(failureInfo.runStatus,EXvProjectRunStatus::Error);
        QCOMPARE(failureInfo.runCode,Ret_Xv_ProjectFlowFailed);
        QCOMPARE(failureInfo.failedFlowIds,QStringList({failureFlow->flowId()}));

        failureConfig.flowErrorPolicy=EXvProjectFlowErrorPolicy::Continue;
        QVERIFY(restored->setProjectConfig(failureConfig));
        startedFlows.clear();
        startConnection=connect(restored,&XvProject::sgProjectFlowRunStart,this,
                                [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(restored->runOnce(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(!restored->isRunning(),5000);
        disconnect(startConnection);
        QCOMPARE(startedFlows,QStringList({failureFlow->flowId(),successFlow->flowId()}));
        failureInfo=restored->projectRunInfo();
        QCOMPARE(failureInfo.runStatus,EXvProjectRunStatus::Error);
        QCOMPARE(failureInfo.runCode,Ret_Xv_ProjectFlowFailed);
        QCOMPARE(failureInfo.failedFlowIds,QStringList({failureFlow->flowId()}));
    }

    void projectConfigLifecycleAndAtomicUpdate()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Configured project");
        QVERIFY(project);
        QCOMPARE(project->projectConfig().mainFlows.count(),0);

        XvFlow *first=project->createXvFlow("First");
        XvFlow *second=project->createXvFlow("Second");
        QVERIFY(first);
        QVERIFY(second);
        XvProjectConfig defaults=project->projectConfig();
        QCOMPARE(defaults.mainFlows.count(),2);
        QCOMPARE(defaults.mainFlows.at(0).flowId,first->flowId());
        QCOMPARE(defaults.mainFlows.at(1).flowId,second->flowId());
        QVERIFY(defaults.mainFlows.at(0).enabled);
        QCOMPARE(defaults.loopInterval,XvProjectConfig::DefaultLoopInterval);
        QCOMPARE(defaults.flowErrorPolicy,EXvProjectFlowErrorPolicy::Stop);

        XvProjectConfig configured;
        configured.mainFlows={XvProjectFlowEntry(second->flowId(),false),
                              XvProjectFlowEntry(first->flowId(),true)};
        configured.loopInterval=325;
        configured.flowErrorPolicy=EXvProjectFlowErrorPolicy::Continue;
        QVERIFY(project->setProjectConfig(configured));
        QVERIFY(project->projectConfig()==configured);

        XvProjectConfig invalid=configured;
        invalid.mainFlows[1].flowId=second->flowId();
        QVERIFY(!project->setProjectConfig(invalid));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(project->projectConfig()==configured);

        invalid=configured;
        invalid.mainFlows.removeLast();
        QVERIFY(!project->setProjectConfig(invalid));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(project->projectConfig()==configured);

        invalid=configured;
        invalid.mainFlows[0].flowId="00000000000000000000000000000000";
        QVERIFY(!project->setProjectConfig(invalid));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(project->projectConfig()==configured);

        invalid=configured;
        invalid.loopInterval=XvProjectConfig::MaximumLoopInterval+1;
        QVERIFY(!project->setProjectConfig(invalid));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(project->projectConfig()==configured);

        invalid=configured;
        invalid.flowErrorPolicy=static_cast<EXvProjectFlowErrorPolicy>(99);
        QVERIFY(!project->setProjectConfig(invalid));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(project->projectConfig()==configured);

        QVERIFY(project->removeXvFlow(second->flowId()));
        const XvProjectConfig afterRemove=project->projectConfig();
        QCOMPARE(afterRemove.mainFlows.count(),1);
        QCOMPARE(afterRemove.mainFlows.at(0).flowId,first->flowId());
        QVERIFY(afterRemove.mainFlows.at(0).enabled);
        QCOMPARE(afterRemove.loopInterval,configured.loopInterval);
        QCOMPARE(afterRemove.flowErrorPolicy,configured.flowErrorPolicy);
    }

    void projectConfigRoundTripEmptyDisabledAndLegacy()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString emptyPath=directory.filePath("empty.xvproj");
        const QString firstPath=directory.filePath("configured.xvproj");
        const QString secondPath=directory.filePath("configured-again.xvproj");
        const QString legacyPath=directory.filePath("legacy.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *empty=manager->createNewXvProject("Empty");
        QVERIFY(empty);
        QVERIFY(manager->saveXvProject(emptyPath));
        QVERIFY(manager->loadXvProject(emptyPath));
        QCOMPARE(manager->getXvProject()->projectConfig().mainFlows.count(),0);

        XvProject *project=manager->createNewXvProject("Round trip config");
        QVERIFY(project);
        XvFlow *first=project->createXvFlow("First");
        XvFlow *second=project->createXvFlow("Second");
        QVERIFY(first);
        QVERIFY(second);
        const QString firstId=first->flowId();
        const QString secondId=second->flowId();

        XvProjectConfig configured;
        configured.mainFlows={XvProjectFlowEntry(secondId,false),
                              XvProjectFlowEntry(firstId,false)};
        configured.loopInterval=0;
        configured.flowErrorPolicy=EXvProjectFlowErrorPolicy::Continue;
        QVERIFY(project->setProjectConfig(configured));
        QVERIFY(manager->saveXvProject(firstPath));
        const QByteArray firstXml=readFile(firstPath);
        QVERIFY(firstXml.contains("flowErrorPolicy=\"continue\""));
        QVERIFY(firstXml.contains("enabled=\"false\""));

        QVERIFY(manager->loadXvProject(firstPath));
        QVERIFY(manager->getXvProject()->projectConfig()==configured);
        QVERIFY(manager->saveXvProject(secondPath));
        QCOMPARE(readFile(secondPath),firstXml);

        QDomDocument legacy=parseDocument(firstPath);
        QDomElement projectElement=legacy.documentElement().firstChildElement("Project");
        projectElement.removeChild(projectElement.firstChildElement("Config"));
        QVERIFY(writeDocument(legacyPath,legacy));
        QVERIFY(manager->loadXvProject(legacyPath));

        XvProjectConfig migrated=manager->getXvProject()->projectConfig();
        QCOMPARE(migrated.mainFlows.count(),2);
        const QStringList sortedIds=firstId<secondId
                ?QStringList{firstId,secondId}:QStringList{secondId,firstId};
        QCOMPARE(migrated.mainFlows.at(0).flowId,sortedIds.at(0));
        QCOMPARE(migrated.mainFlows.at(1).flowId,sortedIds.at(1));
        QVERIFY(migrated.mainFlows.at(0).enabled);
        QVERIFY(migrated.mainFlows.at(1).enabled);
        QCOMPARE(migrated.loopInterval,XvProjectConfig::DefaultLoopInterval);
        QCOMPARE(migrated.flowErrorPolicy,EXvProjectFlowErrorPolicy::Stop);
    }

    void projectConfigRejectsInvalidXmlWithoutReplacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString validPath=directory.filePath("valid-config.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Preserved config");
        QVERIFY(project);
        QVERIFY(project->createXvFlow("First"));
        QVERIFY(project->createXvFlow("Second"));
        QVERIFY(manager->saveXvProject(validPath));
        const XvProjectConfig originalConfig=project->projectConfig();

        auto reject=[&](const QString &name,const std::function<void(QDomDocument&)> &mutate)
        {
            QDomDocument document=parseDocument(validPath);
            mutate(document);
            const QString path=directory.filePath(name+".xvproj");
            QVERIFY(writeDocument(path,document));
            expectLoadRejected(path,project);
            QVERIFY(project->projectConfig()==originalConfig);
        };

        reject("duplicate-flow-ref",[](QDomDocument &document)
        {
            QDomNodeList refs=document.elementsByTagName("FlowRef");
            refs.at(1).toElement().setAttribute("id",refs.at(0).toElement().attribute("id"));
        });
        reject("missing-flow-ref",[](QDomDocument &document)
        {
            QDomElement mainFlows=document.elementsByTagName("MainFlows").at(0).toElement();
            mainFlows.removeChild(mainFlows.firstChildElement("FlowRef"));
        });
        reject("unknown-flow-ref",[](QDomDocument &document)
        {
            document.elementsByTagName("FlowRef").at(0).toElement()
                    .setAttribute("id","00000000000000000000000000000000");
        });
        reject("invalid-enabled",[](QDomDocument &document)
        {
            document.elementsByTagName("FlowRef").at(0).toElement()
                    .setAttribute("enabled","1");
        });
        reject("invalid-interval",[](QDomDocument &document)
        {
            document.elementsByTagName("Config").at(0).toElement()
                    .setAttribute("loopInterval","100000");
        });
        reject("invalid-policy",[](QDomDocument &document)
        {
            document.elementsByTagName("Config").at(0).toElement()
                    .setAttribute("flowErrorPolicy","ignore");
        });
        reject("duplicate-config",[](QDomDocument &document)
        {
            QDomElement projectElement=document.elementsByTagName("Project").at(0).toElement();
            projectElement.appendChild(projectElement.firstChildElement("Config").cloneNode(true));
        });
        reject("unknown-config-field",[](QDomDocument &document)
        {
            document.elementsByTagName("Config").at(0).toElement()
                    .setAttribute("parallel","true");
        });
    }

    void projectSequentialExecutionHonorsOrderAndDisabledFlows()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Sequential project");
        QVERIFY(project);
        XvFlow *first=project->createXvFlow("First");
        XvFlow *disabled=project->createXvFlow("Disabled");
        XvFlow *last=project->createXvFlow("Last");
        QVERIFY(first);
        QVERIFY(disabled);
        QVERIFY(last);
        QVERIFY(first->createXvFunc("BaseDataWriter"));
        QVERIFY(disabled->createXvFunc("BaseDataWriter"));
        QVERIFY(last->createXvFunc("BaseDataWriter"));

        XvProjectConfig config;
        config.mainFlows={XvProjectFlowEntry(last->flowId(),true),
                          XvProjectFlowEntry(disabled->flowId(),false),
                          XvProjectFlowEntry(first->flowId(),true)};
        config.loopInterval=10;
        config.flowErrorPolicy=EXvProjectFlowErrorPolicy::Stop;
        QVERIFY(project->setProjectConfig(config));

        QStringList startedFlows;
        const QMetaObject::Connection connection=connect(
                    project,&XvProject::sgProjectFlowRunStart,this,
                    [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QVERIFY(project->isRunning());
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        disconnect(connection);

        QCOMPARE(startedFlows,QStringList({last->flowId(),first->flowId()}));
        QVERIFY(!project->isRunning());
        const XvProjectRunInfo info=project->projectRunInfo();
        QCOMPARE(info.runIdx,1U);
        QCOMPARE(info.runStatus,EXvProjectRunStatus::Ok);
        QCOMPARE(info.runCode,Ret_Xv_Success);
        QVERIFY(info.currentFlowId.isEmpty());
        QVERIFY(info.failedFlowIds.isEmpty());
        QVERIFY(info.runElapsed>=0.0);

        QCOMPARE(first->runOnce(),Ret_Xv_Success);
        QCOMPARE(first->wait(5000),Ret_Xv_Success);
        QCOMPARE(first->getXvFuncRunStatus(),EXvFlowRunStatus::Ok);
    }

    void projectExecutionSupportsEmptyAndAllDisabledConfigurations()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Empty run project");
        QVERIFY(project);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        QCOMPARE(project->projectRunInfo().runStatus,EXvProjectRunStatus::Ok);

        XvFlow *flow=project->createXvFlow("Disabled");
        QVERIFY(flow);
        QVERIFY(flow->createXvFunc("BaseDataWriter"));
        XvProjectConfig config=project->projectConfig();
        config.mainFlows[0].enabled=false;
        config.loopInterval=0;
        QVERIFY(project->setProjectConfig(config));

        int startCount=0;
        const QMetaObject::Connection connection=connect(
                    project,&XvProject::sgProjectFlowRunStart,this,
                    [&startCount](XvFlow*) { ++startCount; },Qt::DirectConnection);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        disconnect(connection);
        QCOMPARE(startCount,0);
        QCOMPARE(project->projectRunInfo().runStatus,EXvProjectRunStatus::Ok);
    }

    void projectExecutionAppliesStopAndContinueErrorPolicies()
    {
        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Policy project");
        QVERIFY(project);
        XvFlow *errorFlow=project->createXvFlow("Error");
        XvFlow *successFlow=project->createXvFlow("Success");
        QVERIFY(errorFlow);
        QVERIFY(successFlow);
        QVERIFY(errorFlow->createXvFunc("AlwaysErrorFunc"));
        QVERIFY(successFlow->createXvFunc("BaseDataWriter"));
        errorFlow->getFlowConfig()->funcErrorInterruptRun=true;

        XvProjectConfig config;
        config.mainFlows={XvProjectFlowEntry(errorFlow->flowId()),
                          XvProjectFlowEntry(successFlow->flowId())};
        config.flowErrorPolicy=EXvProjectFlowErrorPolicy::Stop;
        QVERIFY(project->setProjectConfig(config));

        QStringList startedFlows;
        QMetaObject::Connection connection=connect(
                    project,&XvProject::sgProjectFlowRunStart,this,
                    [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        disconnect(connection);
        QCOMPARE(startedFlows,QStringList({errorFlow->flowId()}));
        XvProjectRunInfo info=project->projectRunInfo();
        QCOMPARE(info.runStatus,EXvProjectRunStatus::Error);
        QCOMPARE(info.runCode,Ret_Xv_ProjectFlowFailed);
        QCOMPARE(info.failedFlowIds,QStringList({errorFlow->flowId()}));

        config.flowErrorPolicy=EXvProjectFlowErrorPolicy::Continue;
        QVERIFY(project->setProjectConfig(config));
        startedFlows.clear();
        connection=connect(project,&XvProject::sgProjectFlowRunStart,this,
                           [&startedFlows](XvFlow *flow)
        {
            if(flow) startedFlows.append(flow->flowId());
        },Qt::DirectConnection);
        QCOMPARE(project->runOnce(),Ret_Xv_Success);
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        disconnect(connection);
        QCOMPARE(startedFlows,
                 QStringList({errorFlow->flowId(),successFlow->flowId()}));
        info=project->projectRunInfo();
        QCOMPARE(info.runIdx,2U);
        QCOMPARE(info.runStatus,EXvProjectRunStatus::Error);
        QCOMPARE(info.failedFlowIds,QStringList({errorFlow->flowId()}));
    }

    void projectLoopStopWaitAndEditingProtection()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("running-project.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Loop project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Loop flow");
        QVERIFY(flow);
        XvFunc *writer=flow->createXvFunc("BaseDataWriter");
        QVERIFY(writer);
        XvProjectConfig config=project->projectConfig();
        config.loopInterval=1000;
        QVERIFY(project->setProjectConfig(config));
        QVERIFY(manager->saveXvProject(path));

        QCOMPARE(project->runLoop(),Ret_Xv_Success);
        QVERIFY(project->isRunning());
        QCOMPARE(project->runOnce(),Ret_Xv_ProjectRunning);
        QCOMPARE(project->wait(1),Ret_Xv_ProjectWaitTimeOut);
        QVERIFY(!project->createXvFlow("Rejected"));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(!project->removeXvFlow(flow->flowId()));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(!project->setProjectConfig(config));
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QVERIFY(!flow->createXvFunc("BaseDataWriter"));
        QVERIFY(!flow->lastErrorMsg().isEmpty());
        QVERIFY(!writer->addSonFunc(writer));
        QVERIFY(!writer->lastErrorMsg().isEmpty());
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());

        QCOMPARE(project->stop(),Ret_Xv_Success);
        QCOMPARE(project->wait(5000),Ret_Xv_Success);
        QVERIFY(!project->isRunning());
        const XvProjectRunInfo info=project->projectRunInfo();
        QCOMPARE(info.runStatus,EXvProjectRunStatus::Stopped);
        QCOMPARE(info.runCode,Ret_Xv_ProjectStopped);
        QVERIFY(info.currentFlowId.isEmpty());
        QCOMPARE(project->stop(),Ret_Xv_ProjectNoRun);

        flow->getFlowConfig()->loopInterval=1000;
        QCOMPARE(flow->runLoop(),Ret_Xv_Success);
        QVERIFY(flow->isRunning());
        QCOMPARE(project->runOnce(),Ret_Xv_ProjectRunning);
        QVERIFY(!project->lastErrorMsg().isEmpty());
        QCOMPARE(flow->stop(),Ret_Xv_Success);
        QCOMPARE(flow->wait(5000),Ret_Xv_Success);
    }

    void flowExportImportClonesIdentityAndSemantics()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("reusable.xvflow");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Flow library");
        QVERIFY(project);
        XvFlow *source=project->createXvFlow("Reusable flow");
        QVERIFY(source);
        source->getFlowConfig()->loopInterval=425;
        source->getFlowConfig()->funcErrorInterruptRun=true;

        auto writer=qobject_cast<BaseDataWriter*>(source->createXvFunc("BaseDataWriter"));
        auto calculator=qobject_cast<BaseDataIntCalc*>(source->createXvFunc("BaseDataIntCalc"));
        QVERIFY(writer);
        QVERIFY(calculator);
        writer->setFuncName("Source values");
        writer->setCanvasPosition(QPointF(-40.5,12.25));
        calculator->setFuncName("Imported calculation");
        calculator->setCanvasPosition(QPointF(280.0,72.0));
        calculator->setIntCalaType(BaseDataIntCalc::Sub);
        dynamic_cast<XInt*>(writer->getResultsByName("intResult"))->setValue(91);
        dynamic_cast<XInt*>(calculator->getParamsByName("intParam1"))->setValue(7);
        QVERIFY(writer->addSonFunc(calculator));
        QVERIFY(calculator->paramSubscribe("intParam2",writer,"intResult"));

        const QString sourceFlowId=source->flowId();
        const QString sourceWriterId=writer->funcId();
        const QString sourceCalculatorId=calculator->funcId();
        QVERIFY(manager->exportXvFlow(sourceFlowId,path));
        const QByteArray exportedXml=readFile(path);
        QVERIFY(!exportedXml.isEmpty());
        QVERIFY(manager->exportXvFlow(sourceFlowId,path));
        QCOMPARE(readFile(path),exportedXml);
        QCOMPARE(XvCoreManager::flowFileSuffix(),QString("xvflow"));

        QDomDocument document=parseDocument(path);
        QDomElement root=document.documentElement();
        QCOMPARE(root.tagName(),QString("XVisionFlow"));
        QCOMPARE(root.attribute("format"),QString("xvision-flow"));
        QCOMPARE(root.attribute("version"),QString("1"));
        QCOMPARE(root.elementsByTagName("Flow").count(),1);
        QCOMPARE(root.firstChildElement("Flow").attribute("id"),sourceFlowId);

        const int flowCount=project->xvFlowCount();
        QVERIFY(manager->validateXvFlowFile(path));
        QCOMPARE(project->xvFlowCount(),flowCount);
        XvFlow *first=manager->importXvFlow(path);
        XvFlow *second=manager->importXvFlow(path);
        QVERIFY(first);
        QVERIFY(second);
        QCOMPARE(project->xvFlowCount(),flowCount+2);
        QVERIFY(first->flowId()!=sourceFlowId);
        QVERIFY(second->flowId()!=sourceFlowId);
        QVERIFY(first->flowId()!=second->flowId());

        QSet<QString> allIds={sourceFlowId,sourceWriterId,sourceCalculatorId,
                              first->flowId(),second->flowId()};
        for(XvFlow *imported:{first,second})
        {
            QCOMPARE(imported->flowName(),source->flowName());
            QCOMPARE(imported->getFlowConfig()->loopInterval,425U);
            QVERIFY(imported->getFlowConfig()->funcErrorInterruptRun);
            QCOMPARE(imported->xvFuncCount(),2);

            auto importedWriter=qobject_cast<BaseDataWriter*>(
                        findFunctionByRole(imported,"BaseDataWriter"));
            auto importedCalculator=qobject_cast<BaseDataIntCalc*>(
                        findFunctionByRole(imported,"BaseDataIntCalc"));
            QVERIFY(importedWriter);
            QVERIFY(importedCalculator);
            QVERIFY(!allIds.contains(importedWriter->funcId()));
            allIds.insert(importedWriter->funcId());
            QVERIFY(!allIds.contains(importedCalculator->funcId()));
            allIds.insert(importedCalculator->funcId());

            QCOMPARE(importedWriter->funcName(),writer->funcName());
            QCOMPARE(importedWriter->canvasPosition(),QPointF(-40.5,12.25));
            QCOMPARE(importedCalculator->funcName(),calculator->funcName());
            QCOMPARE(importedCalculator->canvasPosition(),QPointF(280.0,72.0));
            QCOMPARE(importedCalculator->intCalaType(),BaseDataIntCalc::Sub);
            QCOMPARE(dynamic_cast<XInt*>(importedWriter->getResultsByName("intResult"))->value(),91);
            QCOMPARE(dynamic_cast<XInt*>(importedCalculator->getParamsByName("intParam1"))->value(),7);
            QVERIFY(importedWriter->existSonFunc(importedCalculator));
            XvFunc::SubscribeInfo subscription;
            QVERIFY(importedCalculator->getParamSubscribe("intParam2",subscription));
            QCOMPARE(subscription.first,static_cast<XvFunc*>(importedWriter));
            QCOMPARE(subscription.second,QString("intResult"));

            const auto tokens=XvTokenMsgManager::getInstance()->getTokenMsgAbles();
            QCOMPARE(tokens.value(imported->flowId()),
                     static_cast<IXvTokenMsgAble*>(imported));
            QCOMPARE(tokens.value(importedWriter->funcId()),
                     static_cast<IXvTokenMsgAble*>(importedWriter));
            QCOMPARE(tokens.value(importedCalculator->funcId()),
                     static_cast<IXvTokenMsgAble*>(importedCalculator));
        }
    }

    void rejectsInvalidFlowFilesWithoutChangingProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString validPath=directory.filePath("valid.xvflow");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Preserved flow project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Source");
        QVERIFY(flow);
        XvFunc *writer=flow->createXvFunc("BaseDataWriter");
        XvFunc *calculator=flow->createXvFunc("BaseDataIntCalc");
        QVERIFY(writer);
        QVERIFY(calculator);
        QVERIFY(writer->addSonFunc(calculator));
        QVERIFY(calculator->paramSubscribe("intParam2",writer,"intResult"));
        QVERIFY(manager->exportXvFlow(flow->flowId(),validPath));
        const int flowCount=project->xvFlowCount();

        QDomDocument document=parseDocument(validPath);
        document.documentElement().setAttribute("unsupported","value");
        const QString attributePath=directory.filePath("attribute.xvflow");
        QVERIFY(writeDocument(attributePath,document));
        expectFlowImportRejected(attributePath,project,flowCount);

        document=parseDocument(validPath);
        document.documentElement().setAttribute("version","999");
        const QString versionPath=directory.filePath("version.xvflow");
        QVERIFY(writeDocument(versionPath,document));
        expectFlowImportRejected(versionPath,project,flowCount);

        document=parseDocument(validPath);
        QDomNodeList functions=document.elementsByTagName("Function");
        QCOMPARE(functions.count(),2);
        functions.at(1).toElement().setAttribute(
                    "id",functions.at(0).toElement().attribute("id"));
        const QString duplicatePath=directory.filePath("duplicate.xvflow");
        QVERIFY(writeDocument(duplicatePath,document));
        expectFlowImportRejected(duplicatePath,project,flowCount);

        document=parseDocument(validPath);
        document.elementsByTagName("Function").at(0).toElement()
                .setAttribute("role","MissingOperatorRole");
        const QString rolePath=directory.filePath("role.xvflow");
        QVERIFY(writeDocument(rolePath,document));
        expectFlowImportRejected(rolePath,project,flowCount);

        document=parseDocument(validPath);
        document.elementsByTagName("Link").at(0).toElement()
                .setAttribute("to","00000000000000000000000000000000");
        const QString linkPath=directory.filePath("link.xvflow");
        QVERIFY(writeDocument(linkPath,document));
        expectFlowImportRejected(linkPath,project,flowCount);

        document=parseDocument(validPath);
        document.elementsByTagName("Subscription").at(0).toElement()
                .setAttribute("sourceFunction","00000000000000000000000000000000");
        const QString subscriptionPath=directory.filePath("subscription.xvflow");
        QVERIFY(writeDocument(subscriptionPath,document));
        expectFlowImportRejected(subscriptionPath,project,flowCount);
    }

    void rejectsInvalidFilesWithoutReplacingProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString validPath=directory.filePath("valid.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Preserved project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Flow");
        QVERIFY(flow);
        XvFunc *writer=flow->createXvFunc("BaseDataWriter");
        XvFunc *calculator=flow->createXvFunc("BaseDataIntCalc");
        QVERIFY(writer);
        QVERIFY(calculator);
        QVERIFY(writer->addSonFunc(calculator));
        QVERIFY(calculator->paramSubscribe("intParam2",writer,"intResult"));
        QVERIFY(manager->saveXvProject(validPath));

        expectLoadRejected(directory.filePath("missing.xvproj"),project);

        const QString malformedPath=directory.filePath("malformed.xvproj");
        QFile malformed(malformedPath);
        QVERIFY(malformed.open(QIODevice::WriteOnly));
        QVERIFY(malformed.write("<XVisionProject>")>0);
        malformed.close();
        expectLoadRejected(malformedPath,project);

        QDomDocument document=parseDocument(validPath);
        QVERIFY(!document.isNull());
        document.documentElement().setAttribute("version","999");
        const QString versionPath=directory.filePath("version.xvproj");
        QVERIFY(writeDocument(versionPath,document));
        expectLoadRejected(versionPath,project);

        document=parseDocument(validPath);
        QDomNodeList functions=document.elementsByTagName("Function");
        QCOMPARE(functions.count(),2);
        functions.at(1).toElement().setAttribute(
                    "id",functions.at(0).toElement().attribute("id"));
        const QString duplicatePath=directory.filePath("duplicate.xvproj");
        QVERIFY(writeDocument(duplicatePath,document));
        expectLoadRejected(duplicatePath,project);

        document=parseDocument(validPath);
        document.elementsByTagName("Function").at(0).toElement()
                .setAttribute("role","MissingOperatorRole");
        const QString rolePath=directory.filePath("role.xvproj");
        QVERIFY(writeDocument(rolePath,document));
        expectLoadRejected(rolePath,project);

        document=parseDocument(validPath);
        QDomElement value=document.elementsByTagName("Value").at(0).toElement();
        value.setAttribute("type","XString");
        const QString valuePath=directory.filePath("value.xvproj");
        QVERIFY(writeDocument(valuePath,document));
        expectLoadRejected(valuePath,project);

        document=parseDocument(validPath);
        QDomElement property=document.elementsByTagName("Property").at(0).toElement();
        property.firstChild().setNodeValue("999");
        const QString enumPath=directory.filePath("enum.xvproj");
        QVERIFY(writeDocument(enumPath,document));
        expectLoadRejected(enumPath,project);

        document=parseDocument(validPath);
        QDomElement subscription=document.elementsByTagName("Subscription").at(0).toElement();
        subscription.setAttribute("sourceFunction","00000000000000000000000000000000");
        const QString subscriptionPath=directory.filePath("subscription.xvproj");
        QVERIFY(writeDocument(subscriptionPath,document));
        expectLoadRejected(subscriptionPath,project);

        document=parseDocument(validPath);
        functions=document.elementsByTagName("Function");
        QDomElement links=document.elementsByTagName("Links").at(0).toElement();
        QDomElement invalidLink=document.createElement("Link");
        invalidLink.setAttribute("from",functions.at(0).toElement().attribute("id"));
        invalidLink.setAttribute("to","00000000000000000000000000000000");
        links.appendChild(invalidLink);
        const QString linkPath=directory.filePath("link.xvproj");
        QVERIFY(writeDocument(linkPath,document));
        expectLoadRejected(linkPath,project);
    }

    void serializationFailurePreservesExistingFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("atomic.xvproj");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Atomic project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Flow");
        QVERIFY(flow);
        QVERIFY(flow->createXvFunc("BaseDataWriter"));
        QVERIFY(manager->saveXvProject(path));
        const QByteArray original=readFile(path);
        QVERIFY(!original.isEmpty());

        QVERIFY(!manager->saveXvProject(directory.path()));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(path),original);

        QVERIFY(flow->createXvFunc("BrokenPersistentFunc"));
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(path),original);
    }

    void flowSerializationFailurePreservesExistingFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("atomic.xvflow");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Atomic flow project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Flow");
        QVERIFY(flow);
        QVERIFY(flow->createXvFunc("BaseDataWriter"));
        QVERIFY(manager->exportXvFlow(flow->flowId(),path));
        const QByteArray original=readFile(path);
        QVERIFY(!original.isEmpty());

        QVERIFY(!manager->exportXvFlow(flow->flowId(),directory.path()));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(path),original);

        QVERIFY(flow->createXvFunc("BrokenPersistentFunc"));
        QVERIFY(!manager->exportXvFlow(flow->flowId(),path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(path),original);
    }

    void runningFlowRejectsFileOperations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path=directory.filePath("running.xvproj");
        const QString flowPath=directory.filePath("running.xvflow");

        XvCoreManager *manager=XvCoreManager::getInstance();
        XvProject *project=manager->createNewXvProject("Running project");
        QVERIFY(project);
        XvFlow *flow=project->createXvFlow("Loop");
        QVERIFY(flow);
        QVERIFY(flow->createXvFunc("BaseDataWriter"));
        flow->getFlowConfig()->loopInterval=250;
        QVERIFY(manager->saveXvProject(path));
        QVERIFY(manager->exportXvFlow(flow->flowId(),flowPath));
        const QByteArray original=readFile(path);
        const QByteArray originalFlow=readFile(flowPath);

        QCOMPARE(flow->runLoop(),Ret_Xv_Success);
        QTRY_VERIFY(flow->isRunning());

        QVERIFY(!manager->validateXvProjectFile(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QVERIFY(!manager->saveXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(path),original);
        QVERIFY(!manager->loadXvProject(path));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(manager->getXvProject(),project);
        QVERIFY(!manager->validateXvFlowFile(flowPath));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QVERIFY(!manager->importXvFlow(flowPath));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QVERIFY(!manager->exportXvFlow(flow->flowId(),flowPath));
        QVERIFY(!manager->lastErrorMsg().isEmpty());
        QCOMPARE(readFile(flowPath),originalFlow);

        QCOMPARE(flow->stop(),Ret_Xv_Success);
        QTest::qWait(300);
        QVERIFY(!flow->isRunning());
    }
};

QTEST_MAIN(ProjectXmlTest)
#include "tst_projectxml.moc"
