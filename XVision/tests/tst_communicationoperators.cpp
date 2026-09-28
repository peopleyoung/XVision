#include <QtTest>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QUdpSocket>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <thread>

#include "CommunicationTransport.h"
#include "HttpJson.h"
#include "ModbusRegister.h"
#include "SerialData.h"
#include "TcpText.h"
#include "UdpText.h"

using namespace XvCore;

namespace
{
template<typename T>
T *parameter(XvFunc &function,const QString &name)
{
    return dynamic_cast<T *>(function.getParamsByName(name));
}

template<typename T>
T *result(XvFunc &function,const QString &name)
{
    return dynamic_cast<T *>(function.getResultsByName(name));
}

class BlockingTcpServer
{
public:
    explicit BlockingTcpServer(const std::function<void(QTcpSocket *)> &handler)
        :m_portFuture(m_portPromise.get_future())
    {
        m_thread=std::thread([this,handler]()
        {
            QTcpServer server;
            if(!server.listen(QHostAddress::LocalHost,0))
            {
                m_portPromise.set_value(0);
                return;
            }
            m_portPromise.set_value(server.serverPort());
            if(!server.waitForNewConnection(3000)) return;
            QTcpSocket *socket=server.nextPendingConnection();
            if(!socket) return;
            handler(socket);
            socket->disconnectFromHost();
            if(socket->state()!=QAbstractSocket::UnconnectedState)
                socket->waitForDisconnected(1000);
        });
    }

    ~BlockingTcpServer() { join(); }
    quint16 port() { return m_portFuture.get(); }
    void join() { if(m_thread.joinable()) m_thread.join(); }

private:
    std::promise<quint16> m_portPromise;
    std::future<quint16> m_portFuture;
    std::thread m_thread;
};

QByteArray readHttpRequest(QTcpSocket *socket)
{
    QByteArray request;
    while(!request.contains("\r\n\r\n") && socket->waitForReadyRead(1000))
        request.append(socket->readAll());
    const int headerEnd=request.indexOf("\r\n\r\n");
    int contentLength=0;
    if(headerEnd>=0)
    {
        const QList<QByteArray> lines=request.left(headerEnd).split('\n');
        for(const QByteArray &line:lines)
        {
            if(line.trimmed().toLower().startsWith("content-length:"))
                contentLength=line.mid(line.indexOf(':')+1).trimmed().toInt();
        }
        while(request.size()-(headerEnd+4)<contentLength && socket->waitForReadyRead(1000))
            request.append(socket->readAll());
        request.append(socket->readAll());
    }
    return request;
}

QByteArray httpResponse(int status,const QByteArray &reason,const QByteArray &body)
{
    return "HTTP/1.1 "+QByteArray::number(status)+" "+reason+"\r\n"
            +"Content-Type: application/json\r\nContent-Length: "
            +QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body;
}

class FakeSerialTransport final:public ISerialDataTransport
{
public:
    bool execute(const SerialDataRequest &request,const CommunicationCancellation &,
                 SerialDataResponse &response,QString &error) override
    {
        ++calls;
        last=request;
        if(!succeed) { error=failure; return false; }
        response=next;
        if(request.write) response.bytesTransferred=request.payload.size();
        return true;
    }

    int calls=0;
    bool succeed=true;
    QString failure="serial failure";
    SerialDataRequest last;
    SerialDataResponse next;
};

class FakeModbusTransport final:public IModbusRegisterTransport
{
public:
    bool execute(const ModbusRegisterRequest &request,const CommunicationCancellation &,
                 ModbusRegisterResponse &response,QString &error) override
    {
        ++calls;
        last=request;
        if(!succeed) { error=failure; return false; }
        response=next;
        return true;
    }

    int calls=0;
    bool succeed=true;
    QString failure="modbus failure";
    ModbusRegisterRequest last;
    ModbusRegisterResponse next;
};

class CancellableHttpTransport final:public IHttpJsonTransport
{
public:
    CancellableHttpTransport():startedFuture(started.get_future()) { }

    bool execute(const HttpJsonRequest &,const CommunicationCancellation &cancellation,
                 HttpJsonResponse &,QString &error) override
    {
        started.set_value();
        while(!cancellation.requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error="cancelled";
        return false;
    }

    std::promise<void> started;
    std::future<void> startedFuture;
};

class CancellableTcpTransport final:public ITcpTextTransport
{
public:
    CancellableTcpTransport():startedFuture(started.get_future()) { }

