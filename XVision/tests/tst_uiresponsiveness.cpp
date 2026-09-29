#include <QtTest>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QComboBox>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QFrame>
#include <QAbstractSpinBox>
#include <QDir>
#include <memory>
#include "SystemXvFactoryPlugin.h"
#include "UiAppearance.h"
#include <QListWidget>
#include <QLineEdit>
#include "ImageAcquisition.h"
#include "TcpText.h"
#include "NClassification.h"
#include <thread>

#include "FrmLogShow.h"
#include "FrmXvFuncAsm.h"
#include "FrmXvFuncType.h"
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
        applyUiAppearance(*qApp);
        SystemXvFactoryPlugin factory;
        QString error;
        QVERIFY2(XvFuncAsm->registerPlugin(factory.getPlgXvFunc(),factory.getPlgXvFuncPresets(),&error),qPrintable(error));
    }
    void toolboxCreatesVisibleDrawerOnFirstClick()
    {
        QWidget host;host.resize(640,480);
        FrmXvFuncAsm toolbox(&host);toolbox.setGeometry(0,0,56,480);
        toolbox.setDrawerParWidget(&host);host.show();
        QCoreApplication::processEvents();
        QVERIFY(host.findChildren<FrmXvFuncType*>().isEmpty());
        QToolButton *category=nullptr;
        for(auto button:toolbox.findChildren<QToolButton*>())
            if(button->property("Type").isValid()) { category=button;break; }
        QVERIFY(category);
        category->click();
        QTRY_COMPARE_WITH_TIMEOUT(host.findChildren<FrmXvFuncType*>().size(),1,1000);
        auto contents=host.findChild<FrmXvFuncType*>();
        QTRY_VERIFY_WITH_TIMEOUT(contents->isVisible(),1000);
        QTRY_VERIFY_WITH_TIMEOUT(contents->visibleRegion().boundingRect().height()>50,1000);
        auto panel=host.findChild<QFrame*>("operatorPopup");
        QVERIFY(panel);
        QCOMPARE(panel->height(),host.height());
        QVERIFY(panel->width()<=host.width());
        category->click();
        QCOMPARE(host.findChildren<FrmXvFuncType*>().size(),1);
        auto search=toolbox.findChild<QLineEdit*>("operatorSearch");
        QVERIFY(search);search->setText("Delayer");
        QTRY_VERIFY_WITH_TIMEOUT(panel->isVisible(),1000);
        auto list=contents->findChild<QListWidget*>("operatorList");
        QVERIFY(list);QCOMPARE(list->count(),123);
        int matches=0;
        for(int index=0;index<list->count();++index)
            if(!list->item(index)->isHidden()) ++matches;
        QCOMPARE(matches,1);
        search->setText("no-such-operator");
        QTRY_VERIFY_WITH_TIMEOUT(list->item(0)->isHidden(),1000);
        QCOMPARE(host.findChildren<FrmXvFuncType*>().size(),1);
    }
    void acquisitionWindowCanResize()
    {
        ImageAcquisition function;
        const auto before=QApplication::topLevelWidgets();
        function.onShowFunc();
        QWidget *window=nullptr;
        for(auto candidate:QApplication::topLevelWidgets())
            if(!before.contains(candidate) && candidate->isVisible()) window=candidate;
        QVERIFY(window);
        QCoreApplication::processEvents();
        QVERIFY2(window->minimumSize()!=window->maximumSize(),
                 "Acquisition parameters are locked to a fixed window size");
        window->resize(640,420);
        QCoreApplication::processEvents();
        QVERIFY2(window->height()<=440,"Parameter window cannot fit a small desktop");
        window->close();
    }
    void communicationWindowFitsSmallDesktop()
    {
        TcpText function;
        const auto before=QApplication::topLevelWidgets();
        function.onShowFunc();
        QWidget *window=nullptr;
        for(auto candidate:QApplication::topLevelWidgets())
            if(!before.contains(candidate) && candidate->isVisible()) window=candidate;
        QVERIFY(window);
        window->resize(640,420);
        QCoreApplication::processEvents();
        qInfo()<<"LAYOUT communication window"<<window->size();
        QVERIFY2(window->height()<=440,"Long parameter form forces the whole dialog beyond the available height");
        auto scroll=window->findChild<QScrollArea*>("operatorParameterScroll");
        QVERIFY2(scroll,"Long parameter forms need a scrollable content area");
        // A 420px dialog may fit this form with compact Windows font/frame metrics.
        // Force actual overflow before requiring a scrollbar; normal 640x420 layouts
        // are still checked for every operator by allParameterWindowsFit().
        window->resize(640,320);
        QTRY_VERIFY_WITH_TIMEOUT(window->height()<=340,1000);
        // Qt may post further layout requests during child polish/show.
        QTRY_VERIFY_WITH_TIMEOUT(scroll->verticalScrollBar()->maximum()>0,1000);
        QVERIFY(scroll->widget()->height()>scroll->viewport()->height());
        window->close();
    }
    void communicationEditorsSurviveTheirOwnSignal()
    {
        TcpText function;
        const auto before=QApplication::topLevelWidgets();
        function.onShowFunc();
        QWidget *window=nullptr;
        for(auto candidate:QApplication::topLevelWidgets())
            if(!before.contains(candidate) && candidate->isVisible()) window=candidate;
        QVERIFY(window);
        QPointer<QComboBox> frameMode;
        for(auto combo:window->findChildren<QComboBox*>())
            if(combo->count()==3 && combo->findData(2)>=0) frameMode=combo;
        QVERIFY(frameMode);
        frameMode->setCurrentIndex(1);
        QVERIFY2(frameMode,"Changing frame mode synchronously destroys the QComboBox that is still emitting its signal");
        QCoreApplication::processEvents();
        window->close();
    }
    void allParameterWindowsFit_data()
    {
        QTest::addColumn<QString>("role");
        for(const auto &info:XvFuncAsm->getXvFuncInfos())
            QTest::newRow(info.role.toUtf8().constData())<<info.role;
    }
    void allParameterWindowsFit()
    {
        QFETCH(QString,role);
        std::unique_ptr<XvFunc> function(XvFuncAsm->createNewXvFunc(role));
        QVERIFY(function);
        const auto before=QApplication::topLevelWidgets();
        QElapsedTimer timer;timer.start();
        function->onShowFunc();
        QWidget *window=nullptr;
        for(auto candidate:QApplication::topLevelWidgets())
            if(!before.contains(candidate) && candidate->isVisible()) window=candidate;
        if(!window)
        {
            QCOMPARE(function->funcRole(),QString("ElapsedTimer"));
            return;
        }
        const QSize target=QSize(640,420).boundedTo(qApp->primaryScreen()->availableGeometry().size()-QSize(32,32));
        window->resize(target);
        QCoreApplication::processEvents();
        QVERIFY2(window->width()<=target.width()+2 && window->height()<=target.height()+2,
                 qPrintable(role+QString(" is oversized: %1x%2").arg(window->width()).arg(window->height())));
        QVERIFY(window->minimumSize()!=window->maximumSize());
        auto scroll=window->findChild<QScrollArea*>("operatorParameterScroll");
        QVERIFY2(scroll,qPrintable(role));
        for(auto editor:scroll->widget()->findChildren<QWidget*>())
        {
            const bool field=qobject_cast<QComboBox*>(editor) || qobject_cast<QAbstractSpinBox*>(editor)
                    || (qobject_cast<QLineEdit*>(editor)
                        && !qobject_cast<QAbstractSpinBox*>(editor->parentWidget())
                        && !qobject_cast<QComboBox*>(editor->parentWidget()));
            if(!field || !editor->isVisibleTo(scroll->widget())) continue;
            QVERIFY2(editor->height()>=qMax(30,window->fontMetrics().height()+12),
                     qPrintable(role+" compressed editor: "+editor->objectName()));
            scroll->ensureWidgetVisible(editor,4,4);
            const QRect bounds(editor->mapTo(scroll->viewport(),QPoint()),editor->size());
            QVERIFY2(scroll->viewport()->rect().intersects(bounds),
                     qPrintable(role+" unreachable editor: "+editor->objectName()));
        }
        QVERIFY2(timer.elapsed()<1000,qPrintable(role+" took more than 1 s to create its parameter window"));
        const QString screenshots=QString::fromLocal8Bit(qgetenv("XVISION_UI_SCREENSHOT_DIR"));
        if(!screenshots.isEmpty() && QStringList{"NObjectDetection.Yolov5","TcpText","ImageAcquisition.Camera","ORegionDetector"}.contains(role))
        {
            QDir().mkpath(screenshots);scroll->verticalScrollBar()->setValue(0);
            window->grab().save(screenshots+"/"+role+".png");
        }
        window->close();
    }
    void destroyedSidebarReleasesExternalPanel()
    {
        QWidget host;host.resize(700,500);host.show();
        auto toolbox=new FrmXvFuncAsm(&host);toolbox->setDrawerParWidget(&host);toolbox->show();
        QToolButton *button=nullptr;
        for(auto candidate:toolbox->findChildren<QToolButton*>())
            if(candidate->property("Type").isValid()) { button=candidate;break; }
        QVERIFY(button);button->click();
        QPointer<QFrame> panel=host.findChild<QFrame*>("operatorPopup");
        QVERIFY(panel);delete toolbox;QVERIFY(panel.isNull());
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
