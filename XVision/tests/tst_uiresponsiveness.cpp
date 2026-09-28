#include <QtTest>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QToolButton>
#include <thread>

#include "FrmLogShow.h"
#include "BaseSystemFuncWdg.h"
#include "Delayer.h"
#include "XInt.h"
#include "XLanguage.h"
#include "XLogger.h"
#include "XvCoreManager.h"
#include "XvProject.h"
#include "XvFlow.h"
#include "XvFuncAssembly.h"

using namespace XvCore;

class UiResponsivenessTests:public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        XLang->init();
        qRegisterMetaType<XLogger::ELogType>();
        QVERIFY(XvFuncAsm->registerXvFunc(Delayer::staticMetaObject));
    }
    void logBurstKeepsGuiResponsive()
    {
        auto window=FrmLogShow::getInstance();
        window->resize(640,320);
        window->show();
        QCoreApplication::processEvents();
        auto editor=window->findChild<QPlainTextEdit*>("ptxtLog");
        QVERIFY(editor);
        QElapsedTimer timer;timer.start();
        for(int index=0;index<10000;++index)
            emit XLog->signalLog(QString("CPU regression log %1").arg(index),XLogger::Info);
        const qint64 elapsed=timer.elapsed();
        qInfo()<<"PERF gui_log_burst_10000_ms"<<elapsed;
        QVERIFY2(elapsed<250,qPrintable(QString("GUI blocked for %1 ms by a log burst").arg(elapsed)));
        QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("CPU regression log 9999"),2000);
        QVERIFY(editor->document()->blockCount()<=200);
    }
    void workerLogBurstKeepsGuiResponsive()
    {
        auto window=FrmLogShow::getInstance();
        auto editor=window->findChild<QPlainTextEdit*>("ptxtLog");
        QVERIFY(editor);
        std::thread producer([] {
            for(int index=0;index<10000;++index)
                emit XLog->signalLog(QString("Worker regression log %1").arg(index),XLogger::Info);
        });
        producer.join();
        QElapsedTimer timer;timer.start();
        QCoreApplication::processEvents();
        const qint64 elapsed=timer.elapsed();
        qInfo()<<"PERF worker_log_drain_10000_ms"<<elapsed;
        QVERIFY2(elapsed<250,qPrintable(QString("GUI event queue blocked for %1 ms").arg(elapsed)));
        QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("Worker regression log 9999"),2000);
    }
    void manualOperatorRunKeepsGuiResponsive()
    {
        auto project=XvCoreManager::getInstance()->createNewXvProject("UI responsiveness");
        QVERIFY(project);
        auto flow=project->createXvFlow("Manual operation");
        QVERIFY(flow);
        auto function=flow->createXvFunc("Delayer");
        QVERIFY(function);
        auto delay=dynamic_cast<XInt*>(function->getParamsByName("dealyMs"));
        QVERIFY(delay);delay->setValue(400);
        BaseSystemFuncWdg window(function);
        auto run=window.findChild<QToolButton*>("btnRun");
        QVERIFY(run);
        QElapsedTimer timer;timer.start();
        run->click();
        const qint64 elapsed=timer.elapsed();
        qInfo()<<"PERF manual_operator_click_ms"<<elapsed;
        const bool responsive=elapsed<100;
        const bool reserved=flow->isRunning();
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),3000);
        QVERIFY2(responsive,"Manual operator execution blocks the GUI thread");
        QVERIFY2(reserved,"Manual execution must reserve its owning flow");
        QCOMPARE(function->getXvFuncRunStatus(),EXvFuncRunStatus::Ok);
    }
    void manualRunOnlyExecutesSelectedNodeAndKeepsStopGuard()
    {
        auto project=XvCoreManager::getInstance()->createNewXvProject("Single step guard");
        QVERIFY(project);
        auto flow=project->createXvFlow("Disconnected nodes");QVERIFY(flow);
        auto selected=flow->createXvFunc("Delayer");
        auto untouched=flow->createXvFunc("Delayer");QVERIFY(selected && untouched);
        dynamic_cast<XInt*>(selected->getParamsByName("dealyMs"))->setValue(400);
        QSignalSpy started(selected,&XvFunc::sgFuncRunStart);
        QCOMPARE(flow->runFunctionOnce(selected->funcId()),Ret_Xv_Success);
        QCOMPARE(flow->runFunctionOnce(selected->funcId()),Ret_Xv_FlowRunning);
        QTRY_COMPARE_WITH_TIMEOUT(started.count(),1,2000);
        QCOMPARE(flow->stop(),Ret_Xv_Success);
        QVERIFY(flow->isRunning());
        QVERIFY(!flow->isEditAllowed());
        QVERIFY(!flow->removeXvFunc(selected->funcId()));
        QCOMPARE(flow->runFunctionOnce(untouched->funcId()),Ret_Xv_FlowRunning);
        QTRY_VERIFY_WITH_TIMEOUT(!flow->isRunning(),2000);
        QVERIFY(flow->isEditAllowed());
        QCOMPARE(untouched->getXvFuncRunInfo().runIdx,0U);
        QCOMPARE(selected->getXvFuncRunInfo().runIdx,1U);
    }
    void loopStopWakesLongInterval()
    {
        auto project=XvCoreManager::getInstance()->createNewXvProject("Stop latency");
        QVERIFY(project);
        auto flow=project->createXvFlow("Long interval");QVERIFY(flow);
        auto function=flow->createXvFunc("Delayer");QVERIFY(function);
        dynamic_cast<XInt*>(function->getParamsByName("dealyMs"))->setValue(0);
        flow->getFlowConfig()->loopInterval=10000;
        QSignalSpy ended(flow,&XvFlow::sgFlowRunEnd);
        QCOMPARE(flow->runLoop(),Ret_Xv_Success);
        QTRY_VERIFY_WITH_TIMEOUT(ended.count()>0,2000);
        QElapsedTimer timer;timer.start();
        QCOMPARE(flow->stop(),Ret_Xv_Success);
        QCOMPARE(flow->wait(500),Ret_Xv_Success);
        QVERIFY(timer.elapsed()<500);
        QVERIFY(!flow->isRunning());
    }
    void clearingLogAlsoClearsPendingMessages()
    {
        auto window=FrmLogShow::getInstance();
        emit XLog->signalLog("must not reappear",XLogger::Warn);
        QVERIFY(QMetaObject::invokeMethod(window,"onClearLog",Qt::DirectConnection));
        QTest::qWait(100);
        auto editor=window->findChild<QPlainTextEdit*>("ptxtLog");
        QVERIFY(editor && editor->toPlainText().isEmpty());
    }

};
QTEST_MAIN(UiResponsivenessTests)
#include "tst_uiresponsiveness.moc"