    bool execute(const TextStreamRequest &,const CommunicationCancellation &cancellation,
                 TextStreamResponse &,QString &error) override
    {
        started.set_value();
        while(!cancellation.requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error="cancelled";
        return false;
    }

    std::promise<void> started;
    std::future<void> startedFuture;
};

class CancellableUdpTransport final:public IUdpTextTransport
{
public:
    CancellableUdpTransport():startedFuture(started.get_future()) { }

    bool execute(const UdpTextRequest &,const CommunicationCancellation &cancellation,
                 UdpTextResponse &,QString &error) override
    {
        started.set_value();
        while(!cancellation.requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error="cancelled";
        return false;
    }

    std::promise<void> started;
    std::future<void> startedFuture;
};

class CancellableSerialTransport final:public ISerialDataTransport
{
public:
    CancellableSerialTransport():startedFuture(started.get_future()) { }

    bool execute(const SerialDataRequest &,const CommunicationCancellation &cancellation,
                 SerialDataResponse &,QString &error) override
    {
        started.set_value();
        while(!cancellation.requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error="cancelled";
        return false;
    }

    std::promise<void> started;
    std::future<void> startedFuture;
};

class CancellableModbusTransport final:public IModbusRegisterTransport
{
public:
    CancellableModbusTransport():startedFuture(started.get_future()) { }

    bool execute(const ModbusRegisterRequest &,const CommunicationCancellation &cancellation,
                 ModbusRegisterResponse &,QString &error) override
    {
        started.set_value();
        while(!cancellation.requested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error="cancelled";
        return false;
    }

    std::promise<void> started;
    std::future<void> startedFuture;
};
}

class CommunicationOperatorTest:public QObject
{
    Q_OBJECT

private slots:
    void codecsAndRegisterOrders()
    {
        QByteArray bytes;
        QString text;
        QString error;
        QVERIFY(encodeCommunicationText(QString::fromUtf8("A\xc3\xa9"),CommunicationEncoding::Utf8,
                                        bytes,error));
        QCOMPARE(bytes,QByteArray("A\xc3\xa9"));
        QVERIFY(decodeCommunicationText(bytes,CommunicationEncoding::Utf8,text,error));
        QCOMPARE(text,QString::fromUtf8("A\xc3\xa9"));
        QVERIFY(!encodeCommunicationText(QString::fromUtf8("\xe6\xb1\x89"),CommunicationEncoding::Latin1,
                                         bytes,error));
        QVERIFY(!decodeCommunicationText(QByteArray("\xff"),CommunicationEncoding::Utf8,text,error));

        qint32 value=0;
        QVERIFY(decodeModbusInt32({0x1234,0x5678},0,0,value,error));
        QCOMPARE(value,qint32(0x12345678));
        QVERIFY(decodeModbusInt32({0x3412,0x7856},1,0,value,error));
        QCOMPARE(value,qint32(0x12345678));
        QVERIFY(decodeModbusInt32({0x5678,0x1234},0,1,value,error));
        QCOMPARE(value,qint32(0x12345678));
        QVERIFY(decodeModbusInt32({0x8000,0x0000},0,0,value,error));
        QCOMPARE(value,std::numeric_limits<qint32>::min());
        QVERIFY(decodeModbusInt32({0xffff,0xffff},0,0,value,error));
        QCOMPARE(value,qint32(-1));
        QVector<quint16> registers;
        QVERIFY(encodeModbusInt16(-2,0,registers,error));
        QCOMPARE(registers,QVector<quint16>({0xfffe}));
        QVERIFY(encodeModbusInt16(-2,1,registers,error));
        QCOMPARE(registers,QVector<quint16>({0xfeff}));
        QVERIFY(!encodeModbusInt16(40000,0,registers,error));
    }

