#include "CommunicationTransport.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSerialPort>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <limits>

#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>

using namespace XvCore;

namespace
{
constexpr int WaitSliceMs=50;
constexpr int SerialQuietMs=50;

bool validTimeout(int timeoutMs,QString &error)
{
    if(timeoutMs<1 || timeoutMs>600000)
    {
        error="Communication timeout must be between 1 and 600000 ms";
        return false;
    }
    return true;
}

bool validMaxBytes(int maxBytes,QString &error)
{
    if(maxBytes<1 || maxBytes>16*1024*1024)
    {
        error="Communication maximum payload must be between 1 and 16 MiB";
        return false;
    }
    return true;
}

int remainingMs(const QElapsedTimer &timer,int timeoutMs)
{
    return std::max(0,timeoutMs-static_cast<int>(timer.elapsed()));
}

bool cancelled(const CommunicationCancellation &cancellation,QString &error)
{
    if(cancellation.requested())
    {
        error="Communication operation cancelled";
        return true;
    }
    return false;
}

bool validUtf8(const QByteArray &bytes)
{
    const auto *data=reinterpret_cast<const unsigned char *>(bytes.constData());
    int index=0;
    while(index<bytes.size())
    {
        const unsigned char first=data[index];
        if(first<=0x7f) { ++index; continue; }
        if(first>=0xc2 && first<=0xdf)
        {
            if(index+1>=bytes.size() || (data[index+1]&0xc0)!=0x80) return false;
            index+=2;
            continue;
        }
        if(first>=0xe0 && first<=0xef)
        {
            if(index+2>=bytes.size() || (data[index+2]&0xc0)!=0x80) return false;
            const unsigned char second=data[index+1];
            if((first==0xe0 && (second<0xa0 || second>0xbf))
                    || (first==0xed && (second<0x80 || second>0x9f))
                    || (first!=0xe0 && first!=0xed && (second&0xc0)!=0x80)) return false;
            index+=3;
            continue;
        }
        if(first>=0xf0 && first<=0xf4)
        {
            if(index+3>=bytes.size() || (data[index+2]&0xc0)!=0x80
                    || (data[index+3]&0xc0)!=0x80) return false;
            const unsigned char second=data[index+1];
            if((first==0xf0 && (second<0x90 || second>0xbf))
                    || (first==0xf4 && (second<0x80 || second>0x8f))
                    || (first!=0xf0 && first!=0xf4 && (second&0xc0)!=0x80)) return false;
            index+=4;
            continue;
        }
        return false;
    }
    return true;
}

bool validQStringUtf16(const QString &text)
{
    for(int index=0;index<text.size();++index)
    {
        const QChar character=text.at(index);
        if(character.isHighSurrogate())
        {
            if(index+1>=text.size() || !text.at(index+1).isLowSurrogate()) return false;
            ++index;
        }
        else if(character.isLowSurrogate()) return false;
    }
    return true;
}

bool appendBounded(QByteArray &target,const QByteArray &chunk,int maxBytes,QString &error)
{
    if(chunk.isEmpty()) return true;
    if(target.size()>maxBytes-chunk.size())
    {
        error="Communication response exceeded the configured maximum";
        return false;
    }
    target.append(chunk);
    return true;
}

bool validHttpHeaderName(const QByteArray &name)
{
    if(name.isEmpty()) return false;
    for(const unsigned char character:name)
    {
        const bool alphaNumeric=(character>='a' && character<='z')
                || (character>='A' && character<='Z')
                || (character>='0' && character<='9');
        const bool symbol=character=='!' || character=='#' || character=='$'
                || character=='%' || character=='&' || character=='\''
                || character=='*' || character=='+' || character=='-'
                || character=='.' || character=='^' || character=='_'
                || character=='`' || character=='|' || character=='~';
        if(!alphaNumeric && !symbol) return false;
    }
    return true;
}

bool validHttpHeaderValue(const QByteArray &value)
{
    for(const unsigned char character:value)
    {
        if((character<0x20 && character!='\t') || character==0x7f) return false;
    }
    return true;
}

bool collectFramed(QIODevice &device,const TextStreamRequest &request,
                   const CommunicationCancellation &cancellation,
                   QByteArray &data,QString &error,bool peerCloseEndsFrame)
{
    QElapsedTimer timer;
    timer.start();
    bool received=false;
    qint64 quietSince=-1;
    while(true)
    {
        if(cancelled(cancellation,error)) return false;
        if(request.frame==CommunicationFrame::Delimiter && received)
        {
            const int end=data.indexOf(request.delimiter);
            if(end>=0)
            {
                data.truncate(end);
                return true;
            }
        }
        if(request.frame==CommunicationFrame::FixedLength
                && data.size()>=request.fixedLength)
        {
            data.truncate(request.fixedLength);
            return true;
        }
        if(request.frame==CommunicationFrame::UntilCloseOrQuiet
                && !peerCloseEndsFrame && received && quietSince>=0
                && timer.elapsed()-quietSince>=SerialQuietMs)
            return true;

        const int remaining=remainingMs(timer,request.timeoutMs);
        if(remaining<=0)
        {
            error="Communication read timed out before a complete frame arrived";
            return false;
        }
        const int slice=std::min(WaitSliceMs,remaining);
        if(!device.waitForReadyRead(slice))
        {
            auto socket=dynamic_cast<QAbstractSocket *>(&device);
            if(peerCloseEndsFrame && socket
                    && socket->state()==QAbstractSocket::UnconnectedState)
            {
                if(request.frame==CommunicationFrame::UntilCloseOrQuiet && received)
                    return true;
                error="TCP peer closed before a complete frame arrived";
                return false;
            }
            if(request.frame==CommunicationFrame::UntilCloseOrQuiet
                    && !peerCloseEndsFrame && received)
            {
                if(quietSince<0) quietSince=timer.elapsed();
                continue;
            }
            continue;
        }
        const QByteArray chunk=device.readAll();
        if(request.frame==CommunicationFrame::Delimiter)
        {
            const QByteArray combined=data+chunk;
            const int end=combined.indexOf(request.delimiter);
            if(end>=0)
            {
                if(end>request.maxBytes)
                {
                    error="Communication response exceeded the configured maximum";
                    return false;
                }
                data=combined.left(end);
                return true;
            }
            if(!appendBounded(data,chunk,request.maxBytes,error)) return false;
        }
        else if(request.frame==CommunicationFrame::FixedLength)
        {
            const int required=request.fixedLength-data.size();
            if(required>0 && !appendBounded(data,chunk.left(required),request.maxBytes,error))
                return false;
            if(data.size()==request.fixedLength) return true;
        }
        else if(!appendBounded(data,chunk,request.maxBytes,error)) return false;
        if(!chunk.isEmpty())
        {
            received=true;
            quietSince=peerCloseEndsFrame?-1:timer.elapsed();
        }
    }
}

bool waitBytesWritten(QAbstractSocket &socket,const CommunicationCancellation &cancellation,
                      const QElapsedTimer &timer,int timeoutMs,QString &error)
{
    while(socket.bytesToWrite()>0)
    {
        if(cancelled(cancellation,error))
        {
            socket.abort();
            return false;
        }
        const int remaining=remainingMs(timer,timeoutMs);
        if(remaining<=0)
        {
            error="Communication write timed out";
            socket.abort();
            return false;
        }
        if(!socket.waitForBytesWritten(std::min(WaitSliceMs,remaining)))
        {
            if(socket.state()!=QAbstractSocket::ConnectedState)
            {
                error=socket.errorString();
                return false;
            }
        }
    }
    return true;
}

bool resolveHostAddress(const QString &host,int timeoutMs,
                        const CommunicationCancellation &cancellation,
                        QHostAddress &address,QString &error)
{
    address=QHostAddress(host);
    if(!address.isNull()) return true;
    if(host.trimmed().isEmpty()) { error="Remote host is empty"; return false; }
    QEventLoop loop;
    QTimer deadline;
    QTimer cancellationPoll;
    deadline.setSingleShot(true);
    bool finished=false;
    QHostInfo resolved;
    const int lookupId=QHostInfo::lookupHost(host,&loop,[&](const QHostInfo &candidate)
    {
        resolved=candidate;
        finished=true;
        loop.quit();
    });
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(&cancellationPoll,&QTimer::timeout,&loop,[&]()
    {
        if(cancellation.requested()) loop.quit();
    });
    deadline.start(timeoutMs);
    cancellationPoll.start(WaitSliceMs);
    loop.exec();
    cancellationPoll.stop();
    if(!finished) QHostInfo::abortHostLookup(lookupId);
    if(cancelled(cancellation,error)) return false;
    if(!finished) { error="Host lookup timed out"; return false; }
    if(resolved.error()!=QHostInfo::NoError || resolved.addresses().isEmpty())
    {
        error=resolved.errorString().isEmpty()?QString("Remote host could not be resolved")
                                               :resolved.errorString();
        return false;
    }
    address=resolved.addresses().first();
    return true;
}

class HttpJsonQtTransport final:public IHttpJsonTransport
{
public:
    bool execute(const HttpJsonRequest &request,
                 const CommunicationCancellation &cancellation,
                 HttpJsonResponse &response,QString &error) override
    {
        if(cancelled(cancellation,error)) return false;
        if(!validTimeout(request.timeoutMs,error)
                || !validMaxBytes(request.maxResponseBytes,error)
                || request.headersJson.size()>request.maxResponseBytes
                || request.body.size()>request.maxResponseBytes)
        {
            if(error.isEmpty()) error="HTTP request exceeded the configured maximum";
            return false;
        }
        const QUrl url(request.url);
        if(!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
        {
            error="HTTP URL is invalid";
            return false;
        }
        QNetworkRequest networkRequest(url);
        if(url.scheme()!="http" && url.scheme()!="https")
        {
            error="HTTP URL must use the http or https scheme";
            return false;
        }
        networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                    QNetworkRequest::ManualRedirectPolicy);
        QJsonParseError headerError;
        const QJsonDocument headers=QJsonDocument::fromJson(request.headersJson,&headerError);
        if(headerError.error!=QJsonParseError::NoError || !headers.isObject())
        {
            error="HTTP headers must be a JSON object";
            return false;
        }
        const QJsonObject headerObject=headers.object();
        for(auto iterator=headerObject.constBegin();iterator!=headerObject.constEnd();++iterator)
        {
            if(iterator.key().compare("Host",Qt::CaseInsensitive)==0
                    || iterator.key().compare("Content-Length",Qt::CaseInsensitive)==0)
            {
                error="HTTP Host and Content-Length headers are managed by Qt";
                return false;
            }
            if(!iterator.value().isString())
            {
                error="HTTP header values must be strings";
                return false;
            }
            const QByteArray name=iterator.key().toUtf8();
            const QByteArray value=iterator.value().toString().toUtf8();
            if(!validHttpHeaderName(name) || !validHttpHeaderValue(value))
            {
                error="HTTP header names or values contain invalid characters";
                return false;
            }
            networkRequest.setRawHeader(name,value);
        }
        if(request.write)
            networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");

        QNetworkAccessManager manager;
        QNetworkReply *reply=request.write
                ?manager.post(networkRequest,request.body)
                :manager.get(networkRequest);
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        deadline.setInterval(request.timeoutMs);
        QTimer cancellationPoll;
        cancellationPoll.setInterval(WaitSliceMs);
        QByteArray body;
        bool oversized=false;
        QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]()
        {
            if(!appendBounded(body,reply->readAll(),request.maxResponseBytes,error))
            {
                oversized=true;
                reply->abort();
                loop.quit();
            }
        });
        QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
        QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
        QObject::connect(&cancellationPoll,&QTimer::timeout,&loop,[&]()
        {
            if(cancellation.requested())
            {
                error="Communication operation cancelled";
                reply->abort();
                loop.quit();
            }
        });
        deadline.start();
        cancellationPoll.start();
        loop.exec();
        cancellationPoll.stop();
        if(!oversized && !appendBounded(body,reply->readAll(),request.maxResponseBytes,error))
            oversized=true;
        if(cancellation.requested())
        {
            reply->deleteLater();
            if(error.isEmpty()) error="Communication operation cancelled";
            return false;
        }
        if(oversized) { reply->deleteLater(); return false; }
        if(!reply->isFinished())
        {
            reply->abort();
            reply->deleteLater();
            error="HTTP request timed out";
            return false;
        }
        const int statusCode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError && statusCode==0)
        {
            error=reply->errorString();
            reply->deleteLater();
            return false;
        }
        response.statusCode=statusCode;
        response.body=body;
        response.bytesSent=request.body.size();
        reply->deleteLater();
        return true;
    }
};

