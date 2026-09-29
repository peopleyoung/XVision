# CPU Runtime Performance Contracts

## 1. Scope / Trigger
CPU ONNX lifecycle/preprocessing, manual execution, stop/edit protection and GUI log delivery. HALCON roles/runtime retired 2026-09-28; 39 roles/84 presets.

## 2. Signatures
- RetXv XvFlow::runFunctionOnce(const QString &functionId): managed XConcurrent execution of one member function only.
- stop(): cancellation request; isRunning() remains true until work completes. wait(0): indefinite wait. Loop intervals are wakeable.
- NOnnxBase::executeModel: immutable shared session snapshot; per-session serialized execution.
- XVISION_ONNX_THREADS: empty selects min(4,max(1,idealThreadCount-1)); integer 1..32 otherwise.

## 3. Contracts
Full SHA-256 before/after model configure/load and on save. Runtime uses native file ID/size/mtime/change time (Windows FILE_BASIC_INFO/Linux nanosecond ctime); changed stamps rehash; unsupported hosts always hash. Never trust size/mtime alone. Candidate metadata commit atomic; UI reads cached lists without waiting on inference. CPU graph ALL, sequential execution, no idle spinning.

GUI producer DirectConnection only enqueues into shared bounded storage, never widgets. Timer drains each 50 ms; <=200 entries, <=8192 displayed chars/entry. File logging complete. Category drawers created on demand; empty categories hidden. Dynamically linked resource collections need unique basenames.

## 4. Validation & Error Matrix
- Missing/nonmember ID: FlowIllegal, nothing starts.
- Busy flow/project/unfinished worker: FlowRunning.
- Stop while active: edit guard persists until completion.
- Same-size rewrite/restored mtime: changed stamp forces hash and rejection; outputs unchanged.
- Same-byte replacement/touch: rehash, accept and refresh stamp.
- Invalid thread setting or model candidate: explicit error, old session survives.

## 5. Good / Base / Bad Cases
Base: CPU identity graph owns output bytes. Good: scanline/LUT with exact pixel semantics. Special/premultiplied formats retain pixelColor rounding. Bad: per-frame full-file scan, per-log queued GUI events, click handlers calling runXvFunc synchronously.

## 6. Required Tests
ui_responsiveness: 10000 GUI/worker logs (<250 ms blockage), latest delivery, clear queue, click (<100 ms), selected-only execution, reject restart, stop/edit guard, long-interval wake. onnx_operators: changed-file rollback, exact pixel reference. systemplugin: 39/84, categories, non-null icons. Real CPU fixture runs on Windows. Distinguish Linux synthetic speed measurements from native Windows acceptance.

## 7. Wrong vs Correct
Wrong: unchanged size/mtime proves bytes. Correct: native identity/change time and invalidation hash, full persistence checks. Wrong: stop clears busy immediately. Correct: cancellation and active execution are separate; completion releases editor protection.

## Lazy widget lifetime regression
Widgets created after a visible parent require explicit show() and initial parent geometry synchronization; do not rely on parent show/resize events that already occurred. Guard constructors against event-loop reentrancy. Assert first-click visibility, unclipped visible region, parent-sized overlay and reuse under queued repeated clicks.
