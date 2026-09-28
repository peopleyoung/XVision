#ifndef UDPTEXT_H
#define UDPTEXT_H

#include "CommunicationOperatorBase.h"
#include "CommunicationTransport.h"
#include "XVFuncSystemGlobal.h"
#include "XVisionRuntimeData.h"

#include <XObjectBaseType>

class CommunicationOperatorWdg;

namespace XvCore
{

class UdpTextParam:public XvBaseParam
{
public:
    UdpTextParam();

    XString *localAddress=nullptr;
    XInt *localPort=nullptr;
    XString *remoteHost=nullptr;
    XInt *remotePort=nullptr;
    XInt *timeoutMs=nullptr;
    XInt *maxBytes=nullptr;
    XInt *encoding=nullptr;
    XString *payload=nullptr;
};

class UdpTextResult:public XvBaseResult
{
public:
    UdpTextResult();

    XString *text=nullptr;
    XByteArray *data=nullptr;
    XInt *bytesTransferred=nullptr;
    XString *senderAddress=nullptr;
    XInt *senderPort=nullptr;
};

class XVFUNCSYSTEM_EXPORT UdpText:public CommunicationOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(UdpText::Mode mode READ mode WRITE setMode)
public:
    enum Mode { Read=0,Write=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit UdpText(QObject *parent=nullptr);
    ~UdpText() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;
    QStringList communicationModeNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    void setTransport(const std::shared_ptr<IUdpTextTransport> &transport);

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;
    QPixmap funcIcon() override;

private:
    UdpTextParam *m_param=nullptr;
    UdpTextResult *m_result=nullptr;
    CommunicationOperatorWdg *m_widget=nullptr;
    std::shared_ptr<IUdpTextTransport> m_transport;
    Mode m_mode=Read;
};

}

#endif // UDPTEXT_H
