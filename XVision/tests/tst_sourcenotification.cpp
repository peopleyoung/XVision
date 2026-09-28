#include <QtTest>

#include <QFile>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>

#include <thread>
#include <atomic>

#include "ImageAcquisition.h"
#include "NotificationOutput.h"
#include "XBool.h"
#include "XImage.h"
#include "XInt.h"
#include "XLogger.h"
#include "XReal.h"
#include "XString.h"
#include "XvNotificationCenter.h"

#if defined(XVISION_ENABLE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

using namespace XvCore;

namespace
{
class TestImageAcquisition:public ImageAcquisition
{
public:
    using ImageAcquisition::executionDirective;
};

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

class SourceNotificationTests:public QObject
{
    Q_OBJECT
private slots:
    void videoContractDefaultsAndDisabledBackend();
    void generatedVideoRangeStepEosAndLoop();
    void notificationModesPublishAndLog();
    void notificationDeliveryCanBeQueuedAcrossThreads();
};

void SourceNotificationTests::videoContractDefaultsAndDisabledBackend()
{
    TestImageAcquisition acquisition;
    QCOMPARE(acquisition.persistentPropertyNames(),
             QStringList({"acqType","localFile","localDir"}));
    const QStringList optional=acquisition.optionalPersistentParameterNames();
    for(const QString &name:{QString("videoPath"),QString("videoStartFrame"),
                             QString("videoEndFrame"),QString("videoFrameStep"),
                             QString("videoLoop")})
    {
        QVERIFY(optional.contains(name));
        QVERIFY2(acquisition.getParamsByName(name),qPrintable(name));
    }
    QCOMPARE(acquisition.videoPath(),QString());
    QCOMPARE(acquisition.videoStartFrame(),0);
    QCOMPARE(acquisition.videoEndFrame(),-1);
    QCOMPARE(acquisition.videoFrameStep(),1);
    QCOMPARE(acquisition.videoLoop(),false);

    const int propertyIndex=acquisition.metaObject()->indexOfProperty("acqType");
    QVERIFY(propertyIndex>=0);
    const QMetaProperty property=acquisition.metaObject()->property(propertyIndex);
    QVERIFY(property.isEnumType());
    QCOMPARE(property.enumerator().keyCount(),4);

    acquisition.setAcqType(ImageAcquisition::Video);
    QCOMPARE(result<XInt>(&acquisition,"acqType")->value(),
             static_cast<int>(ImageAcquisition::Video));
    QCOMPARE(acquisition.executionDirective().kind,XvExecutionDirective::Continue);

    QTemporaryFile source;
    QVERIFY(source.open());
    acquisition.setVideoPath(source.fileName());
    acquisition.setVideoStartFrame(3);
    acquisition.setVideoEndFrame(2);
    result<XImage>(&acquisition,"outputImage")->setValue(
                QImage(2,2,QImage::Format_Grayscale8));
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(!acquisition.getXvFuncRunMsg().isEmpty());
    QVERIFY(result<XImage>(&acquisition,"outputImage")->value().isNull());
    acquisition.setVideoStartFrame(0);
    acquisition.setVideoEndFrame(-1);

#if defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is enabled");
#else
    result<XImage>(&acquisition,"outputImage")->setValue(
                QImage(2,2,QImage::Format_Grayscale8));
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(acquisition.getXvFuncRunMsg().contains("XVISION_ENABLE_OPENCV=ON"));
    QVERIFY(result<XImage>(&acquisition,"outputImage")->value().isNull());
    QCOMPARE(result<XBool>(&acquisition,"videoEndOfStream")->value(),false);
#endif
}

void SourceNotificationTests::generatedVideoRangeStepEosAndLoop()
{
#if !defined(XVISION_ENABLE_OPENCV)
    QSKIP("OpenCV backend is disabled");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path=directory.filePath("frames.avi");
    cv::VideoWriter writer;
    QVERIFY2(writer.open(QFile::encodeName(path).constData(),
                         cv::VideoWriter::fourcc('M','J','P','G'),10.0,
                         cv::Size(16,12),true),"MJPG VideoWriter is unavailable");
    for(int index=0;index<6;++index)
    {
        const int value=index*30;
        writer.write(cv::Mat(12,16,CV_8UC3,cv::Scalar(value,value,value)));
    }
    writer.release();

    TestImageAcquisition acquisition;
    acquisition.setAcqType(ImageAcquisition::Video);
    acquisition.setVideoPath(path);
    acquisition.setVideoStartFrame(1);
    acquisition.setVideoEndFrame(4);
    acquisition.setVideoFrameStep(2);

    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),1);
    QCOMPARE(result<XImage>(&acquisition,"outputImage")->value().size(),QSize(16,12));
    QVERIFY(qAbs(qRed(result<XImage>(&acquisition,"outputImage")->value().pixel(0,0))-30)<=8);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),3);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(result<XImage>(&acquisition,"outputImage")->value().isNull());
    QVERIFY(result<XBool>(&acquisition,"videoEndOfStream")->value());
    QCOMPARE(acquisition.executionDirective().kind,XvExecutionDirective::SelectPorts);
    QVERIFY(acquisition.executionDirective().selectedPorts.isEmpty());
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QVERIFY(result<XBool>(&acquisition,"videoEndOfStream")->value());
    QVERIFY(result<XImage>(&acquisition,"outputImage")->value().isNull());

    acquisition.setVideoLoop(true);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),1);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),3);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),1);
    QCOMPARE(acquisition.executionDirective().kind,XvExecutionDirective::Continue);

    acquisition.setVideoStartFrame(2);
    QVERIFY(result<XImage>(&acquisition,"outputImage")->value().isNull());
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),-1);
    acquisition.setVideoEndFrame(2);
    acquisition.setVideoLoop(false);
    QCOMPARE(acquisition.runXvFunc(),EXvFuncRunStatus::Ok);
    QCOMPARE(result<XInt>(&acquisition,"videoFrameIndex")->value(),2);
