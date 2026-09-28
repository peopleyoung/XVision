#include <QtTest>

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "IXvCamera.h"
#include "XvCameraManager.h"
#include "XvDirectoryCamera.h"

using namespace XvCamera;

class StaticCameraProvider : public XvCameraProvider
{
public:
    StaticCameraProvider(const QString &providerId,
                         const XvCameraDeviceInfo &device,QObject *parent=nullptr)
        :XvCameraProvider(parent),m_providerId(providerId),m_device(device) {}

    QString providerId() const override { return m_providerId; }
    QList<XvCameraDeviceInfo> devices() const override { return {m_device}; }
    IXvCamera *createCamera(const QString &,QObject *) override { return nullptr; }

private:
    QString m_providerId;
    XvCameraDeviceInfo m_device;
};

class CameraTest : public QObject
{
    Q_OBJECT

    static void writeImage(const QString &path,const QColor &color)
    {
        QImage image(4,3,QImage::Format_RGB32);
        image.fill(color);
        QVERIFY2(image.save(path),qPrintable(QString("Unable to save %1").arg(path)));
    }

    static void flushDeferredDeletes()
    {
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }

private slots:
    void init()
    {
        XvCameraMgr->shutdown();
        XvCameraMgr->directoryProvider()->clearDevices();
        flushDeferredDeletes();
    }

    void cleanup()
    {
        XvCameraMgr->shutdown();
        XvCameraMgr->directoryProvider()->clearDevices();
        flushDeferredDeletes();
    }

