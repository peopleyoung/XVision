# Core Quality Guidelines

## Current Tooling Baseline

The project is built with CMake and Qt (Core, Widgets, Xml, Concurrent as
needed). First-party libraries, operator plugins, tests, and the main executable
set C++17 in their owning `CMakeLists.txt`. New first-party code may rely on
C++17, but must not silently raise an individual target beyond that shared
baseline.

There are currently three first-party test targets enabled only with
`-DBUILD_TESTING=ON`:

- `XvCorePersistenceTests` for core, persistence, operator, and project
  integration behavior;
- `XvCameraTests` for the vendor-neutral camera runtime and simulator;
- `XvSystemPluginTests` for dynamic plugin loading, interface discovery, and
  registration metadata of all current operator roles.
- No repository lint or formatting configuration.
- No CI workflow in the repository.
- A Windows-oriented dependency setup with prebuilt CommonUsing/Halcon DLLs.

Do not claim automated coverage that does not exist. Verification is configure,
build, targeted runtime checks where the environment supports them, and careful
source review.

## Required Local Patterns

### Shared library and plugin ABI

- Export public classes with the module macro (`XVCORE_EXPORT`,
  `XVDATA_EXPORT`, `XVFUNCSYSTEM_EXPORT`, etc.).
- Keep public declarations under the module's established include directory.
- For an `XvFunc` plugin class, retain `Q_OBJECT`, `Q_INVOKABLE`, a parent-aware
  constructor, and registration in `SystemXvFactoryPlugin.cpp`.
- Plugin interfaces use `Q_DECLARE_INTERFACE`, `Q_INTERFACES`, and
  `Q_PLUGIN_METADATA` as shown by `IXvFactoryPlugin.h` and
  `SystemXvFactoryPlugin.h`.

### Lifetime and graph integrity

- Give created `QObject`s a parent when the parent owns them. `XvFlow` sets each
  operator parent to the flow; application managers are constructed with the
  application as parent.
- Respect explicit `release()` checks before `deleteLater()`/delete in project,
  flow, and operator removal paths.
- When storing non-owning raw pointers, connect to the relevant destruction
  signal and clear/remove the reference. `XvFunc.cpp`, `XvWorkManager.cpp`, and
  `UiXvDisplayManager.cpp` are references.
- Preserve both sides of graph links and refresh parameter subscriptions after
  unlinking.

### Threading

- Run flows through `XConcurrentManager`/`XThread`, as in `XvFlow.cpp`; do not
  create unmanaged long-running UI threads for the same work.
- Keep UI mutation on the Qt GUI thread. Be explicit about connection type when
  a data dependency must update synchronously (`Qt::DirectConnection` is used by
  operator subscriptions and timer operators).
- Stop/wait/release behavior must remain consistent with `isRunning()` and the
  public `RetXv` contract.

### Build integration

- Include `CommonProjectOutputSet.cmake` for ordinary libraries/executable and
  `CommonXvFuncOutputSet.cmake` for operator plugins.
- Keep required CommonUsing link/include settings in
  `CommonUsing/CommonUsing.cmake` synchronized with the active Debug/Release
  mode documented by the project.
- If a plugin references a third-party runtime, add a post-build
  `copy_if_different` step. The Halcon copy in
  `XvFuncSystem/CMakeLists.txt` and commit `d445006` document why: the plugin
  cannot load when its dependent DLL is absent beside the runtime output.
- Reconfigure CMake after adding globbed sources/resources.

## Scenario: Case-Sensitive Build Inputs

### 1. Scope / Trigger

Apply this contract when editing a CMake Qt component list, adding or renaming a
public header, or changing an include directive. Windows can hide casing drift
that fails immediately on a case-sensitive filesystem.

### 2. Signatures

Qt component names and imported targets use Qt's exact spelling:

```cmake
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Xml)
target_link_libraries(Target PRIVATE Qt${QT_VERSION_MAJOR}::Xml)
```

Public includes must match the tracked filename exactly, for example
`#include "XvFunc.h"` requires `XvCore/include/XvFunc.h`.

### 3. Contracts

- Keep the component name, imported target, tracked header, and include
  directive identical in case.
