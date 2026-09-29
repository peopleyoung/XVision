# VisionMaster Template Matching And Rectification

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## Scenario: Embedded OpenCV Templates And Portable Rectification Results

### 1. Scope / Trigger

Apply this contract when changing `OTemplateMatch`, `ORectification`, their seven VisionMaster presets,
the embedded PNG template extension, rotate-rectangle conversion, match overlays, or their fixed-image
tests. OpenCV remains an optional plugin dependency.

### 2. Signatures

The canonical roles and modes are:

```cpp
OTemplateMatch  // base64, feature, shape, hsv
ORectification  // foreground_rotated_rect, foreground_extract, rotated_rect
```

Shared utility boundaries are:

```cpp
bool encodeTemplatePng(const QImage &,QByteArray &,QSize &,QByteArray &sha256,
                       QString &error);
bool decodeTemplatePng(const QByteArray &,QImage &,QSize &,QByteArray &sha256,
                       QString &error);
bool roiCorners(const XRotateRectRoi &,QVector<QPointF> &,QString &error);
bool setTransformTensor(const std::array<double,9> &,XTensor &,QString &error);
```

### 3. Contracts

- Exact upstream names are mode presets. Projects persist only the canonical role and effective mode.
- Both roles derive from `OpenCvImageOperatorBase`; disabled builds register every role/preset and can
  round-trip parameters and valid PNG assets, while execution reports `XVISION_ENABLE_OPENCV=ON`.
- `XRotateRectRoi` uses pixel coordinates, positive half-lengths and radians. Corner order is local
  top-left, top-right, bottom-right, bottom-left under the existing display angle convention.
- `OTemplateMatch` operation 0 creates a template from the full input or rectified rotate ROI. Operation
  1 finds the accepted template. Base64 uses grayscale correlation, HSV uses HSV correlation, Shape
  searches bounded edge rotations, and Feature uses ORB/Hamming ratio/RANSAC homography.
- Match outputs are `outputImage`, `templateRoi`, `matchCount`, and an exact
  `XObjectList<XMatchResult>` named `matches`. Feature publishes at most one homography result.
- Correlation results are deterministic: score descending, then y/x/angle, followed by bounded overlap
  suppression and maximum count. A normal zero-match search succeeds with an empty list.
- The template is an owned PNG of at most 8 MiB, at most 32768 pixels per dimension and at most 64
  megapixels. It is stored as canonical Base64 with exact decoded length and lowercase SHA-256:

```xml
<PersistentData>
  <OpenCvTemplate format="png" version="1" encoding="base64"
                  length="1234" sha256="64-lowercase-hex">...</OpenCvTemplate>
</PersistentData>
```

- `ORectification` RotatedRect rectifies its supplied ROI. ForegroundExtract thresholds once, selects
  the deterministic largest external contour above minimum area, publishes the black-background
  foreground and identity transform. ForegroundRotatedRect rectifies that same detected ROI.
- Rectification outputs are `outputImage`, `foregroundMask`, transient `XRegion foregroundRegion`,
  `XRotateRectRoi rectifiedRoi`, and transient row-major `XTensor transform` with type `float64`, shape
  `{3,3}` and 72 owned bytes.
- Every SDK computation prepares local images, lists, geometry and tensors. Runtime failure clears role
  transient results but preserves an accepted template. A new template replaces the old bytes only
  after PNG encoding, bounds, digest and result candidates all succeed.
- The application display recognizes `OTemplateMatch` through the shared portable result-name contract; it does not include OpenCV or operator headers.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| OpenCV disabled | Roles/presets/XML/assets remain usable; run returns diagnostic and clears transients |
| Empty image, invalid mode/operation/range or missing template | `Error`; accepted template unchanged |
| ROI non-finite, non-positive or output transform degenerate | `Error`; no partial image/geometry/tensor |
| Shape angle grid exceeds 721 samples or Canny thresholds invalid | Reject before SDK execution |
| Feature config invalid | Reject before SDK execution |
| No normal correlation/feature match | `Ok` with zero count and typed empty list |
| Unknown/duplicate persistent child or attribute | Reject detached load candidate |
| Wrong format/version/encoding, whitespace Base64 or noncanonical padding | Reject candidate |
| Empty/oversized bytes, length/digest mismatch or invalid PNG/dimensions | Reject candidate |
| No foreground over minimum area | `Error`; mask/region/ROI/transform cleared |
| OpenCV exception | Translate to nonempty run error; no exception crosses plugin ABI |

### 5. Good / Base / Bad Cases

- Good: create an asymmetric ROI template, save/load or export/import exact PNG bytes, find it, and draw
  a read-only rotate rectangle from portable results.
- Base: an unconfigured matcher has no `PersistentData`; Find reports missing template. A supplied
  axis-aligned rectangle rectifies to twice its half-lengths.
- Bad: decode unbounded Base64 before checking encoded size, retain a borrowed `cv::Mat`, publish a mask
  before its ROI/matrix validates, or make the UI parse OpenCV geometry.

### 6. Tests Required

- `opencv_template_rectification` always checks PNG ownership/rollback, SHA-256, ROI corners, transform
  bytes, role modes/active parameters, and disabled diagnostics. With OpenCV enabled it checks all four
  match modes and all three rectification modes on deterministic generated images.
- `projectxml` checks all seven aliases normalize to canonical roles/modes, exact template bytes survive
  project and flow round trips, runtime values remain transient, and bad format, digest, length, Base64,
  PNG, attribute, or duplicate extension preserves the installed project.
- `systemplugin` must load the current 39 canonical roles and 84 unique presets and instantiate every entry.
- Run supported Windows Qt 6.4/MSVC2019 Debug and Release builds with OpenCV enabled, then manually
  verify Create/Find state refresh, ROI subscriptions, overlay orientation, mask selection and DLL copy.

### 7. Wrong vs Correct

```cpp
// Wrong: mutate accepted state and result lists before the SDK operation completes.
m_templateAsset=png;
result->matches->clear();
cv::findHomography(...);

// Correct: validate owned candidates, then commit the coherent group once.
QByteArray candidateAsset;
XObjectList candidateMatches("matches",XMatchResult::type());
// Encode/match/validate candidates, then replace results and accepted bytes.
```