class TcpTextQtTransport final:public ITcpTextTransport
{
public:
    bool execute(const TextStreamRequest &request,
                 const CommunicationCancellation &cancellation,
                 TextStreamResponse &response,QString &error) override
    {
        if(cancelled(cancellation,error)) return false;
        if(request.host.trimmed().isEmpty() || request.port==0
                || !validTimeout(request.connectTimeoutMs,error)
                || !validTimeout(request.timeoutMs,error)
                || !validMaxBytes(request.maxBytes,error)
                || (request.write && (request.payload.size()>request.maxBytes
                    || (request.appendDelimiter
                        && (request.delimiter.isEmpty()
                            || request.payload.size()>request.maxBytes-request.delimiter.size()))))
                || (!request.write
                    && !validateCommunicationFrame(request.frame,request.delimiter,
                                                   request.fixedLength,request.maxBytes,error)))
        {
            if(error.isEmpty()) error="TCP endpoint, frame, or payload is invalid";
            return false;
        }
        QTcpSocket socket;
        socket.connectToHost(request.host,request.port);
        QElapsedTimer connectTimer;
        connectTimer.start();
        while(socket.state()!=QAbstractSocket::ConnectedState)
        {
            if(cancelled(cancellation,error)) { socket.abort(); return false; }
            const int remaining=remainingMs(connectTimer,request.connectTimeoutMs);
            if(remaining<=0)
            {
                error="TCP connection timed out";
                socket.abort();
                return false;
            }
            socket.waitForConnected(std::min(WaitSliceMs,remaining));
            if(socket.state()==QAbstractSocket::UnconnectedState
                    && socket.error()!=QAbstractSocket::UnknownSocketError)
            {
                error=socket.errorString();
                return false;
            }
        }
        if(request.write)
        {
            QByteArray payload=request.payload;
            if(request.appendDelimiter) payload.append(request.delimiter);
            const qint64 written=socket.write(payload);
            if(written!=payload.size())
            {
                error="TCP payload could not be queued completely";
                socket.abort();
                return false;
            }
            QElapsedTimer writeTimer;
            writeTimer.start();
            if(!waitBytesWritten(socket,cancellation,writeTimer,request.timeoutMs,error)) return false;
            response.bytesTransferred=payload.size();
        }
        else
        {
            if(!collectFramed(socket,request,cancellation,response.data,error,true)) return false;
            response.bytesTransferred=response.data.size();
        }
        socket.disconnectFromHost();
        if(socket.state()!=QAbstractSocket::UnconnectedState) socket.waitForDisconnected(WaitSliceMs);
        return true;
    }
};

