#ifndef MODBUSREGISTER_H
#define MODBUSREGISTER_H

#include "CommunicationOperatorBase.h"
#include "CommunicationTransport.h"
#include "XVFuncSystemGlobal.h"

#include <XObjectBaseType>

class CommunicationOperatorWdg;

namespace XvCore
{

class ModbusRegisterParam:public XvBaseParam
{
public:
    ModbusRegisterParam();

    XString *host=nullptr;
    XInt *port=nullptr;
    XInt *unitId=nullptr;
    XInt *address=nullptr;
    XInt *timeoutMs=nullptr;
    XInt *byteOrder=nullptr;
    XInt *wordOrder=nullptr;
    XInt *value=nullptr;
};

class ModbusRegisterResult:public XvBaseResult
{
public:
    ModbusRegisterResult();

    XInt *value=nullptr;
    XInt *registerCount=nullptr;
    XBool *written=nullptr;
};

class XVFUNCSYSTEM_EXPORT ModbusRegister:public CommunicationOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(ModbusRegister::Mode mode READ mode WRITE setMode)
public:
    enum Mode { ReadInt32=0,WriteInt16=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit ModbusRegister(QObject *parent=nullptr);
    ~ModbusRegister() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;
    QStringList communicationModeNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    void setTransport(const std::shared_ptr<IModbusRegisterTransport> &transport);

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;
    QPixmap funcIcon() override;

private:
    ModbusRegisterParam *m_param=nullptr;
    ModbusRegisterResult *m_result=nullptr;
    CommunicationOperatorWdg *m_widget=nullptr;
    std::shared_ptr<IModbusRegisterTransport> m_transport;
    Mode m_mode=ReadInt32;
};

}

#endif // MODBUSREGISTER_H
