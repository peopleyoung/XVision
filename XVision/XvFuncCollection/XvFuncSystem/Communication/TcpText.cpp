#include "TcpText.h"

#include "CommunicationOperatorWdg.h"
#include "XLanguage.h"

using namespace XvCore;

TcpTextParam::TcpTextParam()
{
    host=new XString("host","127.0.0.1",this,getLang("XvFuncSystem_Communication_Host","主机"));
    port=new XInt("port",9000,this,getLang("XvFuncSystem_Communication_Port","端口"));
    connectTimeoutMs=new XInt("connectTimeoutMs",5000,this,
                              getLang("XvFuncSystem_Communication_ConnectTimeout","连接超时(ms)"));
    timeoutMs=new XInt("timeoutMs",5000,this,getLang("XvFuncSystem_Communication_Timeout","超时(ms)"));
    maxBytes=new XInt("maxBytes",1024*1024,this,
                      getLang("XvFuncSystem_Communication_MaxBytes","最大字节数"));
    encoding=new XInt("encoding",0,this,getLang("XvFuncSystem_Communication_Encoding","编码"));
    frameMode=new XInt("frameMode",0,this,getLang("XvFuncSystem_Communication_Frame","帧边界"));
    delimiter=new XString("delimiter","\n",this,getLang("XvFuncSystem_Communication_Delimiter","分隔符"));
    fixedLength=new XInt("fixedLength",1,this,getLang("XvFuncSystem_Communication_FixedLength","固定长度"));
    payload=new XString("payload","",this,getLang("XvFuncSystem_Communication_Payload","文本"));
    appendDelimiter=new XBool("appendDelimiter",false,this,
                              getLang("XvFuncSystem_Communication_AppendDelimiter","追加分隔符"));
}

TcpTextResult::TcpTextResult()
{
    text=new XString("text","",this,getLang("XvFuncSystem_Communication_Text","文本结果"));
    data=new XByteArray("data",QByteArray(),this,getLang("XvFuncSystem_Communication_Data","字节结果"));
    bytesTransferred=new XInt("bytesTransferred",0,this,
                              getLang("XvFuncSystem_Communication_BytesTransferred","传输字节数"));
}

TcpText::TcpText(QObject *parent)
    :CommunicationOperatorBase(parent),m_param(new TcpTextParam()),m_result(new TcpTextResult())
{
    _funcRole="TcpText";
    _funcName=getLang("XvFuncSystem_TcpText_Name","TCP文本");
    _funcType=EXvFuncType::Communication;
}

TcpText::~TcpText()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList TcpText::activeParameterNames() const
{
    QStringList names={"host","port","connectTimeoutMs","timeoutMs","maxBytes","encoding"};
    if(m_mode==Read)
    {
        names << "frameMode";
        if(m_param && m_param->frameMode->value()==int(CommunicationFrame::Delimiter)) names << "delimiter";
        if(m_param && m_param->frameMode->value()==int(CommunicationFrame::FixedLength)) names << "fixedLength";
    }
    else
    {
        names << "payload" << "appendDelimiter";
        if(m_param && m_param->appendDelimiter->value()) names << "delimiter";
    }
    return names;
}

QStringList TcpText::communicationModeNames() const
{
    return {getLang("XvFuncSystem_Communication_Read","读取"),
            getLang("XvFuncSystem_Communication_Write","写入")};
}

void TcpText::setTransport(const std::shared_ptr<ITcpTextTransport> &transport)
{
    m_transport=transport;
}

EXvFuncRunStatus TcpText::run()
{
    if(!m_param || !m_result || m_mode<Read || m_mode>Write)
    {
        setCommunicationError("TCP text operator state is incomplete");
        return EXvFuncRunStatus::Error;
    }
    const int encodingValue=m_param->encoding->value();
    const int frameValue=m_param->frameMode->value();
    if(encodingValue<0 || encodingValue>1 || frameValue<0 || frameValue>2
            || m_param->host->value().trimmed().isEmpty()
            || m_param->host->value().trimmed().size()>253
            || m_param->port->value()<1 || m_param->port->value()>65535
            || m_param->connectTimeoutMs->value()<1
            || m_param->connectTimeoutMs->value()>600000
            || m_param->timeoutMs->value()<1 || m_param->timeoutMs->value()>600000
            || m_param->maxBytes->value()<1 || m_param->maxBytes->value()>16*1024*1024)
    {
        setCommunicationError("TCP port, encoding, or frame mode is invalid");
        return EXvFuncRunStatus::Error;
    }
    TextStreamRequest request;
    request.host=m_param->host->value().trimmed();
    request.port=static_cast<quint16>(m_param->port->value());
    request.connectTimeoutMs=m_param->connectTimeoutMs->value();
    request.timeoutMs=m_param->timeoutMs->value();
    request.maxBytes=m_param->maxBytes->value();
    request.encoding=static_cast<CommunicationEncoding>(encodingValue);
    request.frame=static_cast<CommunicationFrame>(frameValue);
    request.fixedLength=m_param->fixedLength->value();
    request.write=m_mode==Write;
    request.appendDelimiter=m_param->appendDelimiter->value();
    QString error;
    if((request.frame==CommunicationFrame::Delimiter || request.appendDelimiter)
            && !encodeCommunicationText(m_param->delimiter->value(),request.encoding,
                                        request.delimiter,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(request.write && !encodeCommunicationText(m_param->payload->value(),request.encoding,
                                                  request.payload,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(!request.write && !validateCommunicationFrame(request.frame,request.delimiter,
                                                       request.fixedLength,request.maxBytes,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(request.write && (request.payload.size()>request.maxBytes
            || (request.appendDelimiter && request.payload.size()>request.maxBytes-request.delimiter.size())))
    {
        setCommunicationError("TCP write frame exceeded the configured maximum");
        return EXvFuncRunStatus::Error;
    }
    TextStreamResponse response;
    const auto transport=m_transport?m_transport:createTcpTextTransport();
    if(!transport || !transport->execute(request,{communicationCancellationFlag()},response,error))
    {
        if(communicationCancelled()) return communicationCancelledStatus();
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    QByteArray resultBytes=response.data;
    if(request.write)
    {
        resultBytes=request.payload;
        if(request.appendDelimiter) resultBytes.append(request.delimiter);
    }
    if(resultBytes.size()>request.maxBytes || response.bytesTransferred!=resultBytes.size())
    {
        setCommunicationError("TCP transport returned inconsistent payload metadata");
        return EXvFuncRunStatus::Error;
    }
    QString resultText;
    if(!decodeCommunicationText(resultBytes,request.encoding,resultText,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(communicationCancelled()) return communicationCancelledStatus();
    m_result->text->setValue(resultText);
    m_result->data->setValue(resultBytes);
    m_result->bytesTransferred->setValue(static_cast<int>(response.bytesTransferred));
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

void TcpText::onShowFunc()
{
    if(!m_widget) m_widget=new CommunicationOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
