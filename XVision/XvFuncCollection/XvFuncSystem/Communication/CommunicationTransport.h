#ifndef COMMUNICATIONTRANSPORT_H
#define COMMUNICATIONTRANSPORT_H

#include "XVFuncSystemGlobal.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

namespace XvCore
{

enum class CommunicationEncoding
{
    Utf8=0,
    Latin1=1
};

enum class CommunicationFrame
{
    Delimiter=0,
    FixedLength=1,
    UntilCloseOrQuiet=2
};

struct CommunicationCancellation
{
    const std::atomic_bool *flag=nullptr;

    bool requested() const
    {
        return flag && flag->load(std::memory_order_relaxed);
    }
};

XVFUNCSYSTEM_EXPORT bool encodeCommunicationText(const QString &text,
                                                   CommunicationEncoding encoding,
                                                   QByteArray &bytes,
                                                   QString &error);
XVFUNCSYSTEM_EXPORT bool decodeCommunicationText(const QByteArray &bytes,
                                                   CommunicationEncoding encoding,
                                                   QString &text,
                                                   QString &error);
XVFUNCSYSTEM_EXPORT bool validateCommunicationFrame(CommunicationFrame frame,
                                                      const QByteArray &delimiter,
                                                      int fixedLength,
                                                      int maxBytes,
                                                      QString &error);
XVFUNCSYSTEM_EXPORT bool decodeModbusInt32(const QVector<quint16> &registers,
                                            int byteOrder,
                                            int wordOrder,
                                            qint32 &value,
                                            QString &error);
XVFUNCSYSTEM_EXPORT bool encodeModbusInt16(qint32 value,
                                            int byteOrder,
                                            QVector<quint16> &registers,
                                            QString &error);

struct HttpJsonRequest
{
    QString url;
    QByteArray headersJson;
    QByteArray body;
    bool write=false;
    int timeoutMs=5000;
    int maxResponseBytes=1024*1024;
};

struct HttpJsonResponse
{
    int statusCode=0;
    QByteArray body;
    qint64 bytesSent=0;
};

class XVFUNCSYSTEM_EXPORT IHttpJsonTransport
{
public:
    virtual ~IHttpJsonTransport()=default;
    virtual bool execute(const HttpJsonRequest &request,
                         const CommunicationCancellation &cancellation,
                         HttpJsonResponse &response,
                         QString &error)=0;
};

struct TextStreamRequest
{
    QString host;
    quint16 port=0;
    int connectTimeoutMs=5000;
    int timeoutMs=5000;
    int maxBytes=1024*1024;
    CommunicationEncoding encoding=CommunicationEncoding::Utf8;
    CommunicationFrame frame=CommunicationFrame::Delimiter;
    QByteArray delimiter;
    int fixedLength=0;
    QByteArray payload;
    bool write=false;
    bool appendDelimiter=false;
};

struct TextStreamResponse
{
    QByteArray data;
    qint64 bytesTransferred=0;
};

class XVFUNCSYSTEM_EXPORT ITcpTextTransport
{
public:
    virtual ~ITcpTextTransport()=default;
    virtual bool execute(const TextStreamRequest &request,
                         const CommunicationCancellation &cancellation,
                         TextStreamResponse &response,
                         QString &error)=0;
};

struct UdpTextRequest
{
    QString localAddress="0.0.0.0";
    quint16 localPort=0;
    QString remoteHost;
    quint16 remotePort=0;
    int timeoutMs=5000;
    int maxBytes=65507;
    CommunicationEncoding encoding=CommunicationEncoding::Utf8;
    QByteArray payload;
    bool write=false;
};

struct UdpTextResponse
{
    QByteArray data;
    qint64 bytesTransferred=0;
    QString senderAddress;
    quint16 senderPort=0;
};

class XVFUNCSYSTEM_EXPORT IUdpTextTransport
{
public:
    virtual ~IUdpTextTransport()=default;
    virtual bool execute(const UdpTextRequest &request,
                         const CommunicationCancellation &cancellation,
                         UdpTextResponse &response,
                         QString &error)=0;
};

struct SerialDataRequest
{
    QString portName;
    int baudRate=115200;
    int dataBits=8;
    int parity=0;
    int stopBits=1;
    int flowControl=0;
    int timeoutMs=5000;
    int maxBytes=1024*1024;
    CommunicationEncoding encoding=CommunicationEncoding::Utf8;
    CommunicationFrame frame=CommunicationFrame::Delimiter;
    QByteArray delimiter;
    int fixedLength=0;
    QByteArray payload;
    bool write=false;
    bool textMode=false;
};

struct SerialDataResponse
{
    QByteArray data;
    qint64 bytesTransferred=0;
};

class XVFUNCSYSTEM_EXPORT ISerialDataTransport
{
public:
    virtual ~ISerialDataTransport()=default;
    virtual bool execute(const SerialDataRequest &request,
                         const CommunicationCancellation &cancellation,
                         SerialDataResponse &response,
                         QString &error)=0;
};

struct ModbusRegisterRequest
{
    QString host;
    quint16 port=502;
    int unitId=1;
    quint16 address=0;
    int timeoutMs=5000;
    int byteOrder=0;
    int wordOrder=0;
    qint32 value=0;
    bool write=false;
};

struct ModbusRegisterResponse
{
    QVector<quint16> registers;
    bool written=false;
};

class XVFUNCSYSTEM_EXPORT IModbusRegisterTransport
{
public:
    virtual ~IModbusRegisterTransport()=default;
    virtual bool execute(const ModbusRegisterRequest &request,
                         const CommunicationCancellation &cancellation,
                         ModbusRegisterResponse &response,
                         QString &error)=0;
};

XVFUNCSYSTEM_EXPORT std::shared_ptr<IHttpJsonTransport> createHttpJsonTransport();
XVFUNCSYSTEM_EXPORT std::shared_ptr<ITcpTextTransport> createTcpTextTransport();
XVFUNCSYSTEM_EXPORT std::shared_ptr<IUdpTextTransport> createUdpTextTransport();
XVFUNCSYSTEM_EXPORT std::shared_ptr<ISerialDataTransport> createSerialDataTransport();
XVFUNCSYSTEM_EXPORT std::shared_ptr<IModbusRegisterTransport> createModbusRegisterTransport();

}

#endif // COMMUNICATIONTRANSPORT_H
