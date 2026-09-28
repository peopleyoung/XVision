#include <QtTest>

#include <QColor>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

#include "NClassification.h"
#include "NInference.h"
#include "NObjectDetection.h"
#include "NSemanticSegmentation.h"
#include "OnnxOperatorUtils.h"

using namespace XvCore;

namespace
{
XOnnxTensorData floatTensor(const QString &name,const QVector<qint64> &dimensions,
                            const QVector<float> &values)
{
    XOnnxTensorData tensor;
    tensor.name=name;
    tensor.elementType="float32";
    tensor.dimensions=dimensions;
    tensor.bytes.resize(values.size()*4);
    for(int index=0;index<values.size();++index)
        std::memcpy(tensor.bytes.data()+index*4,&values[index],4);
    return tensor;
}

template<typename T>
T *parameter(XvFunc *function,const QString &name)
{
    return dynamic_cast<T*>(function->getParamsByName(name));
}

template<typename T>
T *result(XvFunc *function,const QString &name)
{
    return dynamic_cast<T*>(function->getResultsByName(name));
}
}

class OnnxOperatorTests:public QObject
{
    Q_OBJECT
private slots:
    void preprocessLayoutsNormalizationAndLetterbox();
    void classificationAndSoftmax();
    void detectionDecodersAndNms();
    void segmentationLogitsBinaryAndLabels();
    void operatorDefaultsAndDisabledBackend();
};

void OnnxOperatorTests::preprocessLayoutsNormalizationAndLetterbox()
{
    QImage image(2,1,QImage::Format_RGB888);
    image.setPixelColor(0,0,QColor(10,20,30));
    image.setPixelColor(1,0,QColor(40,50,60));
    XOnnxTensorInfo info{"image","float32",{1,3,1,2}};
    XvOnnx::ImagePreprocessConfig config;
    config.pixelScale=1.0;
    config.meanValues="0";
    config.stdValues="1";
    XOnnxTensorData tensor;
    XvOnnx::ImageTransform transform;
    QString error;
    QVERIFY2(XvOnnx::preprocessImage(image,info,config,tensor,transform,error),
             qPrintable(error));
    QCOMPARE(tensor.dimensions,QVector<qint64>({1,3,1,2}));
    QVector<double> values;
    QVERIFY(XvOnnx::tensorToDoubles(tensor,values,error));
    QCOMPARE(values,QVector<double>({10,40,20,50,30,60}));
    QCOMPARE(transform.scaleX,1.0);
    QCOMPARE(transform.padY,0.0);

    info.dimensions={1,1,2,3};
    config.layout=XvOnnx::TensorLayout::Nhwc;
    config.channelOrder=XvOnnx::ChannelOrder::Bgr;
    QVERIFY2(XvOnnx::preprocessImage(image,info,config,tensor,transform,error),
             qPrintable(error));
    QVERIFY(XvOnnx::tensorToDoubles(tensor,values,error));
    QCOMPARE(values,QVector<double>({30,20,10,60,50,40}));

    info.dimensions={1,3,4,4};
    config.layout=XvOnnx::TensorLayout::Nchw;
    config.resizeMode=XvOnnx::ResizeMode::Letterbox;
    QVERIFY2(XvOnnx::preprocessImage(image,info,config,tensor,transform,error),
             qPrintable(error));
    QCOMPARE(transform.modelWidth,4);
    QCOMPARE(transform.modelHeight,4);
    QCOMPARE(transform.scaleX,2.0);
    QCOMPARE(transform.scaleY,2.0);
    QCOMPARE(transform.padX,0.0);
    QCOMPARE(transform.padY,1.0);

    info.dimensions={1,3,-1,-1};
    config.inputWidth=6;
    config.inputHeight=5;
    config.resizeMode=XvOnnx::ResizeMode::Stretch;
    QVERIFY2(XvOnnx::preprocessImage(image,info,config,tensor,transform,error),
             qPrintable(error));
    QCOMPARE(tensor.dimensions,QVector<qint64>({1,3,5,6}));

    info.dimensions={1,3,3,3};
    config.layout=XvOnnx::TensorLayout::Auto;
    QVERIFY(!XvOnnx::preprocessImage(image,info,config,tensor,transform,error));
    QVERIFY(error.contains("ambiguous"));

    info={"gray","uint8",{1,1,1,2}};
    config.inputWidth=0;
    config.inputHeight=0;
    config.layout=XvOnnx::TensorLayout::Nchw;
    config.resizeMode=XvOnnx::ResizeMode::Stretch;
    config.pixelScale=1.0;
    config.meanValues="0";
    config.stdValues="1";
    QVERIFY2(XvOnnx::preprocessImage(image,info,config,tensor,transform,error),
             qPrintable(error));
    QByteArray expectedGray;
    expectedGray.append(static_cast<char>(qGray(QColor(10,20,30).rgb())));
    expectedGray.append(static_cast<char>(qGray(QColor(40,50,60).rgb())));
    QCOMPARE(tensor.bytes,expectedGray);

    info={"image","float32",{1,3,1,2}};
    config.layout=XvOnnx::TensorLayout::Nchw;
    config.stdValues="-1";
    QVERIFY(!XvOnnx::preprocessImage(image,info,config,tensor,transform,error));
    QVERIFY(error.contains("normalization"));
    config.stdValues="1";
    config.meanValues="1,2";
    QVERIFY(!XvOnnx::preprocessImage(image,info,config,tensor,transform,error));
    config.meanValues="0";
    config.pixelScale=std::numeric_limits<double>::infinity();
    QVERIFY(!XvOnnx::preprocessImage(image,info,config,tensor,transform,error));
}