    void httpLoopbackStatusLimitsAndRollback()
    {
        HttpJson function;
        BlockingTcpServer readServer([](QTcpSocket *socket)
        {
            readHttpRequest(socket);
            const QByteArray response=httpResponse(200,"OK","{\"ok\":true}");
            socket->write(response.left(12));
            socket->waitForBytesWritten(1000);
            QThread::msleep(10);
            socket->write(response.mid(12));
            socket->waitForBytesWritten(1000);
        });
        const quint16 readPort=readServer.port();
        QVERIFY(readPort>0);
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/read").arg(readPort));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        readServer.join();
        QCOMPARE(result<XInt>(function,"statusCode")->value(),200);
        QVERIFY(result<XJsonValue>(function,"json")->value().object().value("ok").toBool());

        std::promise<QByteArray> requestPromise;
        auto requestFuture=requestPromise.get_future();
        BlockingTcpServer writeServer([&requestPromise](QTcpSocket *socket)
        {
            requestPromise.set_value(readHttpRequest(socket));
            socket->write(httpResponse(201,"Created","{\"saved\":true}"));
            socket->waitForBytesWritten(1000);
        });
        function.setMode(HttpJson::Write);
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/write").arg(writeServer.port()));
        parameter<XString>(function,"headersJson")->setValue("{\"X-Test\":\"yes\"}");
        parameter<XBool>(function,"useJsonInput")->setValue(true);
        parameter<XJsonValue>(function,"jsonInput")->setValue(
                    QJsonDocument(QJsonObject{{"name","x"}}));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        writeServer.join();
        const QByteArray posted=requestFuture.get();
        QVERIFY(posted.startsWith("POST /write HTTP/1.1"));
        QVERIFY(posted.toLower().contains("x-test: yes\r\n"));
        QVERIFY(posted.contains("{\"name\":\"x\"}"));
        QCOMPARE(result<XInt>(function,"statusCode")->value(),201);

        std::promise<QByteArray> directRequestPromise;
        auto directRequestFuture=directRequestPromise.get_future();
        BlockingTcpServer directWriteServer([&directRequestPromise](QTcpSocket *socket)
        {
            directRequestPromise.set_value(readHttpRequest(socket));
            socket->write(httpResponse(200,"OK","{\"direct\":true}"));
            socket->waitForBytesWritten(1000);
        });
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/direct").arg(directWriteServer.port()));
        parameter<XBool>(function,"useJsonInput")->setValue(false);
        parameter<XString>(function,"payloadJson")->setValue("{\"direct\":1}");
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        directWriteServer.join();
        QVERIFY(directRequestFuture.get().contains("{\"direct\":1}"));
        const int statusBeforeInvalidHeader=result<XInt>(function,"statusCode")->value();
        parameter<XString>(function,"headersJson")->setValue("{not-json");
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XInt>(function,"statusCode")->value(),statusBeforeInvalidHeader);
        parameter<XString>(function,"headersJson")->setValue("{\"X-Test\":\"yes\"}");

        BlockingTcpServer failServer([](QTcpSocket *socket)
        {
            readHttpRequest(socket);
            socket->write(httpResponse(404,"Not Found","{\"error\":\"missing\"}"));
            socket->waitForBytesWritten(1000);
        });
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/missing").arg(failServer.port()));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Fail);
        failServer.join();
        QCOMPARE(result<XInt>(function,"statusCode")->value(),404);
        const QString previousText=result<XString>(function,"responseText")->value();

        BlockingTcpServer malformedServer([](QTcpSocket *socket)
        {
            readHttpRequest(socket);
            socket->write(httpResponse(200,"OK","not-json"));
            socket->waitForBytesWritten(1000);
        });
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/bad").arg(malformedServer.port()));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        malformedServer.join();
        QCOMPARE(result<XInt>(function,"statusCode")->value(),404);
        QCOMPARE(result<XString>(function,"responseText")->value(),previousText);

        BlockingTcpServer oversizedServer([](QTcpSocket *socket)
        {
            readHttpRequest(socket);
            socket->write(httpResponse(200,"OK","{\"long\":\"abcdefghijk\"}"));
            socket->waitForBytesWritten(1000);
        });
        function.setMode(HttpJson::Read);
        parameter<XString>(function,"headersJson")->setValue("{}");
        parameter<XInt>(function,"maxResponseBytes")->setValue(8);
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/large").arg(oversizedServer.port()));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        oversizedServer.join();
        QCOMPARE(result<XInt>(function,"statusCode")->value(),404);

        BlockingTcpServer timeoutServer([](QTcpSocket *socket)
        {
            readHttpRequest(socket);
            QThread::msleep(100);
        });
        parameter<XInt>(function,"maxResponseBytes")->setValue(1024);
        parameter<XInt>(function,"timeoutMs")->setValue(20);
        parameter<XString>(function,"url")->setValue(
                    QString("http://127.0.0.1:%1/timeout").arg(timeoutServer.port()));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        timeoutServer.join();
        QCOMPARE(result<XInt>(function,"statusCode")->value(),404);
    }

