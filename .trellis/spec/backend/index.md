# Core and Backend Development Guidelines

This repository is a Qt/C++ desktop vision application, not a client/server
system. In this spec, "backend" means the non-UI runtime: project and flow
orchestration, operator plugins, shared data objects, logging, concurrency, and
build integration.

## Guidelines Index

| Guide | Project-specific scope |
| --- | --- |
| [Directory Structure](./directory-structure.md) | CMake targets, runtime layers, and ownership boundaries |
| [Persistence and Data Model](./database-guidelines.md) | `XvData`, `XObject`, and the version-1 project XML contract |
| [Error Handling](./error-handling.md) | `RetXv`, run statuses, last-error messages, and plugin failures |
| [Logging](./logging-guidelines.md) | `XLog`/spdlog levels, file output, and UI log signals |
| [Quality](./quality-guidelines.md) | Plugin ABI, lifetime, build, and review checks |
| [Vision Data and Halcon Interop](./vision-data-interop.md) | Geometry values, list ownership, display binding, and image conversion |
| [Historical: Halcon Model Match](./halcon-model-match.md) | Shape-model execution, embedded assets, UI output, and compatibility |
| [Historical: Halcon Semantic Segmentation](./halcon-semantic-segmentation.md) | External DL assets, preprocessing, portable pixel results, and inference fixtures |
| [Historical: Halcon Object Detection](./halcon-object-detection.md) | HALCON detection assets, portable boxes, transient results, and fixture wiring |
| [Project Run Configuration](./project-run-config.md) | Main-flow order, enable state, error policy, XML migration, and UI boundary |
| [Project Sequential Execution](./project-execution.md) | Managed scheduling, stop/wait semantics, final status, and edit protection |
| [Global State and JavaScript](./global-script.md) | Typed project globals, XML persistence, isolated script workers, and atomic scalar updates |
| [Camera Runtime](./camera-runtime.md) | Provider ABI, device identity, acquisition threads, errors, and directory simulation |
| [Operator Catalog](./operator-catalog.md) | 123 distinct entries, Role.EnumKey IDs, removed compatibility IDs, and validation gates |
| [Optional Vision Backend Adapters](./vision-backend-adapters.md) | Optional OpenCV/ONNX SDK discovery, owned image conversion, stable tensor metadata, and runtime values |
| [VisionMaster Presets and Structured Flow Execution](./visionmaster-flow-execution.md) | Atomic aliases, port XML, deterministic branch/join scheduling, and bounded repeat contexts |
| [VisionMaster OpenCV Image Processing](./visionmaster-opencv-image-processing.md) | Eight canonical image families, 34 modes, optional backend execution, and result rollback |
| [VisionMaster OpenCV Morphology And Detection](./visionmaster-opencv-morphology-detection.md) | Five canonical morphology/detector families, 25 modes, structured transient results, and annotated output |
| [VisionMaster Video Source And Notification Output](./visionmaster-video-notification.md) | Video range/EOS semantics, eight notification modes, logging, and queued GUI delivery |
| [VisionMaster ONNX Inference Operators](./visionmaster-onnx-inference.md) | Four canonical CPU inference families, model summaries, preprocessing, decoders, and rollback |
| [VisionMaster Template Matching And Rectification](./visionmaster-template-rectification.md) | Embedded PNG templates, four matching modes, three rectification modes, and portable results |
| [VisionMaster Geometry Creation And Measurement](./visionmaster-geometry-measurement.md) | Strongly typed 2D geometry, seven measurement modes, units, tolerance, annotations, and rollback |
| [VisionMaster Detect Record](./visionmaster-detect-record.md) | SQLite schema v1, per-call connections, append/query/existence modes, transient records, and rollback |
| [VisionMaster Communication Operators](./visionmaster-communication.md) | HTTP/TCP/UDP/serial/Modbus modes, run-thread transports, framing, cancellation, byte order, and rollback |

## Pre-Development Checklist

