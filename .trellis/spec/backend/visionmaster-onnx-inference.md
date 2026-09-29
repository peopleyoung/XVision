# VisionMaster ONNX Inference Operators

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## Scenario: Four Canonical CPU Inference Families

### 1. Scope / Trigger

Apply this contract when changing `NInference`, `NClassification`, `NObjectDetection`,
`NSemanticSegmentation`, their shared preprocessing/decoding code, ONNX model persistence, or the
ten VisionMaster compatibility presets. ONNX Runtime remains an optional plugin-layer dependency.

### 2. Roles And Presets

| Canonical role | Modes | VisionMaster presets |
| --- | --- | --- |
| `NInference` | `Generic`, `Age` | `InferOnnxNodeData`, `AgeInferOnnxNodeData` |
| `NClassification` | `Generic`, `Gender` | `ClsOnnxNodeData`, `GenderClsOnnxNodeData` |
| `NObjectDetection` | `Generic`, `Yolov3`, `Yolov5`, `Yolov5Face` | `ObjDetectOnnxNodeData`, `Yolov3`, `Yolov5OnnxNodeData`, `Yolov5FaceOnnxNodeData` |
| `NSemanticSegmentation` | `Generic`, `Human` | `SemSegOnnxNodeData`, `HumanSemSegOnnxNodeData` |

Presets set the `mode` property and resolve to a canonical role. XML must never persist a preset
name as a role. The current factory inventory is 39 roles and 84 presets.

### 3. Shared Model And Preprocessing Contract

- `NOnnxBase` owns the canonical absolute `.onnx` path, non-zero byte length, SHA-256 digest, and
  cached session. Model bytes and runtime handles remain external and transient.
- Model configuration loads a complete candidate before replacing the accepted path, digest, and
  session. Save/load fully hash the file. Runs validate native identity/change timestamps and rehash on change; unsupported hosts hash every run. Missing/changed bytes are rejected. See cpu-runtime-performance.md.
- Configured XML contains exactly one strict child:

```xml
<PersistentData>
  <OnnxModel format="onnx-runtime" version="1" length="..." sha256="..."/>
</PersistentData>
```

- An unconfigured operator has no `PersistentData`. A configured operator requires it. Unknown
  attributes, children, text, versions, non-canonical lengths, or non-lowercase SHA-256 fail load.
- Image operators accept one non-empty image and one model input with batch 1, `float32` or `uint8`,
  one or three channels, and NCHW or NHWC layout. Ambiguous automatic layout is an error.
- Dynamic width and height require positive configured values. Static dimensions reject a different
  configured value. Resize is stretch or centered letterbox and records the exact rounded x/y scale.
- RGB/BGR selection is explicit. Float preprocessing is `(pixel * pixelScale - mean) / std`; mean
  accepts finite values and standard deviation must be finite and positive. `uint8` requires identity
  normalization.

### 4. Execution And Decode Contracts

- `NInference(Generic)` maps unnamed `XTensor` inputs by model order and named inputs strictly. It
  publishes every owned named output. `Age` preprocesses one image and accepts one numeric output;
  one value is the age and multiple logits produce the expected class index after stable softmax.
- `NClassification` accepts one `[C]` or `[1,C]` output, applies optional softmax, threshold and
  stable score/class ordering, then publishes top-k `XClassificationResult` values. Missing labels
  use `Class <id>`. Gender labels are editable defaults, not a claim about model class order.
- Generic detection accepts `[N,6]` or `[1,N,6]` xyxy rows. YOLOv3/v5 accepts `[N,5+C]` or
  `[1,N,5+C]`; YOLOv5Face accepts 16-column rows and ignores landmarks. All numeric values and
  confidence fields are validated before filtering. Heads merge before deterministic NMS.
- Detection coordinates map through the exact stretch/letterbox transform, clip to the source, and
  reject non-positive boxes. Results use `XDetectionResult` and preserve class-aware or class-agnostic
  NMS selection.
- Segmentation accepts `[1,H,W]` integer labels, `[1,C,H,W]`/`[1,H,W,C]` logits, or one-channel
  binary logits. Label IDs are non-negative and bounded. Labels resize with nearest-neighbor;
  confidence resizes bilinearly. Results use `XSegmentationResult` plus mask, overlay, and confidence
  images at source resolution.
- Every run clears previous visible results first, decodes into candidates, and commits only after
  complete success. No operator publishes a partial list, mask, or stale validity flag.

### 5. Disabled Backend And Error Matrix

| Condition | Required result |
| --- | --- |
| ONNX Runtime disabled | Roles, presets, UI, and XML remain available; configured model bytes may round-trip; run fails with the enable option |
| Model unconfigured, missing, or changed | Run/save/load fails; no result remains published |
| Input count/name/type/shape/byte mismatch | Session run fails and caller output list is unchanged |
| Unsupported/string tensor | Reject before runtime execution |
| Ambiguous layout or unresolved dynamic size | Reject before allocating an input tensor |
| Output type/fixed shape differs from metadata | Reject the complete run |
| Bad score, class ID, box, label, or non-finite value | Reject the complete decode |
| Runtime or C++ exception | Translate to `false`/error text; never cross the plugin ABI |

### 6. Tests Required

- `onnx_operators` covers NCHW/NHWC, RGB/BGR, grayscale, stretch/letterbox, dynamic sizes,
  normalization rejection, scalar reading, classification, all detection decoders/NMS, and the
  segmentation output forms.
- The disabled build creates all four roles, configures an external file, runs them, and verifies
  explicit diagnostics plus result clearing.
- `visionmaster_backend_boundary` always covers unloaded/disabled execution and failed-load rollback.
  With ONNX Runtime, `XVISION_ONNX_TEST_MODEL` validates metadata and
  `XVISION_ONNX_EXECUTION_TEST_MODEL` must identify a one-input/one-output identity model; it proves
  execution, dynamic-dimension resolution, output ownership, and failure rollback.
- `systemplugin` asserts the current 39 unique roles, 84 unique presets, and all ten ONNX mappings.
  `projectxml` asserts canonical role/mode round trips, transient results, strict model metadata, and
  changed-file rejection.
- Real classification, detection, and segmentation assets remain external acceptance gates. Until
  Windows CPU execution is recorded, matrix rows stay `implemented_external_validation_pending`.

### 7. Out Of Scope

Training, conversion, download, GPU providers, multiple-image batches, multi-input image models,
instance segmentation, landmark publication, tracking, and automatic inference of arbitrary model
business semantics require separate contracts.