- After a case-only rename of a globbed header, reconfigure CMake before
  rebuilding.
- A case-sensitive Linux configure/build is a useful supplemental check, but
  does not replace the supported Windows Qt/Halcon gate.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Qt component uses `XML` instead of `Xml` | Configure fails; correct the component name |
| Include case differs from the tracked header | Compile fails; rename the header or include consistently |
| Glob cache still names a case-renamed file | Reconfigure, then rebuild |
| Linux-only bundled dependency cannot build | Record the boundary; do not claim the Windows gate passed |

### 5. Good/Base/Bad Cases

- Good: Qt `Xml`, `Qt::Xml`, `XvFunc.h`, and all includes match exactly.
- Base: a Windows build succeeds, and a supplemental case-sensitive build also
  compiles the affected first-party targets.
- Bad: relying on NTFS case folding while committed CMake components or public
  includes use a different spelling.

### 6. Tests Required

- Configure with Qt 6.4 after CMake component changes and assert generation
  completes.
- Build every affected first-party target after a public-header rename.
- Run the target's existing Qt Test suite; for camera changes this includes
  `ctest -R '^camera$' --output-on-failure`.

### 7. Wrong vs Correct

```cmake
# Wrong: requests a non-existent case-sensitive Qt package.
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core XML)

# Correct: matches Qt6XmlConfig.cmake and Qt::Xml.
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Xml)
```

## Scenario: Shared-Library Consumer Build Contracts

### 1. Scope / Trigger

Apply this contract when a target directly includes a Qt module header, a
helper is consumed outside its owning shared library, or a core class grants
access to a UI class declared in the global namespace.

### 2. Signatures

```cmake
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Xml)
target_link_libraries(OperatorPlugin PRIVATE Qt${QT_VERSION_MAJOR}::Xml)
```

```cpp
// XvCore/include/XvXmlUtils.h
XVCORE_EXPORT bool validateAttributes(const QDomElement &element,
                                      const QStringList &allowed,
                                      const QStringList &required,
                                      QString &error);

class OperatorWdg;
namespace XvCore {
class Operator : public XvFunc { friend class ::OperatorWdg; };
}
```

### 3. Contracts

- A target declares and links every Qt module whose headers or symbols it uses;
  a private dependency of another library is not a consumer dependency.
- Helpers consumed across a shared-library boundary live in the owner's public
  `include/` directory and every free function or variable is marked with the
  owner's export macro.
- When the domain class is namespaced and its Widget is global, forward-declare
  the Widget globally and qualify the friend as `::WidgetName`.
- Do not expose a private source directory through a consumer include path to
  work around a missing public API.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Consumer includes `<QtXml>` without `Qt::Xml` | Configure/link `Qt::Xml` on that consumer |
| Plugin includes a header under `XvCore/Core` | Move the contract to `XvCore/include` and export it |
| Cross-DLL free function lacks `XVCORE_EXPORT` | Export its declaration before Windows linking |
| Namespaced class uses `friend class GlobalWdg` | Use `friend class ::GlobalWdg` |
| Bundled dependency fails only on Linux | Record the boundary; do not modify it without scope |

### 5. Good / Base / Bad Cases

- Good: the plugin declares `Qt::Xml`, includes only `XvCore/include`, and links
  against exported `XvXml` functions.
- Base: core and plugin sources compile on a case-sensitive Qt 6.4 toolchain;
  the supported Windows toolchain completes the DLL/plugin link.
- Bad: compilation succeeds only because another library leaks Qt include paths,
  MSVC accepts an ineffective friend declaration, or a consumer includes `Core/`.

### 6. Tests Required

- Reconfigure after moving a globbed public header.
- Build the owning shared library through its final link step.
- Compile every source in the consuming plugin and complete the plugin link on
  supported Windows Qt/Halcon.
- Run `projectxml` and `systemplugin` to exercise the exported persistence API
  and dynamic plugin load.

### 7. Wrong vs Correct

```cpp
// Wrong: declares XvCore::OperatorWdg and grants no access to the global Widget.
friend class OperatorWdg;

// Correct: matches the global configuration Widget declaration.
friend class ::OperatorWdg;
```

## Review Checklist

