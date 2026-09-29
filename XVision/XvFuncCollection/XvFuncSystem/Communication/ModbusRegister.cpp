#include "ModbusRegister.h"

#include "CommunicationOperatorWdg.h"
#include "XLanguage.h"

using namespace XvCore;

ModbusRegisterParam::ModbusRegisterParam()
{
    host=new XString("host","127.0.0.1",this,getLang("XvFuncSystem_Communication_Host","主机"));
    port=new XInt("port",502,this,getLang("XvFuncSystem_Communication_Port","端口"));
    unitId=new XInt("unitId",1,this,getLang("XvFuncSystem_Modbus_UnitId","从站地址"));
    address=new XInt("address",0,this,getLang("XvFuncSystem_Modbus_Address","寄存器地址"));
    timeoutMs=new XInt("timeoutMs",5000,this,getLang("XvFuncSystem_Communication_Timeout","超时(ms)"));
    byteOrder=new XInt("byteOrder",0,this,getLang("XvFuncSystem_Modbus_ByteOrder","字节序"));
    wordOrder=new XInt("wordOrder",0,this,getLang("XvFuncSystem_Modbus_WordOrder","字序"));
    value=new XInt("value",0,this,getLang("XvFuncSystem_Modbus_Value","数值"));
}

ModbusRegisterResult::ModbusRegisterResult()
{
    value=new XInt("value",0,this,getLang("XvFuncSystem_Modbus_ResultValue","寄存器值"));
    registerCount=new XInt("registerCount",0,this,getLang("XvFuncSystem_Modbus_RegisterCount","寄存器数量"));
    written=new XBool("written",false,this,getLang("XvFuncSystem_Modbus_Written","写入成功"));
}

ModbusRegister::ModbusRegister(QObject *parent)
    :CommunicationOperatorBase(parent),m_param(new ModbusRegisterParam()),
      m_result(new ModbusRegisterResult())
{
    _funcRole="ModbusRegister";
    _funcName=getLang("XvFuncSystem_ModbusRegister_Name","Modbus寄存器");
    _funcType=EXvFuncType::Communication;
}

ModbusRegister::~ModbusRegister()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QStringList ModbusRegister::activeParameterNames() const
{
    QStringList names={"host","port","unitId","address","timeoutMs","byteOrder"};
    if(m_mode==ReadInt32) names << "wordOrder";
    else names << "value";
    return names;
}

QStringList ModbusRegister::communicationModeNames() const
{
    return {getLang("XvFuncSystem_Modbus_ReadInt32","读取int32"),
            getLang("XvFuncSystem_Modbus_WriteInt16","写入int16")};
}

void ModbusRegister::setTransport(const std::shared_ptr<IModbusRegisterTransport> &transport)
{
    m_transport=transport;
}

EXvFuncRunStatus ModbusRegister::run()
{
    if(!m_param || !m_result || m_mode<ReadInt32 || m_mode>WriteInt16)
    {
        setCommunicationError("Modbus register operator state is incomplete");
        return EXvFuncRunStatus::Error;
    }
    const int port=m_param->port->value();
    const int unitId=m_param->unitId->value();
    const int address=m_param->address->value();
    const int byteOrder=m_param->byteOrder->value();
    const int wordOrder=m_param->wordOrder->value();
    const int value=m_param->value->value();
    if(m_param->host->value().trimmed().isEmpty()
            || m_param->host->value().trimmed().size()>253 || port<1 || port>65535
            || unitId<1 || unitId>247 || address<0 || address>65535
            || (m_mode==ReadInt32 && address==65535)
            || m_param->timeoutMs->value()<1 || m_param->timeoutMs->value()>600000
            || (byteOrder!=0 && byteOrder!=1) || (wordOrder!=0 && wordOrder!=1)
            || (m_mode==WriteInt16 && (value<-32768 || value>32767)))
    {
        setCommunicationError("Modbus endpoint, address, order, timeout, or value is invalid");
        return EXvFuncRunStatus::Error;
    }
    ModbusRegisterRequest request;
    request.host=m_param->host->value().trimmed();
    request.port=static_cast<quint16>(port);
    request.unitId=unitId;
    request.address=static_cast<quint16>(address);
    request.timeoutMs=m_param->timeoutMs->value();
    request.byteOrder=byteOrder;
    request.wordOrder=wordOrder;
    request.value=value;
    request.write=m_mode==WriteInt16;
    ModbusRegisterResponse response;
    QString error;
    const auto transport=m_transport?m_transport:createModbusRegisterTransport();
    if(!transport || !transport->execute(request,{communicationCancellationFlag()},response,error))
    {
        if(communicationCancelled()) return communicationCancelledStatus();
        setCommunicationError(error);
        return EXvFuncRunStatus::Error;
    }
    int resultValue=value;
    int resultRegisterCount=1;
    bool resultWritten=true;
    if(!request.write)
    {
        qint32 decoded=0;
        if(!decodeModbusInt32(response.registers,byteOrder,wordOrder,decoded,error))
        {
            setCommunicationError(error);
            return EXvFuncRunStatus::Error;
        }
        resultValue=decoded;
        resultRegisterCount=2;
        resultWritten=false;
    }
    else if(!response.written)
    {
        setCommunicationError("Modbus transport did not confirm the register write");
        return EXvFuncRunStatus::Error;
    }
    if(communicationCancelled()) return communicationCancelledStatus();
    m_result->value->setValue(resultValue);
    m_result->registerCount->setValue(resultRegisterCount);
    m_result->written->setValue(resultWritten);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
}

void ModbusRegister::onShowFunc()
{
    if(!m_widget) m_widget=new CommunicationOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
