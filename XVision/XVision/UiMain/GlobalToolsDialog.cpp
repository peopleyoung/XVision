#include "GlobalToolsDialog.h"
#include "GlobalScript.h"
#include "ProjectScript.h"
#include "XvProject.h"
#include <QApplication>
#include <algorithm>
#include <functional>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QSplitter>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextDocument>
#include <QCheckBox>
namespace {
class EditableDialog : public QDialog {
public:
    using QDialog::QDialog;
    std::function<bool()> confirmClose;
    void reject() override {if(!confirmClose || confirmClose())QDialog::reject();}
};
QString typeLabel(const QString &type) {
    if(type=="int")return QStringLiteral("整数");if(type=="real")return QStringLiteral("实数");if(type=="bool")return QStringLiteral("布尔");return QStringLiteral("字符串");
}
QString quoted(const QString &text) {
    auto array=QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);return QString::fromUtf8(array.mid(1,array.size()-2));
}
QString expression(const ProjectValue &value) {
    const QString api=value.group==QStringLiteral("全局变量")?"globals":value.parameter?"parameters":"results";
    return api+".get("+quoted(value.key)+")";
}
bool available(XvCore::XvProject *project,QWidget *parent) {
    if(project)return true;QMessageBox::information(parent,QStringLiteral("项目为空"),QStringLiteral("请先创建或打开项目。"));return false;
}
QLabel *hint(QVBoxLayout *layout,const QString &text,QWidget *parent) {
    auto label=new QLabel(text,parent);label->setWordWrap(true);layout->addWidget(label);return label;
}
}
void showGlobalManager(XvCore::XvProject *initial,QWidget *parent) {
    if(!available(initial,parent))return;QPointer<XvCore::XvProject> project(initial);
    EditableDialog dialog(parent);dialog.setObjectName("globalManagerDialog");dialog.setWindowTitle(QStringLiteral("全局管理 — 项目共享变量"));dialog.resize(790,500);
    auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(18,16,18,16);layout->setSpacing(12);
    hint(layout,QStringLiteral("管理可在全局脚本中读写的项目共享变量。名称使用英文字母、数字和下划线；应用后请保存项目。"),&dialog);
    auto table=new QTableWidget(0,4,&dialog);table->setObjectName("globalVariableTable");table->setHorizontalHeaderLabels({QStringLiteral("变量名"),QStringLiteral("类型"),QStringLiteral("当前值"),QStringLiteral("说明")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0,160);table->setColumnWidth(1,100);table->setColumnWidth(2,190);layout->addWidget(table,1);
    bool dirty=false;
    const auto append=[&](const QString &name,const QJsonObject &variable){
        int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(name));
        auto types=new QComboBox(table);for(const QString &type:{"int","real","bool","string"})types->addItem(typeLabel(type),type);
        types->setCurrentIndex(types->findData(variable.value("type").toString()));table->setCellWidget(row,1,types);
        table->setItem(row,2,new QTableWidgetItem(scalarValueText(variable.value("value"))));table->setItem(row,3,new QTableWidgetItem(variable.value("description").toString()));table->setRowHeight(row,36);
        QObject::connect(types,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&]{dirty=true;});
    };
    auto state=project->globalState();for(auto it=state.variables.begin();it!=state.variables.end();++it)append(it.key(),it.value().toObject());
    auto actions=new QHBoxLayout;layout->addLayout(actions);auto add=new QPushButton(QStringLiteral("新增变量"),&dialog);auto remove=new QPushButton(QStringLiteral("删除选中"),&dialog);actions->addWidget(add);actions->addWidget(remove);actions->addStretch();
    auto status=hint(layout,QString(),&dialog);status->setObjectName("globalManagerStatus");
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    quint64 revision=project->globalRevision();
    QObject::connect(table,&QTableWidget::itemChanged,&dialog,[&]{dirty=true;});
    dialog.confirmClose=[&]{return !dirty || QMessageBox::question(&dialog,QStringLiteral("尚未应用"),QStringLiteral("全局变量修改尚未应用，确定放弃这些修改吗？"),QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Discard;};
    QObject::connect(add,&QPushButton::clicked,&dialog,[&]{if(table->rowCount()>=256){status->setText(QStringLiteral("最多支持 256 个变量"));return;}QString name;int index=1;do{name="value"+QString::number(index++);}while([&]{for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->text()==name)return true;return false;}());append(name,{{"type","int"},{"value",0},{"description",""}});table->setCurrentCell(table->rowCount()-1,0);table->editItem(table->currentItem());dirty=true;});
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{auto rows=table->selectionModel()->selectedRows();std::sort(rows.begin(),rows.end(),[](const QModelIndex &a,const QModelIndex &b){return a.row()>b.row();});for(const auto &row:rows)table->removeRow(row.row());if(!rows.isEmpty())dirty=true;});
    QObject::connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{
        table->setCurrentCell(-1,-1);buttons->setFocus();
        if(!project || project->globalRevision()!=revision){status->setText(QStringLiteral("项目或全局配置已改变，请重新打开此窗口。"));return;}
        auto next=project->globalState();next.variables={};QString error;
        for(int row=0;row<table->rowCount();++row){const QString name=table->item(row,0)->text().trimmed();const auto type=qobject_cast<QComboBox*>(table->cellWidget(row,1))->currentData().toString();QJsonValue value;
            if(next.variables.contains(name)){error=QStringLiteral("变量名重复: ")+name;break;}
            if(!parseScalarText(type,table->item(row,2)->text(),value,error)){error=QStringLiteral("第 %1 行：%2").arg(row+1).arg(error);break;}
            next.variables.insert(name,QJsonObject{{"type",type},{"value",value},{"description",table->item(row,3)->text()}});
        }
        if(!error.isEmpty()){status->setText(error);return;}
        if(!project->setGlobalState(next)){status->setText(project->lastErrorMsg());return;}
        dirty=false;revision=project->globalRevision();status->setText(QStringLiteral("已应用 %1 个变量，请保存项目以写入文件。").arg(next.variables.size()));
    });
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const auto lock=[&]{const bool editable=project && !project->hasActiveExecution();table->setEnabled(editable);add->setEnabled(editable);remove->setEnabled(editable);buttons->button(QDialogButtonBox::Apply)->setEnabled(editable);if(!editable)status->setText(QStringLiteral("运行期间仅可查看全局变量。"));};
    QTimer timer;QObject::connect(&timer,&QTimer::timeout,&dialog,lock);timer.start(300);lock();dialog.exec();
}
void showVariableMonitor(XvCore::XvProject *initial,QWidget *parent) {
    if(!available(initial,parent))return;QPointer<XvCore::XvProject> project(initial);
    QDialog dialog(parent);dialog.setObjectName("variableMonitorDialog");dialog.setWindowTitle(QStringLiteral("变量管理 — 全局变量、参数与结果"));dialog.resize(900,560);
    auto layout=new QVBoxLayout(&dialog);auto search=new QLineEdit(&dialog);search->setPlaceholderText(QStringLiteral("搜索变量、流程或算子名称"));layout->addWidget(search);
    auto tree=new QTreeWidget(&dialog);tree->setObjectName("variableMonitorTree");tree->setColumnCount(4);tree->setHeaderLabels({QStringLiteral("来源 / 名称"),QStringLiteral("类型"),QStringLiteral("当前值"),QStringLiteral("脚本引用")});tree->setAlternatingRowColors(true);tree->setUniformRowHeights(true);tree->setColumnWidth(0,280);tree->setColumnWidth(1,70);tree->setColumnWidth(2,170);layout->addWidget(tree,1);
    auto status=hint(layout,QStringLiteral("显示标量变量快照；运行期间暂停读取，避免干扰检测。"),&dialog);
    auto row=new QHBoxLayout;auto refresh=new QPushButton(QStringLiteral("刷新"),&dialog);auto copy=new QPushButton(QStringLiteral("复制脚本引用"),&dialog);auto automatic=new QCheckBox(QStringLiteral("自动刷新"),&dialog);automatic->setChecked(true);row->addWidget(refresh);row->addWidget(copy);row->addWidget(automatic);row->addStretch();auto close=new QPushButton(QStringLiteral("关闭"),&dialog);row->addWidget(close);layout->addLayout(row);
    QVector<ProjectValue> values;
    const auto render=[&]{const QString selected=tree->currentItem()?tree->currentItem()->text(3):QString();tree->clear();const auto query=search->text().trimmed();int count=0;for(const auto &value:values){if(!(value.group+value.name+value.key).contains(query,Qt::CaseInsensitive))continue;if(++count>2000)break;auto item=new QTreeWidgetItem(tree,{value.group+" / "+value.name,typeLabel(value.type),scalarValueText(value.value).left(512),expression(value)});item->setToolTip(2,scalarValueText(value.value).left(4096));if(item->text(3)==selected)tree->setCurrentItem(item);}status->setText(QStringLiteral("共 %1 个标量变量，显示 %2 项；图像和复合对象请在结果面板查看。").arg(values.size()).arg(std::min(count,2000)));};
    const auto update=[&]{ProjectScriptSnapshot snapshot;QString error;if(!captureProjectScript(project,snapshot,error)){status->setText(error);return;}values=snapshot.values;render();};
    QObject::connect(refresh,&QPushButton::clicked,&dialog,update);QObject::connect(search,&QLineEdit::textChanged,&dialog,[&]{render();});QObject::connect(copy,&QPushButton::clicked,&dialog,[&]{if(tree->currentItem())QApplication::clipboard()->setText(tree->currentItem()->text(3));});QObject::connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    QTimer timer;QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{if(automatic->isChecked())update();});timer.start(1000);update();dialog.exec();
}
void showGlobalScript(XvCore::XvProject *initial,QWidget *parent) {
    if(!available(initial,parent))return;QPointer<XvCore::XvProject> project(initial);
    EditableDialog dialog(parent);dialog.setObjectName("globalScriptDialog");dialog.setWindowTitle(QStringLiteral("全局脚本 — JavaScript"));dialog.resize(1000,680);
    auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(16,14,16,14);
    hint(layout,QStringLiteral("手动运行项目级 JavaScript。globals.get/set 读写全局变量，parameters.get/set 读写未绑定的标量参数，results.get 读取结果，console.log 输出日志。双击右侧变量插入引用。"),&dialog);
    auto split=new QSplitter(&dialog);auto editor=new QPlainTextEdit(split);editor->setObjectName("globalScriptEditor");editor->setLineWrapMode(QPlainTextEdit::NoWrap);editor->setStyleSheet("font-family:Consolas,monospace;font-size:13px");editor->setTabStopDistance(32);
    auto references=new QTreeWidget(split);references->setHeaderLabels({QStringLiteral("可用变量"),QStringLiteral("类型")});references->setColumnWidth(0,220);references->setUniformRowHeights(true);split->setStretchFactor(0,3);split->setStretchFactor(1,2);layout->addWidget(split,1);
    editor->setPlainText(project->globalState().script);editor->setPlaceholderText(QStringLiteral("// 先在全局管理中创建 count 整数变量\nconst next = globals.get(\"count\") + 1;\nglobals.set(\"count\", next);\nconsole.log(\"计数\", next);"));editor->document()->setModified(false);
    auto controls=new QHBoxLayout;auto save=new QPushButton(QStringLiteral("保存脚本"),&dialog);auto run=new QPushButton(QStringLiteral("运行"),&dialog);auto stop=new QPushButton(QStringLiteral("停止"),&dialog);stop->setEnabled(false);auto timeout=new QSpinBox(&dialog);timeout->setRange(100,30000);timeout->setValue(5000);timeout->setSuffix(QStringLiteral(" 毫秒"));controls->addWidget(save);controls->addWidget(run);controls->addWidget(stop);controls->addWidget(new QLabel(QStringLiteral("超时"),&dialog));controls->addWidget(timeout);controls->addStretch();auto close=new QPushButton(QStringLiteral("关闭"),&dialog);controls->addWidget(close);layout->addLayout(controls);
    auto output=new QPlainTextEdit(&dialog);output->setObjectName("globalScriptOutput");output->setReadOnly(true);output->setMaximumBlockCount(300);output->setMaximumHeight(145);layout->addWidget(output);
    ProjectScriptSnapshot snapshot;GlobalScriptRunner runner(&dialog);
    quint64 revision=project->globalRevision();
    const auto refresh=[&]{ProjectScriptSnapshot current;QString error;references->clear();if(!captureProjectScript(project,current,error)){output->appendPlainText(error);return;}int count=0;for(const auto &value:current.values){if(++count>2000)break;auto item=new QTreeWidgetItem(references,{value.group+" / "+value.name,typeLabel(value.type)});item->setData(0,Qt::UserRole,expression(value));item->setToolTip(0,expression(value));}};
    const auto saveSource=[&]()->bool {if(!project || project->globalRevision()!=revision){output->appendPlainText(QStringLiteral("项目或全局配置已改变，请重新打开脚本窗口。"));return false;}auto state=project->globalState();state.script=editor->toPlainText();if(!project->setGlobalState(state)){output->appendPlainText(project->lastErrorMsg());return false;}revision=project->globalRevision();editor->document()->setModified(false);return true;};
    dialog.confirmClose=[&] {
        if(!editor->document()->isModified())return true;
        const auto answer=QMessageBox::question(&dialog,QStringLiteral("保存脚本"),QStringLiteral("脚本尚未保存到项目，是否保存后关闭？"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
        return answer==QMessageBox::Discard || (answer==QMessageBox::Save && saveSource());
    };
    const auto active=[&](bool running){run->setEnabled(!running);save->setEnabled(!running);stop->setEnabled(running);editor->setReadOnly(running);timeout->setEnabled(!running);};
    QObject::connect(save,&QPushButton::clicked,&dialog,[&]{if(saveSource())output->appendPlainText(QStringLiteral("脚本已保存到当前项目；请保存项目文件以保留。"));});
    QObject::connect(run,&QPushButton::clicked,&dialog,[&]{if(!saveSource())return;QString error;if(!captureProjectScript(project,snapshot,error)){output->appendPlainText(error);return;}auto input=snapshot.input;input.insert("script",editor->toPlainText());active(true);output->appendPlainText(QStringLiteral("正在运行…"));if(!runner.start(input,timeout->value())){active(false);output->appendPlainText(QStringLiteral("无法运行：输入超出限制或引擎仍在运行。"));}});
    QObject::connect(stop,&QPushButton::clicked,&runner,&GlobalScriptRunner::stop);
    QObject::connect(&runner,&GlobalScriptRunner::finished,&dialog,[&](bool success,const QJsonObject &result,const QString &message){
        active(false);QString error=message;if(success)success=applyProjectScript(snapshot,result,error);
        if(success){revision=project->globalRevision();for(const auto log:result.value("logs").toArray())output->appendPlainText(log.toString());output->appendPlainText(QStringLiteral("运行完成，变量和参数已更新；请保存项目。"));refresh();}
        else output->appendPlainText(QStringLiteral("运行失败：")+error);
    });
    QObject::connect(references,&QTreeWidget::itemDoubleClicked,&dialog,[&](QTreeWidgetItem *item,int){if(!runner.isRunning())editor->insertPlainText(item->data(0,Qt::UserRole).toString());});
    QObject::connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);refresh();dialog.exec();runner.stop();
}