- Ownership of every new raw pointer is visible and teardown cannot double
  delete it.
- Null/type validation occurs before mutation or cast.
- `RetXv`, Boolean, pointer, and run-status meanings are not mixed.
- Operator role strings remain globally unique; plugin registration failures
  are handled and logged.
- UI/domain boundaries stay one-way: core libraries do not include application
  screen code.
- New user-visible strings use the language helper; new icons/styles are listed
  in the owning `.qrc`.
- No unrelated cleanup is mixed into a behavioral change.
- Bundled dependency source is untouched unless explicitly in scope.

## Verification

At minimum, configure and build the affected target with the project's Qt and
third-party SDK environment. For operator changes, also verify:

1. The plugin binary is emitted under `Bin[D]/XvFuncCollection`.
2. Required third-party DLLs are copied to the runtime location.
3. `XvPluginManager` loads the plugin and logs successful operator registration.
4. The operator can be created from its role, run, and removed without stale UI
   or graph pointers.

Run the persistence suite with:

```bash
cmake -S XVision -B <build-dir> -DBUILD_TESTING=ON
cmake --build <build-dir> --config Debug --target XvCorePersistenceTests
ctest --test-dir <build-dir> -R projectxml --output-on-failure
```

The supported Windows presets build and run all three targets. Do not replace
the `systemplugin` test with a file-existence check: it must load the plugin
through `QPluginLoader` and instantiate every advertised operator metaobject.

Keep new tests under `XVision/tests/`, outside bundled third-party test trees,
and guard them with `BUILD_TESTING` so normal application builds are unchanged.

## Scenario: Full-Feature Windows Packaging

### 1. Scope / Trigger

Apply when changing Windows build verification, runtime deployment, or packaging.

### 2. Signatures

- `Package-WindowsBuild.ps1 -Configuration Release -OpenCvRoot <sdk> -OnnxRuntimeRoot <sdk>`
- `Verify-WindowsBuild.ps1 -FullFeatures [-SkipBuild]` accepts the same SDK roots.
- SDK parameter defaults are `XVISION_OPENCV_ROOT` and `XVISION_ONNXRUNTIME_ROOT`.
- `pwsh -NoProfile -File XVision/tests/TestWindowsPackaging.ps1` runs portable script checks.

### 3. Contracts

- Generic CMake defaults stay optional. Full-feature packaging requires OpenCV, ONNX Runtime,
  the system plugin, CommonUsing, Breakpad, and tests enabled in the actual build cache.
- `SkipBuild` skips compilation only; cache/runtime validation and tests still run. `SkipTests`
  explicitly records unverified status and cannot count as full acceptance.
- Pass both the EXE and dynamically loaded plugin to windeployqt. `--sql` is a module flag;
  `sqlite` must not be passed as an extra executable. Check Qt SQL and configuration-specific
  platform/SQLite plugin names after deployment.
- Run the system-plugin test again in the staging directory with build/SDK locations removed
  from PATH; fail packaging on missing dependencies or role/preset mismatches.
- PowerShell scripts containing non-ASCII paths use UTF-8 BOM for Windows PowerShell 5.1.
- Qt Test programs with custom positional inputs construct QCoreApplication with the original
  arguments, then pass only Qt Test arguments to qExec.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| SDK directory absent | Fail before configuring |
| Reused cache omits/disables required backend | Refuse full-feature package |
| Backend DLL absent | Fail before ZIP creation |
| Debug package contains only qwindows.dll | Fail; require qwindowsd.dll |
| Staged plugin cannot load without SDK PATH | Fail and retain diagnostic error |
| DLL/matrix arguments reach qExec | Fix runner; these are not test selectors |

### 5. Good / Base / Bad Cases

- Good: all runtime checks and staged 39-role/84-preset checks pass before ZIP creation.
- Base: portable script checks pass, while Windows build and real-device acceptance remain pending.
- Bad: optional backends are OFF, but file existence is presented as full functionality.

### 6. Tests Required

Run TestWindowsPackaging.ps1 and the POSIX test_windows_packaging_flow.py mock-tool fixtures,
then Debug/Release Windows verification and staged plugin loading.
Assert missing/disabled features and missing SDK DLLs fail. Verify GUI, real models, and hardware
separately; portable script tests cannot establish Windows application acceptance.

