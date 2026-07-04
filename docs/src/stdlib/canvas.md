# `canvas`

Deterministic SVG canvas and immediate-mode GUI widgets.

`canvas` is a pure-Dune immediate-mode drawing layer for scripts that need more
than charts. It records deterministic drawing commands, renders them to SVG, can
save the SVG through `fs`, and can open it in the VM native canvas window where
that backend is available.

The module includes low-level primitives (`line`, `polyline`, `polygon`,
`rect`, `rounded_rect`, `circle`, `ellipse`, `path`, `image`, and text alignment
helpers) and higher-level GUI widgets (`panel`, `button`, `checkbox`, `toggle`,
`slider`, `progress`, `tabs`, `toolbar`, `badge`, `input`, `select_box`,
`metric`, and `table`). The widgets are rendered as SVG, so they are stable in
tests and safe in headless runs.

> Auto-generated from `stdlib/canvas.dn` by `tools/gen_stdlib_docs.py`.

### `record Style`

**Methods:**

- `static fn default(): Style` — Build the default drawing style: dark stroke, no fill, one-pixel line.
- `fn with_stroke(color: text): Style` — Return a copy with a different stroke color.
- `fn with_fill(color: text): Style` — Return a copy with a different fill color.
- `fn with_width(width: real64): Style` — Return a copy with a different stroke width.
- `fn with_font_size(size: int): Style` — Return a copy with a different text size.
- `fn with_opacity(opacity: real64): Style` — Return a copy with a different opacity.
- `fn with_dash(dash: text): Style` — Return a copy with an SVG stroke-dasharray such as "4 2".

### `record Canvas`

**Methods:**