    void devicesAreStableAndValidated()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider);
        QSignalSpy changedSpy(XvCameraMgr,&XvCameraManager::devicesChanged);

        QVERIFY(provider->addDevice("z-device",first.path(),"Z camera"));
        QVERIFY(provider->addDevice("a-device",second.path(),"A camera"));
        QVERIFY(!provider->addDevice("a-device",second.path()));
        QVERIFY(!provider->addDevice("bad/device",second.path()));
        QVERIFY(changedSpy.count()>=2);

        const QList<XvCameraDeviceInfo> devices=XvCameraMgr->devices();
        QCOMPARE(devices.count(),2);
        QCOMPARE(devices.at(0).deviceId,
                 XvDirectoryCameraProvider::deviceIdForLocalId("a-device"));
        QCOMPARE(devices.at(1).deviceId,
                 XvDirectoryCameraProvider::deviceIdForLocalId("z-device"));
        QVERIFY(devices.at(0).simulated);
        QCOMPARE(devices.at(0).providerId,
                 XvDirectoryCameraProvider::staticProviderId());

        XvCameraDeviceInfo duplicate=devices.at(0);
        duplicate.providerId="duplicate-provider";
        auto duplicateProvider=new StaticCameraProvider(
                    duplicate.providerId,duplicate);
        QVERIFY(XvCameraMgr->registerProvider(duplicateProvider));
        const QList<XvCameraDeviceInfo> mergedDevices=XvCameraMgr->devices();
        QCOMPARE(mergedDevices.count(),1);
        QCOMPARE(mergedDevices.first().deviceId,devices.at(1).deviceId);
        IXvCamera *duplicateCamera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(duplicate.deviceId,&duplicateCamera),
                 EXvCameraError::InvalidArgument);
        QVERIFY(!duplicateCamera);
        QVERIFY(XvCameraMgr->unregisterProvider(duplicate.providerId));
    }

    void managerOpenCloseAndErrorPaths()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeImage(directory.filePath("frame.png"),Qt::red);
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("open-close",directory.path()));
        const QString id=XvDirectoryCameraProvider::deviceIdForLocalId("open-close");

        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera("missing",&camera),EXvCameraError::NotFound);
        QVERIFY(!camera);
        QVERIFY(!XvCameraMgr->lastError().isEmpty());

        QCOMPARE(XvCameraMgr->openCamera(id,&camera),EXvCameraError::None);
        QVERIFY(camera);
        QCOMPARE(camera->status(),EXvCameraStatus::Open);
        QCOMPARE(camera->close(),EXvCameraError::None);
        XvCameraFrame closedFrame;
        QCOMPARE(camera->grabFrame(closedFrame,1),EXvCameraError::NotOpen);
        QCOMPARE(camera->open(),EXvCameraError::None);
        IXvCamera *sameCamera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&sameCamera),
                 EXvCameraError::AlreadyOpen);
        QCOMPARE(sameCamera,camera);

        QPointer<IXvCamera> guard(camera);
        QCOMPARE(XvCameraMgr->closeCamera(id),EXvCameraError::None);
        QVERIFY(!XvCameraMgr->camera(id));
        IXvCamera *reopened=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&reopened),EXvCameraError::None);
        QVERIFY(reopened);
        QVERIFY(reopened!=camera);
        flushDeferredDeletes();
        QVERIFY(guard.isNull());
        QCOMPARE(XvCameraMgr->camera(id),reopened);
        QCOMPARE(XvCameraMgr->closeCamera(id),EXvCameraError::None);
        flushDeferredDeletes();
        QCOMPARE(XvCameraMgr->closeCamera(id),EXvCameraError::NotOpen);
    }

    void framesUseDeterministicOrderAndSequence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeImage(directory.filePath("20-red.png"),Qt::red);
        writeImage(directory.filePath("10-green.bmp"),Qt::green);
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("ordered",directory.path()));
        const QString id=XvDirectoryCameraProvider::deviceIdForLocalId("ordered");

        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&camera),EXvCameraError::None);
        XvCameraFrame first;
        XvCameraFrame second;
        QCOMPARE(camera->grabFrame(first,100),EXvCameraError::None);
        QCOMPARE(camera->grabFrame(second,100),EXvCameraError::None);
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        QCOMPARE(first.deviceId,id);
        QCOMPARE(first.sequence,quint64(1));
        QCOMPARE(second.sequence,quint64(2));
        QCOMPARE(first.image.pixelColor(0,0),QColor(Qt::green));
        QCOMPARE(second.image.pixelColor(0,0),QColor(Qt::red));
        QCOMPARE(first.capturedAtUtc.timeSpec(),Qt::UTC);

        XvCameraFrame exhausted;
        QCOMPARE(camera->grabFrame(exhausted,100),EXvCameraError::Disconnected);
        QVERIFY(!exhausted.isValid());
        QCOMPARE(camera->status(),EXvCameraStatus::Disconnected);
    }

    void parametersAndTimeoutAreValidated()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeImage(directory.filePath("frame.png"),Qt::blue);
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("parameters",directory.path(),QString(),true,40));
        const QString id=XvDirectoryCameraProvider::deviceIdForLocalId("parameters");

        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&camera),EXvCameraError::None);
        QCOMPARE(camera->parameters().count(),3);
        QCOMPARE(camera->parameter("loop").toBool(),true);
        QCOMPARE(camera->parameter("frameIntervalMs").toUInt(),40U);
        QCOMPARE(camera->setParameter("directory",directory.path()),
                 EXvCameraError::Unsupported);
        QCOMPARE(camera->setParameter("loop",QString("true")),
                 EXvCameraError::InvalidArgument);
        QCOMPARE(camera->setParameter("frameIntervalMs",-1),
                 EXvCameraError::InvalidArgument);
        QCOMPARE(camera->setParameter("frameIntervalMs",60001),
                 EXvCameraError::InvalidArgument);
        QCOMPARE(camera->setParameter("missing",1),EXvCameraError::NotFound);

        XvCameraFrame frame;
        QCOMPARE(camera->grabFrame(frame,1),EXvCameraError::Timeout);
        QVERIFY(!frame.isValid());
        QCOMPARE(camera->status(),EXvCameraStatus::Open);
        QCOMPARE(camera->setParameter("frameIntervalMs",0),EXvCameraError::None);
        QCOMPARE(camera->grabFrame(frame,0),EXvCameraError::None);
        QCOMPARE(frame.sequence,quint64(1));
        QCOMPARE(camera->grabFrame(frame,0),EXvCameraError::None);
        QCOMPARE(frame.sequence,quint64(2));
    }

    void emptyAndCorruptSourcesFailClearly()
    {
        QTemporaryDir emptyDirectory;
        QVERIFY(emptyDirectory.isValid());
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("empty",emptyDirectory.path()));
        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(
                     XvDirectoryCameraProvider::deviceIdForLocalId("empty"),&camera),
                 EXvCameraError::IoError);
        QVERIFY(!camera);

        QTemporaryDir corruptDirectory;
        QVERIFY(corruptDirectory.isValid());
        QFile corrupt(corruptDirectory.filePath("broken.png"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly));
        QCOMPARE(corrupt.write("not-an-image"),qint64(12));
        corrupt.close();
        QVERIFY(provider->addDevice("corrupt",corruptDirectory.path()));
        const QString corruptId=
                XvDirectoryCameraProvider::deviceIdForLocalId("corrupt");
        QCOMPARE(XvCameraMgr->openCamera(corruptId,&camera),EXvCameraError::None);
        QVERIFY(camera);
        XvCameraFrame frame;
        QCOMPARE(camera->grabFrame(frame,100),EXvCameraError::IoError);
        QCOMPARE(camera->status(),EXvCameraStatus::Error);
        QVERIFY(!camera->lastError().isEmpty());
    }

    void continuousAcquisitionStopsAndRejectsMutations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeImage(directory.filePath("01.png"),Qt::black);
        writeImage(directory.filePath("02.png"),Qt::white);
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("continuous",directory.path(),QString(),true,5));
        const QString id=XvDirectoryCameraProvider::deviceIdForLocalId("continuous");

        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&camera),EXvCameraError::None);
        QSignalSpy frameSpy(camera,&IXvCamera::frameReady);
        QCOMPARE(camera->startContinuous(),EXvCameraError::None);
        QCOMPARE(camera->status(),EXvCameraStatus::Streaming);
        QCOMPARE(camera->startContinuous(),EXvCameraError::Busy);
        QCOMPARE(camera->setParameter("loop",false),EXvCameraError::Busy);
        XvCameraFrame frame;
        QCOMPARE(camera->grabFrame(frame,100),EXvCameraError::Busy);
        QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count()>=3,2000);

        QCOMPARE(camera->stopContinuous(),EXvCameraError::None);
        QCOMPARE(camera->status(),EXvCameraStatus::Open);
        const int stoppedCount=frameSpy.count();
        QTest::qWait(30);
        QCOMPARE(frameSpy.count(),stoppedCount);

        QCOMPARE(camera->startContinuous(),EXvCameraError::None);
        QCOMPARE(camera->stopContinuous(),EXvCameraError::None);
        QCOMPARE(camera->status(),EXvCameraStatus::Open);

        QCOMPARE(camera->startContinuous(),EXvCameraError::None);
        QPointer<IXvCamera> guard(camera);
        XvCameraMgr->shutdown();
        flushDeferredDeletes();
        QVERIFY(guard.isNull());
    }

    void disconnectAndShutdownConvergeThreads()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeImage(directory.filePath("frame.png"),Qt::yellow);
        XvDirectoryCameraProvider *provider=XvCameraMgr->directoryProvider();
        QVERIFY(provider->addDevice("disconnect",directory.path(),QString(),true,100));
        const QString id=XvDirectoryCameraProvider::deviceIdForLocalId("disconnect");

        IXvCamera *camera=nullptr;
        QCOMPARE(XvCameraMgr->openCamera(id,&camera),EXvCameraError::None);
        auto directoryCamera=qobject_cast<XvDirectoryCamera*>(camera);
        QVERIFY(directoryCamera);
        QSignalSpy disconnectedSpy(camera,&IXvCamera::disconnected);
        QCOMPARE(camera->startContinuous(),EXvCameraError::None);
        QCOMPARE(directoryCamera->simulateDisconnect("test disconnect"),
                 EXvCameraError::None);
        QCOMPARE(camera->stopContinuous(),EXvCameraError::None);
        QCOMPARE(camera->status(),EXvCameraStatus::Disconnected);
        QCOMPARE(disconnectedSpy.count(),1);
        XvCameraFrame frame;
        QCOMPARE(camera->grabFrame(frame,1),EXvCameraError::Disconnected);

        QPointer<IXvCamera> guard(camera);
        XvCameraMgr->shutdown();
        QVERIFY(!XvCameraMgr->camera(id));
        flushDeferredDeletes();
        QVERIFY(guard.isNull());
    }
};

QTEST_MAIN(CameraTest)
#include "tst_camera.moc"