class UdpTextQtTransport final:public IUdpTextTransport
{
public:
    bool execute(const UdpTextRequest &request,
                 const CommunicationCancellation &cancellation,
                 UdpTextResponse &response,QString &error) override
    {
        if(cancelled(cancellation,error)) return false;
        if(!validTimeout(request.timeoutMs,error) || !validMaxBytes(request.maxBytes,error)
                || request.maxBytes>65507)
        {
            if(error.isEmpty()) error="UDP maximum payload cannot exceed 65507 bytes";
            return false;
        }
        QUdpSocket socket;
        if(request.write)
        {
            QHostAddress address;
            if(!resolveHostAddress(request.remoteHost,request.timeoutMs,cancellation,address,error))
                return false;
            if(request.remotePort==0 || request.payload.size()>request.maxBytes)
            {
                error="UDP destination or payload is invalid";
                return false;
            }
            const qint64 written=socket.writeDatagram(request.payload,address,request.remotePort);
            if(written!=request.payload.size())
            {
                error=socket.errorString();
                return false;
            }
            response.bytesTransferred=written;
            return true;
        }
        QHostAddress localAddress(request.localAddress);
        if(localAddress.isNull()) { error="UDP local address is invalid"; return false; }
        if(request.localPort==0 || !socket.bind(localAddress,request.localPort,
                                                QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint))
        {
            error=socket.errorString().isEmpty()?QString("UDP bind failed"):socket.errorString();
            return false;
        }
        QElapsedTimer timer;
        timer.start();
        while(!socket.hasPendingDatagrams())
        {
            if(cancelled(cancellation,error)) return false;
            const int remaining=remainingMs(timer,request.timeoutMs);
            if(remaining<=0) { error="UDP read timed out"; return false; }
            socket.waitForReadyRead(std::min(WaitSliceMs,remaining));
        }
        const qint64 size=socket.pendingDatagramSize();
        if(size<0 || size>request.maxBytes) { error="UDP datagram exceeded the configured maximum"; return false; }
        response.data.resize(static_cast<int>(size));
        QHostAddress sender;
        quint16 senderPort=0;
        const qint64 read=socket.readDatagram(response.data.data(),response.data.size(),&sender,&senderPort);
        if(read!=size) { error=socket.errorString(); return false; }
        response.bytesTransferred=read;
        response.senderAddress=sender.toString();
        response.senderPort=senderPort;
        return true;
    }
};

