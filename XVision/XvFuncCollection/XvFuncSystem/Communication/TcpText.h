#ifndef TCPTEXT_H
#define TCPTEXT_H

#include "CommunicationOperatorBase.h"
#include "CommunicationTransport.h"
#include "XVFuncSystemGlobal.h"
#include "XVisionRuntimeData.h"

#include <XObjectBaseType>

class CommunicationOperatorWdg;

namespace XvCore
{

class TcpTextParam:public XvBaseParam
{
public:
    TcpTextParam();

    XString *host=nullptr;
    XInt *port=nullptr;
    XInt *connectTimeoutMs=nullptr;
    XInt *timeoutMs=nullptr;
    XInt *maxBytes=nullptr;
    XInt *encoding=nullptr;
    XInt *frameMode=nullptr;
    XString *delimiter=nullptr;
    XInt *fixedLength=nullptr;
    XString *payload=nullptr;
    XBool *appendDelimiter=nullptr;
};

class TcpTextResult:public XvBaseResult
{
public:
    TcpTextResult();

    XString *text=nullptr;
    XByteArray *data=nullptr;
    XInt *bytesTransferred=nullptr;
};

class XVFUNCSYSTEM_EXPORT TcpText:public CommunicationOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Read=0,Write=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit TcpText(QObject *parent=nullptr);
    ~TcpText() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;
    QStringList communicationModeNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    void setTransport(const std::shared_ptr<ITcpTextTransport> &transport);

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;


private:
    TcpTextParam *m_param=nullptr;
    TcpTextResult *m_result=nullptr;
    CommunicationOperatorWdg *m_widget=nullptr;
    std::shared_ptr<ITcpTextTransport> m_transport;
    Mode m_mode=Read;
};

}

#endif // TCPTEXT_H
