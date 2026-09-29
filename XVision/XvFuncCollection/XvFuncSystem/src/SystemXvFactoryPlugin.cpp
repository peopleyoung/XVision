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
XvFuncPreset modePreset(const QString &role,const QString &modeKey,
                        const QString &label,int value,const QString &property="mode")
{
    XvFuncPreset preset;
    preset.alias=role+"."+modeKey;
    preset.displayName=getLang("XvOperator_"+preset.alias,label);
    preset.canonicalRole=role;
    preset.properties.insert(property,value);
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
    // Only distinct non-default configurations belong in the catalog.
    // Historical compatibility IDs intentionally have no registration or fallback.
    QList<XvFuncPreset> presets;
    presets.append(modePreset("ImageAcquisition","Camera",QStringLiteral("相机采集"),ImageAcquisition::Camera,"acqType"));
    presets.append(modePreset("ImageAcquisition","Dir",QStringLiteral("文件夹图像采集"),ImageAcquisition::Dir,"acqType"));
    presets.append(modePreset("ImageAcquisition","Video",QStringLiteral("视频采集"),ImageAcquisition::Video,"acqType"));
    presets.append(modePreset("ORectification","ForegroundExtract",QStringLiteral("前景提取"),ORectification::ForegroundExtract));
    presets.append(modePreset("ORectification","RotatedRect",QStringLiteral("旋转矩形校正"),ORectification::RotatedRect));
    presets.append(modePreset("OPointFeature","Akaze",QStringLiteral("AKAZE 特征检测"),OPointFeature::Akaze));
    presets.append(modePreset("OPointFeature","Brisk",QStringLiteral("BRISK 特征检测"),OPointFeature::Brisk));
    presets.append(modePreset("OPointFeature","Fast",QStringLiteral("FAST 特征检测"),OPointFeature::Fast));
    presets.append(modePreset("OPointFeature","Freak",QStringLiteral("FREAK 特征描述"),OPointFeature::Freak));
    presets.append(modePreset("OPointFeature","Kaze",QStringLiteral("KAZE 特征检测"),OPointFeature::Kaze));
    presets.append(modePreset("OPointFeature","Mser",QStringLiteral("MSER 区域检测"),OPointFeature::Mser));
    presets.append(modePreset("OPointFeature","Star",QStringLiteral("STAR 特征检测"),OPointFeature::Star));
    presets.append(modePreset("OPointFeature","Subpixel",QStringLiteral("亚像素角点"),OPointFeature::Subpixel));
    presets.append(modePreset("OTemplateMatch","Feature",QStringLiteral("特征点模板匹配"),OTemplateMatch::Feature));
    presets.append(modePreset("OTemplateMatch","Hsv",QStringLiteral("HSV 模板匹配"),OTemplateMatch::Hsv));
    presets.append(modePreset("OTemplateMatch","Shape",QStringLiteral("形状模板匹配"),OTemplateMatch::Shape));
    presets.append(modePreset("OImageAnalysis","Histogram",QStringLiteral("直方图"),OImageAnalysis::Histogram));
    presets.append(modePreset("OImageAnalysis","Hog",QStringLiteral("方向梯度特征"),OImageAnalysis::Hog));
    presets.append(modePreset("OImageAnalysis","Subdiv2d",QStringLiteral("平面细分"),OImageAnalysis::Subdiv2d));
    presets.append(modePreset("OImageArithmetic","BitwiseNot",QStringLiteral("按位取反"),OImageArithmetic::BitwiseNot));
    presets.append(modePreset("OImageArithmetic","MultiplyDivide",QStringLiteral("乘除运算"),OImageArithmetic::MultiplyDivide));
    presets.append(modePreset("OImageArithmetic","Pow",QStringLiteral("幂运算"),OImageArithmetic::Pow));
    presets.append(modePreset("OImageColor","HsvInRange",QStringLiteral("HSV 范围筛选"),OImageColor::HsvInRange));
    presets.append(modePreset("OImageColor","Normalize",QStringLiteral("归一化"),OImageColor::Normalize));
    presets.append(modePreset("OImageColor","SplitBgr",QStringLiteral("BGR 通道分离"),OImageColor::SplitBgr));
    presets.append(modePreset("OImageComposition","SeamlessClone",QStringLiteral("无缝融合"),OImageComposition::SeamlessClone));
    presets.append(modePreset("OImageComposition","Stitching",QStringLiteral("图像拼接"),OImageComposition::Stitching));
    presets.append(modePreset("OImageFilter","DetailEnhance",QStringLiteral("细节增强"),OImageFilter::DetailEnhance));
    presets.append(modePreset("OImageFilter","EdgePreserving",QStringLiteral("保边滤波"),OImageFilter::EdgePreserving));
    presets.append(modePreset("OImageFilter","GaussianBlur",QStringLiteral("高斯滤波"),OImageFilter::GaussianBlur));
    presets.append(modePreset("OImageFilter","PencilSketch",QStringLiteral("铅笔素描"),OImageFilter::PencilSketch));
    presets.append(modePreset("OImageFilter","Stylization",QStringLiteral("风格化"),OImageFilter::Stylization));
    presets.append(modePreset("OImageModel","BackgroundSubtraction",QStringLiteral("背景消除"),OImageModel::BackgroundSubtraction));
    presets.append(modePreset("OImageModel","Svm",QStringLiteral("支持向量机"),OImageModel::Svm));
    presets.append(modePreset("OImageThreshold","PixelCondition",QStringLiteral("像素阈值条件"),OImageThreshold::PixelCondition));
    presets.append(modePreset("OImageTransform","Homography",QStringLiteral("单应变换"),OImageTransform::Homography));
    presets.append(modePreset("OImageTransform","Repeat",QStringLiteral("图像重复"),OImageTransform::Repeat));
    presets.append(modePreset("OImageTransform","Resize",QStringLiteral("尺寸调整"),OImageTransform::Resize));
    presets.append(modePreset("OImageTransform","Rotate",QStringLiteral("图像旋转"),OImageTransform::Rotate));
    presets.append(modePreset("OImageTransform","Transpose",QStringLiteral("图像转置"),OImageTransform::Transpose));
    presets.append(modePreset("OImageTransform","WarpAffine",QStringLiteral("仿射变换"),OImageTransform::WarpAffine));
    presets.append(modePreset("OImageTransform","WarpPerspective",QStringLiteral("透视变换"),OImageTransform::WarpPerspective));
    presets.append(modePreset("OMorphology","Close",QStringLiteral("闭运算"),OMorphology::Close));
    presets.append(modePreset("OMorphology","Dilate",QStringLiteral("膨胀运算"),OMorphology::Dilate));
    presets.append(modePreset("OMorphology","Erode",QStringLiteral("腐蚀运算"),OMorphology::Erode));
    presets.append(modePreset("OMorphology","Gradient",QStringLiteral("形态学梯度"),OMorphology::Gradient));
    presets.append(modePreset("OMorphology","Open",QStringLiteral("开运算"),OMorphology::Open));
    presets.append(modePreset("OMorphology","TopHat",QStringLiteral("顶帽运算"),OMorphology::TopHat));
    presets.append(modePreset("GeometryMeasure","LineCircle",QStringLiteral("直线到圆测量"),GeometryMeasure::LineCircle));
    presets.append(modePreset("GeometryMeasure","LineLine",QStringLiteral("直线到直线测量"),GeometryMeasure::LineLine));
    presets.append(modePreset("GeometryMeasure","LineLineAngle",QStringLiteral("直线夹角测量"),GeometryMeasure::LineLineAngle));
    presets.append(modePreset("GeometryMeasure","PointCircle",QStringLiteral("点到圆测量"),GeometryMeasure::PointCircle));
    presets.append(modePreset("GeometryMeasure","PointLine",QStringLiteral("点到直线测量"),GeometryMeasure::PointLine));
    presets.append(modePreset("GeometryMeasure","PointPoint",QStringLiteral("点到点测量"),GeometryMeasure::PointPoint));
    presets.append(modePreset("OCascadeDetector","Lbp",QStringLiteral("LBP 级联检测"),OCascadeDetector::Lbp));
    presets.append(modePreset("ORegionDetector","Contours",QStringLiteral("轮廓提取"),ORegionDetector::Contours));
    presets.append(modePreset("ORegionDetector","HoughCircles",QStringLiteral("霍夫圆检测"),ORegionDetector::HoughCircles));
    presets.append(modePreset("ORegionDetector","HoughLines",QStringLiteral("霍夫直线检测"),ORegionDetector::HoughLines));
    presets.append(modePreset("ORegionDetector","HoughLinesP",QStringLiteral("概率霍夫直线"),ORegionDetector::HoughLinesP));
    presets.append(modePreset("ORegionDetector","RenderBlobs",QStringLiteral("斑点绘制"),ORegionDetector::RenderBlobs));
    presets.append(modePreset("HttpJson","Write",QStringLiteral("HTTP 写入数据"),HttpJson::Write));
    presets.append(modePreset("ModbusRegister","WriteInt16",QStringLiteral("Modbus 写入整数"),ModbusRegister::WriteInt16));
    presets.append(modePreset("SerialData","ReadText",QStringLiteral("串口读取文本"),SerialData::ReadText));
    presets.append(modePreset("SerialData","WriteBytes",QStringLiteral("串口写入字节"),SerialData::WriteBytes));
    presets.append(modePreset("SerialData","WriteText",QStringLiteral("串口写入文本"),SerialData::WriteText));
    presets.append(modePreset("TcpText","Write",QStringLiteral("TCP 写入文本"),TcpText::Write));
    presets.append(modePreset("UdpText","Write",QStringLiteral("UDP 写入文本"),UdpText::Write));
    presets.append(modePreset("NClassification","Gender",QStringLiteral("性别分类"),NClassification::Gender));
    presets.append(modePreset("NInference","Age",QStringLiteral("年龄估计"),NInference::Age));
    presets.append(modePreset("NObjectDetection","Yolov3",QStringLiteral("YOLOv3 目标检测"),NObjectDetection::Yolov3));
    presets.append(modePreset("NObjectDetection","Yolov5",QStringLiteral("YOLOv5 目标检测"),NObjectDetection::Yolov5));
    presets.append(modePreset("NObjectDetection","Yolov5Face",QStringLiteral("YOLOv5 人脸检测"),NObjectDetection::Yolov5Face));
    presets.append(modePreset("NSemanticSegmentation","Human",QStringLiteral("人体分割"),NSemanticSegmentation::Human));
    presets.append(modePreset("DetectRecord","ClassRecord",QStringLiteral("保存分类记录"),DetectRecord::ClassRecord));
    presets.append(modePreset("DetectRecord","HasRecord",QStringLiteral("查询检测记录"),DetectRecord::HasRecord));
    presets.append(modePreset("DetectRecord","ObjectRecord",QStringLiteral("保存目标记录"),DetectRecord::ObjectRecord));
    presets.append(modePreset("LoopFlow","ForeachImages",QStringLiteral("遍历图像"),LoopFlow::ForeachImages));
    presets.append(modePreset("NotificationOutput","Dialog",QStringLiteral("弹窗通知"),NotificationOutput::Dialog));
    presets.append(modePreset("NotificationOutput","Error",QStringLiteral("错误通知"),NotificationOutput::Error));
    presets.append(modePreset("NotificationOutput","Fatal",QStringLiteral("严重错误通知"),NotificationOutput::Fatal));
    presets.append(modePreset("NotificationOutput","Info",QStringLiteral("信息通知"),NotificationOutput::Info));
    presets.append(modePreset("NotificationOutput","Ng",QStringLiteral("不合格结果输出"),NotificationOutput::Ng));
    presets.append(modePreset("NotificationOutput","Success",QStringLiteral("成功通知"),NotificationOutput::Success));
    presets.append(modePreset("NotificationOutput","Warning",QStringLiteral("警告通知"),NotificationOutput::Warning));
    return presets;
}
