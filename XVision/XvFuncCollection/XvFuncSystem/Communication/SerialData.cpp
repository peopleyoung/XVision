#include "SerialData.h"

#include "CommunicationOperatorWdg.h"
#include "XLanguage.h"

using namespace XvCore;

namespace
{
bool decodeHexPayload(const QString &text,QByteArray &bytes,QString &error)
{
    QByteArray cleaned;
    for(const QChar character:text)
    {
        if(character.isSpace()) continue;
        const ushort value=character.unicode();
        const bool hex=(value>='0' && value<='9') || (value>='a' && value<='f')
                || (value>='A' && value<='F');
        if(!hex)
        {
            error="Serial byte payload must contain only hexadecimal digits and whitespace";
            return false;
        }
        cleaned.append(static_cast<char>(value));
    }
    if(cleaned.size()%2!=0)
    {
        error="Serial hexadecimal payload must have an even digit count";
        return false;
    }
    bytes=QByteArray::fromHex(cleaned);
    return true;
}

bool validSerialEnums(int dataBits,int parity,int stopBits,int flowControl)
{
    const bool dataValid=dataBits>=5 && dataBits<=8;
    const bool parityValid=parity==0 || parity==2 || parity==3 || parity==4 || parity==5;
    const bool stopValid=stopBits==1 || stopBits==2 || stopBits==3;
    const bool flowValid=flowControl>=0 && flowControl<=2;
    return dataValid && parityValid && stopValid && flowValid;
}
}

SerialDataParam::SerialDataParam()
{
    portName=new XString("portName","COM1",this,getLang("XvFuncSystem_Serial_PortName","串口"));
    baudRate=new XInt("baudRate",115200,this,getLang("XvFuncSystem_Serial_BaudRate","波特率"));
    dataBits=new XInt("dataBits",8,this,getLang("XvFuncSystem_Serial_DataBits","数据位"));
    parity=new XInt("parity",0,this,getLang("XvFuncSystem_Serial_Parity","校验位"));
    stopBits=new XInt("stopBits",1,this,getLang("XvFuncSystem_Serial_StopBits","停止位"));
    flowControl=new XInt("flowControl",0,this,getLang("XvFuncSystem_Serial_FlowControl","流控制"));
    timeoutMs=new XInt("timeoutMs",5000,this,getLang("XvFuncSystem_Communication_Timeout","超时(ms)"));
    maxBytes=new XInt("maxBytes",1024*1024,this,getLang("XvFuncSystem_Communication_MaxBytes","最大字节数"));
    encoding=new XInt("encoding",0,this,getLang("XvFuncSystem_Communication_Encoding","编码"));
    frameMode=new XInt("frameMode",0,this,getLang("XvFuncSystem_Communication_Frame","帧边界"));
    delimiter=new XString("delimiter","\n",this,getLang("XvFuncSystem_Communication_Delimiter","分隔符"));
    fixedLength=new XInt("fixedLength",1,this,getLang("XvFuncSystem_Communication_FixedLength","固定长度"));
    useByteInput=new XBool("useByteInput",false,this,getLang("XvFuncSystem_Serial_UseByteInput","使用字节输入"));
    byteInput=new XByteArray("byteInput",QByteArray(),this,getLang("XvFuncSystem_Serial_ByteInput","字节输入"));
    payload=new XString("payload","",this,getLang("XvFuncSystem_Communication_Payload","数据"));
}

SerialDataResult::SerialDataResult()
{
    data=new XByteArray("data",QByteArray(),this,getLang("XvFuncSystem_Communication_Data","字节结果"));
    text=new XString("text","",this,getLang("XvFuncSystem_Communication_Text","文本结果"));
    bytesTransferred=new XInt("bytesTransferred",0,this,
                              getLang("XvFuncSystem_Communication_BytesTransferred","传输字节数"));
}

SerialData::SerialData(QObject *parent)
    :CommunicationOperatorBase(parent),m_param(new SerialDataParam()),m_result(new SerialDataResult())
{
    _funcRole="SerialData";
    _funcName=getLang("XvFuncSystem_SerialData_Name","串口数据");
    _funcType=EXvFuncType::Communication;
}

SerialData::~SerialData()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList SerialData::activeParameterNames() const
{
    QStringList names={"portName","baudRate","dataBits","parity","stopBits","flowControl",
                       "timeoutMs","maxBytes"};
    if(m_mode==ReadBytes || m_mode==ReadText)
    {
        if(m_mode==ReadText) names << "encoding";
        names << "frameMode";
        if(m_param && m_param->frameMode->value()==int(CommunicationFrame::Delimiter)) names << "delimiter";
        if(m_param && m_param->frameMode->value()==int(CommunicationFrame::FixedLength)) names << "fixedLength";
    }
    else if(m_mode==WriteBytes)
    {
        names << "useByteInput";
        names << ((m_param && m_param->useByteInput->value())?"byteInput":"payload");
    }
    else names << "encoding" << "payload";
    return names;
}

