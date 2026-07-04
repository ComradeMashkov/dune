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

## Multiple series

A chart holds any number of series. Overlay them by chaining `add_line`,
`add_scatter`, and `add_bar` on one chart — each series is auto-assigned a
colour from the palette, `.label(...)` names the most recently added series, and
`legend(true)` draws the key:

```dn
import io;
import plot;

chart = plot.line([3.0, 7.0, 5.0, 9.0, 8.0, 12.0]).label("revenue")
    .add_line([2.0, 4.0, 3.5, 6.0, 5.5, 7.0]).label("cost")
    .add_scatter([0.0, 1.0, 2.0, 3.0, 4.0, 5.0], [4.0, 6.5, 4.5, 8.0, 7.0, 10.5]).label("samples")
    .title("Quarterly results")
    .x_label("quarter").y_label("value")
    .legend(true).size(560, 320);

io.println(plot.svg(chart).contains("<svg")); // 1
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="560" height="320" viewBox="0 0 560 320">
<title>Quarterly results</title>
<rect width="560" height="320" fill="#ffffff"/>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="524" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">0</text>
<text x="524" y="274" text-anchor="middle">6</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">12</text>
</g>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="141,204 217,135 294,169 371,100 447,117 524,48"/>
<polyline class="series line" fill="none" stroke="#dc2626" stroke-width="2" points="141,221 217,187 294,195 371,152 447,161 524,135"/>
<g class="series scatter" fill="#16a34a">
<circle cx="64" cy="187" r="3"/>
<circle cx="141" cy="143" r="3"/>
<circle cx="217" cy="178" r="3"/>
<circle cx="294" cy="117" r="3"/>
<circle cx="371" cy="135" r="3"/>
<circle cx="447" cy="74" r="3"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="280" y="28" text-anchor="middle" font-size="18" font-weight="700">Quarterly results</text>
<text x="280" y="304" text-anchor="middle" font-size="13">quarter</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">value</text>
</g>
<g class="legend" font-family="sans-serif" font-size="12" fill="#0f172a">
<rect x="392" y="18" width="12" height="12" fill="#2563eb"/>
<text x="410" y="28">revenue</text>
<rect x="392" y="36" width="12" height="12" fill="#dc2626"/>
<text x="410" y="46">cost</text>
<rect x="392" y="54" width="12" height="12" fill="#16a34a"/>
<text x="410" y="64">samples</text>
</g>
</svg>
</figure>

## Grids

Add a background grid with `.grid(cells)` (major divisions per axis) and, for a
finer mesh, `.minor_grid(subdivisions)` (extra lines inside each major cell):

```dn
import plot;

chart = plot.line([3.0, 7.0, 5.0, 9.0, 8.0, 12.0])
    .title("With grid")
    .grid(6)         // 6 major cells per axis
    .minor_grid(4);  // 4 minor lines inside each cell
```

## Subplots

`plot.subplots(charts, cols)` composes several charts into one figure as a grid
of `cols` columns. Each chart keeps its own title, grid, and series and is scaled
into an equal cell. The result is a normal SVG, so it works with every output
path — `save_subplots_svg` / `save_subplots_html` to a file, or
`show_subplots_native` to open it in the native window:

```dn
import plot;

top_left = plot.line([1.0, 3.0, 2.0]).title("A").grid(4).size(380, 240);
top_right = plot.bar([2.0, 5.0, 3.0]).title("B").grid(4).size(380, 240);

figure = plot.subplots([top_left, top_right], 2); // one SVG, two cells
```

## More examples

See the [**Plot gallery**](../guides/plot_gallery.md) for ten worked examples —
sine and cosine, damped oscillation, Gaussian, logistic and polynomial curves,
scatter clouds, bars, histograms, and a four-panel subplot — each with the Dune
code and its rendered image.

> Auto-generated from `stdlib/plot.dn` by `tools/gen_stdlib_docs.py`.

### `record Chart`

**Methods:**

- `static fn empty(): Chart`
- `fn title(value: text): Chart` — Return a copy of the chart with its title set. — e.g. `plot.svg(plot.line([1.0, 2.0]).title("Sales")).contains("Sales")  // 1`
- `fn x_label(value: text): Chart` — Return a copy with the x-axis label set. — e.g. `plot.line([1.0, 2.0]).x_label("time")`
- `fn y_label(value: text): Chart` — Return a copy with the y-axis label set. — e.g. `plot.line([1.0, 2.0]).y_label("value")`
- `fn legend(enabled: bool): Chart` — Return a copy with the legend shown or hidden. — e.g. `plot.line([1.0, 2.0]).label("trend").legend(true)`
- `fn size(width: int, height: int): Chart` — Return a copy sized to `width` x `height` pixels (both must be positive). — e.g. `plot.svg(plot.line([1.0, 2.0]).size(640, 480)).contains("width=\"640\"")  // 1`
- `fn grid(cells: int): Chart` — Return a copy with a background grid of `cells` major divisions per axis — e.g. `plot.svg(plot.line([1.0, 2.0]).grid(5)).contains("plot-grid")  // 1`
- `fn minor_grid(subdivisions: int): Chart` — Return a copy that also draws `subdivisions` minor grid lines inside each — e.g. `plot.svg(plot.line([1.0, 2.0]).grid(4).minor_grid(5)).contains("plot-grid-minor")  // 1`
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

### `fn subplots(charts: [Chart], cols: int): text`

Arrange several charts into one SVG as a grid of `cols` columns (rows are filled left-to-right, top-to-bottom). Each chart keeps its own size and is scaled into an equal cell taken from the first chart's dimensions.

**Example:**
```dune
plot.subplots([plot.line([1.0, 2.0]), plot.bar([3.0, 1.0])], 2).contains("<svg")  // 1
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

### `fn subplots_html(charts: [Chart], cols: int): text`

Wrap a grid of charts in a standalone HTML document.

**Example:**
```dune
plot.subplots_html([plot.line([1.0, 2.0])], 1).contains("<!doctype")  // 1
```

### `fn save_subplots_svg(charts: [Chart], cols: int, path: text): outcome.Outcome<text, text>`

Write a grid of charts as SVG to `path`.

**Example:**
```dune
plot.save_subplots_svg([plot.line([1.0, 2.0]), plot.bar([2.0, 1.0])], 2, "grid.svg").is_done()  // 1
```

### `fn save_subplots_html(charts: [Chart], cols: int, path: text): outcome.Outcome<text, text>`

Write a grid of charts as an HTML document to `path`.

**Example:**
```dune
plot.save_subplots_html([plot.line([1.0, 2.0])], 1, "grid.html").is_done()  // 1
```

### `fn show_subplots_native(charts: [Chart], cols: int): outcome.Outcome<text, text>`

Open a grid of charts in the native plot window where the VM supports it. The window renders the composed SVG, so subplots display exactly as they save.

### `fn show(chart: Chart): outcome.Outcome<text, text>`