    void tcpLoopbackFramesWritesAndRollback()
    {
        TcpText function;
        BlockingTcpServer delimiterServer([](QTcpSocket *socket)
        {
            socket->write("h\xc3",2);
            socket->waitForBytesWritten(1000);
            QThread::msleep(10);
            socket->write("\xa9\nignored",9);
            socket->waitForBytesWritten(1000);
        });
        parameter<XString>(function,"host")->setValue("127.0.0.1");
        parameter<XInt>(function,"port")->setValue(delimiterServer.port());
        parameter<XString>(function,"delimiter")->setValue("\n");
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        delimiterServer.join();
        QCOMPARE(result<XString>(function,"text")->value(),QString::fromUtf8("h\xc3\xa9"));

        BlockingTcpServer fixedServer([](QTcpSocket *socket)
        {
            socket->write("abcXYZ");
            socket->waitForBytesWritten(1000);
        });
        parameter<XInt>(function,"port")->setValue(fixedServer.port());
        parameter<XInt>(function,"frameMode")->setValue(1);
        parameter<XInt>(function,"fixedLength")->setValue(3);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        fixedServer.join();
        QCOMPARE(result<XString>(function,"text")->value(),QString("abc"));

        BlockingTcpServer closeServer([](QTcpSocket *socket)
        {
            socket->write("bye");
            socket->waitForBytesWritten(1000);
        });
        parameter<XInt>(function,"port")->setValue(closeServer.port());
        parameter<XInt>(function,"frameMode")->setValue(2);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        closeServer.join();
        QCOMPARE(result<XString>(function,"text")->value(),QString("bye"));

        std::promise<QByteArray> writtenPromise;
        auto writtenFuture=writtenPromise.get_future();
        BlockingTcpServer writeServer([&writtenPromise](QTcpSocket *socket)
        {
            QByteArray data;
            while(!data.contains('\n') && socket->waitForReadyRead(1000)) data.append(socket->readAll());
            data.append(socket->readAll());
            writtenPromise.set_value(data);
        });
        function.setMode(TcpText::Write);
        parameter<XInt>(function,"port")->setValue(writeServer.port());
        parameter<XInt>(function,"encoding")->setValue(1);
        parameter<XString>(function,"payload")->setValue(QString::fromLatin1("\xe9"));
        parameter<XBool>(function,"appendDelimiter")->setValue(true);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        writeServer.join();
        QCOMPARE(writtenFuture.get(),QByteArray("\xe9\n",2));
        const QString previous=result<XString>(function,"text")->value();
        parameter<XInt>(function,"maxBytes")->setValue(2);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XString>(function,"text")->value(),previous);

        BlockingTcpServer timeoutServer([](QTcpSocket *) { QThread::msleep(100); });
        function.setMode(TcpText::Read);
        parameter<XInt>(function,"port")->setValue(timeoutServer.port());
        parameter<XInt>(function,"maxBytes")->setValue(1024);
        parameter<XInt>(function,"timeoutMs")->setValue(20);
        parameter<XInt>(function,"frameMode")->setValue(0);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        timeoutServer.join();
        QCOMPARE(result<XString>(function,"text")->value(),previous);
    }

