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


**Preview** — a GUI scene built from canvas widgets (`panel`, `button`, `checkbox`, `slider`, `progress`, `tabs`, `badge`, `metric`) and rendered by `canvas.svg()`:

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="420" height="250" viewBox="0 0 420 250">
<title>Controls</title>
<rect x="0" y="0" width="420" height="250" fill="#f8fafc"/>
<g class="canvas-grid">
<line x1="0" y1="0" x2="0" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="42" y1="0" x2="42" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="84" y1="0" x2="84" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="126" y1="0" x2="126" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="168" y1="0" x2="168" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="210" y1="0" x2="210" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="252" y1="0" x2="252" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="294" y1="0" x2="294" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="336" y1="0" x2="336" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="378" y1="0" x2="378" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="420" y1="0" x2="420" y2="250" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="0" x2="420" y2="0" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="42" x2="420" y2="42" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="84" x2="420" y2="84" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="126" x2="420" y2="126" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="168" x2="420" y2="168" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
<line x1="0" y1="210" x2="420" y2="210" stroke="#e2e8f0" fill="none" stroke-width="0.5" opacity="0.45"/>
</g>
<rect x="20" y="20" width="190" height="96" rx="6" ry="6" stroke="#cbd5e1" fill="#ffffff" stroke-width="1" opacity="1"/>
<text x="32" y="42" fill="#0f172a" font-family="sans-serif" font-size="14" opacity="1" text-anchor="start">Tools</text>
<line x1="20" y1="54" x2="210" y2="54" stroke="#e2e8f0" fill="none" stroke-width="1" opacity="1"/>
<rect x="36" y="64" width="74" height="30" rx="5" ry="5" stroke="#1d4ed8" fill="#2563eb" stroke-width="1" opacity="1"/>
<text x="73" y="83" fill="#ffffff" font-family="sans-serif" font-size="12" opacity="1" text-anchor="middle">Run</text>
<rect x="120" y="64" width="74" height="30" rx="5" ry="5" stroke="#94a3b8" fill="#f8fafc" stroke-width="1" opacity="1"/>
<text x="157" y="83" fill="#0f172a" font-family="sans-serif" font-size="12" opacity="1" text-anchor="middle">Stop</text>
<rect x="36" y="104" width="14" height="14" rx="3" ry="3" stroke="#64748b" fill="#ffffff" stroke-width="1" opacity="1"/>
<polyline points="39,111 42,114 47,108" stroke="#2563eb" fill="none" stroke-width="2" opacity="1"/>
<text x="58" y="116" fill="#0f172a" font-family="sans-serif" font-size="12" opacity="1" text-anchor="start">snap</text>
<text x="240" y="50" fill="#0f172a" font-family="sans-serif" font-size="12" opacity="1" text-anchor="start">zoom</text>
<rect x="240" y="56" width="150" height="6" rx="3" ry="3" stroke="none" fill="#e2e8f0" stroke-width="0" opacity="1"/>
<rect x="240" y="56" width="93" height="6" rx="3" ry="3" stroke="none" fill="#2563eb" stroke-width="0" opacity="1"/>
<circle cx="333" cy="59" r="7" stroke="#1d4ed8" fill="#ffffff" stroke-width="1" opacity="1"/>
<text x="390" y="80" fill="#475569" font-family="sans-serif" font-size="11" opacity="1" text-anchor="end">62</text>
<rect x="240" y="104" width="150" height="12" rx="6" ry="6" stroke="none" fill="#e2e8f0" stroke-width="0" opacity="1"/>
<rect x="240" y="104" width="93" height="12" rx="6" ry="6" stroke="none" fill="#16a34a" stroke-width="0" opacity="1"/>
<rect x="20" y="150" width="63.3333333333333" height="28" rx="5" ry="5" stroke="#1d4ed8" fill="#2563eb" stroke-width="1" opacity="1"/>
<text x="51.6666666666667" y="168" fill="#ffffff" font-family="sans-serif" font-size="12" opacity="1" text-anchor="middle">Draw</text>
<rect x="83.3333333333333" y="150" width="63.3333333333333" height="28" rx="5" ry="5" stroke="#94a3b8" fill="#f8fafc" stroke-width="1" opacity="1"/>
<text x="115" y="168" fill="#0f172a" font-family="sans-serif" font-size="12" opacity="1" text-anchor="middle">Data</text>
<rect x="146.666666666667" y="150" width="63.3333333333333" height="28" rx="5" ry="5" stroke="#94a3b8" fill="#f8fafc" stroke-width="1" opacity="1"/>
<text x="178.333333333333" y="168" fill="#0f172a" font-family="sans-serif" font-size="12" opacity="1" text-anchor="middle">View</text>
<rect x="240" y="150" width="44" height="22" rx="11" ry="11" stroke="none" fill="#16a34a" stroke-width="0" opacity="1"/>
<text x="262" y="165" fill="#ffffff" font-family="sans-serif" font-size="11" opacity="1" text-anchor="middle">LIVE</text>
<rect x="300" y="140" width="90" height="56" rx="6" ry="6" stroke="#cbd5e1" fill="#ffffff" stroke-width="1" opacity="1"/>
<text x="312" y="162" fill="#64748b" font-family="sans-serif" font-size="11" opacity="1" text-anchor="start">fps</text>
<text x="312" y="192" fill="#0f172a" font-family="sans-serif" font-size="24" opacity="1" text-anchor="start">60</text>
</svg>
</figure>

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
- `fn background(color: text): Canvas` — Return a copy with a new background color. — e.g. `canvas.new("s", 100, 100).background("#f8fafc").to_svg().contains("#f8fafc")  // 1`
- `fn view_box(x: real64, y: real64, width: real64, height: real64): Canvas` — Return a copy with a custom SVG viewBox.
- `fn clear(): Canvas` — Drop all drawing commands but keep canvas size, title, background, and viewBox.
- `fn line(x1: real64, y1: real64, x2: real64, y2: real64, style: Style): Canvas` — Draw a straight line.
- `fn line(x1: real64, y1: real64, x2: real64, y2: real64, color: text): Canvas` — Draw a straight line with a stroke color. — e.g. `canvas.new("s", 100, 100).line(0.0, 0.0, 50.0, 50.0, "#000").to_svg().contains("<line")  // 1`
- `fn polyline(xs: [real64], ys: [real64], style: Style): Canvas` — Draw a connected polyline.
- `fn polyline(xs: [real64], ys: [real64], color: text): Canvas` — Draw a connected polyline with a stroke color.
- `fn polygon(xs: [real64], ys: [real64], style: Style): Canvas` — Draw a closed polygon.
- `fn polygon(xs: [real64], ys: [real64], stroke_color: text, fill_color: text): Canvas` — Draw a closed polygon with stroke and fill colors.
- `fn rect(x: real64, y: real64, width: real64, height: real64, style: Style): Canvas` — Draw a rectangle.
- `fn rect(x: real64, y: real64, width: real64, height: real64, stroke_color: text, fill_color: text): Canvas` — Draw a rectangle with stroke and fill colors. — e.g. `canvas.new("s", 100, 100).rect(10.0, 10.0, 40.0, 30.0, "#000", "#eee").to_svg().contains("<rect")  // 1`
- `fn rounded_rect(x: real64, y: real64, width: real64, height: real64, radius: real64, style: Style): Canvas` — Draw a rounded rectangle.
- `fn rounded_rect(x: real64, y: real64, width: real64, height: real64, radius: real64, stroke_color: text, fill_color: text): Canvas` — Draw a rounded rectangle with stroke and fill colors.
- `fn circle(cx: real64, cy: real64, radius: real64, style: Style): Canvas` — Draw a circle.
- `fn circle(cx: real64, cy: real64, radius: real64, stroke_color: text, fill_color: text): Canvas` — Draw a circle with stroke and fill colors. — e.g. `canvas.new("s", 100, 100).circle(50.0, 50.0, 20.0, "#000", "none").to_svg().contains("<circle")  // 1`
- `fn ellipse(cx: real64, cy: real64, rx: real64, ry: real64, style: Style): Canvas` — Draw an ellipse.
- `fn ellipse(cx: real64, cy: real64, rx: real64, ry: real64, stroke_color: text, fill_color: text): Canvas` — Draw an ellipse with stroke and fill colors.
- `fn text(x: real64, y: real64, value: text, style: Style): Canvas` — Draw left-aligned text.
- `fn text(x: real64, y: real64, value: text, color: text): Canvas` — Draw left-aligned text with a fill color. — e.g. `canvas.new("s", 100, 100).text(10.0, 20.0, "hi", "#000").to_svg().contains(">hi<")  // 1`
- `fn text_center(x: real64, y: real64, value: text, style: Style): Canvas` — Draw center-aligned text.
- `fn text_right(x: real64, y: real64, value: text, style: Style): Canvas` — Draw right-aligned text.
- `fn path(data: text, style: Style): Canvas` — Draw a raw SVG path. Useful for icons and custom shapes.
- `fn image(x: real64, y: real64, width: real64, height: real64, href: text): Canvas` — Draw an SVG image reference.
- `fn grid(step: real64, color: text): Canvas` — Draw a background grid aligned to the current viewBox. — e.g. `canvas.new("s", 100, 100).grid(20.0, "#eee").to_svg().contains("canvas-grid")  // 1`
- `fn axes(origin_x: real64, origin_y: real64, color: text): Canvas` — Draw horizontal and vertical axes at the given origin.
- `fn point(x: real64, y: real64, radius: real64, color: text): Canvas` — Draw a filled point.
- `fn arrow(x1: real64, y1: real64, x2: real64, y2: real64, color: text): Canvas` — Draw a line with a simple endpoint marker.
- `fn panel(x: real64, y: real64, width: real64, height: real64, title: text): Canvas` — Draw a titled panel for dashboards and tool surfaces. — e.g. `canvas.new("s", 100, 100).panel(4.0, 4.0, 80.0, 40.0, "Box").to_svg().contains(">Box<")  // 1`
- `fn button(x: real64, y: real64, width: real64, height: real64, label: text, active: bool): Canvas` — Draw a button. The active state uses a stronger fill and inverted text. — e.g. `canvas.new("s", 100, 100).button(10.0, 10.0, 60.0, 24.0, "OK", true).to_svg().contains(">OK<")  // 1`
- `fn checkbox(x: real64, y: real64, label: text, checked: bool): Canvas` — Draw a checkbox with a label. — e.g. `canvas.new("s", 100, 100).checkbox(10.0, 10.0, "on", true).to_svg().contains(">on<")  // 1`
- `fn toggle(x: real64, y: real64, width: real64, label: text, on: bool): Canvas` — Draw a compact on/off toggle.
- `fn slider(x: real64, y: real64, width: real64, min: real64, max: real64, value: real64, label: text): Canvas` — Draw a slider with a label and current value. — e.g. `canvas.new("s", 200, 60).slider(10.0, 30.0, 150.0, 0.0, 100.0, 42.0, "zoom").to_svg().contains(">zoom<")  // 1`
- `fn progress(x: real64, y: real64, width: real64, height: real64, value: real64, color: text): Canvas` — Draw a 0..1 progress bar.
- `fn tabs(x: real64, y: real64, width: real64, labels: [text], active_index: int): Canvas` — Draw a row of tabs. The active index is zero-based.
- `fn toolbar(x: real64, y: real64, labels: [text], active_index: int): Canvas` — Draw a toolbar as a sequence of compact buttons.
- `fn badge(x: real64, y: real64, label: text, color: text): Canvas` — Draw a small status badge.
- `fn input(x: real64, y: real64, width: real64, value: text, placeholder: text): Canvas` — Draw a text input-like field with a placeholder.
- `fn select_box(x: real64, y: real64, width: real64, label: text, value: text): Canvas` — Draw a read-only select box.
- `fn metric(x: real64, y: real64, width: real64, height: real64, label: text, value: text): Canvas` — Draw a metric tile with a label and large value.
- `fn table(x: real64, y: real64, column_widths: [real64], headers: [text], rows: [[text]]): Canvas` — Draw a simple table with fixed column widths.
- `fn to_svg(): text` — Render to deterministic SVG text. — e.g. `canvas.new("s", 100, 80).to_svg().starts_with("<svg")  // 1`
- `fn save_svg(path: text): outcome.Outcome<text, text>` — Save the SVG document to disk. — e.g. `canvas.new("s", 100, 80).save_svg("scene.svg").is_done()  // 1`
- `fn show_native(): outcome.Outcome<text, text>` — Open the SVG in the VM native canvas window when supported.

### `fn new(title: text, width: int, height: int): Canvas`

Create an empty canvas with an explicit pixel size.

**Example:**
```dune
canvas.svg(canvas.new("scene", 320, 200)).contains("width=\"320\"")  // 1
```

### `fn new(title: text): Canvas`

Create an empty canvas with the default size.

**Example:**
```dune
canvas.new("scene")
```

### `fn style(stroke_color: text, fill_color: text, stroke_width: real64): Style`

Build a custom style from stroke, fill, and stroke width.

**Example:**
```dune
canvas.style("#0f172a", "#eef2ff", 2.0)
```

### `fn stroke(color: text): Style`

Build a stroke-only style.

**Example:**
```dune
canvas.stroke("#2563eb").with_width(2.0)
```

### `fn fill(color: text): Style`

Build a fill-only style.

**Example:**
```dune
canvas.fill("#dbeafe")
```

### `fn svg(scene: Canvas): text`

Render a canvas to deterministic SVG text.

**Example:**
```dune
canvas.svg(canvas.new("s", 100, 80)).starts_with("<svg")  // 1
```

### `fn native_tools(): [text]`

List the interactive tools exposed by the native canvas window.

**Example:**
```dune
canvas.native_tools().len()  // 7
```