class SerialDataQtTransport final:public ISerialDataTransport
{
public:
    bool execute(const SerialDataRequest &request,
                 const CommunicationCancellation &cancellation,
                 SerialDataResponse &response,QString &error) override
    {
        if(cancelled(cancellation,error)) return false;
        if(request.portName.trimmed().isEmpty() || request.baudRate<1
                || !validTimeout(request.timeoutMs,error)
                || !validMaxBytes(request.maxBytes,error)
                || (request.write && request.payload.size()>request.maxBytes)
                || (!request.write
                    && !validateCommunicationFrame(request.frame,request.delimiter,
                                                   request.fixedLength,request.maxBytes,error)))
        {
            if(error.isEmpty()) error="Serial endpoint, frame, or payload is invalid";
            return false;
        }
        QSerialPort port;
        port.setPortName(request.portName);
        if(!port.setBaudRate(request.baudRate)
                || !port.setDataBits(static_cast<QSerialPort::DataBits>(request.dataBits))
                || !port.setParity(static_cast<QSerialPort::Parity>(request.parity))
                || !port.setStopBits(static_cast<QSerialPort::StopBits>(request.stopBits))
                || !port.setFlowControl(static_cast<QSerialPort::FlowControl>(request.flowControl)))
        {
            error=port.errorString();
            return false;
        }
        if(!port.open(QIODevice::ReadWrite)) { error=port.errorString(); return false; }
        if(request.write)
        {
            const qint64 written=port.write(request.payload);
            if(written!=request.payload.size()) { error=port.errorString(); return false; }
            QElapsedTimer timer;
            timer.start();
            while(port.bytesToWrite()>0)
            {
                if(cancelled(cancellation,error)) return false;
                const int remaining=remainingMs(timer,request.timeoutMs);
                if(remaining<=0) { error="Serial write timed out"; return false; }
                port.waitForBytesWritten(std::min(WaitSliceMs,remaining));
            }
            response.bytesTransferred=written;
            return true;
        }
        TextStreamRequest framed;
        framed.timeoutMs=request.timeoutMs;
        framed.maxBytes=request.maxBytes;
        framed.frame=request.frame;
        framed.delimiter=request.delimiter;
        framed.fixedLength=request.fixedLength;
        if(!collectFramed(port,framed,cancellation,response.data,error,false)) return false;
        response.bytesTransferred=response.data.size();
        return true;
    }
};