    void udpLoopbackReadWriteAndTimeout()
    {
        std::promise<quint16> portPromise;
        std::promise<QByteArray> dataPromise;
        auto portFuture=portPromise.get_future();
        auto dataFuture=dataPromise.get_future();
        std::thread receiver([&]()
        {
            QUdpSocket socket;
            if(!socket.bind(QHostAddress::LocalHost,0))
            {
                portPromise.set_value(0);
                dataPromise.set_value(QByteArray());
                return;
            }
            portPromise.set_value(socket.localPort());
            if(!socket.waitForReadyRead(2000)) { dataPromise.set_value(QByteArray()); return; }
            QByteArray data;
            data.resize(static_cast<int>(socket.pendingDatagramSize()));
            socket.readDatagram(data.data(),data.size());
            dataPromise.set_value(data);
        });
        UdpText writer;
        writer.setMode(UdpText::Write);
        parameter<XString>(writer,"remoteHost")->setValue("127.0.0.1");
        parameter<XInt>(writer,"remotePort")->setValue(portFuture.get());
        parameter<XString>(writer,"payload")->setValue(QString::fromUtf8("h\xc3\xa9"));
        QCOMPARE(writer.runXvFunc(),EXvFuncRunStatus::Ok);
        receiver.join();
        QCOMPARE(dataFuture.get(),QString::fromUtf8("h\xc3\xa9").toUtf8());

        QUdpSocket reservation;
        QVERIFY(reservation.bind(QHostAddress::LocalHost,0));
        const quint16 readPort=reservation.localPort();
        reservation.close();
        std::thread sender([readPort]()
        {
            QThread::msleep(30);
            QUdpSocket socket;
            socket.writeDatagram("reply",QHostAddress::LocalHost,readPort);
        });
        UdpText reader;
        parameter<XString>(reader,"localAddress")->setValue("127.0.0.1");
        parameter<XInt>(reader,"localPort")->setValue(readPort);
        QCOMPARE(reader.runXvFunc(),EXvFuncRunStatus::Ok);
        sender.join();
        QCOMPARE(result<XString>(reader,"text")->value(),QString("reply"));
        QVERIFY(!result<XString>(reader,"senderAddress")->value().isEmpty());
        const QString previous=result<XString>(reader,"text")->value();
        parameter<XInt>(reader,"timeoutMs")->setValue(20);
        QCOMPARE(reader.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XString>(reader,"text")->value(),previous);

        QUdpSocket oversizedReservation;
        QVERIFY(oversizedReservation.bind(QHostAddress::LocalHost,0));
        const quint16 oversizedPort=oversizedReservation.localPort();
        oversizedReservation.close();
        std::thread oversizedSender([oversizedPort]()
        {
            QThread::msleep(30);
            QUdpSocket socket;
            socket.writeDatagram("12345678",QHostAddress::LocalHost,oversizedPort);
        });
        parameter<XInt>(reader,"localPort")->setValue(oversizedPort);
        parameter<XInt>(reader,"timeoutMs")->setValue(1000);
        parameter<XInt>(reader,"maxBytes")->setValue(4);
        QCOMPARE(reader.runXvFunc(),EXvFuncRunStatus::Error);
        oversizedSender.join();
        QCOMPARE(result<XString>(reader,"text")->value(),previous);
    }

    void serialModesValidationAndRollback()
    {
        auto transport=std::make_shared<FakeSerialTransport>();
        SerialData function;
        function.setTransport(transport);
        function.setMode(SerialData::ReadText);
        transport->next.data="hello";
        transport->next.bytesTransferred=5;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XString>(function,"text")->value(),QString("hello"));
        QVERIFY(transport->last.textMode);
        QVERIFY(!transport->last.write);

        function.setMode(SerialData::ReadBytes);
        parameter<XInt>(function,"frameMode")->setValue(1);
        parameter<XInt>(function,"fixedLength")->setValue(2);
        transport->next.data=QByteArray("\x01\xfe",2);
        transport->next.bytesTransferred=2;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XString>(function,"text")->value(),QString("01 fe"));
        QVERIFY(!transport->last.textMode);
        QCOMPARE(transport->last.frame,CommunicationFrame::FixedLength);
        QCOMPARE(transport->last.fixedLength,2);

        parameter<XInt>(function,"frameMode")->setValue(2);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(transport->last.frame,CommunicationFrame::UntilCloseOrQuiet);

        function.setMode(SerialData::WriteBytes);
        parameter<XString>(function,"payload")->setValue("01 ff 7A");
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(transport->last.payload,QByteArray::fromHex("01ff7a"));
        parameter<XBool>(function,"useByteInput")->setValue(true);
        parameter<XByteArray>(function,"byteInput")->setValue(QByteArray("\x00\x01",2));
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(transport->last.payload,QByteArray("\x00\x01",2));

        function.setMode(SerialData::WriteText);
        parameter<XString>(function,"payload")->setValue("serial-text");
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(transport->last.payload,QByteArray("serial-text"));
        QVERIFY(transport->last.textMode);

        const QByteArray previous=result<XByteArray>(function,"data")->value();
        transport->succeed=false;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XByteArray>(function,"data")->value(),previous);
        transport->succeed=true;
        function.setMode(SerialData::WriteBytes);
        parameter<XBool>(function,"useByteInput")->setValue(false);
        parameter<XString>(function,"payload")->setValue("0xz1");
        const int calls=transport->calls;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(transport->calls,calls);
        parameter<XInt>(function,"dataBits")->setValue(9);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
    }