#endif
}

void SourceNotificationTests::notificationModesPublishAndLog()
{
    NotificationOutput output;
    QCOMPARE(output.funcRole(),QString("NotificationOutput"));
    QCOMPARE(output.persistentPropertyNames(),QStringList({"mode"}));
    QVERIFY(parameter<XImage>(&output,"inputImage"));
    QVERIFY(parameter<XString>(&output,"message"));
    QVERIFY(result<XImage>(&output,"outputImage"));

    QSignalSpy notificationSpy(XvNotificationMgr,
            &XvNotificationCenter::notificationPublished);
    QVector<XLogger::ELogType> loggedTypes;
    connect(XLog,&XLogger::signalLog,&output,
            [&](const QString &,XLogger::ELogType type)
    {
        loggedTypes.append(type);
    });
    const QVector<int> expectedLevels={XLOG_LEVEL_EVENT,XLOG_LEVEL_WARN,
        XLOG_LEVEL_INFO,XLOG_LEVEL_EVENT,XLOG_LEVEL_WARN,XLOG_LEVEL_ERROR,
        XLOG_LEVEL_CRITICAL,XLOG_LEVEL_INFO};
    const QStringList expectedMessages={"OK","NG","Info","Success","Warning",
                                        "Error","Fatal","Dialog"};
    QImage input(2,2,QImage::Format_Grayscale8);
    input.fill(81);
    parameter<XImage>(&output,"inputImage")->setValue(input);

    for(int index=0;index<8;++index)
    {
        output.setMode(NotificationOutput::Mode(index));
        parameter<XString>(&output,"message")->setValue(QString());
        const int notificationCount=notificationSpy.count();
        const int logCount=loggedTypes.count();
        QCOMPARE(output.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(notificationSpy.count(),notificationCount+1);
        QCOMPARE(loggedTypes.count(),logCount+1);
        const QList<QVariant> event=notificationSpy.last();
        QCOMPARE(static_cast<int>(event.at(0).value<XvNotificationKind>()),index);
        QCOMPARE(event.at(1).toString(),expectedMessages.at(index));
        QCOMPARE(event.at(2).toString(),output.funcId());
        QCOMPARE(event.at(3).toBool(),index==NotificationOutput::Dialog);
        QCOMPARE(static_cast<int>(loggedTypes.last()),expectedLevels.at(index));
        QVERIFY(result<XBool>(&output,"published")->value());
        QCOMPARE(result<XBool>(&output,"accepted")->value(),
                 index!=NotificationOutput::Ng);
        QCOMPARE(result<XInt>(&output,"notificationKind")->value(),index);
        QCOMPARE(result<XString>(&output,"publishedMessage")->value(),
                 expectedMessages.at(index));
        QCOMPARE(result<XImage>(&output,"outputImage")->value(),input);
    }

    parameter<XString>(&output,"message")->setValue(
                QString(XvNotificationCenter::MaximumMessageLength+1,'x'));
    const int notificationCount=notificationSpy.count();
    const int logCount=loggedTypes.count();
    QCOMPARE(output.runXvFunc(),EXvFuncRunStatus::Error);
    QVERIFY(!result<XBool>(&output,"published")->value());
    QVERIFY(result<XImage>(&output,"outputImage")->value().isNull());
    QVERIFY(!output.getXvFuncRunMsg().isEmpty());
    QCOMPARE(notificationSpy.count(),notificationCount);
    QCOMPARE(loggedTypes.count(),logCount);

    output.setMode(static_cast<NotificationOutput::Mode>(99));
    QCOMPARE(output.runXvFunc(),EXvFuncRunStatus::Error);
    QCOMPARE(notificationSpy.count(),notificationCount);
    QCOMPARE(loggedTypes.count(),logCount);
}

void SourceNotificationTests::notificationDeliveryCanBeQueuedAcrossThreads()
{
    NotificationOutput output;
    output.setMode(NotificationOutput::Dialog);
    parameter<XString>(&output,"message")->setValue("worker notification");
    (void)XLog;
    QObject receiver;
    bool called=false;
    QThread *deliveryThread=nullptr;
    QString deliveredMessage;
    XvNotificationKind deliveredKind=XvNotificationKind::Info;
    bool deliveredModal=false;
    connect(XvNotificationMgr,&XvNotificationCenter::notificationPublished,
            &receiver,[&](XvNotificationKind kind,const QString &message,
                          const QString &,bool modal)
    {
        called=true;
        deliveryThread=QThread::currentThread();
        deliveredMessage=message;
        deliveredKind=kind;
        deliveredModal=modal;
    },Qt::QueuedConnection);

    std::atomic_int runStatus{static_cast<int>(EXvFuncRunStatus::Init)};
    std::thread publisher([&]()
    {
        runStatus.store(static_cast<int>(output.runXvFunc()));
    });
    publisher.join();
    QCOMPARE(runStatus.load(),static_cast<int>(EXvFuncRunStatus::Ok));
    QVERIFY(!called);
    QTRY_VERIFY_WITH_TIMEOUT(called,2000);
    QCOMPARE(deliveryThread,QThread::currentThread());
    QCOMPARE(deliveredMessage,QString("worker notification"));
    QCOMPARE(static_cast<int>(deliveredKind),
             static_cast<int>(XvNotificationKind::Dialog));
    QVERIFY(deliveredModal);
}

QTEST_MAIN(SourceNotificationTests)
#include "tst_sourcenotification.moc"
