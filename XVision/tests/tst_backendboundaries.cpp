#include <QtTest>

#include <QFileInfo>
#include <QTemporaryFile>

#include <algorithm>
#include <iterator>
#include <limits>

#include "OnnxSession.h"

#if defined(XVISION_ENABLE_OPENCV)
#include "XvOpenCvImageInterop.h"
#include <opencv2/core.hpp>
#endif

class VisionBackendBoundaryTests:public QObject
{
    Q_OBJECT
private slots:
    void onnxRejectsInvalidModel();
    void onnxExecutionBoundary();
    void onnxExecutionFixture();
    void onnxMetadataFixture();
    void openCvSupportedFormats();
    void openCvPaddedStrideAndOwnership();
    void openCvInvalidInputPreservesTargets();
};

void VisionBackendBoundaryTests::onnxExecutionBoundary()
{
    OnnxSession session;
    XOnnxTensorData input;
    input.name="input";
    input.elementType="float32";
    input.dimensions={1};
    input.bytes=QByteArray(4,0);
    QList<XOnnxTensorData> outputs;
    XOnnxTensorData sentinel;
    sentinel.name="sentinel";
    sentinel.elementType="uint8";
    sentinel.dimensions={1};
    sentinel.bytes=QByteArray(1,'x');
    outputs.append(sentinel);
    QString error;
    QVERIFY(!session.run({input},outputs,&error));
    QVERIFY(!error.isEmpty());
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    QVERIFY(error.contains("not loaded"));
#else
    QVERIFY(error.contains("XVISION_ENABLE_ONNXRUNTIME=ON"));
#endif
    QCOMPARE(outputs.size(),1);
    QCOMPARE(outputs.first().name,QString("sentinel"));
    QCOMPARE(outputs.first().bytes,QByteArray(1,'x'));
}

