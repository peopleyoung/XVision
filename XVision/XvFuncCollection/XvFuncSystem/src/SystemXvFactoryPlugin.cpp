#include "SystemXvFactoryPlugin.h"
#include "ImageAcquisition.h"
#include "BaseDataWriter.h"
#include "BaseDataBoolCalc.h"
#include "BaseDataIntCalc.h"
#include "BaseDataRealCalc.h"
#include "BaseDataStringProcess.h"


#include "NClassification.h"
#include "NInference.h"
#include "NObjectDetection.h"
#include "NSemanticSegmentation.h"

#include "ORectification.h"
#include "OTemplateMatch.h"

#include "GeometryCreate.h"
#include "GeometryMeasure.h"

#include "DetectRecord.h"

#include "HttpJson.h"
#include "ModbusRegister.h"
#include "SerialData.h"
#include "TcpText.h"
#include "UdpText.h"

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

#include "ConditionalFlow.h"
#include "LoopFlow.h"

#include "Delayer.h"
#include "ElapsedTimer.h"
#include "LogOutput.h"
#include "NotificationOutput.h"

using namespace XvCore;

namespace
{
XvFuncPreset acquisitionPreset(const QString &alias,ImageAcquisition::AcqType type)
{
    XvFuncPreset preset;
    preset.alias=alias;
    preset.displayName=getUiText(alias);
    preset.canonicalRole="ImageAcquisition";
    preset.properties.insert("acqType",static_cast<int>(type));
    return preset;
}

XvFuncPreset modePreset(const QString &alias,const QString &canonicalRole,int mode)
{
    XvFuncPreset preset;
    preset.alias=alias;
    preset.displayName=getUiText(alias);
    preset.canonicalRole=canonicalRole;
    preset.properties.insert("mode",mode);
    return preset;
}

XvFuncPreset rolePreset(const QString &alias,const QString &canonicalRole)
{
    XvFuncPreset preset;
    preset.alias=alias;
    preset.displayName=getUiText(alias);
    preset.canonicalRole=canonicalRole;
    return preset;
}
}

SystemXvFactoryPlugin::SystemXvFactoryPlugin(QObject *parent)
    :QObject{parent}
{

}

QString SystemXvFactoryPlugin::name() const
{
    return "SystemXvFactoryPlugin";
}

QList<QMetaObject> SystemXvFactoryPlugin::getPlgXvFunc()
{
    QList<QMetaObject> lst;
    ADD_XVFUNC(lst,ImageAcquisition);
    ADD_XVFUNC(lst,BaseDataWriter);
    ADD_XVFUNC(lst,BaseDataBoolCalc);
    ADD_XVFUNC(lst,BaseDataIntCalc);
    ADD_XVFUNC(lst,BaseDataRealCalc);
    ADD_XVFUNC(lst,BaseDataStringProcess);


    ADD_XVFUNC(lst,NInference);
    ADD_XVFUNC(lst,NClassification);
    ADD_XVFUNC(lst,NObjectDetection);
    ADD_XVFUNC(lst,NSemanticSegmentation);

    ADD_XVFUNC(lst,OTemplateMatch);
    ADD_XVFUNC(lst,ORectification);

    ADD_XVFUNC(lst,GeometryCreate);
    ADD_XVFUNC(lst,GeometryMeasure);

    ADD_XVFUNC(lst,DetectRecord);

    ADD_XVFUNC(lst,HttpJson);
    ADD_XVFUNC(lst,TcpText);
    ADD_XVFUNC(lst,UdpText);
    ADD_XVFUNC(lst,SerialData);
    ADD_XVFUNC(lst,ModbusRegister);

    ADD_XVFUNC(lst,OImageArithmetic);
    ADD_XVFUNC(lst,OImageFilter);
    ADD_XVFUNC(lst,OImageColor);
    ADD_XVFUNC(lst,OImageThreshold);
    ADD_XVFUNC(lst,OImageTransform);
    ADD_XVFUNC(lst,OImageAnalysis);
    ADD_XVFUNC(lst,OImageModel);
    ADD_XVFUNC(lst,OImageComposition);

    ADD_XVFUNC(lst,OMorphology);
    ADD_XVFUNC(lst,ORegionDetector);
    ADD_XVFUNC(lst,OCodeDetector);
    ADD_XVFUNC(lst,OPointFeature);
    ADD_XVFUNC(lst,OCascadeDetector);

    ADD_XVFUNC(lst,ConditionalFlow);
    ADD_XVFUNC(lst,LoopFlow);

    ADD_XVFUNC(lst,Delayer);
    ADD_XVFUNC(lst,ElapsedTimer);
    ADD_XVFUNC(lst,LogOutput);
    ADD_XVFUNC(lst,NotificationOutput);
    return lst;
}

