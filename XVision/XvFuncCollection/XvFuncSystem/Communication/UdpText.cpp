#include "UdpText.h"

#include "CommunicationOperatorWdg.h"
#include "XLanguage.h"

using namespace XvCore;

UdpTextParam::UdpTextParam()
{
    localAddress=new XString("localAddress","0.0.0.0",this,
                             getLang("XvFuncSystem_Communication_LocalAddress","本地地址"));
    localPort=new XInt("localPort",9001,this,getLang("XvFuncSystem_Communication_LocalPort","本地端口"));
    remoteHost=new XString("remoteHost","127.0.0.1",this,
                           getLang("XvFuncSystem_Communication_RemoteHost","远程主机"));
    remotePort=new XInt("remotePort",9001,this,
                        getLang("XvFuncSystem_Communication_RemotePort","远程端口"));
    timeoutMs=new XInt("timeoutMs",5000,this,getLang("XvFuncSystem_Communication_Timeout","超时(ms)"));
    maxBytes=new XInt("maxBytes",65507,this,getLang("XvFuncSystem_Communication_MaxBytes","最大字节数"));
    encoding=new XInt("encoding",0,this,getLang("XvFuncSystem_Communication_Encoding","编码"));
    payload=new XString("payload","",this,getLang("XvFuncSystem_Communication_Payload","文本"));
}

UdpTextResult::UdpTextResult()
{
    text=new XString("text","",this,getLang("XvFuncSystem_Communication_Text","文本结果"));
    data=new XByteArray("data",QByteArray(),this,getLang("XvFuncSystem_Communication_Data","字节结果"));
    bytesTransferred=new XInt("bytesTransferred",0,this,
                              getLang("XvFuncSystem_Communication_BytesTransferred","传输字节数"));
    senderAddress=new XString("senderAddress","",this,
                              getLang("XvFuncSystem_Communication_SenderAddress","发送方地址"));
    senderPort=new XInt("senderPort",0,this,getLang("XvFuncSystem_Communication_SenderPort","发送方端口"));
}

UdpText::UdpText(QObject *parent)
    :CommunicationOperatorBase(parent),m_param(new UdpTextParam()),m_result(new UdpTextResult())
{
    _funcRole="UdpText";
    _funcName=getLang("XvFuncSystem_UdpText_Name","UDP文本");
    _funcType=EXvFuncType::DataProcessing;
}

UdpText::~UdpText()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList UdpText::activeParameterNames() const
{
    QStringList names={"timeoutMs","maxBytes","encoding"};
    if(m_mode==Read) names=QStringList{"localAddress","localPort"}+names;
    else
    {
        names=QStringList{"remoteHost","remotePort"}+names;
        names << "payload";
    }
    return names;
}

QStringList UdpText::communicationModeNames() const
{
    return {getLang("XvFuncSystem_Communication_Read","读取"),
            getLang("XvFuncSystem_Communication_Write","写入")};
}

void UdpText::setTransport(const std::shared_ptr<IUdpTextTransport> &transport)
{
    m_transport=transport;
}

EXvFuncRunStatus UdpText::run()
{
    if(!m_param || !m_result || m_mode<Read || m_mode>Write)
    {
        setCommunicationError("UDP text operator state is incomplete");
        return EXvFuncRunStatus::Error;
    }
    const int encodingValue=m_param->encoding->value();
    if(encodingValue<0 || encodingValue>1 || m_param->timeoutMs->value()<1
            || m_param->timeoutMs->value()>600000 || m_param->maxBytes->value()<1
            || m_param->maxBytes->value()>65507
            || (m_mode==Read && m_param->localAddress->value().trimmed().isEmpty())
            || (m_mode==Write && (m_param->remoteHost->value().trimmed().isEmpty()
                                  || m_param->remoteHost->value().trimmed().size()>253)))
    {
        setCommunicationError("UDP timeout, payload limit, or encoding is invalid");
        return EXvFuncRunStatus::Error;
    }
    UdpTextRequest request;
    request.write=m_mode==Write;
    request.localAddress=m_param->localAddress->value().trimmed();
    request.remoteHost=m_param->remoteHost->value().trimmed();
    request.timeoutMs=m_param->timeoutMs->value();
    request.maxBytes=m_param->maxBytes->value();
    request.encoding=static_cast<CommunicationEncoding>(encodingValue);
    const int port=request.write?m_param->remotePort->value():m_param->localPort->value();
    if(port<1 || port>65535)
    {
        setCommunicationError("UDP port is invalid");
        return EXvFuncRunStatus::Error;
    }
    if(request.write) request.remotePort=static_cast<quint16>(port);
    else request.localPort=static_cast<quint16>(port);
    QString error;
    if(request.write && !encodeCommunicationText(m_param->payload->value(),request.encoding,
                                                  request.payload,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(request.payload.size()>request.maxBytes)
    {
        setCommunicationError("UDP payload exceeded the configured maximum");
        return EXvFuncRunStatus::Error;
    }
    UdpTextResponse response;
    const auto transport=m_transport?m_transport:createUdpTextTransport();
    if(!transport || !transport->execute(request,{communicationCancellationFlag()},response,error))
    {
        if(communicationCancelled()) return communicationCancelledStatus();
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    const QByteArray resultBytes=request.write?request.payload:response.data;
    if(resultBytes.size()>request.maxBytes || response.bytesTransferred!=resultBytes.size()
            || (!request.write && (response.senderAddress.trimmed().isEmpty()
                                   || response.senderPort==0)))
    {
        setCommunicationError("UDP transport returned inconsistent datagram metadata");
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
    m_result->senderAddress->setValue(request.write?QString():response.senderAddress);
    m_result->senderPort->setValue(request.write?0:response.senderPort);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

QPixmap UdpText::funcIcon()
{
    return QPixmap(":/images/LogOutput.svg");
}

void UdpText::onShowFunc()
{
    if(!m_widget) m_widget=new CommunicationOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