class ModbusRegisterQtTransport final:public IModbusRegisterTransport
{
public:
    bool execute(const ModbusRegisterRequest &request,
                 const CommunicationCancellation &cancellation,
                 ModbusRegisterResponse &response,QString &error) override
    {
        if(cancelled(cancellation,error)) return false;
        if(request.host.trimmed().isEmpty() || request.port==0 || request.unitId<1
                || request.unitId>247 || (!request.write && request.address==65535)
                || !validTimeout(request.timeoutMs,error))
            { if(error.isEmpty()) error="Modbus endpoint or unit is invalid"; return false; }
        QElapsedTimer operationTimer;
        operationTimer.start();
        QModbusTcpClient client;
        client.setConnectionParameter(QModbusDevice::NetworkAddressParameter,request.host);
        client.setConnectionParameter(QModbusDevice::NetworkPortParameter,request.port);
        client.setTimeout(request.timeoutMs);
        client.setNumberOfRetries(0);
        QEventLoop connectionLoop;
        QTimer connectionTimeout;
        QTimer cancellationPoll;
        connectionTimeout.setSingleShot(true);
        QObject::connect(&client,&QModbusDevice::stateChanged,&connectionLoop,[&](QModbusDevice::State state)
        {
            if(state==QModbusDevice::ConnectedState || state==QModbusDevice::UnconnectedState)
                connectionLoop.quit();
        });
        QObject::connect(&connectionTimeout,&QTimer::timeout,&connectionLoop,&QEventLoop::quit);
        QObject::connect(&cancellationPoll,&QTimer::timeout,&connectionLoop,[&]()
        {
            if(cancellation.requested()) connectionLoop.quit();
        });
        if(!client.connectDevice()) { error=client.errorString(); return false; }
        connectionTimeout.start(remainingMs(operationTimer,request.timeoutMs));
        cancellationPoll.start(WaitSliceMs);
        if(client.state()==QModbusDevice::ConnectingState) connectionLoop.exec();
        connectionTimeout.stop();
        cancellationPoll.stop();
        if(cancellation.requested()) { client.disconnectDevice(); error="Communication operation cancelled"; return false; }
        if(client.state()!=QModbusDevice::ConnectedState)
        {
            client.disconnectDevice();
            error=client.errorString().isEmpty()?QString("Modbus connection timed out"):client.errorString();
            return false;
        }
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,request.address,
                             request.write?1:2);
        if(request.write)
        {
            QVector<quint16> encoded;
            if(!encodeModbusInt16(request.value,request.byteOrder,encoded,error))
            {
                client.disconnectDevice();
                return false;
            }
            unit.setValue(0,encoded.first());
        }
        QModbusReply *reply=request.write
                ?client.sendWriteRequest(unit,request.unitId)
                :client.sendReadRequest(unit,request.unitId);
        if(!reply) { error=client.errorString(); client.disconnectDevice(); return false; }
        const int requestRemaining=remainingMs(operationTimer,request.timeoutMs);
        if(requestRemaining<=0)
        {
            error="Modbus operation timed out";
            reply->deleteLater();
            client.disconnectDevice();
            return false;
        }
        QEventLoop replyLoop;
        QTimer replyTimeout;
        QTimer replyCancellation;
        replyTimeout.setSingleShot(true);
        QObject::connect(reply,&QModbusReply::finished,&replyLoop,&QEventLoop::quit);
        QObject::connect(&replyTimeout,&QTimer::timeout,&replyLoop,&QEventLoop::quit);
        QObject::connect(&replyCancellation,&QTimer::timeout,&replyLoop,[&]()
        {
            if(cancellation.requested()) { client.disconnectDevice(); replyLoop.quit(); }
        });
        replyTimeout.start(requestRemaining);
        replyCancellation.start(WaitSliceMs);
        if(!reply->isFinished()) replyLoop.exec();
        replyCancellation.stop();
        if(cancellation.requested()) { error="Communication operation cancelled"; reply->deleteLater(); client.disconnectDevice(); return false; }
        if(!reply->isFinished()) { error="Modbus request timed out"; client.disconnectDevice(); reply->deleteLater(); return false; }
        if(reply->error()!=QModbusDevice::NoError)
        {
            error=reply->errorString();
            reply->deleteLater();
            client.disconnectDevice();
            return false;
        }
        if(request.write)
        {
            response.written=true;
        }
        else
        {
            const QModbusDataUnit result=reply->result();
            response.registers.reserve(result.valueCount());
            for(uint index=0;index<result.valueCount();++index)
                response.registers.append(result.value(index));
        }
        reply->deleteLater();
        client.disconnectDevice();
        return true;
    }
};
}