void OnnxOperatorTests::classificationAndSoftmax()
{
    QString error;
    QVector<double> scores;
    QVERIFY(XvOnnx::softmax({0.0,1.0,2.0},scores,error));
    QCOMPARE(scores.size(),3);
    QVERIFY(qAbs(std::accumulate(scores.cbegin(),scores.cend(),0.0)-1.0)<1e-12);
    QVERIFY(scores.at(2)>scores.at(1));

    const XOnnxTensorData tensor=floatTensor("scores",{1,3},{0.0f,1.0f,2.0f});
    QVector<XvOnnx::ClassificationValue> values;
    QVERIFY2(XvOnnx::decodeClassification(
                 tensor,true,2,0.0,{"zero","one","two"},values,error),qPrintable(error));
    QCOMPARE(values.size(),2);
    QCOMPARE(values.at(0).classId,2);
    QCOMPARE(values.at(0).className,QString("two"));
    QCOMPARE(values.at(1).classId,1);

    const XOnnxTensorData invalid=floatTensor("scores",{2},{0.2f,1.2f});
    QVERIFY(!XvOnnx::decodeClassification(invalid,false,2,0.0,{},values,error));
    QVERIFY(!error.isEmpty());

    const XOnnxTensorData scalar=floatTensor(
                "age",QVector<qint64>(),QVector<float>{42.0f});
    QVector<double> scalarValue;
    QVERIFY2(XvOnnx::tensorToDoubles(scalar,scalarValue,error),qPrintable(error));
    QCOMPARE(scalarValue,QVector<double>{42.0});
    double age=0.0;
    QVERIFY2(XvOnnx::decodeAge(scalar,age,error),qPrintable(error));
    QCOMPARE(age,42.0);
    const XOnnxTensorData ageLogits=floatTensor("age",{3},{0.0f,0.0f,10.0f});
    QVERIFY2(XvOnnx::decodeAge(ageLogits,age,error),qPrintable(error));
    QVERIFY(age>1.99 && age<=2.0);
    const XOnnxTensorData invalidAge=floatTensor("age",{}, {-1.0f});
    QVERIFY(!XvOnnx::decodeAge(invalidAge,age,error));
}