    void modbusModesValidationAndRollback()
    {
        auto transport=std::make_shared<FakeModbusTransport>();
        transport->next.registers={0x1234,0x5678};
        ModbusRegister function;
        function.setTransport(transport);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QCOMPARE(result<XInt>(function,"value")->value(),int(0x12345678));
        QCOMPARE(result<XInt>(function,"registerCount")->value(),2);
        QVERIFY(!result<XBool>(function,"written")->value());
        QCOMPARE(transport->last.address,quint16(0));

        function.setMode(ModbusRegister::WriteInt16);
        parameter<XInt>(function,"value")->setValue(-2);
        transport->next.written=true;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Ok);
        QVERIFY(transport->last.write);
        QCOMPARE(transport->last.value,qint32(-2));
        QVERIFY(result<XBool>(function,"written")->value());
        const int previous=result<XInt>(function,"value")->value();
        transport->succeed=false;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XInt>(function,"value")->value(),previous);
        transport->succeed=true;
        function.setMode(ModbusRegister::ReadInt32);
        transport->next.registers={0x1234};
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(result<XInt>(function,"value")->value(),previous);
        function.setMode(ModbusRegister::WriteInt16);
        parameter<XInt>(function,"value")->setValue(40000);
        const int calls=transport->calls;
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(transport->calls,calls);
        parameter<XInt>(function,"value")->setValue(0);
        parameter<XInt>(function,"unitId")->setValue(0);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(transport->calls,calls);
        parameter<XInt>(function,"unitId")->setValue(1);
        function.setMode(ModbusRegister::ReadInt32);
        parameter<XInt>(function,"address")->setValue(65535);
        QCOMPARE(function.runXvFunc(),EXvFuncRunStatus::Error);
        QCOMPARE(transport->calls,calls);
    }

    void cancellationRequestIsObserved()
    {
        auto transport=std::make_shared<CancellableHttpTransport>();
        HttpJson function;
        function.setTransport(transport);
        std::atomic_int status{int(EXvFuncRunStatus::Init)};
        std::thread runner([&]() { status.store(int(function.runXvFunc())); });
        transport->startedFuture.wait();
        const bool httpReleased=function.release();
        runner.join();
        QVERIFY(!httpReleased);
        QCOMPARE(status.load(),int(EXvFuncRunStatus::Fail));
        QVERIFY(function.release());

        auto tcpTransport=std::make_shared<CancellableTcpTransport>();
        TcpText tcp;
        tcp.setTransport(tcpTransport);
        std::thread tcpRunner([&]() { status.store(int(tcp.runXvFunc())); });
        tcpTransport->startedFuture.wait();
        const bool tcpReleased=tcp.release();
        tcpRunner.join();
        QVERIFY(!tcpReleased);
        QCOMPARE(status.load(),int(EXvFuncRunStatus::Fail));
        QVERIFY(tcp.release());

        auto udpTransport=std::make_shared<CancellableUdpTransport>();
        UdpText udp;
        parameter<XInt>(udp,"localPort")->setValue(9000);
        udp.setTransport(udpTransport);
        std::thread udpRunner([&]() { status.store(int(udp.runXvFunc())); });
        udpTransport->startedFuture.wait();
        const bool udpReleased=udp.release();
        udpRunner.join();
        QVERIFY(!udpReleased);
        QCOMPARE(status.load(),int(EXvFuncRunStatus::Fail));
        QVERIFY(udp.release());

        auto serialTransport=std::make_shared<CancellableSerialTransport>();
        SerialData serial;
        serial.setTransport(serialTransport);
        std::thread serialRunner([&]() { status.store(int(serial.runXvFunc())); });
        serialTransport->startedFuture.wait();
        const bool serialReleased=serial.release();
        serialRunner.join();
        QVERIFY(!serialReleased);
        QCOMPARE(status.load(),int(EXvFuncRunStatus::Fail));
        QVERIFY(serial.release());

        auto modbusTransport=std::make_shared<CancellableModbusTransport>();
        ModbusRegister modbus;
        modbus.setTransport(modbusTransport);
        std::thread modbusRunner([&]() { status.store(int(modbus.runXvFunc())); });
        modbusTransport->startedFuture.wait();
        const bool modbusReleased=modbus.release();
        modbusRunner.join();
        QVERIFY(!modbusReleased);
        QCOMPARE(status.load(),int(EXvFuncRunStatus::Fail));
        QVERIFY(modbus.release());
    }
};

QTEST_GUILESS_MAIN(CommunicationOperatorTest)
#include "tst_communicationoperators.moc"
