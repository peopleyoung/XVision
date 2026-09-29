# Operator Catalog Contract

## 1. Scope / Trigger
Effective 2026-09-29. Applies to factory registration, toolbox entries, drag payloads, persistence, tests and packaging. Replaces the historical VisionMaster compatibility matrix. Preserve algorithm behavior; do not restore retired entry IDs.

## 2. Signatures
SystemXvFactoryPlugin::getPlgXvFuncList() publishes 39 canonical classes. getPlgXvFuncPresets() publishes 84 non-default configurations. XvFuncAssembly::getXvFuncInfos() exposes 123 entries.
The UTF-8 file doc/开发工作/算子目录.csv has six fields: entry_id,canonical_role,property,value,display_name,category.
Validate with: cmake -DCATALOG=<absolute CSV path> -P XVision/scripts/VerifyOperatorCatalog.cmake.

## 3. Contracts
Canonical entries use Role. Non-default modes use Role.EnumKey (ImageAcquisition.Camera, TcpText.Write, NObjectDetection.Yolov5). Defaults have no redundant preset. Properties are mode/acqType with enum integers; non-mode canonical entries use -,-. Chinese labels are presentation only. Drag payloads use entry_id. XML continues to save canonical roles plus complete parameters; do not introduce old-ID remapping. All 126 former preset IDs are unregistered.
A unique (role,property,value) row is necessary but not sufficient: compare complete initialized persistent configurations after stripping only instance identity/name/position.
Role icons resolve :/operators/<Role>.svg through XvFunc::funcIcon(), with generic fallback for external plugins.

## 4. Validation & Error Matrix
- Duplicate ID or configuration: catalog check fails.
- Non-default unqualified ID or unknown canonical role: catalog check fails.
- Former preset ID: lookup and creation fail; no fallback.
- Canonical project plus valid parameters: round trip preserves configuration.
- Failed factory batch: existing registration survives, tested by systemplugin.

## 5. Good / Base / Bad Cases
Base: ImageAcquisition means its default File mode. Good: ImageAcquisition.Camera is one independent Camera entry. Bad: advertising several directory names that initialize identical parameters.

## 6. Tests Required
CTest operator_catalog and operator_catalog_rejections validate the CSV. Both Windows testPresets in CMakePresets.json must select these names, not the retired matrix names; TestWindowsPackaging.ps1 guards both preset filters. systemplugin loads the real plugin and CSV, asserts 39/84/123, instantiates every entry, compares full normalized XML configurations, checks non-null icons and rejects all 126 retired IDs. projectxml verifies canonical persistence and configured modes. Windows packaging must rerun the staged plugin test using this CSV with sanitized DLL search paths.

## 7. Wrong vs Correct
Wrong: keep removed IDs registered but hide them in the sidebar. Correct: remove registration and prove both lookup and construction fail. Wrong: use a renamed CSV while a packaging fixture accepts any path. Correct: assert the exact new filename, header and 123 rows in the fixture.