void OnnxOperatorTests::detectionDecodersAndNms()
{
    XvOnnx::ImageTransform transform;
    transform.sourceWidth=100;
    transform.sourceHeight=100;
    transform.modelWidth=100;
    transform.modelHeight=100;
    QString error;
    QVector<XvOnnx::DetectionValue> values;
    const XOnnxTensorData generic=floatTensor("boxes",{1,3,6},{
        10,10,50,50,0.9f,0,
        12,12,48,48,0.8f,0,
        12,12,48,48,0.7f,1
    });
    QVERIFY2(XvOnnx::decodeDetections(
                 {generic},XvOnnx::DetectionDecoder::GenericXyxy,transform,
                 false,0.1,0.5,false,10,{"A","B"},values,error),qPrintable(error));
    QCOMPARE(values.size(),2);
    QCOMPARE(values.at(0).classId,0);
    QCOMPARE(values.at(1).classId,1);
    QCOMPARE(values.at(0).width,40.0);

    const XOnnxTensorData yolo=floatTensor("head",{2,7},{
        50,50,20,10,0.8f,0.1f,0.9f,
        10,10,4,4,0.2f,0.9f,0.1f
    });
    QVERIFY2(XvOnnx::decodeDetections(
                 {yolo},XvOnnx::DetectionDecoder::Yolov5,transform,
                 false,0.5,0.5,false,10,{"A","B"},values,error),qPrintable(error));
    QCOMPARE(values.size(),1);
    QCOMPARE(values.first().classId,1);
    QVERIFY(qAbs(values.first().score-0.72)<1e-6);
    QVERIFY2(XvOnnx::decodeDetections(
                 {yolo},XvOnnx::DetectionDecoder::Yolov3,transform,
                 false,0.5,0.5,false,10,{"A","B"},values,error),qPrintable(error));
    QCOMPARE(values.size(),1);
    QCOMPARE(values.first().classId,1);

    const XOnnxTensorData invalidYolo=floatTensor(
                "head",{1,7},{50,50,20,10,0.8f,1.1f,0.9f});
    QVERIFY(!XvOnnx::decodeDetections(
                {invalidYolo},XvOnnx::DetectionDecoder::Yolov5,transform,
                false,0.5,0.5,false,10,{},values,error));
    QVERIFY(error.contains("class scores"));

    QVector<float> face(16,0.0f);
    face[0]=50; face[1]=50; face[2]=20; face[3]=20;
    face[4]=0.9f; face[15]=0.8f;
    const XOnnxTensorData faceTensor=floatTensor("face",{1,1,16},face);
    QVERIFY2(XvOnnx::decodeDetections(
                 {faceTensor},XvOnnx::DetectionDecoder::Yolov5Face,transform,
                 false,0.5,0.5,false,10,{"Face"},values,error),qPrintable(error));
    QCOMPARE(values.size(),1);
    QCOMPARE(values.first().className,QString("Face"));
}

void OnnxOperatorTests::segmentationLogitsBinaryAndLabels()
{
    QString error;
    XvOnnx::SegmentationValue value;
    const XOnnxTensorData logits=floatTensor("mask",{1,2,1,2},{2,0,0,2});
    QVERIFY2(XvOnnx::decodeSegmentation(
                 logits,XvOnnx::TensorLayout::Nchw,2,1,0.5,
                 {"Background","Object"},value,error),qPrintable(error));
    QCOMPARE(value.labels,QVector<qint32>({0,1}));
    QCOMPARE(value.confidences.size(),2);
    QVERIFY(value.confidences.at(0)>0.8f);

    const XOnnxTensorData binary=floatTensor("human",{1,1,1,2},{-10,10});
    QVERIFY2(XvOnnx::decodeSegmentation(
                 binary,XvOnnx::TensorLayout::Nchw,2,1,0.5,
                 {"Background","Human"},value,error),qPrintable(error));
    QCOMPARE(value.labels,QVector<qint32>({0,1}));

    XOnnxTensorData labels;
    labels.name="labels";
    labels.elementType="int64";
    labels.dimensions={1,1,2};
    labels.bytes.resize(16);
    const qint64 labelValues[2]={1,0};
    std::memcpy(labels.bytes.data(),labelValues,16);
    QVERIFY2(XvOnnx::decodeSegmentation(
                 labels,XvOnnx::TensorLayout::Auto,2,1,0.5,
                 {"Background","Object"},value,error),qPrintable(error));
    QCOMPARE(value.labels,QVector<qint32>({1,0}));
    QCOMPARE(value.confidences,QVector<float>({1.0f,1.0f}));
}