- Read `directory-structure.md` before adding a target, module, or operator.
- Read `database-guidelines.md` when changing `XObject`, parameters/results, or
  project serialization.
- Read `error-handling.md` and `logging-guidelines.md` for runtime behavior.
- Read `quality-guidelines.md` before changing CMake, plugin loading, ownership,
  or flow execution.
- Read `vision-data-interop.md` when changing geometry objects, structured value
  XML, ROI display bindings, or QImage/Halcon conversion.
- Read `halcon-model-match.md` when changing HModelMatch parameters, execution,
  embedded shape-model assets, or match overlays.
- Read `halcon-semantic-segmentation.md` when changing segmentation values,
  external DL assets, preprocessing, inference outputs, or fixture wiring.
- Read `project-run-config.md` when changing project flow lifecycle, project
  execution order/policy, project configuration XML, or its editor.
- Read `project-execution.md` when changing project/flow start, stop, wait,
  runtime status, execution signals, or run-time editing protection.
- Read `camera-runtime.md` when changing camera providers, device identity,
  acquisition lifecycle, frame delivery, or camera-consuming operators.
- Treat `XVision/3rdparty/`, `XVision/XVision/QBreakpad/3rdparty/`, and
  `XVision/CommonUsing/Project/XLog/3rdparty/` as bundled dependencies, not as
  examples of XVision conventions.
- Read `operator-catalog.md` when adding or regrouping an operator, changing IDs or updating catalog/package acceptance.
- Read `vision-backend-adapters.md` when changing OpenCV/ONNX discovery, image conversion,
  ONNX session metadata, shared tensors, or transient VisionMaster runtime values.
- Read `visionmaster-flow-execution.md` when changing plugin presets, flow ports, link XML,
  branch/join scheduling, conditional control, or bounded repeat execution.
- Read `visionmaster-opencv-image-processing.md` when changing an `OImage*` role, image alias,
  OpenCV algorithm parameter, model/composition state, or image-operator test.
- Read `visionmaster-opencv-morphology-detection.md` when changing morphology, region/code/cascade
  detection, point features, their OpenCV parameters, structured results, or detector tests.
- Read `visionmaster-video-notification.md` when changing video acquisition, source/output aliases,
  notification events/logging, GUI presentation, OpenCV video I/O, or source/notification tests.
- Read `visionmaster-onnx-inference.md` when changing an `N*` inference role, ONNX model persistence,
  preprocessing, classification/detection/segmentation decoding, aliases, or inference tests.
- Read `visionmaster-template-rectification.md` when changing `OTemplateMatch`, `ORectification`,
  embedded PNG assets, matching/rectification presets, ROI conversion, transforms, or overlays.
- Read `visionmaster-geometry-measurement.md` when changing `GeometryCreate`, `GeometryMeasure`,
  their formulas, typed geometry subscriptions, scale/units/tolerance, annotations, or presets.
- Read `visionmaster-detect-record.md` when changing `DetectRecord`, its SQLite schema/connection
  lifecycle, record presets, typed record subscriptions, query results, or database tests.
- Read `visionmaster-communication.md` when changing HTTP/TCP/UDP/serial/Modbus operators,
  transport interfaces, framing, cancellation, communication presets, Qt modules, or tests.

## Quality Check

- The change stays inside the owning CMake target and does not create a reverse
  dependency from a lower layer to the application UI.
- Public shared-library classes use the target export macro.
- New operators satisfy the Qt meta-object/plugin registration contract.
- Failures use the return/status/message channel expected by the caller.
- Runtime dependencies required by a plugin are copied beside the produced
  binary when needed.
- Configure/build the affected target in the supported Qt environment and run
  `XvCorePersistenceTests` for persistence changes; there is no lint target.

All Trellis spec documentation is written in English.

- [CPU runtime performance contracts](cpu-runtime-performance.md): ONNX caching, threading, manual execution and GUI queue boundaries.

HALCON role/adapter guidance is historical after 2026-09-28. Do not restore the retired dependency; current template/inference development uses OpenCV/ONNX and CPU runtime contracts.
