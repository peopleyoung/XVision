#include "HttpJson.h"

#include "CommunicationOperatorWdg.h"
#include "XLanguage.h"

#include <QJsonObject>
#include <QJsonParseError>

using namespace XvCore;

HttpJsonParam::HttpJsonParam()
{
    url=new XString("url","http://127.0.0.1:8080/",this,
                    getUiText(getLang("XvFuncSystem_HttpJson_Url","请求地址")));
    timeoutMs=new XInt("timeoutMs",5000,this,
                       getLang("XvFuncSystem_Communication_Timeout","超时(ms)"));
    maxResponseBytes=new XInt("maxResponseBytes",1024*1024,this,
                              getLang("XvFuncSystem_Communication_MaxBytes","最大字节数"));
    headersJson=new XString("headersJson","{}",this,
                            getLang("XvFuncSystem_HttpJson_Headers","请求头(JSON)"));
    useJsonInput=new XBool("useJsonInput",false,this,
                           getLang("XvFuncSystem_HttpJson_UseInput","使用JSON输入"));
    jsonInput=new XJsonValue("jsonInput",QJsonDocument(QJsonObject()),this,
                             getLang("XvFuncSystem_HttpJson_Input","JSON输入"));
    payloadJson=new XString("payloadJson","{}",this,
                            getLang("XvFuncSystem_HttpJson_Payload","请求JSON"));
}

HttpJsonResult::HttpJsonResult()
{
    json=new XJsonValue("json",QJsonDocument(QJsonObject()),this,
                        getLang("XvFuncSystem_HttpJson_Result","JSON结果"));
    responseText=new XString("responseText","",this,
                             getLang("XvFuncSystem_HttpJson_ResponseText","响应文本"));
    statusCode=new XInt("statusCode",0,this,
                        getLang("XvFuncSystem_HttpJson_Status","状态码"));
    bytesReceived=new XInt("bytesReceived",0,this,
                           getLang("XvFuncSystem_Communication_BytesReceived","接收字节数"));
    bytesSent=new XInt("bytesSent",0,this,
                       getLang("XvFuncSystem_Communication_BytesSent","发送字节数"));
}

HttpJson::HttpJson(QObject *parent)
    :CommunicationOperatorBase(parent),m_param(new HttpJsonParam()),m_result(new HttpJsonResult())
{
    _funcRole="HttpJson";
    _funcName=getUiText(getLang("XvFuncSystem_HttpJson_Name","HTTP 数据通信"));
    _funcType=EXvFuncType::DataProcessing;
}

HttpJson::~HttpJson()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList HttpJson::activeParameterNames() const
{
    QStringList names={"url","timeoutMs","maxResponseBytes","headersJson"};
    if(m_mode==Write)
    {
        names << "useJsonInput";
        names << ((m_param && m_param->useJsonInput->value())?"jsonInput":"payloadJson");
    }
    return names;
}

QStringList HttpJson::communicationModeNames() const
{
    return {getLang("XvFuncSystem_Communication_Read","读取"),
            getLang("XvFuncSystem_Communication_Write","写入")};
}

void HttpJson::setTransport(const std::shared_ptr<IHttpJsonTransport> &transport)
{
    m_transport=transport;
}

EXvFuncRunStatus HttpJson::run()
{
    if(!m_param || !m_result || m_mode<Read || m_mode>Write)
    {
        setCommunicationError("HTTP JSON operator state is incomplete");
        return EXvFuncRunStatus::Error;
    }
    HttpJsonRequest request;
    request.url=m_param->url->value().trimmed();
    request.timeoutMs=m_param->timeoutMs->value();
    request.maxResponseBytes=m_param->maxResponseBytes->value();
    request.headersJson=m_param->headersJson->value().toUtf8();
    request.write=m_mode==Write;
    if(request.url.isEmpty() || request.url.size()>8192
            || request.timeoutMs<1 || request.timeoutMs>600000
            || request.maxResponseBytes<1 || request.maxResponseBytes>16*1024*1024
            || request.headersJson.size()>request.maxResponseBytes)
    {
        setCommunicationError("HTTP URL, timeout, or response limit is invalid");
        return EXvFuncRunStatus::Error;
    }
    if(request.write)
    {
        QJsonDocument bodyDocument;
        if(m_param->useJsonInput->value())
            bodyDocument=m_param->jsonInput->value();
        else
        {
            QJsonParseError parseError;
            const QByteArray directBody=m_param->payloadJson->value().toUtf8();
            if(directBody.size()>request.maxResponseBytes)
            {
                setCommunicationError("HTTP request payload exceeded the configured maximum");
                return EXvFuncRunStatus::Error;
            }
            bodyDocument=QJsonDocument::fromJson(directBody,&parseError);
            if(parseError.error!=QJsonParseError::NoError)
            {
                setCommunicationError("HTTP request payload is not valid JSON");
                return EXvFuncRunStatus::Error;
            }
        }
        if(bodyDocument.isNull() || (!bodyDocument.isObject() && !bodyDocument.isArray()))
        {
            setCommunicationError("HTTP request payload must be a JSON object or array");
            return EXvFuncRunStatus::Error;
        }
        request.body=bodyDocument.toJson(QJsonDocument::Compact);
        if(request.body.size()>request.maxResponseBytes)
        {
            setCommunicationError("HTTP request payload exceeded the configured maximum");
            return EXvFuncRunStatus::Error;
        }
    }
    HttpJsonResponse response;
    QString error;
    const auto transport=m_transport?m_transport:createHttpJsonTransport();
    if(!transport || !transport->execute(request,{communicationCancellationFlag()},response,error))
    {
        if(communicationCancelled()) return communicationCancelledStatus();
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(response.body.size()>request.maxResponseBytes
            || response.bytesSent!=request.body.size())
    {
        setCommunicationError("HTTP transport returned inconsistent payload metadata");
        return EXvFuncRunStatus::Error;
    }
    const QByteArray responseBytes=response.body;
    QJsonParseError parseError;
    const QJsonDocument document=QJsonDocument::fromJson(responseBytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError
            || (!document.isObject() && !document.isArray()))
    {
        setCommunicationError("HTTP response is not a complete JSON object or array");
        return EXvFuncRunStatus::Error;
    }
    if(response.statusCode<100 || response.statusCode>599)
    {
        setCommunicationError("HTTP response status code is invalid");
        return EXvFuncRunStatus::Error;
    }
    if(communicationCancelled()) return communicationCancelledStatus();
    if(!m_result->json->setValue(document))
    {
        setCommunicationError("HTTP JSON result could not be committed");
        return EXvFuncRunStatus::Error;
    }
    m_result->responseText->setValue(QString::fromUtf8(responseBytes));
    m_result->statusCode->setValue(response.statusCode);
    m_result->bytesReceived->setValue(response.body.size());
    m_result->bytesSent->setValue(static_cast<int>(response.bytesSent));
    if(response.statusCode<200 || response.statusCode>=300)
    {
        setRunMsg(QString("HTTP request completed with status %1").arg(response.statusCode));
        return EXvFuncRunStatus::Fail;
    }
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

QPixmap HttpJson::funcIcon()
{
    return QPixmap(":/images/LogOutput.svg");
}

void HttpJson::onShowFunc()
{
    if(!m_widget) m_widget=new CommunicationOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