### 7. Wrong vs Correct

- Wrong: `QTEST_GUILESS_MAIN(Test)` with CTest arguments `<plugin.dll> <matrix.csv>`.
- Correct: keep those inputs in QCoreApplication, and use `QTest::qExec(&test,1,argv)`
  so Qt Test executes the test instead of reporting an unknown function.


## Scenario: Reflected Operator Enum Properties

Apply when adding or changing an operator enum exposed through Q_PROPERTY.
The enum name and numeric values are persistence contracts; preserve them.

- For an enum declared by the current class, use its local name, for example
  Q_PROPERTY(Mode mode READ mode WRITE setMode), and register it with Q_ENUM(Mode).
- For an enum declared by another class, include the full namespace, for example
  Q_PROPERTY(XvCore::NOnnxBase::InputLayout outputLayout READ outputLayout WRITE setOutputLayout).
- Do not use a partially qualified Class::Mode in a class inside namespace XvCore.
  Qt's metaobject scope lookup compares that scope with XvCore::Class and can fail.
- Do not work around reflection failures by weakening preset validation or
  treating enum properties as arbitrary integers. Fix the property declaration.

Verification must assert QMetaProperty::isEnumType(), enumerator key counts,
integer write/read round trips, and registration of persisted operator presets.
Run existing operator contract tests plus projectxml and systemplugin on the
supported Windows Qt6 build. A local moc parse alone cannot verify reflection.

Regression evidence (2026-09-28): the standalone Qt5.12.8 and Qt6.4.0 reproductions returned
false for a partially qualified local enum and true for both the corrected local
name and the fully qualified inherited enum. Native Qt6.4.0 tests exposed the
same root cause as an incompatible acqType preset property. See the Windows
cloud-build task research for native run IDs and subsequent acceptance results.


## Scenario: Decoded Image Format at the HALCON Boundary

QImage PNG decoding and directory-camera frames can produce Format_RGB32 even
when the training image was Format_RGB888. HalconImageInterop::toHalcon must
accept RGB32 through its owned RGB888 channel-normalization path, alongside its
existing supported formats. Do not assume image-file loading preserves format.

Keep byte-for-byte pixel/ownership round-trip coverage for RGB32. Run both the
file-acquisition and camera-acquisition template-match project integration cases.
The OpenCV operator utility already normalizes decoded color images before its
lower-level conversion boundary; that separate contract need not be broadened.

For XML value tests, compare parsed numeric attributes rather than assuming a
short decimal spelling of a full-precision double. Exclude runtime payloads by
exact field/type names, not broad substrings matching legitimate configuration
such as descriptorSize and descriptorChannels.


## Scenario: Multiple Instances in XML Rejection Fixtures

When a fixture has several operators with the same canonical role, select the
saved operator by its captured ID; after import regenerates IDs, select by the
expected mode and assert uniqueness. Never assume UUID-sorted function order.

For negative XML tests, assert the target node exists before cloning or
appending children, and assert the serialized document differs after mutation.
Appending a null QDomNode can silently leave a valid document unchanged; an
unexpected successful project load then also invalidates old project pointers.
A rejection test must mutate the intended asset, preserve all rejection
assertions, and never choose its import target merely by asset presence.

Regression evidence: Qt6.4.0 reproduces an assetless first-role selection in
3/4 function orders. ID-targeted duplicate-container, duplicate-template and
unknown-child mutations all change the intended node (12/12 order checks).


## Scenario: Declared Qt Modules Missing from Release Imports

A full-feature package must include every root Qt DLL declared by
Get-XVisionQtRuntimeFiles, even if the linker eliminates its imports. Stage
these DLLs from the selected Qt SDK, then pass them as windeployqt inputs with
the application and dynamic plugin. Keep required-file validation and
configuration-specific suffixes. Let windeployqt process dependencies and
patch QtCore after staging; do not overwrite its patched output afterward.

Mock coverage must simulate automatic deployment omitting Concurrent and
StateMachine, verify their actual presence in the final archive, and reject
missing SDK modules. Preserve native staged-plugin and extracted-GUI gates.
