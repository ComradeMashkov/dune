# `plot`

Deterministic SVG/HTML chart rendering.

`plot` builds deterministic chart specs in pure Dune and renders them to SVG or
HTML. The first backend set covers line, scatter, bar, and histogram charts,
file output through `fs.write_text`, headless-safe capture backends, and a
platform-native display backend for `show()` where the VM supports it.


**Preview** — a line chart built with `plot.line(...).title(...).legend(true)` and rendered by `plot.svg()`:

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="560" height="320" viewBox="0 0 560 320">
<title>Monthly growth</title>
<rect width="560" height="320" fill="#ffffff"/>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="524" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">1</text>
<text x="524" y="274" text-anchor="middle">6</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">12</text>
</g>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,204 156,135 248,169 340,100 432,117 524,48"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="280" y="28" text-anchor="middle" font-size="18" font-weight="700">Monthly growth</text>
<text x="280" y="304" text-anchor="middle" font-size="13">month</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">value</text>
</g>
<g class="legend" font-family="sans-serif" font-size="12" fill="#0f172a">
</g>
</svg>
</figure>

> Auto-generated from `stdlib/plot.dn` by `tools/gen_stdlib_docs.py`.

### `record Chart`

**Methods:**

- `static fn empty(): Chart`
- `fn title(value: text): Chart` — Return a copy of the chart with its title set. — e.g. `plot.svg(plot.line([1.0, 2.0]).title("Sales")).contains("Sales")  // 1`
- `fn x_label(value: text): Chart` — Return a copy with the x-axis label set. — e.g. `plot.line([1.0, 2.0]).x_label("time")`
- `fn y_label(value: text): Chart` — Return a copy with the y-axis label set. — e.g. `plot.line([1.0, 2.0]).y_label("value")`
- `fn legend(enabled: bool): Chart` — Return a copy with the legend shown or hidden. — e.g. `plot.line([1.0, 2.0]).label("trend").legend(true)`
- `fn size(width: int, height: int): Chart` — Return a copy sized to `width` x `height` pixels (both must be positive). — e.g. `plot.svg(plot.line([1.0, 2.0]).size(640, 480)).contains("width=\"640\"")  // 1`
- `fn label(value: text): Chart`
- `fn add_line(xs: [real64], ys: [real64]): Chart`
- `fn add_line(ys: [real64]): Chart`
- `fn add_scatter(xs: [real64], ys: [real64]): Chart`
- `fn add_scatter(ys: [real64]): Chart`
- `fn add_bar(xs: [real64], ys: [real64]): Chart`
- `fn add_bar(ys: [real64]): Chart`

### `fn empty(): Chart`

### `fn line(xs: [real64], ys: [real64]): Chart`

A line chart from paired x/y data.

**Example:**
```dune
plot.svg(plot.line([0.0, 1.0, 2.0], [3.0, 7.0, 5.0])).contains("<svg")  // 1
```

### `fn line(ys: [real64]): Chart`

A line chart from y values, with x taken as the index 0, 1, 2, ...

**Example:**
```dune
plot.svg(plot.line([1.0, 3.0, 2.0])).contains("<svg")  // 1
```

### `fn scatter(xs: [real64], ys: [real64]): Chart`

A scatter chart from paired x/y data.

**Example:**
```dune
plot.svg(plot.scatter([1.0, 2.0], [3.0, 4.0])).contains("<circle")  // 1
```

### `fn scatter(ys: [real64]): Chart`

A scatter chart from y values against their index.

**Example:**
```dune
plot.scatter([3.0, 1.0, 4.0])
```

### `fn bar(xs: [real64], ys: [real64]): Chart`

A bar chart from paired x/y data.

**Example:**
```dune
plot.svg(plot.bar([1.0, 2.0], [3.0, 5.0])).contains("<rect")  // 1
```

### `fn bar(ys: [real64]): Chart`

A bar chart from y values against their index.

**Example:**
```dune
plot.svg(plot.bar([3.0, 1.0, 2.0])).contains("<rect")  // 1
```

### `fn histogram(values: [real64], bins: int): Chart`

A histogram: bucket `values` into `bins` equal-width bars.

**Example:**
```dune
plot.svg(plot.histogram([1.0, 2.0, 2.0, 3.0], 3)).contains("<svg")  // 1
```

### `fn use_backend(name: text): unit`

Select the display backend for `show`. Supported MVP backends are:
- "none": return a clear unsupported/headless error;
- "svg": return the deterministic SVG text;
- "html": return a deterministic HTML wrapper containing the SVG.
- "native": open a platform-native plot window when the VM supports it.

### `fn backend(): text`

The active backend used by `show`. Reads `DUNE_PLOT_BACKEND` if set, otherwise the value last passed to `use_backend`.

**Example:**
```dune
plot.use_backend("svg"); plot.backend()  // "svg"
```

### `fn available_backends(): [text]`

The backend names `show` understands.

**Example:**
```dune
plot.available_backends().len()  // 4
```

### `fn svg(chart: Chart): text`

Render `chart` to deterministic standalone SVG text.

**Example:**
```dune
plot.svg(plot.line([1.0, 2.0])).starts_with("<svg")  // 1
```

### `fn html(chart: Chart): text`

Render `chart` to a deterministic standalone HTML document wrapping the SVG.

**Example:**
```dune
plot.html(plot.line([1.0, 2.0])).contains("<!doctype")  // 1
```

### `fn save_svg(chart: Chart, path: text): outcome.Outcome<text, text>`

Write `chart` as SVG to `path`, returning an `Outcome` for the write.

**Example:**
```dune
plot.save_svg(plot.line([1.0, 2.0]), "chart.svg").is_done()  // 1
```

### `fn save_html(chart: Chart, path: text): outcome.Outcome<text, text>`

Write `chart` as an HTML document to `path`, returning an `Outcome`.

**Example:**
```dune
plot.save_html(plot.line([1.0, 2.0]), "chart.html").is_done()  // 1
```

### `fn show_native(chart: Chart): outcome.Outcome<text, text>`

Open `chart` in a native plot window where the VM supports it; returns an `Outcome` describing whether the window opened (a headless-safe error if not).

### `fn show(chart: Chart): outcome.Outcome<text, text>`