void OnnxOperatorTests::operatorDefaultsAndDisabledBackend()
{
    NInference inference;
    NClassification classification;
    NObjectDetection detection;
    NSemanticSegmentation segmentation;
    QCOMPARE(inference.funcRole(),QString("NInference"));
    QCOMPARE(classification.funcRole(),QString("NClassification"));
    QCOMPARE(detection.funcRole(),QString("NObjectDetection"));
    QCOMPARE(segmentation.funcRole(),QString("NSemanticSegmentation"));
    QCOMPARE(inference.mode(),NInference::Generic);
    classification.setMode(NClassification::Gender);
    QCOMPARE(classification.classNames(),QString("Female\nMale"));
    detection.setMode(NObjectDetection::Yolov5Face);
    QCOMPARE(detection.resizeMode(),NOnnxBase::Letterbox);
    QCOMPARE(detection.classNames(),QString("Face"));
    segmentation.setMode(NSemanticSegmentation::Human);
    QCOMPARE(segmentation.classNames(),QString("Background\nHuman"));
    for(XvFunc *function:{static_cast<XvFunc*>(&inference),
                          static_cast<XvFunc*>(&classification),
                          static_cast<XvFunc*>(&detection),
                          static_cast<XvFunc*>(&segmentation)})
    {
        QVERIFY(function->persistentPropertyNames().contains("modelPath"));
        QVERIFY(function->persistentPropertyNames().contains("mode"));
    }

#if defined(XVISION_ENABLE_ONNXRUNTIME)
    QSKIP("Disabled-backend operator contract requires XVISION_ENABLE_ONNXRUNTIME=OFF");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString modelPath=directory.filePath("disabled.onnx");
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    QCOMPARE(model.write("not-a-real-model"),qint64(16));
    model.close();
    QImage input(2,2,QImage::Format_RGB888);
    input.fill(Qt::black);
    QString error;
    QVERIFY2(inference.configureModel(modelPath,error),qPrintable(error));
    QVERIFY2(classification.configureModel(modelPath,error),qPrintable(error));
    QVERIFY2(detection.configureModel(modelPath,error),qPrintable(error));
    QVERIFY2(segmentation.configureModel(modelPath,error),qPrintable(error));
    classification.setMode(NClassification::Generic);
    detection.setMode(NObjectDetection::Generic);
    segmentation.setMode(NSemanticSegmentation::Generic);
    parameter<XImage>(&classification,"inputImage")->setValue(input);
    parameter<XImage>(&detection,"inputImage")->setValue(input);
    parameter<XImage>(&segmentation,"inputImage")->setValue(input);
    result<XImage>(&classification,"outputImage")->setValue(input);
    result<XImage>(&detection,"outputImage")->setValue(input);
    result<XImage>(&segmentation,"colorMask")->setValue(input);

    for(XvFunc *function:{static_cast<XvFunc*>(&inference),
                          static_cast<XvFunc*>(&classification),
                          static_cast<XvFunc*>(&detection),
                          static_cast<XvFunc*>(&segmentation)})
    {
        QCOMPARE(function->runXvFunc(),EXvFuncRunStatus::Error);
        QVERIFY(function->getXvFuncRunMsg().contains("XVISION_ENABLE_ONNXRUNTIME=ON"));
    }
    QVERIFY(result<XObjectList>(&inference,"outputs")->values().isEmpty());
    QVERIFY(result<XImage>(&classification,"outputImage")->value().isNull());
    QVERIFY(result<XImage>(&detection,"outputImage")->value().isNull());
    QVERIFY(result<XImage>(&segmentation,"colorMask")->value().isNull());
#endif
}

QTEST_MAIN(OnnxOperatorTests)
#include "tst_onnxoperators.moc"
