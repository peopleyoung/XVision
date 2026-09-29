# Qt Component Guidelines

## Designer Form Pattern

Designer-backed widgets keep a generated-UI pointer, call `setupUi()` in the
constructor, perform programmatic setup in `initFrm()`, and delete the UI
wrapper in the destructor:

```cpp
FrmXvFlowConfig::FrmXvFlowConfig(XvFlow *flow, QWidget *parent)
    : XFramelessWidget(parent), m_flow(flow),
      ui(new Ui::FrmXvFlowConfig) {
    ui->setupUi(this->centralWidget());
    initFrm();
}

FrmXvFlowConfig::~FrmXvFlowConfig() {
    delete ui;
}
```

References:

- `XVision/XVision/UiWork/UiXvFlow/FrmXvFlowConfig.*`.
- `XVision/XvFuncCollection/XvFuncSystem/ImageAcquisition/ImageAcquisitionWdg.*`.
- `XVision/XVision/UiCore/FrmVisionDisplay.*`.

Use `centralWidget()` for `XFramelessWidget` forms, matching the existing
widgets. Keep generated layout in `.ui`; use C++ setup for runtime-created
controls, signals, domain bindings, language text, icons, and values that cannot
be represented statically.

## Initialization and Refresh

`initFrm()` is the normal one-time setup location:

- Validate the injected domain pointer before using it.
- Set localized labels, window title, icons, ranges, and tooltips.
- Populate combo boxes/tabs with typed item data.
- Establish connections after widgets have their initial values.

Operator widgets implement `onShow()` to hydrate controls from the current
operator state. `BaseSystemFuncWdg::showEvent()` temporarily sets
`m_bShowing=false`, calls `onShow()`, then re-enables user-driven binding
updates. Preserve this guard so refreshing a form does not unsubscribe or
overwrite domain state.

`BaseDataIntCalcWdg.cpp` demonstrates the complete pattern: initialize labels
from `XObject` metadata, populate localized operation choices, bind parameter
sources, parse direct values, refresh results after a run, and restore all
values on show.

## Domain/Widget Pairing

An operator UI is paired with its `XvFunc` subtype:

- The operator owns parameter/result objects and execution.
- The widget receives the typed operator pointer and uses
  `getFunc<T>()` before accessing it.
- The operator lazily creates one widget in `onShowFunc()`, then calls
  `show()`/`raise()`.
- The operator destroys the cached widget in its destructor.

`BaseDataIntCalc` and `ImageAcquisition` are references. Do not move operator
execution or result ownership into the widget.

## Composition and Helpers

Reuse established UI helpers rather than repeating setup:

- `BaseSystemFuncWdg` for run/top controls, binding combo boxes, and form events.
- Functions in `XVision/XVision/Utils/UiUtils.h` for common button/layout setup.
- `XvViewManager` accessors for application screens/managers.
- `XMat...` controls from XWidget where the current screen already uses them.

Application UI coordinators (`UiXvWorkManager`, `UiXvDisplayManager`) may create
toolbar controls programmatically because their content depends on domain/dock
state. Keep that code in the coordinator rather than inflating a passive form.

## Lifetime

- Prefer a `QObject` parent for runtime-created controls; examples pass `this`
  to `XMatTabs`, buttons, timers, and managers.
- A transient dialog that owns itself may call `deleteLater()` in its close
  event, as `FrmXvFlowConfig` does.
- If a widget stores a non-owning domain pointer, react to the domain object's
  destruction and stop using it. `FrmXvFuncResult` and
  `UiXvDisplayManager` clear current operator bindings on `destroyed`.
- Do not manually delete a child already exclusively owned by a Qt parent unless
  the existing class explicitly owns that lifetime.

## User-Facing Content

Use `getLang(key, fallback)` for titles, labels, menu actions, tooltips, and
messages. Use `QIcon(":/..." )`/`QPixmap(":/..." )` for registered resource
assets. `AppMainWindow.cpp` and `BaseSystemFuncWdg.cpp` are the main examples.

The repository has no formal accessibility framework. Existing accessible
signals are visible labels, tooltips on icon controls, standard Qt focus/input
widgets, and text alternatives. Preserve these behaviors and do not introduce
an unlabeled icon-only action.

## Common Mistakes