QStringList SerialData::communicationModeNames() const
{
    return {getLang("XvFuncSystem_Serial_ReadBytes","读取字节"),
            getLang("XvFuncSystem_Serial_ReadText","读取文本"),
            getLang("XvFuncSystem_Serial_WriteBytes","写入字节"),
            getLang("XvFuncSystem_Serial_WriteText","写入文本")};
}

void SerialData::setTransport(const std::shared_ptr<ISerialDataTransport> &transport)
{
    m_transport=transport;
}

EXvFuncRunStatus SerialData::run()
{
    if(!m_param || !m_result || m_mode<ReadBytes || m_mode>WriteText)
    {
        setCommunicationError("Serial data operator state is incomplete");
        return EXvFuncRunStatus::Error;
    }
    if(m_param->portName->value().trimmed().isEmpty()
            || m_param->portName->value().trimmed().size()>255
            || m_param->baudRate->value()<1
            || m_param->baudRate->value()>4000000
            || !validSerialEnums(m_param->dataBits->value(),m_param->parity->value(),
                                 m_param->stopBits->value(),m_param->flowControl->value()))
    {
        setCommunicationError("Serial port settings are invalid");
        return EXvFuncRunStatus::Error;
    }
    const int encodingValue=m_param->encoding->value();
    const int frameValue=m_param->frameMode->value();
    if(encodingValue<0 || encodingValue>1 || frameValue<0 || frameValue>2)
    {
        setCommunicationError("Serial encoding or frame mode is invalid");
        return EXvFuncRunStatus::Error;
    }
    if(m_param->timeoutMs->value()<1 || m_param->timeoutMs->value()>600000
            || m_param->maxBytes->value()<1 || m_param->maxBytes->value()>16*1024*1024)
    {
        setCommunicationError("Serial timeout or payload limit is invalid");
        return EXvFuncRunStatus::Error;
    }
    SerialDataRequest request;
    request.portName=m_param->portName->value().trimmed();
    request.baudRate=m_param->baudRate->value();
    request.dataBits=m_param->dataBits->value();
    request.parity=m_param->parity->value();
    request.stopBits=m_param->stopBits->value();
    request.flowControl=m_param->flowControl->value();
    request.timeoutMs=m_param->timeoutMs->value();
    request.maxBytes=m_param->maxBytes->value();
    request.encoding=static_cast<CommunicationEncoding>(encodingValue);
    request.frame=static_cast<CommunicationFrame>(frameValue);
    request.fixedLength=m_param->fixedLength->value();
    request.write=m_mode==WriteBytes || m_mode==WriteText;
    request.textMode=m_mode==ReadText || m_mode==WriteText;
    QString error;
    if(!request.write)
    {
        if(request.frame==CommunicationFrame::Delimiter
                && !encodeCommunicationText(m_param->delimiter->value(),request.encoding,
                                            request.delimiter,error))
        {
            setCommunicationError(error);
            return EXvFuncRunStatus::Error;
        }
        if(!validateCommunicationFrame(request.frame,request.delimiter,request.fixedLength,
                                       request.maxBytes,error))
        {
            setCommunicationError(error);
            return EXvFuncRunStatus::Error;
        }
    }
    else if(m_mode==WriteBytes)
    {
        if(m_param->useByteInput->value()) request.payload=m_param->byteInput->value();
        else if(m_param->payload->value().size()>request.maxBytes*3+1024)
        {
            setCommunicationError("Serial hexadecimal payload exceeded the configured maximum");
            return EXvFuncRunStatus::Error;
        }
        else if(!decodeHexPayload(m_param->payload->value(),request.payload,error))
        {
            setCommunicationError(error);
            return EXvFuncRunStatus::Error;
        }
    }
    else if(!encodeCommunicationText(m_param->payload->value(),request.encoding,
                                     request.payload,error))
    {
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    if(request.payload.size()>request.maxBytes)
    {
        setCommunicationError("Serial payload exceeded the configured maximum");
        return EXvFuncRunStatus::Error;
    }
    SerialDataResponse response;
    const auto transport=m_transport?m_transport:createSerialDataTransport();
    if(!transport || !transport->execute(request,{communicationCancellationFlag()},response,error))
    {
        if(communicationCancelled()) return communicationCancelledStatus();
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    const QByteArray resultBytes=request.write?request.payload:response.data;
    if(resultBytes.size()>request.maxBytes || response.bytesTransferred!=resultBytes.size())
    {
        setCommunicationError("Serial transport returned inconsistent payload metadata");
        return EXvFuncRunStatus::Error;
    }
    QString resultText;
    if(request.textMode)
    {
        if(!decodeCommunicationText(resultBytes,request.encoding,resultText,error))
        {
            setCommunicationError(error);
            return EXvFuncRunStatus::Error;
        }
    }
    else resultText=QString::fromLatin1(resultBytes.toHex(' '));
    if(communicationCancelled()) return communicationCancelledStatus();
    m_result->data->setValue(resultBytes);
    m_result->text->setValue(resultText);
    m_result->bytesTransferred->setValue(static_cast<int>(response.bytesTransferred));
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

QPixmap SerialData::funcIcon()
{
    return QPixmap(":/images/LogOutput.svg");
}

void SerialData::onShowFunc()
{
    if(!m_widget) m_widget=new CommunicationOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
