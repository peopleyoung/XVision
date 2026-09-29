# VisionMaster Geometry Creation And Measurement

Catalog note (2026-09-29): [Operator Catalog](./operator-catalog.md) governs current IDs and counts. Historical aliases in examples are not accepted creation IDs.

## Scenario: Strongly Typed 2D Geometry And Deterministic Measurement

### 1. Scope / Trigger

Apply this contract when changing `GeometryCreate`, `GeometryMeasure`, their eight VisionMaster
presets, 2D measurement formulas, tolerance results, annotations, or geometry subscriptions. These
roles use existing SDK-neutral `XvData` values and must not acquire an OpenCV or HALCON dependency.

### 2. Signatures

The canonical roles and selectors are:

```cpp
GeometryCreate::ShapeType  // Point, Line, Circle, Rectangle
GeometryMeasure::Mode      // CircleCircle, LineCircle, LineLineAngle, LineLine,
                           // PointCircle, PointLine, PointPoint
```

The pure formula boundary returns an unscaled raw value and deterministic annotation points:

```cpp
struct Candidate {
    double rawValue;
    QPointF annotationStart;
    QPointF annotationEnd;
};

bool pointLine(const QPointF &point,const QPointF &lineStart,const QPointF &lineEnd,
               Candidate &candidate,QString &error);
bool circleCircle(const QPointF &firstCenter,double firstRadius,
                  const QPointF &secondCenter,double secondRadius,
                  Candidate &candidate,QString &error);
```

The other five functions follow the same candidate/error signature in
`GeometryMeasurementUtils.h`.

### 3. Contracts

- `CreateShapeNodeData` is a role preset over `GeometryCreate`. Its four result ports always exist as
  `XPoint2D point`, `XLine2D line`, `XCircle2D circle`, and `XRect2D rectangle`; `createdType` selects
  the active value. Inactive ports reset to valid neutral values after each successful run.
- The seven measurement names are mode presets over `GeometryMeasure`. Project and flow XML persist
  only the canonical role, effective selector, scalar configuration, and direct typed geometry values.
- `XLine2D` is an infinite supporting line. For `d=b-a`, signed line distance is
  `cross(d,q-a)/|d|`. Lines shorter than the scale-aware epsilon are rejected.
- Raw distance formulas are: PointPoint `|p2-p1|`; PointLine signed line distance; PointCircle
  `|p-c|-r`; LineCircle `abs(signed_line_distance(c))-r`; CircleCircle `|c2-c1|-r1-r2`; LineLine
  zero when nonparallel and signed line-1-to-line-2 distance when parallel.
- LineLineAngle is the smallest undirected angle in `[0,pi/2]`. Its raw value is radians. Distance
  `absoluteValue` is applied after raw computation, then multiplied by strictly positive `scale`.
  Angles ignore scale and convert to canonical `deg` or `rad`.
- Inclusive finite `lowerLimit` and `upperLimit` determine `passed`. The same value, unit, limits, and
  status are published in `XMeasurementResult measurement` together with `value`, `rawValue`,
  `annotationStart`, and `annotationEnd`.
- `inputImage` is optional. When present, `outputImage` is an owned ARGB32 copy containing operands,
  the dimension line, endpoints, value/unit, and localized OK/NG. Geometry math succeeds with a null
  input and publishes a null output image.
- Prepare all typed values and the optional image before replacing results. Invalid configuration or
  geometry preserves the previous coherent result group and returns a nonempty run message.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Invalid shape/mode enum | `Error`; previous results unchanged |
| Non-finite point/coordinate/limit/scale | `Error` before result mutation |
| Equal line endpoints or scale-aware degenerate line | `Error`; candidate argument unchanged |
| Non-positive circle radius or rectangle extent | `Error`; creator results unchanged |
| Distance scale non-positive or trimmed unit empty | `Error`; previous measurement unchanged |
| Angle unit outside Degrees/Radians | `Error`; scale is ignored for valid angle modes |
| Lower limit greater than upper or precision outside 0..9 | `Error`; no annotation replacement |
| Tangent surfaces | Raw clearance is exactly zero within floating-point tolerance |
| Intersecting supporting lines | LineLine raw value is zero and annotation is the intersection |
| Parallel supporting lines | Signed perpendicular value and projection endpoints are published |
| No input image | `Ok` for valid geometry; `outputImage` is null |
| Annotation cannot be represented safely | `Error`; previous structured and image results unchanged |

### 5. Good / Base / Bad Cases

- Good: subscribe a created line and point, measure signed PointLine distance with `scale=0.25` and
  `unit=mm`, and publish a boundary-inclusive OK result plus an owned annotation image.
- Base: default PointPoint returns 5 px for `(0,0)` to `(3,4)`; default parallel lines return 10 px;
  angle mode reports 0 deg and does not consume scale.
- Bad: treat stored line endpoints as a finite segment, multiply radians by distance scale, erase the
  last valid result before validating tolerance, or persist annotation/result images in XML v1.

### 6. Tests Required

- `geometry_measurement` asserts all seven formulas, tangent/overlap, parallel/intersecting lines,
  acute/perpendicular/collinear angles, degenerate rejection, four creator types, selectors, signed and
  absolute values, scaling, angle units, inclusive OK/NG, owned pixels, and rollback.
- `projectxml` asserts all eight aliases normalize, typed line/point/circle parameters survive project
  and flow round trips, selectors/scalars survive, and runtime geometry/measurement/image results are
  absent from XML.
- `systemplugin` loads the current 39 canonical roles and 84 unique presets, then instantiates every role
  and alias with the expected selector.
- Run Windows Qt 6.4/MSVC2019 Debug and Release builds and manually verify typed subscriptions, selector
  refresh, direct scalar editing, result display, and green/red annotations.

### 7. Wrong vs Correct

```cpp
// Wrong: scale early, lose the raw geometry sign, and clear the old result first.
result->value->setValue(qAbs(raw*scale));
result->outputImage->setValue(QImage());

// Correct: validate raw geometry, conversion, tolerance, typed values and image first.
Candidate candidate;
XMeasurementResult prepared;
QImage annotated;
// Commit the coherent group only after every candidate is valid.
```