void VisionBackendBoundaryTests::onnxExecutionFixture()
{
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    const QString modelPath=qEnvironmentVariable("XVISION_ONNX_EXECUTION_TEST_MODEL");
    if(modelPath.isEmpty())
        QSKIP("Set XVISION_ONNX_EXECUTION_TEST_MODEL to an identity model for execution validation");

    OnnxSession session;
    QString error;
    QVERIFY2(session.load(modelPath,&error),qPrintable(error));
    const QList<XOnnxTensorInfo> modelInputs=session.inputs();
    const QList<XOnnxTensorInfo> modelOutputs=session.outputs();
    QCOMPARE(modelInputs.size(),1);
    QCOMPARE(modelOutputs.size(),1);
    QCOMPARE(modelOutputs.first().elementType,modelInputs.first().elementType);

    const auto elementSize=[](const QString &type)
    {
        if(type=="uint8" || type=="int8" || type=="bool") return 1;
        if(type=="uint16" || type=="int16" || type=="float16" || type=="bfloat16") return 2;
        if(type=="uint32" || type=="int32" || type=="float32") return 4;
        if(type=="uint64" || type=="int64" || type=="float64" || type=="complex64") return 8;
        if(type=="complex128") return 16;
        return 0;
    };
    const int bytesPerElement=elementSize(modelInputs.first().elementType);
    QVERIFY2(bytesPerElement>0,qPrintable(modelInputs.first().elementType));
    QVector<qint64> dimensions=modelInputs.first().dimensions;
    qint64 elementCount=1;
    for(qint64 &dimension:dimensions)
    {
        if(dimension<=0) dimension=2;
        QVERIFY(dimension>0);
        QVERIFY(elementCount<=std::numeric_limits<int>::max()/dimension);
        elementCount*=dimension;
    }
    QVERIFY(elementCount<=std::numeric_limits<int>::max()/bytesPerElement);
    XOnnxTensorData input;
    input.name=modelInputs.first().name;
    input.elementType=modelInputs.first().elementType;
    input.dimensions=dimensions;
    input.bytes=QByteArray(static_cast<int>(elementCount*bytesPerElement),
                           input.elementType=="bool"?'\1':'\x2a');
    QList<XOnnxTensorData> outputs;
    QVERIFY2(session.run({input},outputs,&error),qPrintable(error));
    QCOMPARE(outputs.size(),1);
    QCOMPARE(outputs.first().name,modelOutputs.first().name);
    QCOMPARE(outputs.first().elementType,input.elementType);
    QCOMPARE(outputs.first().dimensions,input.dimensions);
    QCOMPARE(outputs.first().bytes,input.bytes);
    const QByteArray ownedOutput=outputs.first().bytes;
    input.bytes.fill('\0');
    QCOMPARE(outputs.first().bytes,ownedOutput);

    XOnnxTensorData invalid=input;
    invalid.dimensions.append(1);
    XOnnxTensorData sentinel;
    sentinel.name="sentinel";
    sentinel.elementType="uint8";
    sentinel.dimensions={1};
    sentinel.bytes=QByteArray(1,'x');
    outputs={sentinel};
    QVERIFY(!session.run({invalid},outputs,&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(outputs.size(),1);
    QCOMPARE(outputs.first().name,QString("sentinel"));
    QCOMPARE(outputs.first().bytes,QByteArray(1,'x'));
#else
    QSKIP("ONNX Runtime backend is disabled");
#endif
}

void VisionBackendBoundaryTests::onnxRejectsInvalidModel()
{
    OnnxSession session;
    QString error;
    QVERIFY(!session.load("relative-model.onnx",&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!session.isLoaded());

    QTemporaryFile invalidModel;
    QVERIFY(invalidModel.open());
    QCOMPARE(invalidModel.write("not-an-onnx-model"),qint64(17));
    const QString path=invalidModel.fileName();
    invalidModel.close();
    QVERIFY(QFileInfo(path).isReadable());

    error.clear();
    QVERIFY(!session.load(path,&error));
    QVERIFY(!error.isEmpty());
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    QVERIFY2(error.startsWith("ONNX Runtime failed to load model:"),qPrintable(error));
#else
    QVERIFY2(error.contains("backend is disabled"),qPrintable(error));
#endif
    QVERIFY(!session.isLoaded());
    QVERIFY(session.modelPath().isEmpty());
}

void VisionBackendBoundaryTests::onnxMetadataFixture()
{
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    const QString modelPath=qEnvironmentVariable("XVISION_ONNX_TEST_MODEL");
    if(modelPath.isEmpty())
        QSKIP("Set XVISION_ONNX_TEST_MODEL to run real ONNX metadata validation");

    OnnxSession session;
    QString error;
    QVERIFY2(session.load(modelPath,&error),qPrintable(error));
    QVERIFY(session.isLoaded());
    QCOMPARE(session.modelPath(),QFileInfo(modelPath).absoluteFilePath());
    QVERIFY(!session.inputs().isEmpty());
    QVERIFY(!session.outputs().isEmpty());
    for(const XOnnxTensorInfo &info:session.inputs()+session.outputs())
    {
        QVERIFY(!info.name.isEmpty());
        QVERIFY(!info.elementType.isEmpty());
        QVERIFY(!info.elementType.at(0).isDigit());
    }

    QTemporaryFile invalidModel;
    QVERIFY(invalidModel.open());
    QCOMPARE(invalidModel.write("invalid"),qint64(7));
    const QString invalidPath=invalidModel.fileName();
    invalidModel.close();
    const QString loadedPath=session.modelPath();
    const QList<XOnnxTensorInfo> loadedInputs=session.inputs();
    QVERIFY(!session.load(invalidPath,&error));
    QVERIFY(session.isLoaded());
    QCOMPARE(session.modelPath(),loadedPath);
    QCOMPARE(session.inputs().size(),loadedInputs.size());
#else
    QSKIP("ONNX Runtime backend is disabled");
#endif
}

void VisionBackendBoundaryTests::openCvSupportedFormats()
{
#if defined(XVISION_ENABLE_OPENCV)
    const QList<QImage::Format> formats={
        QImage::Format_Grayscale8,
        QImage::Format_RGB888,
        QImage::Format_RGBA8888,
        QImage::Format_ARGB32
    };
    for(QImage::Format format:formats)
    {
        QImage source(2,2,format);
        QVERIFY(!source.isNull());
        source.fill(Qt::black);
        source.setPixelColor(0,0,QColor(12,34,56,78));
        source.setPixelColor(1,0,QColor(90,80,70,60));

        cv::Mat matrix;
        QString error="stale";
        QVERIFY2(XvOpenCvImageInterop::toMat(source,matrix,&error),qPrintable(error));
        QVERIFY(error.isEmpty());
        QCOMPARE(matrix.rows,source.height());
        QCOMPARE(matrix.cols,source.width());
        QCOMPARE(matrix.depth(),CV_8U);
        QCOMPARE(matrix.channels(),format==QImage::Format_Grayscale8?1:
                 (format==QImage::Format_RGB888?3:4));

        QImage restored;
        QVERIFY2(XvOpenCvImageInterop::toQImage(matrix,restored,&error),qPrintable(error));
        QVERIFY(error.isEmpty());
        QCOMPARE(restored.pixelColor(0,0),source.pixelColor(0,0));
        QCOMPARE(restored.pixelColor(1,0),source.pixelColor(1,0));
    }
#else
    QSKIP("OpenCV backend is disabled");
#endif
}

void VisionBackendBoundaryTests::openCvPaddedStrideAndOwnership()
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat converted;
    QString error;
    {
        QByteArray pixels(16,'\0');
        const uchar row0[]={10,20,30,40,50,60,201,202};
        const uchar row1[]={70,80,90,100,110,120,203,204};
        std::copy(std::begin(row0),std::end(row0),
                  reinterpret_cast<uchar*>(pixels.data()));
        std::copy(std::begin(row1),std::end(row1),
                  reinterpret_cast<uchar*>(pixels.data())+8);
        const QImage source(reinterpret_cast<const uchar*>(pixels.constData()),2,2,8,
                            QImage::Format_RGB888);
        QVERIFY2(XvOpenCvImageInterop::toMat(source,converted,&error),qPrintable(error));
    }
    const cv::Vec3b pixel00=converted.at<cv::Vec3b>(0,0);
    const cv::Vec3b pixel01=converted.at<cv::Vec3b>(0,1);
    const cv::Vec3b pixel10=converted.at<cv::Vec3b>(1,0);
    const cv::Vec3b pixel11=converted.at<cv::Vec3b>(1,1);
    QCOMPARE(pixel00[0],uchar(30));
    QCOMPARE(pixel00[1],uchar(20));
    QCOMPARE(pixel00[2],uchar(10));
    QCOMPARE(pixel01[0],uchar(60));
    QCOMPARE(pixel01[1],uchar(50));
    QCOMPARE(pixel01[2],uchar(40));
    QCOMPARE(pixel10[0],uchar(90));
    QCOMPARE(pixel10[1],uchar(80));
    QCOMPARE(pixel10[2],uchar(70));
    QCOMPARE(pixel11[0],uchar(120));
    QCOMPARE(pixel11[1],uchar(110));
    QCOMPARE(pixel11[2],uchar(100));

    QImage ownedImage;
    {
        cv::Mat matrix(1,1,CV_8UC3,cv::Scalar(3,2,1));
        QVERIFY2(XvOpenCvImageInterop::toQImage(matrix,ownedImage,&error),qPrintable(error));
        matrix.setTo(cv::Scalar(30,20,10));
    }
    QCOMPARE(ownedImage.pixelColor(0,0),QColor(1,2,3));
#else
    QSKIP("OpenCV backend is disabled");
#endif
}

void VisionBackendBoundaryTests::openCvInvalidInputPreservesTargets()
{
#if defined(XVISION_ENABLE_OPENCV)
    cv::Mat matrix(1,1,CV_8UC1,cv::Scalar(42));
    QImage unsupported(2,2,QImage::Format_Mono);
    QString error;
    QVERIFY(!XvOpenCvImageInterop::toMat(unsupported,matrix,&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(matrix.rows,1);
    QCOMPARE(matrix.cols,1);
    QCOMPARE(matrix.at<uchar>(0,0),uchar(42));

    QImage target(1,1,QImage::Format_RGB888);
    target.fill(QColor(1,2,3));
    const QImage before=target.copy();
    const cv::Mat invalid(1,1,CV_16UC1,cv::Scalar(5));
    error.clear();
    QVERIFY(!XvOpenCvImageInterop::toQImage(invalid,target,&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(target,before);
#else
    QSKIP("OpenCV backend is disabled");
#endif
}

QTEST_MAIN(VisionBackendBoundaryTests)
#include "tst_backendboundaries.moc"
