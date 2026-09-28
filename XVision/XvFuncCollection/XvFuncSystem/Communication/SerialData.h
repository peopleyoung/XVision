#ifndef SERIALDATA_H
#define SERIALDATA_H

#include "CommunicationOperatorBase.h"
#include "CommunicationTransport.h"
#include "XVFuncSystemGlobal.h"
#include "XVisionRuntimeData.h"

#include <XObjectBaseType>

class CommunicationOperatorWdg;

namespace XvCore
{

class SerialDataParam:public XvBaseParam
{
public:
    SerialDataParam();

    XString *portName=nullptr;
    XInt *baudRate=nullptr;
    XInt *dataBits=nullptr;
    XInt *parity=nullptr;
    XInt *stopBits=nullptr;
    XInt *flowControl=nullptr;
    XInt *timeoutMs=nullptr;
    XInt *maxBytes=nullptr;
    XInt *encoding=nullptr;
    XInt *frameMode=nullptr;
    XString *delimiter=nullptr;
    XInt *fixedLength=nullptr;
    XBool *useByteInput=nullptr;
    XByteArray *byteInput=nullptr;
    XString *payload=nullptr;
};

class SerialDataResult:public XvBaseResult
{
public:
    SerialDataResult();

    XByteArray *data=nullptr;
    XString *text=nullptr;
    XInt *bytesTransferred=nullptr;
};

class XVFUNCSYSTEM_EXPORT SerialData:public CommunicationOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(SerialData::Mode mode READ mode WRITE setMode)
public:
    enum Mode { ReadBytes=0,ReadText=1,WriteBytes=2,WriteText=3 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit SerialData(QObject *parent=nullptr);
    ~SerialData() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;
    QStringList communicationModeNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    void setTransport(const std::shared_ptr<ISerialDataTransport> &transport);

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;
    QPixmap funcIcon() override;

private:
    SerialDataParam *m_param=nullptr;
    SerialDataResult *m_result=nullptr;
    CommunicationOperatorWdg *m_widget=nullptr;
    std::shared_ptr<ISerialDataTransport> m_transport;
    Mode m_mode=ReadBytes;
};

}

#endif // SERIALDATA_H
