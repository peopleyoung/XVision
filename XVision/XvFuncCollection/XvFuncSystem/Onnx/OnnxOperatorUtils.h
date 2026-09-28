#ifndef ONNXOPERATORUTILS_H
#define ONNXOPERATORUTILS_H

#include "XVFuncSystemGlobal.h"
#include "OnnxSession.h"

#include <QImage>
#include <QStringList>
#include <QVector>

namespace XvOnnx
{
enum class TensorLayout
{
    Auto=0,
    Nchw=1,
    Nhwc=2
};

enum class ChannelOrder
{
    Rgb=0,
    Bgr=1
};

enum class ResizeMode
{
    Stretch=0,
    Letterbox=1
};

struct XVFUNCSYSTEM_EXPORT ImagePreprocessConfig
{
    TensorLayout layout=TensorLayout::Auto;
    ChannelOrder channelOrder=ChannelOrder::Rgb;
    ResizeMode resizeMode=ResizeMode::Stretch;
    int inputWidth=0;
    int inputHeight=0;
    int paddingValue=114;
    double pixelScale=1.0/255.0;
    QString meanValues="0";
    QString stdValues="1";
};

struct XVFUNCSYSTEM_EXPORT ImageTransform
{
    int sourceWidth=0;
    int sourceHeight=0;
    int modelWidth=0;
    int modelHeight=0;
    double scaleX=1.0;
    double scaleY=1.0;
    double padX=0.0;
    double padY=0.0;
};

struct XVFUNCSYSTEM_EXPORT ClassificationValue
{
    int classId=0;
    QString className;
    double score=0.0;
};

enum class DetectionDecoder
{
    GenericXyxy=0,
    Yolov3=1,
    Yolov5=2,
    Yolov5Face=3
};

struct XVFUNCSYSTEM_EXPORT DetectionValue
{
    double x=0.0;
    double y=0.0;
    double width=0.0;
    double height=0.0;
    int classId=0;
    QString className;
    double score=0.0;
};

struct XVFUNCSYSTEM_EXPORT SegmentationValue
{
    int width=0;
    int height=0;
    QVector<qint32> labels;
    QVector<float> confidences;
    QVector<qint32> classIds;
    QStringList classNames;
    QVector<QRgb> classColors;
};

XVFUNCSYSTEM_EXPORT QStringList parseClassNames(const QString &text);
XVFUNCSYSTEM_EXPORT bool tensorToDoubles(const XOnnxTensorData &tensor,
                                         QVector<double> &values,
                                         QString &error);
XVFUNCSYSTEM_EXPORT bool softmax(const QVector<double> &values,
                                QVector<double> &scores,
                                QString &error);
XVFUNCSYSTEM_EXPORT bool decodeAge(const XOnnxTensorData &tensor,
                                  double &age,QString &error);
XVFUNCSYSTEM_EXPORT bool preprocessImage(const QImage &image,
                                         const XOnnxTensorInfo &inputInfo,
                                         const ImagePreprocessConfig &config,
                                         XOnnxTensorData &tensor,
                                         ImageTransform &transform,
                                         QString &error);
XVFUNCSYSTEM_EXPORT bool decodeClassification(const XOnnxTensorData &tensor,
                                              bool applySoftmax,int topK,
                                              double minScore,
                                              const QStringList &classNames,
                                              QVector<ClassificationValue> &values,
                                              QString &error);
XVFUNCSYSTEM_EXPORT bool decodeDetections(const QList<XOnnxTensorData> &outputs,
                                         DetectionDecoder decoder,
                                         const ImageTransform &transform,
                                         bool normalizedCoordinates,
                                         double confidenceThreshold,
                                         double iouThreshold,
                                         bool classAgnostic,
                                         int maximumDetections,
                                         const QStringList &classNames,
                                         QVector<DetectionValue> &values,
                                         QString &error);
XVFUNCSYSTEM_EXPORT bool decodeSegmentation(const XOnnxTensorData &output,
                                           TensorLayout outputLayout,
                                           int sourceWidth,int sourceHeight,
                                           double binaryThreshold,
                                           const QStringList &classNames,
                                           SegmentationValue &value,
                                           QString &error);
}

#endif // ONNXOPERATORUTILS_H