- `static fn new(title: text, width: int, height: int): Canvas` — Create an empty canvas with an explicit pixel size.
- `fn title(value: text): Canvas` — Return a copy with a new native window / SVG title.
- `fn background(color: text): Canvas` — Return a copy with a new background color.
- `fn view_box(x: real64, y: real64, width: real64, height: real64): Canvas` — Return a copy with a custom SVG viewBox.
- `fn clear(): Canvas` — Drop all drawing commands but keep canvas size, title, background, and viewBox.
- `fn line(x1: real64, y1: real64, x2: real64, y2: real64, style: Style): Canvas` — Draw a straight line.
- `fn line(x1: real64, y1: real64, x2: real64, y2: real64, color: text): Canvas` — Draw a straight line with a stroke color.
- `fn polyline(xs: [real64], ys: [real64], style: Style): Canvas` — Draw a connected polyline.
- `fn polyline(xs: [real64], ys: [real64], color: text): Canvas` — Draw a connected polyline with a stroke color.
- `fn polygon(xs: [real64], ys: [real64], style: Style): Canvas` — Draw a closed polygon.
- `fn polygon(xs: [real64], ys: [real64], stroke_color: text, fill_color: text): Canvas` — Draw a closed polygon with stroke and fill colors.
- `fn rect(x: real64, y: real64, width: real64, height: real64, style: Style): Canvas` — Draw a rectangle.
- `fn rect(x: real64, y: real64, width: real64, height: real64, stroke_color: text, fill_color: text): Canvas` — Draw a rectangle with stroke and fill colors.
- `fn rounded_rect(x: real64, y: real64, width: real64, height: real64, radius: real64, style: Style): Canvas` — Draw a rounded rectangle.
- `fn rounded_rect(x: real64, y: real64, width: real64, height: real64, radius: real64, stroke_color: text, fill_color: text): Canvas` — Draw a rounded rectangle with stroke and fill colors.
- `fn circle(cx: real64, cy: real64, radius: real64, style: Style): Canvas` — Draw a circle.
- `fn circle(cx: real64, cy: real64, radius: real64, stroke_color: text, fill_color: text): Canvas` — Draw a circle with stroke and fill colors.
- `fn ellipse(cx: real64, cy: real64, rx: real64, ry: real64, style: Style): Canvas` — Draw an ellipse.
- `fn ellipse(cx: real64, cy: real64, rx: real64, ry: real64, stroke_color: text, fill_color: text): Canvas` — Draw an ellipse with stroke and fill colors.
- `fn text(x: real64, y: real64, value: text, style: Style): Canvas` — Draw left-aligned text.
- `fn text(x: real64, y: real64, value: text, color: text): Canvas` — Draw left-aligned text with a fill color.
- `fn text_center(x: real64, y: real64, value: text, style: Style): Canvas` — Draw center-aligned text.
- `fn text_right(x: real64, y: real64, value: text, style: Style): Canvas` — Draw right-aligned text.
- `fn path(data: text, style: Style): Canvas` — Draw a raw SVG path. Useful for icons and custom shapes.
- `fn image(x: real64, y: real64, width: real64, height: real64, href: text): Canvas` — Draw an SVG image reference.
- `fn grid(step: real64, color: text): Canvas` — Draw a background grid aligned to the current viewBox.
- `fn axes(origin_x: real64, origin_y: real64, color: text): Canvas` — Draw horizontal and vertical axes at the given origin.
- `fn point(x: real64, y: real64, radius: real64, color: text): Canvas` — Draw a filled point.
- `fn arrow(x1: real64, y1: real64, x2: real64, y2: real64, color: text): Canvas` — Draw a line with a simple endpoint marker.
- `fn panel(x: real64, y: real64, width: real64, height: real64, title: text): Canvas` — Draw a titled panel for dashboards and tool surfaces.
- `fn button(x: real64, y: real64, width: real64, height: real64, label: text, active: bool): Canvas` — Draw a button. The active state uses a stronger fill and inverted text.
- `fn checkbox(x: real64, y: real64, label: text, checked: bool): Canvas` — Draw a checkbox with a label.
- `fn toggle(x: real64, y: real64, width: real64, label: text, on: bool): Canvas` — Draw a compact on/off toggle.
- `fn slider(x: real64, y: real64, width: real64, min: real64, max: real64, value: real64, label: text): Canvas` — Draw a slider with a label and current value.
- `fn progress(x: real64, y: real64, width: real64, height: real64, value: real64, color: text): Canvas` — Draw a 0..1 progress bar.
- `fn tabs(x: real64, y: real64, width: real64, labels: [text], active_index: int): Canvas` — Draw a row of tabs. The active index is zero-based.
- `fn toolbar(x: real64, y: real64, labels: [text], active_index: int): Canvas` — Draw a toolbar as a sequence of compact buttons.
- `fn badge(x: real64, y: real64, label: text, color: text): Canvas` — Draw a small status badge.
- `fn input(x: real64, y: real64, width: real64, value: text, placeholder: text): Canvas` — Draw a text input-like field with a placeholder.
- `fn select_box(x: real64, y: real64, width: real64, label: text, value: text): Canvas` — Draw a read-only select box.
- `fn metric(x: real64, y: real64, width: real64, height: real64, label: text, value: text): Canvas` — Draw a metric tile with a label and large value.
- `fn table(x: real64, y: real64, column_widths: [real64], headers: [text], rows: [[text]]): Canvas` — Draw a simple table with fixed column widths.
- `fn to_svg(): text` — Render to deterministic SVG text.
- `fn save_svg(path: text): outcome.Outcome<text, text>` — Save the SVG document to disk.
- `fn show_native(): outcome.Outcome<text, text>` — Open the SVG in the VM native canvas window when supported.

### `fn new(title: text, width: int, height: int): Canvas`

Create an empty canvas with an explicit pixel size.

### `fn new(title: text): Canvas`

Create an empty canvas with the default size.

### `fn style(stroke_color: text, fill_color: text, stroke_width: real64): Style`

Build a custom style from stroke, fill, and stroke width.

### `fn stroke(color: text): Style`

Build a stroke-only style.

### `fn fill(color: text): Style`

Build a fill-only style.

### `fn svg(scene: Canvas): text`

Render a canvas to deterministic SVG text.

### `fn native_tools(): [text]`

List the interactive tools exposed by the native canvas window.
