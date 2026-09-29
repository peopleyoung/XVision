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
#include "../XvCamera/Core/HardwareBackend.h"
#include "../XvCamera/Core/VendorLibrary.h"
#include <QTimer>
#include <atomic>
#include <future>
#include <thread>

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

namespace {
struct HardwareState
{
    std::atomic_int reads{0},opens{0},closes{0};
    std::atomic_bool timeout{false},offline{false},empty{false};
    std::atomic_int delay{0};
};
class TestHardwareBackend final : public Hardware::Backend
{
public:
    explicit TestHardwareBackend(std::shared_ptr<HardwareState> state):state(std::move(state)) {}
    Hardware::Result open() override
    { ++state->opens; if(state->delay>0) QThread::msleep(unsigned(state->delay.load())); return {}; }
    void close() override { ++state->closes; }
    Hardware::Result read(QImage &image,unsigned int timeout) override
    {
        ++state->reads;
        if(state->offline) return {EXvCameraError::Disconnected,QStringLiteral("测试设备断线")};
        if(state->timeout) { QThread::msleep(timeout); return {EXvCameraError::Timeout,QStringLiteral("测试超时")}; }
        if(!state->empty) { image=QImage(3,2,QImage::Format_RGB32); image.fill(Qt::red); }
        return {};
    }
    std::shared_ptr<HardwareState> state;
};
class TestHardwareSystem final : public Hardware::System
{
public:
    QString id() const override { return "hardware-test"; }
    QString name() const override { return QStringLiteral("硬件测试后端"); }
    Hardware::Result scan(QList<Hardware::Device> &devices) override
    {
        ++scans; QThread::msleep(80);
        Hardware::Device device;
        device.info.providerId=id(); device.info.deviceId="hardware-test:serial-42";
        devices.append(device); return {};
    }
    std::unique_ptr<Hardware::Backend> create(const Hardware::Device &) override
    { return std::make_unique<TestHardwareBackend>(state); }
    std::shared_ptr<HardwareState> state=std::make_shared<HardwareState>();
    std::atomic_int scans{0};
};
struct EnvironmentValue
{
    QByteArray name,old; bool wasSet;
    EnvironmentValue(const char *key,const QByteArray &value):name(key),old(qgetenv(key)),wasSet(qEnvironmentVariableIsSet(key)) { qputenv(key,value); }
    ~EnvironmentValue() { if(wasSet) qputenv(name.constData(),old); else qunsetenv(name.constData()); }
};
Hardware::Device hardwareDevice()
{
    Hardware::Device device; device.info.providerId="hardware-test"; device.info.deviceId="hardware-test:serial-42";
    return device;
}
}

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
    void hardwareFramesAreOwnedAndFailuresClearResults()
    {
        auto state=std::make_shared<HardwareState>();
        Hardware::Camera camera(hardwareDevice(),std::make_unique<TestHardwareBackend>(state));
        QCOMPARE(camera.open(),EXvCameraError::None);
        XvCameraFrame frame;
        QCOMPARE(camera.grabFrame(frame,100),EXvCameraError::None);
        QCOMPARE(frame.sequence,quint64(1));
        const QImage retained=frame.image;
        state->timeout=true;
        QElapsedTimer timer; timer.start();
        QCOMPARE(camera.grabFrame(frame,20),EXvCameraError::Timeout);
        QVERIFY(timer.elapsed()<250); QVERIFY(!frame.isValid());
        QCOMPARE(camera.status(),EXvCameraStatus::Open);
        state->timeout=false;
        QCOMPARE(camera.grabFrame(frame,0),EXvCameraError::None);
        QCOMPARE(frame.sequence,quint64(2));
        state->empty=true;
        QCOMPARE(camera.grabFrame(frame,10),EXvCameraError::IoError); QVERIFY(!frame.isValid());
        state->offline=true;
        QSignalSpy offline(&camera,&IXvCamera::disconnected);
        QCOMPARE(camera.grabFrame(frame,10),EXvCameraError::Disconnected);
        QCOMPARE(camera.status(),EXvCameraStatus::Disconnected);
        QTRY_COMPARE(offline.count(),1);
        QCOMPARE(retained.pixelColor(0,0),QColor(Qt::red));
        QCOMPARE(camera.close(),EXvCameraError::None);
    }
    void hardwareStopIsBoundedAndDeliveryIsCoalesced()
    {
        auto state=std::make_shared<HardwareState>();
        Hardware::Camera camera(hardwareDevice(),std::make_unique<TestHardwareBackend>(state));
        QCOMPARE(camera.open(),EXvCameraError::None);
        QSignalSpy frames(&camera,&IXvCamera::frameReady);
        for(int i=0;i<15;++i)
        {
            QCOMPARE(camera.startContinuous(),EXvCameraError::None);
            QCOMPARE(camera.stopContinuous(),EXvCameraError::None);
            QCOMPARE(camera.status(),EXvCameraStatus::Open);
        }
        QCOMPARE(camera.startContinuous(),EXvCameraError::None);
        QThread::msleep(35); // Simulate a GUI that has not consumed frames yet.
        QVERIFY(state->reads.load()>1);
        QCoreApplication::processEvents();
        QVERIFY(frames.count()<=2);
        QCOMPARE(camera.stopContinuous(),EXvCameraError::None);
        const int stoppedFrames=frames.count();
        QTest::qWait(30); QCOMPARE(frames.count(),stoppedFrames);
        state->timeout=true;
        QCOMPARE(camera.startContinuous(),EXvCameraError::None);
        QTest::qWait(10);
        QElapsedTimer timer; timer.start();
        QCOMPARE(camera.close(),EXvCameraError::None);
        QVERIFY2(timer.elapsed()<400,"Stopping must not wait for a full operator timeout");
    }
    void hardwareDiscoveryIsCachedAndNonBlocking()
    {
        auto system=std::make_shared<TestHardwareSystem>();
        Hardware::Provider provider(system);
        QSignalSpy changed(&provider,&XvCameraProvider::devicesChanged);
        QElapsedTimer timer; timer.start();
        provider.refreshDevices(); provider.refreshDevices();
        QVERIFY(timer.elapsed()<50); QVERIFY(provider.devices().isEmpty());
        QTRY_VERIFY(!provider.isRefreshing());
        QCOMPARE(system->scans.load(),1); QCOMPARE(provider.devices().size(),1);
        QVERIFY(changed.count()>=2);
        auto camera=std::unique_ptr<IXvCamera>(provider.createCamera("hardware-test:serial-42"));
        QVERIFY(camera); QCOMPARE(camera->open(),EXvCameraError::None);
        QVERIFY(!provider.createCamera("hardware-test:missing"));
    }
    void hardwareOpeningDoesNotBlockGuiEvents()
    {
        auto system=std::make_shared<TestHardwareSystem>();
        auto provider=new Hardware::Provider(system);
        QVERIFY(XvCameraMgr->registerProvider(provider));
        provider->refreshDevices(); QTRY_VERIFY(!provider->isRefreshing());
        system->state->delay=120;
        std::atomic_bool done{false}; EXvCameraError result=EXvCameraError::Internal;
        IXvCamera *camera=nullptr;
        int ticks=0; QTimer timer; timer.setInterval(5);
        connect(&timer,&QTimer::timeout,this,[&]() { ++ticks; }); timer.start();
        auto work=std::async(std::launch::async,[&]() { result=XvCameraMgr->openCamera("hardware-test:serial-42",&camera); done=true; });
        QTRY_VERIFY(done.load()); work.get(); timer.stop();
        QCOMPARE(result,EXvCameraError::None); QVERIFY(camera); QVERIFY(ticks>=3);
        QCOMPARE(camera->thread(),XvCameraMgr->thread());
        QCOMPARE(XvCameraMgr->closeCamera(camera->deviceInfo().deviceId),EXvCameraError::None);
        QVERIFY(XvCameraMgr->unregisterProvider("hardware-test"));
    }
    void hardwareCloseCancelsAnOpeningDevice()
    {
        auto system=std::make_shared<TestHardwareSystem>();
        auto provider=new Hardware::Provider(system);
        QVERIFY(XvCameraMgr->registerProvider(provider));
        provider->refreshDevices(); QTRY_VERIFY(!provider->isRefreshing());
        system->state->delay=120;
        std::atomic_bool done{false}; EXvCameraError result=EXvCameraError::Internal;
        auto work=std::async(std::launch::async,[&]() { result=XvCameraMgr->openCamera("hardware-test:serial-42"); done=true; });
        QTRY_VERIFY(system->state->opens.load()>0);
        QCOMPARE(XvCameraMgr->closeCamera("hardware-test:serial-42"),EXvCameraError::Busy);
        QTRY_VERIFY(done.load()); work.get();
        QCOMPARE(result,EXvCameraError::NotOpen); QVERIFY(!XvCameraMgr->camera("hardware-test:serial-42"));
        QVERIFY(XvCameraMgr->unregisterProvider("hardware-test"));
    }
    void industrialDriversAreOptionalAndDiagnosed()
    {
        EnvironmentValue mvs("XVISION_MVS_LIBRARY","/definitely/missing/MvCameraControl.dll");
        EnvironmentValue galaxy("XVISION_GALAXY_LIBRARY","/definitely/missing/GxIAPI.dll");
        for(const auto &system:{Hardware::makeMvsSystem(),Hardware::makeGalaxySystem()})
        {
            QList<Hardware::Device> devices;
            const auto result=system->scan(devices);
            QCOMPARE(result.error,EXvCameraError::Unsupported); QVERIFY(devices.isEmpty());
            QVERIFY(result.message.contains("SDK")); QVERIFY(result.message.contains("XVISION_"));
        }
    }
    void industrialAdaptersExecuteSdkContract_data()
    {
        QTest::addColumn<bool>("mvs");
        QTest::newRow("Hikrobot-MVS") << true;
        QTest::newRow("Daheng-Galaxy") << false;
    }
    void industrialAdaptersExecuteSdkContract()
    {
        QFETCH(bool,mvs);
        const QByteArray path=mvs?QByteArray(XVISION_MOCK_MVS_PATH):QByteArray(XVISION_MOCK_GALAXY_PATH);
        EnvironmentValue environment(mvs?"XVISION_MVS_LIBRARY":"XVISION_GALAXY_LIBRARY",path);
        QLibrary library(QString::fromUtf8(path)); QVERIFY2(library.load(),qPrintable(library.errorString()));
        using SetFault=void (XV_CAMERA_CALL *)(int);
        using Count=int (XV_CAMERA_CALL *)();
        const auto setFault=reinterpret_cast<SetFault>(library.resolve("XvMockSetFault"));
        const auto outstanding=reinterpret_cast<Count>(library.resolve("XvMockOutstanding"));
        const auto handles=reinterpret_cast<Count>(library.resolve("XvMockHandles"));
        QVERIFY(setFault); QVERIFY(outstanding); QVERIFY(handles); setFault(0);
        auto system=mvs?Hardware::makeMvsSystem():Hardware::makeGalaxySystem();
        QList<Hardware::Device> devices;
        const auto scan=system->scan(devices); QVERIFY2(bool(scan),qPrintable(scan.message)); QCOMPARE(devices.size(),1);
        QCOMPARE(devices.first().info.serialNumber,mvs?QString("TEST-MVS-001"):QString("TEST-GALAXY-001"));
        Hardware::Camera camera(devices.first(),system->create(devices.first()));
        QCOMPARE(camera.open(),EXvCameraError::None); QCOMPARE(handles(),1);
        QVERIFY(camera.parameters().size()>=3);
        QCOMPARE(camera.setParameter("ExposureTime",1200.0),EXvCameraError::None);
        QCOMPARE(camera.parameter("ExposureTime").toDouble(),1200.0);
        QCOMPARE(camera.setParameter("Gain",4.0),EXvCameraError::None);
        QCOMPARE(camera.parameter("Gain").toDouble(),4.0);
        QCOMPARE(camera.setParameter("Gain",-999.0),EXvCameraError::InvalidArgument);
        QCOMPARE(camera.setParameter("Gain",QString("4")),EXvCameraError::InvalidArgument);
        QCOMPARE(camera.setParameter("TriggerMode",QString("Software")),EXvCameraError::None);
        XvCameraFrame frame;
        QCOMPARE(camera.grabFrame(frame,100),EXvCameraError::None);
        QCOMPARE(frame.image.pixelColor(0,0).red(),1); // MVS mock mutates SDK memory when released.
        QCOMPARE(outstanding(),0);
        const QImage retained=frame.image;
        setFault(3);
        QCOMPARE(camera.grabFrame(frame,10),EXvCameraError::IoError); QVERIFY(!frame.isValid()); QCOMPARE(outstanding(),0);
        setFault(2);
        QCOMPARE(camera.grabFrame(frame,5),EXvCameraError::Timeout); QCOMPARE(camera.status(),EXvCameraStatus::Open);
        setFault(1);
        QCOMPARE(camera.grabFrame(frame,10),EXvCameraError::Disconnected); QCOMPARE(camera.status(),EXvCameraStatus::Disconnected);
        QCOMPARE(camera.close(),EXvCameraError::None); QCOMPARE(handles(),0);
        QCOMPARE(retained.pixelColor(0,0).red(),1);
        setFault(5);
        QVERIFY(camera.open()!=EXvCameraError::None); QCOMPARE(handles(),0);
        setFault(0);
        QCOMPARE(camera.open(),EXvCameraError::None); QCOMPARE(camera.close(),EXvCameraError::None); QCOMPARE(handles(),0);
    }
    void packedPixelsValidateBoundsAndOwnMemory()
    {
        QByteArray pixels(12,char(100)); QImage image;
        QCOMPARE(Hardware::copyPackedImage(pixels.constData(),4,3,Hardware::Abi::Mono8,12,image).error,EXvCameraError::None);
        pixels.fill(char(0)); QCOMPARE(image.pixelColor(0,0).red(),100);
        QCOMPARE(Hardware::copyPackedImage(pixels.constData(),4,3,Hardware::Abi::Rgb8,12,image).error,EXvCameraError::IoError);
        QVERIFY(image.isNull());
        QCOMPARE(Hardware::copyPackedImage(pixels.constData(),0,3,Hardware::Abi::Mono8,12,image).error,EXvCameraError::IoError);
        for(int pattern=0;pattern<4;++pattern)
        {
            pixels.fill(char(70));
            QCOMPARE(Hardware::copyPackedImage(pixels.constData(),4,3,0x01080008+pattern,12,image).error,EXvCameraError::None);
            QCOMPARE(image.pixelColor(0,0),QColor(70,70,70));
            QCOMPARE(image.pixelColor(3,2),QColor(70,70,70));
        }
        const uchar rgb[]={20,40,80};
        QCOMPARE(Hardware::copyPackedImage(rgb,1,1,Hardware::Abi::Bgr8,3,image).error,EXvCameraError::None);
        QCOMPARE(image.pixelColor(0,0),QColor(80,40,20));
    }

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