bool XvCore::encodeCommunicationText(const QString &text,CommunicationEncoding encoding,
                                     QByteArray &bytes,QString &error)
{
    if(encoding==CommunicationEncoding::Utf8)
    {
        if(!validQStringUtf16(text))
        {
            error="Text contains an unpaired UTF-16 surrogate";
            return false;
        }
        bytes=text.toUtf8();
        return true;
    }
    if(encoding==CommunicationEncoding::Latin1)
    {
        for(const QChar character:text)
        {
            if(character.unicode()>0xff)
            {
                error="Text contains characters not representable in Latin-1";
                return false;
            }
        }
        bytes=text.toLatin1();
        return true;
    }
    error="Unsupported communication encoding";
    return false;
}

bool XvCore::decodeCommunicationText(const QByteArray &bytes,CommunicationEncoding encoding,
                                     QString &text,QString &error)
{
    if(encoding==CommunicationEncoding::Latin1) { text=QString::fromLatin1(bytes); return true; }
    if(encoding==CommunicationEncoding::Utf8)
    {
        if(!validUtf8(bytes))
        {
            error="Received bytes are not valid UTF-8";
            return false;
        }
        text=QString::fromUtf8(bytes);
        return true;
    }
    error="Unsupported communication encoding";
    return false;
}

