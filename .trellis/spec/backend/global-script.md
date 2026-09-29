# Project Globals and Isolated JavaScript

XvGlobalState owns typed JSON scalars and JavaScript source. Project XML v1 has one optional Globals extension. Missing means empty; present data is strict. Names are unique ASCII identifiers, integers are signed 32-bit, reals finite, and strings XML-safe. Limits: 256 variables, 64K value characters, 1K description, 256K script.

XvProject::setGlobalState is owner-thread and idle-only, with a monotonic revision. ProjectScript copies idle scalar values, retains QPointer/revision, compares current values before applying, rejects subscribed parameters, validates all changes, and rolls back parameter writes on failure. Never read live result containers while a project or individual flow/function is running.

GlobalScriptRunner launches the same executable with --script-worker before SingleApplication/GUI initialization. QJSEngine receives JSON only, without host filesystem/network/UI objects. Pipes are asynchronous. Timeout/cancel kills the disposable worker; output/log size is bounded. Windows Job Objects limit memory and terminate workers when their owner closes. This is reliability isolation, not a hostile-code sandbox.

Scripts run explicitly from the editor. Do not silently add automatic startup or run hooks. UI must report syntax/runtime line errors, cancellation, timeout, stale projects and validation failures.

Verify global_tools, projectxml and packaged Windows worker smoke checks. QtQml is copied and verified by WindowsBuild.Common.ps1. Only the desktop target uses WIN32_EXECUTABLE.