QList<XvFuncPreset> SystemXvFactoryPlugin::getPlgXvFuncPresets()
{
    QList<XvFuncPreset> presets;
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
        presets.append(acquisitionPreset(alias,ImageAcquisition::Dir));
    presets.append(acquisitionPreset("CameraCaptureNodeData",ImageAcquisition::Camera));
    presets.append(acquisitionPreset("CameraNodeData",ImageAcquisition::Camera));
    presets.append(acquisitionPreset("SrcVideoFilesNodeData",ImageAcquisition::Video));

    presets.append(modePreset("OKOutputNodeData","NotificationOutput",
                              NotificationOutput::Ok));
    presets.append(modePreset("NGOutputNodeData","NotificationOutput",
                              NotificationOutput::Ng));
    presets.append(modePreset("ShowInfoNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Info));
    presets.append(modePreset("ShowSuccessNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Success));
    presets.append(modePreset("ShowWarnNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Warning));
    presets.append(modePreset("ShowErrorNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Error));
    presets.append(modePreset("ShowFatalNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Fatal));
    presets.append(modePreset("ShowDialogNotifyMessageOutputNodeData","NotificationOutput",
                              NotificationOutput::Dialog));

    presets.append(rolePreset("OpenCVConditionNodeData","ConditionalFlow"));
    presets.append(modePreset("ForNodeData","LoopFlow",LoopFlow::For));
    presets.append(modePreset("ForeachSplitResultImageNodeData","LoopFlow",
                              LoopFlow::ForeachImages));

    presets.append(modePreset("InferOnnxNodeData","NInference",NInference::Generic));
    presets.append(modePreset("AgeInferOnnxNodeData","NInference",NInference::Age));
    presets.append(modePreset("ClsOnnxNodeData","NClassification",NClassification::Generic));
    presets.append(modePreset("GenderClsOnnxNodeData","NClassification",NClassification::Gender));
    presets.append(modePreset("ObjDetectOnnxNodeData","NObjectDetection",
                              NObjectDetection::Generic));
    presets.append(modePreset("Yolov3","NObjectDetection",NObjectDetection::Yolov3));
    presets.append(modePreset("Yolov5OnnxNodeData","NObjectDetection",
                              NObjectDetection::Yolov5));
    presets.append(modePreset("Yolov5FaceOnnxNodeData","NObjectDetection",
                              NObjectDetection::Yolov5Face));
    presets.append(modePreset("SemSegOnnxNodeData","NSemanticSegmentation",
                              NSemanticSegmentation::Generic));
    presets.append(modePreset("HumanSemSegOnnxNodeData","NSemanticSegmentation",
                              NSemanticSegmentation::Human));

    presets.append(modePreset("Base64TemplateMatchNodeData","OTemplateMatch",
                              OTemplateMatch::Base64));
    presets.append(modePreset("FeaturePointTemplateMatch","OTemplateMatch",
                              OTemplateMatch::Feature));
    presets.append(modePreset("ShapeTemplateMatch","OTemplateMatch",
                              OTemplateMatch::Shape));
    presets.append(modePreset("HSVTemplateMatch","OTemplateMatch",
                              OTemplateMatch::Hsv));
    presets.append(modePreset("ForegroundRotatedRectRectification","ORectification",
                              ORectification::ForegroundRotatedRect));
    presets.append(modePreset("TakeoffForegroundInfo","ORectification",
                              ORectification::ForegroundExtract));
    presets.append(modePreset("RotatedRectRectification","ORectification",
                              ORectification::RotatedRect));

    presets.append(rolePreset("CreateShapeNodeData","GeometryCreate"));
    presets.append(modePreset("CircleToCircleMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::CircleCircle));
    presets.append(modePreset("LineToCircleMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::LineCircle));
    presets.append(modePreset("LineToLineAngleMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::LineLineAngle));
    presets.append(modePreset("LineToLineMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::LineLine));
    presets.append(modePreset("PointToCircleMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::PointCircle));
    presets.append(modePreset("PointToLineMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::PointLine));
    presets.append(modePreset("PointToPointMesauseNodeData","GeometryMeasure",
                              GeometryMeasure::PointPoint));

    presets.append(modePreset("DetectRecordNodeData","DetectRecord",
                              DetectRecord::Record));
    presets.append(modePreset("ClassDetectRecordNodeData","DetectRecord",
                              DetectRecord::ClassRecord));
    presets.append(modePreset("ObjectDetectRecordNodeData","DetectRecord",
                              DetectRecord::ObjectRecord));
    presets.append(modePreset("HasDetectRecordNodeData","DetectRecord",
                              DetectRecord::HasRecord));

    presets.append(modePreset("HttpReadJsonNodeData","HttpJson",HttpJson::Read));
    presets.append(modePreset("HttpWriteJsonNodeData","HttpJson",HttpJson::Write));
    presets.append(modePreset("TcpReadStringNodeData","TcpText",TcpText::Read));
    presets.append(modePreset("TcpWriteStringNodeData","TcpText",TcpText::Write));
    presets.append(modePreset("UdpReadStringNodeData","UdpText",UdpText::Read));
    presets.append(modePreset("UdpWriteStringNodeData","UdpText",UdpText::Write));
    presets.append(modePreset("SerialReadByteNodeData","SerialData",SerialData::ReadBytes));
    presets.append(modePreset("SerialReadStringNodeData","SerialData",SerialData::ReadText));
    presets.append(modePreset("SerialWriteByteNodeData","SerialData",SerialData::WriteBytes));
    presets.append(modePreset("SerialWriteStringNodeData","SerialData",SerialData::WriteText));
    presets.append(modePreset("IntReadableModbusNodeData","ModbusRegister",
                              ModbusRegister::ReadInt32));
    presets.append(modePreset("ShortWriteableModbusNodeData","ModbusRegister",
                              ModbusRegister::WriteInt16));

    presets.append(modePreset("AddSutract","OImageArithmetic",OImageArithmetic::AddSubtract));
    presets.append(modePreset("BitwiseNot","OImageArithmetic",OImageArithmetic::BitwiseNot));
    presets.append(modePreset("MultiplayDivide","OImageArithmetic",OImageArithmetic::MultiplyDivide));
    presets.append(modePreset("Pow","OImageArithmetic",OImageArithmetic::Pow));

    presets.append(modePreset("Blur","OImageFilter",OImageFilter::BoxBlur));
    presets.append(modePreset("GaussianBlur","OImageFilter",OImageFilter::GaussianBlur));
    presets.append(modePreset("DetailEnhance","OImageFilter",OImageFilter::DetailEnhance));
    presets.append(modePreset("EdgePreservingFilter","OImageFilter",OImageFilter::EdgePreserving));
    presets.append(modePreset("PencilSketch","OImageFilter",OImageFilter::PencilSketch));
    presets.append(modePreset("Stylization","OImageFilter",OImageFilter::Stylization));

    presets.append(modePreset("CvtColor","OImageColor",OImageColor::Convert));
    presets.append(modePreset("HSVInRange","OImageColor",OImageColor::HsvInRange));
    presets.append(modePreset("Normalize","OImageColor",OImageColor::Normalize));
    presets.append(modePreset("SplitBGR","OImageColor",OImageColor::SplitBgr));

    presets.append(modePreset("Threshold","OImageThreshold",OImageThreshold::Threshold));
    presets.append(modePreset("PixelThresholdIfConditionNodeData","OImageThreshold",
                              OImageThreshold::PixelCondition));

    presets.append(modePreset("Flip","OImageTransform",OImageTransform::Flip));
    presets.append(modePreset("HomographyTransform","OImageTransform",OImageTransform::Homography));
    presets.append(modePreset("Repeat","OImageTransform",OImageTransform::Repeat));
    presets.append(modePreset("Resize","OImageTransform",OImageTransform::Resize));
    presets.append(modePreset("Rotate","OImageTransform",OImageTransform::Rotate));
    presets.append(modePreset("Transpose","OImageTransform",OImageTransform::Transpose));
    presets.append(modePreset("WarpAffineTransform","OImageTransform",OImageTransform::WarpAffine));
    presets.append(modePreset("WarpPerspectiveTransform","OImageTransform",
                              OImageTransform::WarpPerspective));

    presets.append(modePreset("Canny","OImageAnalysis",OImageAnalysis::Canny));
    presets.append(modePreset("Hist","OImageAnalysis",OImageAnalysis::Histogram));
    presets.append(modePreset("Hog","OImageAnalysis",OImageAnalysis::Hog));
    presets.append(modePreset("Subdiv2D","OImageAnalysis",OImageAnalysis::Subdiv2d));

    presets.append(modePreset("DnnSuperres","OImageModel",OImageModel::SuperResolution));
    presets.append(modePreset("MOG","OImageModel",OImageModel::BackgroundSubtraction));
    presets.append(modePreset("SVM","OImageModel",OImageModel::Svm));

    presets.append(modePreset("SeamlessCloneBackground","OImageComposition",
                              OImageComposition::Background));
    presets.append(modePreset("SeamlessClone","OImageComposition",
                              OImageComposition::SeamlessClone));
    presets.append(modePreset("Stitching","OImageComposition",OImageComposition::Stitching));

    presets.append(modePreset("BlackHat","OMorphology",OMorphology::BlackHat));
    presets.append(modePreset("Close","OMorphology",OMorphology::Close));
    presets.append(modePreset("Dilate","OMorphology",OMorphology::Dilate));
    presets.append(modePreset("Erode","OMorphology",OMorphology::Erode));
    presets.append(modePreset("Gradient","OMorphology",OMorphology::Gradient));
    presets.append(modePreset("Open","OMorphology",OMorphology::Open));
    presets.append(modePreset("TopHat","OMorphology",OMorphology::TopHat));

    presets.append(modePreset("BlobDetector","ORegionDetector",ORegionDetector::Blob));
    presets.append(modePreset("FindContours","ORegionDetector",ORegionDetector::Contours));
    presets.append(modePreset("HoughCircles","ORegionDetector",ORegionDetector::HoughCircles));
    presets.append(modePreset("RenderBlobs","ORegionDetector",ORegionDetector::RenderBlobs));
    presets.append(modePreset("HoughLines","ORegionDetector",ORegionDetector::HoughLines));
    presets.append(modePreset("HoughLinesP","ORegionDetector",ORegionDetector::HoughLinesP));

    presets.append(rolePreset("QRCode","OCodeDetector"));

    presets.append(modePreset("CornerHarris","OPointFeature",OPointFeature::Harris));
    presets.append(modePreset("CornerSubPix","OPointFeature",OPointFeature::Subpixel));
    presets.append(modePreset("AKazeFeatureDetector","OPointFeature",OPointFeature::Akaze));
    presets.append(modePreset("BriskFeatureDetector","OPointFeature",OPointFeature::Brisk));
    presets.append(modePreset("FastFeatureDetector","OPointFeature",OPointFeature::Fast));
    presets.append(modePreset("FreakFeatureDetector","OPointFeature",OPointFeature::Freak));
    presets.append(modePreset("KazeFeatureDetector","OPointFeature",OPointFeature::Kaze));
    presets.append(modePreset("MserFeatureDetector","OPointFeature",OPointFeature::Mser));
    presets.append(modePreset("StarFeatureDetector","OPointFeature",OPointFeature::Star));

    presets.append(modePreset("HaarCascade","OCascadeDetector",OCascadeDetector::Haar));
    presets.append(modePreset("LbpCascade","OCascadeDetector",OCascadeDetector::Lbp));
    return presets;
}
