#include <QtTest>
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QDialog>
#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <cstring>
#include "GlobalScript.h"
#include "ProjectScript.h"
#include "GlobalToolsDialog.h"
#include "UiAppearance.h"
#include "XLanguage.h"
#include "XvProject.h"
#include "XvFlow.h"
#include "XvFunc.h"
#include "XvFuncAssembly.h"
#include "XFlowGraphicsRouting.h"
#include "XFlowGraphicsConnectLink.h"
#include "XFlowGraphicsRectItem.h"
using namespace XvCore;
class ScalarFixture:public XvFunc {
    Q_OBJECT
public:
    Q_INVOKABLE explicit ScalarFixture(QObject *parent=nullptr):XvFunc(parent) {
        _funcRole="ScalarFixture";_funcName="测试算子";_funcType=EXvFuncType::Other;
        new XInt("number",7,&m_parameters);new XString("text","before",&m_parameters);new XInt("output",42,&m_results);
    }
    XvBaseParam *getParam() const override {return const_cast<XvBaseParam*>(&m_parameters);}
    XvBaseResult *getResult() const override {return const_cast<XvBaseResult*>(&m_results);}
private:XvBaseParam m_parameters;XvBaseResult m_results;
};
class GlobalToolsTests:public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    static XvGlobalState initialState() {return {{{"count",QJsonObject{{"type","int"},{"value",1},{"description",QStringLiteral("计数")}}}},"globals.set('count', 2);"};}
    static QJsonObject input(const QString &script) {return {{"script",script},{"globals",QJsonObject{{"count",1}}},{"parameters",QJsonObject{{"p",7}}},{"results",QJsonObject{{"r",42}}}};}