- Editing generated `ui_*.h` output.
- Connecting signals before initial control population when that triggers
  unintended model writes.
- Keeping durable operator/flow state only in a widget.
- Creating a second operator configuration window on every open action.
- Hard-coding visible text without the language helper.
- Adding an image/style without registering it in the owning `.qrc`.

## Technology-Blue Theme and Chinese Labels

Apply appearance through Utils/UiAppearance.cpp before creating widgets. The
application palette, TechBlue.css, dock QSS and XMat defaults work together.
QSS does not recolor custom-painted flow nodes or image canvases by itself.
Use the consistent 24x24 round-cap SVG family for category and toolbar actions.

getUiText() translates built-in display labels and reflected enum captions.
Do not translate serialized roles, aliases, property keys, user data values,
or enum itemData. Continue using getLang() for existing keyed translations.
Qt dialogs use embedded Chinese QM resources; retain their TS source and notices
in Res/translations and packaged licenses. Validate with ui_appearance and the
systemplugin catalog checks.

## Resizable Operator Forms and Deferred Rebuilds
BaseSystemFuncWdg::centralWidget() returns operatorParameterContent, hosted by the shared operatorParameterScroll. Add Designer/dynamic layouts there. Use initPreferredSize(), not fixed window sizes or nested scroll areas. The base bounds the first window to the current screen and enforces readable editor heights and wrapping form labels; long pages must remain scrollable at 640x420 and at high DPI.

A mode or binding signal must call scheduleParameterRebuild(). This posts one context-bound zero-delay callback and coalesces repeated signals. Never synchronously removeRow()/delete the editor emitting that signal: changing TcpText frameMode reproduced SIGSEGV before this fix. Override rebuildParameters() and keep onShow() hydration guarded. Domain and subscription-source references use QPointer; pending callbacks must stop when the domain disappears.

The toolbox uses one lazily created QFrame and one QListWidget, reused for categories and debounced global search. Do not allocate one QWidget per operator or enter processEvents() during panel construction. The visually external panel must be deleted with its sidebar; QPointer also handles external-parent destruction. Completed QDrag instances use deleteLater().

Use the 24x24 blue SVG family, explicit tooltip/accessibility text and registered resources. ui_responsiveness checks all 123 catalog entries, scroll access, non-compressed fields, signal-triggered rebuilds, first-click visibility, search, panel reuse and lifetime. Review screenshots from the actual application and long parameter forms.

Qt 6 on Windows may post additional layout requests during polish/show. Geometry regressions must wait for the promised final state with bounded QTRY assertions; one processEvents() call is not a completion barrier. Keep the positive scroll-range/content-height assertions, rather than dropping them.

Scroll regression fixtures must first force content overflow independently of platform font/frame metrics (the communication fixture uses a 640x320 viewport). A form that fits a 640x420 window correctly has no scrollbar; normal-size layout checks and forced-overflow checks are separate contracts.

## Toolbar Icon Rendering
Use registered 24x24 round-cap SVGs and standard QToolButton controls with explicit
icon-only mode, accessible names and localized tooltips. Scope toolbar QSS through
properties so main-strip ID selectors do not override pressed/disabled states.
On Qt 5, enable AA_EnableHighDpiScaling and AA_UseHighDpiPixmaps before constructing
the application. Inspect actual 150%/200% captures: an SVG resource alone does not
prevent a low-resolution pixmap path. Keep these attributes guarded out on Qt 6.

## Runtime Themes and Orthogonal Links

UiAppearance owns five stable theme IDs and user-scoped INI preferences. Theme changes update QSS/palette, property-tagged dock styles, graphics scenes and original-vector icon engines. Unknown saved IDs fall back to tech-blue. Verify light-theme logs, custom controls and icons.

Qt5 QApplication palette changes do not reliably notify non-widget graphics through QGuiApplication::paletteChanged. Graph objects expose refreshThemePalette, invoked once per retained scene on a theme switch. New objects read QApplication::palette. The graph library must not depend on app theme IDs.

Orthogonal links share one QPainterPath for paint, arrows, length, hit testing and bounds. Emit posChanged on ItemPositionHasChanged so subscribers read the committed position. Call prepareGeometryChange before replacing paths. Current routing avoids endpoint rectangles, not all scene obstacles.