bool XvCore::validateCommunicationFrame(CommunicationFrame frame,const QByteArray &delimiter,
                                        int fixedLength,int maxBytes,QString &error)
{
    if(!validMaxBytes(maxBytes,error)) return false;
    switch(frame)
    {
    case CommunicationFrame::Delimiter:
        if(delimiter.isEmpty() || delimiter.size()>maxBytes)
        {
            error="Delimiter framing requires a nonempty delimiter within the payload limit";
            return false;
        }
        return true;
    case CommunicationFrame::FixedLength:
        if(fixedLength<1 || fixedLength>maxBytes)
        {
            error="Fixed-length framing requires a positive length within the payload limit";
            return false;
        }
        return true;
    case CommunicationFrame::UntilCloseOrQuiet:
        return true;
    }
    error="Unsupported communication frame mode";
    return false;
}

bool XvCore::decodeModbusInt32(const QVector<quint16> &registers,int byteOrder,int wordOrder,
                               qint32 &value,QString &error)
{
    if(registers.size()!=2 || (byteOrder!=0 && byteOrder!=1) || (wordOrder!=0 && wordOrder!=1))
    {
        error="Modbus int32 decoding requires two registers and valid byte/word order";
        return false;
    }
    auto swapBytes=[](quint16 item)->quint16 { return quint16((item>>8)|(item<<8)); };
    quint16 high=byteOrder==1?swapBytes(registers.at(0)):registers.at(0);
    quint16 low=byteOrder==1?swapBytes(registers.at(1)):registers.at(1);
    if(wordOrder==1) std::swap(high,low);
    value=static_cast<qint32>((static_cast<quint32>(high)<<16)|low);
    return true;
}

bool XvCore::encodeModbusInt16(qint32 value,int byteOrder,QVector<quint16> &registers,QString &error)
{
    if(value<std::numeric_limits<qint16>::min() || value>std::numeric_limits<qint16>::max()
            || (byteOrder!=0 && byteOrder!=1))
    {
        error="Modbus int16 value or byte order is invalid";
        return false;
    }
    quint16 encoded=static_cast<quint16>(static_cast<qint16>(value));
    if(byteOrder==1) encoded=quint16((encoded>>8)|(encoded<<8));
    registers={encoded};
    return true;
}

std::shared_ptr<IHttpJsonTransport> XvCore::createHttpJsonTransport()
{ return std::make_shared<HttpJsonQtTransport>(); }
std::shared_ptr<ITcpTextTransport> XvCore::createTcpTextTransport()
{ return std::make_shared<TcpTextQtTransport>(); }
std::shared_ptr<IUdpTextTransport> XvCore::createUdpTextTransport()
{ return std::make_shared<UdpTextQtTransport>(); }
std::shared_ptr<ISerialDataTransport> XvCore::createSerialDataTransport()
{ return std::make_shared<SerialDataQtTransport>(); }
std::shared_ptr<IModbusRegisterTransport> XvCore::createModbusRegisterTransport()
{ return std::make_shared<ModbusRegisterQtTransport>(); }