private slots:
    void initTestCase() {
        qRegisterMetaType<QJsonObject>();QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,m_config.path());
        XLang->init();applyUiAppearance(*qApp);QVERIFY(XvFuncAsm->registerXvFunc(ScalarFixture::staticMetaObject));
    }
    void globalsXmlRoundTripAndStrictValidation() {
        auto state=initialState();state.variables.insert("text",QJsonObject{{"type","string"},{"value",QStringLiteral("换行\n<&>\"文本")},{"description",""}});state.script="// ]]>\nconsole.log('中文');";
        QDomDocument doc;doc.appendChild(state.toXml(doc));QDomDocument decoded;QVERIFY(decoded.setContent(doc.toByteArray()));XvGlobalState restored;QString error;
        QVERIFY2(XvGlobalState::fromXml(decoded.documentElement(),restored,error),qPrintable(error));QCOMPARE(restored.variables,state.variables);QCOMPARE(restored.script,state.script);
        decoded.documentElement().appendChild(decoded.documentElement().firstChildElement("Variable").cloneNode(true));QVERIFY(!XvGlobalState::fromXml(decoded.documentElement(),restored,error));QCOMPARE(restored.variables,state.variables);
        state.variables["count"]=QJsonObject{{"type","int"},{"value",1.5},{"description",""}};QVERIFY(!state.validate(error));
    }
    void projectPersistsGlobalsAndLoadsOldFiles() {
        QDomDocument doc;{XvProject original;QVERIFY(original.setGlobalState(initialState()));doc.appendChild(original.toXmlElement(doc));}
        {XvProject restored;auto element=doc.documentElement();QVERIFY2(restored.fromXmlElement(element),qPrintable(restored.lastErrorMsg()));QCOMPARE(restored.globalState().variables,initialState().variables);}
        auto root=doc.documentElement();root.removeChild(root.firstChildElement("Globals"));XvProject legacy;QVERIFY(legacy.fromXmlElement(root));QVERIFY(legacy.globalState().variables.isEmpty());
    }
    void runningProjectRejectsGlobalEdits() {
        XvProject project;QVERIFY(project.setGlobalState(initialState()));QCOMPARE(project.runLoop(),Ret_Xv_Success);QVERIFY(project.isRunning());auto next=initialState();next.script="changed";QVERIFY(!project.setGlobalState(next));QCOMPARE(project.globalState().script,initialState().script);project.stop();QCOMPARE(project.wait(3000),Ret_Xv_Success);
    }
    void engineReadsWritesAndReturnsLogs() {
        GlobalScriptRunner runner;QSignalSpy spy(&runner,&GlobalScriptRunner::finished);
        QVERIFY(runner.start(input("globals.set('count',globals.get('count')+1);parameters.set('p',results.get('r'));console.log('完成',globals.get('count'));")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(),1,5000);const auto args=spy.takeFirst();QVERIFY2(args.at(0).toBool(),qPrintable(args.at(2).toString()));const auto result=args.at(1).toJsonObject();QCOMPARE(result.value("globals").toObject().value("count").toInt(),2);QCOMPARE(result.value("parameters").toObject().value("p").toInt(),42);QCOMPARE(result.value("logs").toArray().first().toString(),QStringLiteral("完成 2"));
    }
    void engineErrorsDoNotReturnPartialChanges_data() {QTest::addColumn<QString>("script");QTest::newRow("syntax")<<"const = ;";QTest::newRow("runtime")<<"globals.set('count',5);throw new Error('停止');";QTest::newRow("missing")<<"globals.get('missing');";QTest::newRow("nonfinite")<<"globals.set('count',Infinity);";}
    void engineErrorsDoNotReturnPartialChanges() {QFETCH(QString,script);GlobalScriptRunner runner;QSignalSpy spy(&runner,&GlobalScriptRunner::finished);QVERIFY(runner.start(input(script)));QTRY_COMPARE_WITH_TIMEOUT(spy.count(),1,5000);QVERIFY(!spy.first().at(0).toBool());QVERIFY(!spy.first().at(2).toString().isEmpty());QVERIFY(!spy.first().at(1).toJsonObject().contains("globals"));}
    void timeoutAndStopKeepUiResponsive() {
        GlobalScriptRunner runner;QSignalSpy spy(&runner,&GlobalScriptRunner::finished);int beats=0;QTimer timer;connect(&timer,&QTimer::timeout,this,[&]{++beats;});timer.start(10);
        QVERIFY(runner.start(input("while(true){}"),250));QVERIFY(!runner.start(input("")));QTRY_COMPARE_WITH_TIMEOUT(spy.count(),1,3000);QVERIFY(!spy.takeFirst().at(0).toBool());QVERIFY(beats>5);
        QVERIFY(runner.start(input("while(true){}"),5000));QTimer::singleShot(100,&runner,&GlobalScriptRunner::stop);QTRY_COMPARE_WITH_TIMEOUT(spy.count(),1,3000);QVERIFY(!spy.takeFirst().at(0).toBool());
        QVERIFY(runner.start(input("console.log('恢复');")));QTRY_COMPARE_WITH_TIMEOUT(spy.count(),1,5000);QVERIFY(spy.first().at(0).toBool());
    }
    void projectCommitValidatesAllChangesAndRejectsStaleSnapshots() {
        XvProject project;QVERIFY(project.setGlobalState(initialState()));auto flow=project.createXvFlow("流程");QVERIFY(flow);auto function=flow->createXvFunc("ScalarFixture");QVERIFY(function);
        ProjectScriptSnapshot snapshot;QString error;QVERIFY(captureProjectScript(&project,snapshot,error));QString key=flow->flowId()+"/"+function->funcId()+"/number";QJsonObject result{{"globals",QJsonObject{{"count",2}}},{"parameters",QJsonObject{{key,9}}}};
        auto bad=result;bad["parameters"]=QJsonObject{{key,1.5}};QVERIFY(!applyProjectScript(snapshot,bad,error));QCOMPARE(function->getParamsByName("number")->toString(),QString("7"));QCOMPARE(project.globalState().variables.value("count").toObject().value("value").toInt(),1);
        QVERIFY2(applyProjectScript(snapshot,result,error),qPrintable(error));QCOMPARE(function->getParamsByName("number")->toString(),QString("9"));QCOMPARE(project.globalState().variables.value("count").toObject().value("value").toInt(),2);
        QVERIFY(!applyProjectScript(snapshot,result,error));QVERIFY(captureProjectScript(&project,snapshot,error));dynamic_cast<XInt*>(function->getParamsByName("number"))->setValue(11);QVERIFY(!applyProjectScript(snapshot,result,error));
    }
    void boundParametersRejectScriptWrites() {
        XvProject project;auto flow=project.createXvFlow("f");auto source=flow->createXvFunc("ScalarFixture");auto destination=flow->createXvFunc("ScalarFixture");QVERIFY(source && destination);QVERIFY(source->addSonFunc(destination));QVERIFY(destination->paramSubscribe("number",source,"output"));
        ProjectScriptSnapshot snapshot;QString error;QVERIFY(captureProjectScript(&project,snapshot,error));const auto key=flow->flowId()+"/"+destination->funcId()+"/number";
        QVERIFY(!applyProjectScript(snapshot,{{"globals",QJsonObject()},{"parameters",QJsonObject{{key,5}}}},error));QVERIFY(destination->isParamSubscribe("number"));
    }
    void portRoutesAvoidEndpointsAndFollowNodeMoves() {
        const QRectF source(-60,-30,120,60),target(-280,-120,120,60);auto path=makeOrthogonalPath({60,0},{-280,-90},source,target);
        QVERIFY(path.elementCount()>3);
        for(int i=1;i<path.elementCount();++i) {auto a=path.elementAt(i-1),b=path.elementAt(i);QPointF mid((a.x+b.x)/2,(a.y+b.y)/2);QVERIFY(!source.adjusted(1,1,-1,-1).contains(mid));QVERIFY(!target.adjusted(1,1,-1,-1).contains(mid));QVERIFY(a.x==b.x || a.y==b.y);}
        XFlowGraphicsRectItem first,second;second.setPos(240,100);XFlowGraphicsConnectLink link;QVERIFY(link.setFatherXItemKey(&first,"Right"));QVERIFY(link.setSonXItemKey(&second,"Left"));auto before=link.connectionPath();second.setPos(-200,-150);QVERIFY(link.connectionPath()!=before);QCOMPARE(link.connectionPath().currentPosition(),link.sonEndPos());
    }
    void destroyedProjectsRejectResults() {ProjectScriptSnapshot snapshot;QString error;auto project=new XvProject;QVERIFY(captureProjectScript(project,snapshot,error));delete project;QVERIFY(!applyProjectScript(snapshot,{{"globals",QJsonObject()},{"parameters",QJsonObject()}},error));}
    void globalManagerActuallyAppliesEdits() {
        XvProject project;QVERIFY(project.setGlobalState(initialState()));bool applied=false;QTimer::singleShot(0,this,[&]{auto dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;auto table=dialog->findChild<QTableWidget*>("globalVariableTable");auto buttons=dialog->findChild<QDialogButtonBox*>();if(table && buttons){table->item(0,2)->setText("17");buttons->button(QDialogButtonBox::Apply)->click();applied=true;}dialog->reject();});showGlobalManager(&project,nullptr);QVERIFY(applied);QCOMPARE(project.globalState().variables.value("count").toObject().value("value").toInt(),17);
    }
    void settingsDialogAppliesUserSelection() {
        bool changed=false;
        QTimer::singleShot(0,this,[&]{auto dialog=qobject_cast<QDialog*>(qApp->activeModalWidget());if(!dialog)return;auto selector=dialog->findChild<QComboBox*>("themeSelector");if(selector){selector->setCurrentIndex(selector->findData("emerald"));changed=true;}dialog->accept();});
        showUiSettings(nullptr);QVERIFY(changed);QCOMPARE(currentUiThemeId(),QString("emerald"));applyUiTheme(*qApp,"tech-blue");
    }
    void themesUpdateExistingGraphicsAndPersist() {
        QGraphicsScene scene;QGraphicsView view(&scene);XFlowGraphicsRectItem node;XFlowGraphicsConnectLink link;scene.addItem(&node);scene.addItem(&link);QCOMPARE(uiThemes().size(),5);QSet<QRgb> backgrounds;
        for(const auto &theme:uiThemes()) {applyUiTheme(*qApp,theme.id);QCOMPARE(currentUiThemeId(),theme.id);auto palette=qApp->palette();backgrounds.insert(palette.color(QPalette::Window).rgb());QCOMPARE(node.textPen().color(),palette.color(QPalette::Text));QCOMPARE(node.itemRectBrush().color(),palette.color(QPalette::Button));QCOMPARE(link.textPen().color(),palette.color(QPalette::Text));QVERIFY(std::abs(palette.color(QPalette::Window).lightness()-palette.color(QPalette::Text).lightness())>140);}
        QCOMPARE(backgrounds.size(),5);applyUiAppearance(*qApp);QCOMPARE(currentUiThemeId(),QString("nebula"));applyUiTheme(*qApp,"invalid",false);QCOMPARE(currentUiThemeId(),QString("tech-blue"));
    }
    void orthogonalRoutesAndHitTargets() {
        const QList<QPair<QPointF,QPointF>> cases={{{0,0},{240,120}},{{240,80},{0,0}},{{0,0},{0,180}},{{0,0},{0,0}},{{0,0},{-130,-150}}};
        for(const auto &points:cases) {auto path=makeOrthogonalPath(points.first,points.second);QCOMPARE(path.currentPosition(),points.second);for(int i=1;i<path.elementCount();++i){const auto a=path.elementAt(i-1),b=path.elementAt(i);QVERIFY(qFuzzyCompare(a.x+1,b.x+1)||qFuzzyCompare(a.y+1,b.y+1));}}
        XFlowGraphicsConnectLink link;link.setFatherStartPos({0,0});link.setSonEndPos({240,120});auto path=link.connectionPath();QVERIFY(path.elementCount()>=3);for(int i=1;i<path.elementCount();++i){auto a=path.elementAt(i-1),b=path.elementAt(i);QPointF midpoint((a.x+b.x)/2,(a.y+b.y)/2);QVERIFY(link.shape().contains(midpoint));QVERIFY(link.boundingRect().contains(midpoint));}auto before=link.boundingRect();link.setSonEndPos({-200,-200});QVERIFY(link.boundingRect()!=before);QCOMPARE(link.connectionPath().currentPosition(),QPointF(-200,-200));
    }
};
int main(int argc,char **argv) {if(argc==2 && std::strcmp(argv[1],"--script-worker")==0)return runGlobalScriptWorker(argc,argv);QApplication app(argc,argv);GlobalToolsTests tests;return QTest::qExec(&tests,argc,argv);}
#include "tst_globaltools.moc"
