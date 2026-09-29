#ifndef HTTPJSON_H
#define HTTPJSON_H

#include "CommunicationOperatorBase.h"
#include "CommunicationTransport.h"
#include "XVFuncSystemGlobal.h"
#include "XVisionRuntimeData.h"

#include <XObjectBaseType>

class CommunicationOperatorWdg;

namespace XvCore
{

class HttpJsonParam:public XvBaseParam
{
public:
    HttpJsonParam();

    XString *url=nullptr;
    XInt *timeoutMs=nullptr;
    XInt *maxResponseBytes=nullptr;
    XString *headersJson=nullptr;
    XBool *useJsonInput=nullptr;
    XJsonValue *jsonInput=nullptr;
    XString *payloadJson=nullptr;
};

class HttpJsonResult:public XvBaseResult
{
public:
    HttpJsonResult();

    XJsonValue *json=nullptr;
    XString *responseText=nullptr;
    XInt *statusCode=nullptr;
    XInt *bytesReceived=nullptr;
    XInt *bytesSent=nullptr;
};

class XVFUNCSYSTEM_EXPORT HttpJson:public CommunicationOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
public:
    enum Mode { Read=0,Write=1 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit HttpJson(QObject *parent=nullptr);
    ~HttpJson() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;
    QStringList communicationModeNames() const override;
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    void setTransport(const std::shared_ptr<IHttpJsonTransport> &transport);

public slots:
    void onShowFunc() override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;


private:
    HttpJsonParam *m_param=nullptr;
    HttpJsonResult *m_result=nullptr;
    CommunicationOperatorWdg *m_widget=nullptr;
    std::shared_ptr<IHttpJsonTransport> m_transport;
    Mode m_mode=Read;
};

}

#endif // HTTPJSON_H
