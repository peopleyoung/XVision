#include "XLanguage.h"
#include "NotificationOutput.h"
#include "NotificationOutputWdg.h"

#include "XLogger.h"
#include "XvNotificationCenter.h"

using namespace XvCore;

NotificationOutput::NotificationOutput(QObject *parent)
    :XvFunc(parent),m_param(new NotificationOutputParam()),
      m_result(new NotificationOutputResult())
{
    _funcRole="NotificationOutput";
    _funcName=getUiText("Notification Output");
    _funcType=EXvFuncType::Other;
}

NotificationOutput::~NotificationOutput()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QString NotificationOutput::defaultMessage() const
{
    switch(m_mode)
    {
    case Ok: return "OK";
    case Ng: return "NG";
    case Info: return "Info";
    case Success: return "Success";
    case Warning: return "Warning";
    case Error: return "Error";
    case Fatal: return "Fatal";
    case Dialog: return "Dialog";
    }
    return QString();
}

bool NotificationOutput::readPersistentData(const QDomElement &dataElement,QString &error)
{
    if(!dataElement.isNull())
    {
        error="NotificationOutput does not support <PersistentData>";
        return false;
    }
    const int value=static_cast<int>(m_mode);
    if(value<static_cast<int>(Ok) || value>static_cast<int>(Dialog))
    {
        error="NotificationOutput mode is invalid";
        return false;
    }
    return true;
}

EXvFuncRunStatus NotificationOutput::run()
{
    m_result->published->setValue(false);
    m_result->accepted->setValue(false);
    m_result->publishedMessage->setValue(QString());
    m_result->outputImage->setValue(QImage());
    const int value=static_cast<int>(m_mode);
    if(value<static_cast<int>(Ok) || value>static_cast<int>(Dialog))
    {
        setRunMsg("Notification mode is invalid");
        return EXvFuncRunStatus::Error;
    }

    QString message=m_param->message->value().trimmed();
    if(message.isEmpty()) message=defaultMessage();
    if(message.size()>XvNotificationCenter::MaximumMessageLength)
    {
        setRunMsg(QString("Notification message exceeds %1 characters")
                  .arg(XvNotificationCenter::MaximumMessageLength));
        return EXvFuncRunStatus::Error;
    }

    const XvNotificationKind kind=static_cast<XvNotificationKind>(value);
    if(!XvNotificationMgr->publish(kind,message,funcId()))
    {
        setRunMsg("Notification event was rejected");
        return EXvFuncRunStatus::Error;
    }

    XLogger::ELogType logType=XLogger::Info;
    switch(m_mode)
    {
    case Ok:
    case Success:
        logType=XLogger::Event;
        break;
    case Ng:
    case Warning:
        logType=XLogger::Warn;
        break;
    case Error:
        logType=XLogger::Error;
        break;
    case Fatal:
        logType=XLogger::Critical;
        break;
    case Info:
    case Dialog:
        logType=XLogger::Info;
        break;
    }
    XLog->log(message,logType,true,__FILE__,__FUNCTION__);

    m_result->outputImage->setValue(m_param->inputImage->value());
    m_result->published->setValue(true);
    m_result->accepted->setValue(m_mode!=Ng);
    m_result->notificationKind->setValue(value);
    m_result->publishedMessage->setValue(message);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

void NotificationOutput::onShowFunc()
{
    if(!m_widget) m_widget=new NotificationOutputWdg(this);
    m_widget->show();
    m_widget->raise();
}
