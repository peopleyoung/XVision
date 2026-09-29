# VisionMaster Communication Operators

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## 1. Scope / Trigger

Use this contract when changing `HttpJson`, `TcpText`, `UdpText`, `SerialData`,
`ModbusRegister`, their twelve VisionMaster presets, transport interfaces, framing,
cancellation, Qt communication dependencies, or communication tests. Implementations
live under `XVision/XvFuncCollection/XvFuncSystem/Communication/`.

Communication operators are synchronous `XvFunc` roles. Their production Qt device is
owned entirely by the flow worker thread for one run. The widget edits value parameters
only and must never own or access a socket, network reply, serial port, or Modbus client.

## 2. Signatures

The exact canonical modes are:

```cpp
HttpJson::Mode          { Read=0, Write=1 };
TcpText::Mode           { Read=0, Write=1 };
UdpText::Mode           { Read=0, Write=1 };
SerialData::Mode        { ReadBytes=0, ReadText=1, WriteBytes=2, WriteText=3 };
ModbusRegister::Mode    { ReadInt32=0, WriteInt16=1 };
```

All production and substitute transports use value-only boundaries:

```cpp
bool IHttpJsonTransport::execute(request,cancellation,response,error);
bool ITcpTextTransport::execute(request,cancellation,response,error);
bool IUdpTextTransport::execute(request,cancellation,response,error);
bool ISerialDataTransport::execute(request,cancellation,response,error);
bool IModbusRegisterTransport::execute(request,cancellation,response,error);
```

`CommunicationCancellation` contains a non-owning `const std::atomic_bool *`. Each
operator exposes a typed `setTransport(std::shared_ptr<I...>)`; null selects the Qt
production factory. `CommunicationOperatorBase` uses a second atomic flag to cover the
entire delegated `runXvFunc()` call. `release()` reads that flag, requests cancellation,
and returns false while running, then retains the base deletion contract after the run
ends. Do not use the base class's non-atomic run status for this cross-thread decision.

## 3. Contracts

The alias map is exact:

| Alias set | Canonical role and modes |
| --- | --- |
| `HttpReadJsonNodeData`, `HttpWriteJsonNodeData` | `HttpJson` 0, 1 |
| `TcpReadStringNodeData`, `TcpWriteStringNodeData` | `TcpText` 0, 1 |
| `UdpReadStringNodeData`, `UdpWriteStringNodeData` | `UdpText` 0, 1 |
| `SerialReadByteNodeData`, `SerialReadStringNodeData`, `SerialWriteByteNodeData`, `SerialWriteStringNodeData` | `SerialData` 0..3 |
| `IntReadableModbusNodeData`, `ShortWriteableModbusNodeData` | `ModbusRegister` 0, 1 |

HTTP read is GET and write is POST. Headers are a JSON object with string values; Host
and Content-Length remain Qt-owned. Direct/subscribed request and every response
document must be a JSON object or array. A valid decoded non-2xx response publishes
status/body and returns `Fail`; timeout, network, cap, or JSON errors publish nothing.

Text encoding is `Utf8=0` or `Latin1=1`, with invalid bytes/characters rejected. TCP
read framing is `Delimiter=0`, `FixedLength=1`, or `UntilCloseOrQuiet=2`; serial uses
the same values, with the last mode meaning one 50-ms quiet slice after initial data.
TCP writes may append a delimiter. UDP consumes exactly one datagram and never combines
packets. Deadlines do not reset on packets or state transitions, and polling slices are
at most 50 ms. TCP has explicit connect and operation deadlines; Modbus's single timeout
covers connection plus request.

Serial opens/closes one configured port per run. Direct byte writes accept only an even
count of hexadecimal digits after ASCII whitespace removal; subscribed byte writes use
`XByteArray`. Modbus uses Qt SerialBus Modbus TCP holding registers: read two registers
as signed int32, or write one signed int16. Byte order is big/little within each word;
word order is high/low word first for int32.

Only `mode` is a persistent property. All scalar endpoint/protocol/payload settings use
generic XML v1 persistence. `jsonInput`, `byteInput`, every `XJsonValue`/`XByteArray`
result, and all scalar results are transient; typed subscriptions remain persistent.

## 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Empty host/URL/port name, port outside `1..65535`, timeout outside `1..600000` | Reject before transport; `Error`; preserve results |
| Maximum outside `1..16 MiB`, or UDP maximum above 65507 | Reject before I/O |
| Invalid header/direct JSON, non-string header, reserved HTTP header | Reject before request |
| HTTP network/timeout/cancel/cap/invalid JSON | Abort in run thread; no result publication |
| Valid HTTP status outside `200..299` | Publish decoded response and status; return `Fail` |
| Invalid encoding or unrepresentable Latin-1 / invalid UTF-8 | Reject; preserve results |
| Empty delimiter, invalid fixed length, incomplete/oversized frame | `Error`; preserve results |
| TCP/UDP timeout, partial write, bind/connect/peer error | Close local socket; preserve results |
| Invalid serial enum, odd/non-hex payload, open/read/write error | Close local port; preserve results |
| Modbus unit outside `1..247`, bad span/order/int16 range, reply/protocol error | Disconnect client; preserve results |
| `release()` while running | Set cancellation flag, return false, adapter aborts its own device within 50 ms |
| Transport returns malformed register/result shape | Reject before result commit |

## 5. Good / Base / Bad Cases

- Good: a split HTTP/TCP response crosses several packets, completes at the configured
  boundary, decodes once, and atomically publishes typed plus scalar results.
- Base: an empty UDP/text payload is one valid datagram/write; byte writes may transmit
  an empty array. An empty HTTP response is invalid because response JSON is strict.
- Bad: a response exceeds its cap or cancellation arrives during a wait. The run-thread
  adapter aborts/closes its resource, returns a nonempty message, and every previous
  result remains unchanged.

## 6. Tests Required

`XvCommunicationOperatorTests` must assert:

- strict UTF-8/Latin-1 and Modbus signed byte/word conversion helpers;
- local HTTP GET/POST, headers/typed JSON, split responses, non-2xx `Fail`, malformed
  JSON, caps, timeout/cancellation, and rollback;
- local TCP delimiter/fixed/close reads, partial packets, complete delimiter writes,
  limits/timeouts/cancellation, and rollback;
- local UDP one-datagram read/write, sender metadata, encoding, limit/timeout, rollback;
- injected serial four modes, hex/typed bytes, config/framing, transport failure,
  cancellation, lifecycle call count, and rollback;
- injected Modbus signed boundaries, address/unit/order, read/write confirmation,
  transport/cancellation failures, malformed registers, and rollback.

`XvCorePersistenceTests` asserts all twelve aliases, scalar settings, canonical roles,
JSON/byte typed subscriptions, flow export/import, and transient omission.
`XvSystemPluginTests` asserts exactly 39 roles and 84 unique presets. Windows CTest and
runtime verification require `communication_operators` and Qt6 Network, SerialPort, and
SerialBus DLLs.

## 7. Wrong vs Correct

```cpp
// Wrong: release is called on another thread and directly closes a run-thread socket.
bool TcpText::release() {
    m_socket->abort();
    return true;
}

// Correct: release changes only the atomic request. The executing adapter owns cleanup.
bool CommunicationOperatorBase::release() {
    if (m_communicationRunning.load(std::memory_order_acquire)) {
        m_cancelRequested.store(true,std::memory_order_relaxed);
        return false;
    }
    return XvFunc::release();
}
```
