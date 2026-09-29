# VisionMaster Detect Record

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## 1. Scope / Trigger

Use this contract when changing the canonical `DetectRecord` operator, its four
VisionMaster presets, SQLite schema, repository connection lifecycle, record UI,
or temporary-database tests. The SQLite file is operator business data. It does
not replace project/flow XML and does not turn `XvData` into an ORM layer.

The implementation lives under
`XVision/XvFuncCollection/XvFuncSystem/Record/`. SQL is owned by
`DetectRecordRepository`; the operator converts between repository values and
`XDetectRecord`; the widget only edits operator parameters and subscriptions.

## 2. Signatures

The canonical operator has one persisted enum property:

```cpp
class DetectRecord : public XvFunc {
    Q_PROPERTY(DetectRecord::Mode mode READ mode WRITE setMode)
public:
    enum Mode { Record=0, ClassRecord=1, ObjectRecord=2, HasRecord=3 };
    QStringList persistentPropertyNames() const override { return {"mode"}; }
};
```

The repository boundary is value-only and synchronous:

```cpp
bool append(path, busyTimeoutMs, record, error);
bool queryByCategory(path, busyTimeoutMs, category, limit, records, error);
bool queryByObject(path, busyTimeoutMs, objectId, limit, records, error);
bool exists(path, busyTimeoutMs, category, objectId, found, error);
```

Schema version 1 is selected by `PRAGMA user_version = 1` and owns the exact
columns `id`, `record_id`, `category`, `object_id`, `timestamp_ms`, `outcome`,
and `details_json`. It also owns the unique record-ID index and category/object
time-order indexes declared in `DetectRecordRepository.cpp`.

## 3. Contracts

The preset map is exact:

| Alias | Mode | Operation |
| --- | --- | --- |
| `DetectRecordNodeData` | `Record` | append one record |
| `ClassDetectRecordNodeData` | `ClassRecord` | category query |
| `ObjectDetectRecordNodeData` | `ObjectRecord` | object query |
| `HasDetectRecordNodeData` | `HasRecord` | optional category/object existence query |

Persistent scalar parameters are `databasePath`, `busyTimeoutMs`, `category`,
`objectId`, `useRecordInput`, `recordId`, `timestampMs`, `outcome`, `detailsJson`,
and `limit`. `record` is an `XDetectRecord` runtime parameter: it is omitted as a
direct XML value and may be restored through a typed subscription. Results
`records`, `latestRecord`, `recordCount`, `exists`, and `insertedRecordId` are
always transient.

`Record` requires nonempty category/object values and either the subscribed
record or direct scalar fields. `ClassRecord` and `ObjectRecord` return at most
`limit` rows ordered by `timestamp_ms DESC, id DESC`. `HasRecord` requires at
least one nonempty filter and combines two supplied filters with `AND`; its count
is `0` or `1`. Empty queries publish an empty list, false existence, and the
valid neutral latest record.

Each repository call creates a unique named QSQLITE connection, uses it only in
the calling thread, destroys all queries and database handles, then calls
`QSqlDatabase::removeDatabase`. Values are prepared/bound. Append and initial
schema creation are separate transactions. The database parent directory must
already exist. Windows deployment copies `qsqlite.dll` (Release) or
`qsqlited.dll` (Debug) to `Bin[/D]/sqldrivers`.

## 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Empty path, nonexistent parent, or existing non-file path | `Error`; no result change; do not create directories |
| `busyTimeoutMs` outside `0..60000` | Reject before opening a connection |
| Query limit outside `1..10000` | Reject before opening a connection |
| Missing required category/object/filter | Reject with a nonempty run message |
| Empty/oversized ID, category, object, or outcome | Reject before SQL execution |
| Direct timestamp fractional, negative, non-finite, or above exact-double range | Reject before conversion to `qint64` |
| Details not a JSON object or above 1 MiB compact UTF-8 | Reject before append/result publication |
| QSQLITE unavailable or database open/configuration fails | `Error` with driver context; remove any named connection |
| Schema version above 1 or malformed version-1 table/index | Reject; do not guess or overwrite the schema |
| Version 0 new/empty database | Create and validate the complete schema in one transaction |
| Duplicate `record_id`, lock/busy, insert, or commit failure | Roll back append; preserve the previous operator results |
| Stored row has invalid timestamp/text/JSON | Abort the whole query; publish no partial list |

## 5. Good / Base / Bad Cases

- Good: append three records, query category and object filters, obtain stable
  time/row-ID order, and confirm combined existence without leaked connections.
- Base: query a valid empty database and publish zero records, false existence,
  empty inserted ID, and the neutral latest record.
- Bad: append a duplicate ID or query malformed stored JSON; return `Error`, keep
  every result from the prior successful run, and leave the database transaction
  and connection registry clean.

## 6. Tests Required

`XvDetectRecordTests` uses `QTemporaryDir` and the real QSQLITE driver. Assert:

- schema version/table creation and records surviving reopen;
- direct and typed `XDetectRecord` append, compact JSON round-trip, and unique IDs;
- category/object stable ordering, query limits, empty query, and all existence filters;
- duplicate append rollback, malformed schema/stored JSON, invalid path/config,
  path isolation, failure result preservation, and no `xvision_detect_record_*`
  connection after each operation.

`XvCorePersistenceTests` must assert the default entry and three non-default presets normalize to `DetectRecord`,
mode/scalar settings survive project and flow round-trip, a `record <- latestRecord`
subscription survives, and no transient record/result value is serialized.
`XvSystemPluginTests` must assert the current 39 roles, 84 unique presets, and the
four exact mode values. The matrix/catalog verifier remains at exactly 123 entries.

## 7. Wrong vs Correct

```cpp
// Wrong: cache one default connection and expose SQL rows to the widget.
QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE");
widget->runQuery(db,"SELECT ... '"+userFilter+"'");

// Correct: the operator passes validated values to the repository; each call
// owns one named connection and binds external values.
QList<DetectRecordRepository::Record> candidate;
if(!repository.queryByCategory(path,timeout,category,limit,candidate,error))
    return EXvFuncRunStatus::Error;
// Convert all rows first, then publish the complete result candidate.
```
