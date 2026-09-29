#include "FrmLogShow.h"
#include "ui_FrmLogShow.h"
#include <QMutexLocker>
#include <QScrollBar>
#include <QMenu>
#include <QDesktopServices>
#include <QTimer>
#include <deque>
#include <QApplication>
#include <QTextBlock>
#include <QEvent>

namespace {
QColor logTextColor(XLogger::ELogType level) {
    const auto palette=QApplication::palette();
    const bool light=palette.color(QPalette::Base).lightness()>128;
    switch(level) {
    case XLogger::Trace:return palette.color(QPalette::Disabled,QPalette::Text);
    case XLogger::Debug:case XLogger::Event:return palette.color(QPalette::Link);
    case XLogger::Warn:return QColor(light?"#946100":"#F5BF69");
    case XLogger::Error:case XLogger::Critical:return QColor(light?"#B92344":"#FF7C8D");
    default:return palette.color(QPalette::Text);
    }
}
}
struct FrmLogShow::PendingLogs
{
    QMutex mutex;
    std::deque<QPair<QString,XLogger::ELogType>> entries;
};

FrmLogShow *FrmLogShow::s_Instance = NULL;
FrmLogShow *FrmLogShow::getInstance() {
  if (!s_Instance) {
    static QMutex s_Mutex;
    QMutexLocker locker(&s_Mutex);
    if (!s_Instance) {
      s_Instance = new FrmLogShow();
    }
  }
  return s_Instance;
}



FrmLogShow::FrmLogShow(QWidget *parent) :
    BaseWidget(parent),
    ui(new Ui::FrmLogShow)
{
    ui->setupUi(this);
    initFrm();
}

FrmLogShow::~FrmLogShow()
{
    delete ui;
}

void FrmLogShow::initFrm()
{
    this->setWindowIcon(QIcon(":/images/Ui/FrmLogShow.svg"));
    this->setWindowTitle(getLang(App_Ui_FrmLogShow,"日志显示"));
    ui->ptxtLog->setRippleStyle(XMatCommonDef::NoRipple);
    // Producers only enqueue text. No widget access or per-message GUI events.
    m_pendingLogs=std::make_shared<PendingLogs>();
    const auto pending=m_pendingLogs;
    connect(XLog,&XLogger::signalLog,this,
            [pending](const QString &message,const XLogger::ELogType &type) {
        QMutexLocker lock(&pending->mutex);
        if(pending->entries.size()>=LOG_MAX_COUNT) pending->entries.pop_front();
        // The file logger keeps the complete message; bound presentation memory.
        pending->entries.emplace_back(message.left(8192),type);
    },Qt::DirectConnection);
    auto timer=new QTimer(this);
    timer->setInterval(50);
    connect(timer,&QTimer::timeout,this,&FrmLogShow::flushPendingLogs);
    timer->start();
    m_mapType[XLogger::Trace]=getLang(App_LogType_Trace,"追踪");
    m_mapType[XLogger::Debug]=getLang(App_LogType_Debug,"调试");
    m_mapType[XLogger::Info]=getLang(App_LogType_Info,"信息");
    m_mapType[XLogger::Event]=getLang(App_LogType_Event,"事件");
    m_mapType[XLogger::Warn]=getLang(App_LogType_Warn,"警告");
    m_mapType[XLogger::Error]=getLang(App_LogType_Error,"错误");
    m_mapType[XLogger::Critical]=getLang(App_LogType_Critical,"致命");


    QPlainTextEdit *ptxtLog=ui->ptxtLog;
    //设置自定义菜单
    ptxtLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ptxtLog, &QPlainTextEdit::customContextMenuRequested, this,&FrmLogShow::onLogcustomContextMenuRequested);
    //设置日志最多显示一百行
    ptxtLog->setMaximumBlockCount(LOG_MAX_COUNT);
    //设置只读
    if(!ptxtLog->isReadOnly())
    {
        ptxtLog->setReadOnly(true);
    }
}

void FrmLogShow::onSignalLog(const QString &log, const XLogger::ELogType &logType)
{
    QTextCharFormat fmt;
    const QColor color=logTextColor(logType);
    fmt.setFontWeight(QFont::Normal);
    fmt.setProperty(QTextFormat::UserProperty+1,int(logType));
    auto *ptxt=ui->ptxtLog;
    //字体色
    fmt.setForeground(QBrush(color));
    //设置光标到文本末尾
    ptxt->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    //文本框使用以上设定
    ptxt->mergeCurrentCharFormat(fmt);
    QString logShow=m_mapType[logType]+":"+log;
    ptxt->appendPlainText(logShow);

}

void FrmLogShow::flushPendingLogs()
{
    std::deque<QPair<QString,XLogger::ELogType>> entries;
    {
        QMutexLocker lock(&m_pendingLogs->mutex);
        entries.swap(m_pendingLogs->entries);
    }
    if(entries.empty()) return;
    auto editor=ui->ptxtLog;
    auto scrollbar=editor->verticalScrollBar();
    const int previousPosition=scrollbar->value();
    const bool followTail=previousPosition>=scrollbar->maximum();
    editor->setUpdatesEnabled(false);
    for(const auto &entry:entries) onSignalLog(entry.first,entry.second);
    scrollbar->setValue(followTail?scrollbar->maximum():previousPosition);
    editor->setUpdatesEnabled(true);
}

void FrmLogShow::onLogcustomContextMenuRequested(const QPoint &pos)
{
    QMenu menu;
    auto action=menu.addAction(getLang(App_Ui_FrmLogShowClearLog,"清除日志"),this,SLOT(onClearLog()));
    action->setIcon(QIcon(":/images/Ui/FrmLogShowActionClearLog.svg"));
    action=menu.addAction(getLang(App_Ui_FrmLogOpenLogDir,"打开日志目录"),this,SLOT(onOpenLogDir()));
    action->setIcon(QIcon(":/images/Ui/FrmLogShowActionOpenLogDir.svg"));
    menu.exec(QCursor::pos());
}

void FrmLogShow::onClearLog()
{
    {
        QMutexLocker lock(&m_pendingLogs->mutex);
        m_pendingLogs->entries.clear();
    }
    ui->ptxtLog->clear();
}

void FrmLogShow::onOpenLogDir()
{
    QDesktopServices deskSer;
    QString logDir=qApp->applicationDirPath()+"/Log";
    deskSer.openUrl(QUrl("file:///"+logDir));
}


void FrmLogShow::changeEvent(QEvent *event) {
    BaseWidget::changeEvent(event);
    if(event->type()!=QEvent::PaletteChange || !ui || !ui->ptxtLog)return;
    auto document=ui->ptxtLog->document();
    for(auto block=document->begin();block.isValid();block=block.next()) {
        if(block.begin().atEnd())continue;
        const auto format=block.begin().fragment().charFormat();
        if(!format.hasProperty(QTextFormat::UserProperty+1))continue;
        const auto level=static_cast<XLogger::ELogType>(format.intProperty(QTextFormat::UserProperty+1));
        QTextCursor cursor(block);cursor.select(QTextCursor::BlockUnderCursor);
        QTextCharFormat color;color.setForeground(logTextColor(level));cursor.mergeCharFormat(color);
    }
}
