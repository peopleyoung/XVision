# VisionMaster Presets and Structured Flow Execution

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## Scenario: Canonical Presets, Port Links, and Dynamic DAG Control

### 1. Scope / Trigger

Use this contract when adding a VisionMaster creation alias, changing plugin operator registration,
adding an output port, modifying `<Link>` XML, changing flow scheduling, or implementing a
conditional/repeat operator. The persisted graph remains a DAG; repeat is a bounded scheduler
operation over a DAG subgraph, not a persisted back edge.

### 2. Signatures

```cpp
bool XvFuncAssembly::registerPlugin(const QList<QMetaObject> &functions,
                                    const QList<XvFuncPreset> &presets,
                                    QString *error=nullptr);

virtual QStringList XvFunc::outputPorts() const;
bool XvFunc::addSonFunc(XvFunc *child,const QString &port);
bool XvFunc::setSonFuncPort(XvFunc *child,const QString &port);
QString XvFunc::sonFuncPort(XvFunc *child) const;

virtual XvExecutionDirective XvFunc::executionDirective() const;
virtual bool XvFunc::prepareIteration(int iteration,QString &error);
```

```xml
<Link from="source-id" to="target-id"/>
<Link from="branch-id" to="target-id" fromPort="true"/>
```

### 3. Contracts

- A plugin registers its canonical metaobjects and presets atomically. Every metaobject must create
  an `XvFunc`; roles and aliases are non-empty, trimmed, and unique across both registries.
- A preset targets a canonical role, writes only writable names returned by
  `persistentPropertyNames()`, and uses exact supported scalar types for properties and parameters.
  Registration applies and reads back every value on a disposable candidate before committing.
- Alias discovery uses alias display/type/icon metadata, but alias creation immediately returns the
  canonical object. `funcRole()` and project XML never contain the alias.
- Ordinary operators declare exactly `default`. Port names are non-empty, trimmed, and unique.
  A stored parent/child link and its port map update together; duplicate `(from,to)` links are
  forbidden even when the requested port differs.
- XML v1 accepts optional `fromPort`. Absence means `default`; writers omit `default` and write every
  non-default port. Unknown attributes, unknown ports, self-links, duplicates, and cycles reject the
  detached load candidate. Flow-import ID rewriting leaves `fromPort` unchanged.
- The scheduler resolves every edge as selected or skipped. A node runs only after all internal
  incoming edges resolve and at least one is selected. If all are skipped, the node does not run and
  propagates skipped state. Ready nodes are ordered by persistent function ID.
- `Continue` selects every declared output port. `SelectPorts` contains unique declared names.
  Interrupting operator errors and directive errors stop the context with flow error status.
- `Repeat` names distinct declared `body` and `done` ports and uses an iteration count in
  `[0, XvExecutionDirective::MaximumIterationCount]`. Both ports require links.
- A repeat body is the closed descendant subgraph reached through `body`, stopping before direct
  `done` targets. Every body node must reach a done barrier and may not have an external parent other
  than the repeat entry. The outer context skips this body, executes it in a fresh child context per
  iteration, then selects only `done`.
- `prepareIteration()` publishes index/value/image results before each body context. It then emits a
  result-update notification so existing parameter subscriptions read the current iteration value.
  Nested repeat uses the same child-context contract. Stop and interrupting body errors abort the
  remaining iterations.
- `ConditionalFlow` owns `true/false` ports. `LoopFlow` owns `body/done`, supports exclusive-end
  `For` and `ForeachImages`, and keeps image lists/current images transient. Scalar loop mode/start/
  end/step configuration remains deterministic XML data.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Duplicate role/alias or unknown preset target | Reject the complete plugin batch; live maps unchanged |
| Unknown/non-persistent preset field or wrong type | Reject the complete batch with field diagnostic |
| Alias-created project save | Persist canonical role and effective preset configuration only |
| Missing `fromPort` on ordinary link | Load as `default` and preserve old XML bytes |
| Missing `fromPort` on a source without `default` | Reject the detached candidate |
| Unknown port, duplicate link, asymmetric topology, cycle | Reject before execution |
| Join has unresolved incoming edge | Do not run or skip the join yet |
| Every incoming edge is skipped | Skip node and propagate skipped state |
| Repeat count is negative or above the hard maximum | Flow error; no body execution |
| Repeat lacks body/done link or body escapes its barrier | Flow error; no partial outer continuation |
| Zero repeat count | Do not run body; select done exactly once |
| Stop or interrupting error inside body | Abort remaining iterations and do not run done |

### 5. Good / Base / Bad Cases

- Good: `ImageAcquisition.Dir` creates `ImageAcquisition` with directory mode, saves as
  `ImageAcquisition`, and loads without alias knowledge.
- Base: an old ordinary link has only `from/to`; a normal fork selects both branches and a join runs
  once after both finish.
- Bad: persist an alias as `role`, register an alias before its canonical role exists, treat the
  first arriving join edge as readiness, or implement loop by adding a graph cycle.

### 6. Tests Required

- `systemplugin` verifies canonical roles, all advertised presets, effective creation values, and
  failed-batch rollback. Defaults use ConditionalFlow and LoopFlow; the non-default iteration entry is LoopFlow.ForeachImages.
- `projectxml` verifies alias normalization, default/non-default port XML, strict invalid-port load,
  deterministic fork/join order, skipped propagation, both conditional branches, bounded for and
  foreach loops, zero count, negative step, zero step, maximum overflow, body error, stop, nested
  contexts, iteration-result subscriptions, and transient image persistence.
- Run `git diff --check`, `operator_catalog`, `operator_catalog_rejections`, and the SDK fixture.
  A missing Qt SDK must be recorded and must not be reported as runtime validation.
- Update the current catalog and its runtime checks together; the old compatibility matrix is historical.

### 7. Wrong vs Correct

```cpp
// Wrong: alias becomes a second persistence identity.
function->_funcRole="ImageAcquisition.Dir";

// Correct: use alias only to select a validated canonical preset.
XvFunc *function=assembly->createNewXvFunc("ImageAcquisition.Dir");
Q_ASSERT(function->funcRole()=="ImageAcquisition");
```

```cpp
// Wrong: run a join as soon as any parent happens to finish.
queue.push(join);

// Correct: wait until every incoming edge is Selected or Skipped.
if(allIncomingResolved && anyIncomingSelected) ready.append(join);
```
