# `plot`

Deterministic SVG/HTML chart rendering.

`plot` builds deterministic chart specs in pure Dune and renders them to SVG or
HTML. The first backend set covers line, scatter, bar, and histogram charts,
file output through `fs.write_text`, and headless-safe `show()` behavior via
`Outcome<text, text>`.

> Auto-generated from `stdlib/plot.dn` by `tools/gen_stdlib_docs.py`.

### `record Chart`

**Methods:**

- `static fn empty(): Chart`
- `fn title(value: text): Chart`
- `fn x_label(value: text): Chart`
- `fn y_label(value: text): Chart`
- `fn legend(enabled: bool): Chart`
- `fn size(width: int, height: int): Chart`
- `fn label(value: text): Chart`
- `fn add_line(xs: [real64], ys: [real64]): Chart`
- `fn add_line(ys: [real64]): Chart`
- `fn add_scatter(xs: [real64], ys: [real64]): Chart`
- `fn add_scatter(ys: [real64]): Chart`
- `fn add_bar(xs: [real64], ys: [real64]): Chart`
- `fn add_bar(ys: [real64]): Chart`

### `fn empty(): Chart`

### `fn line(xs: [real64], ys: [real64]): Chart`

### `fn line(ys: [real64]): Chart`

### `fn scatter(xs: [real64], ys: [real64]): Chart`

### `fn scatter(ys: [real64]): Chart`

### `fn bar(xs: [real64], ys: [real64]): Chart`

### `fn bar(ys: [real64]): Chart`

### `fn histogram(values: [real64], bins: int): Chart`

### `fn use_backend(name: text): unit`

Select the display backend for `show`. Supported MVP backends are: - "none": return a clear unsupported/headless error; - "svg": return the deterministic SVG text; - "html": return a deterministic HTML wrapper containing the SVG.

### `fn backend(): text`

### `fn available_backends(): [text]`

### `fn svg(chart: Chart): text`

### `fn html(chart: Chart): text`

### `fn save_svg(chart: Chart, path: text): outcome.Outcome<text, text>`

### `fn save_html(chart: Chart, path: text): outcome.Outcome<text, text>`

### `fn show(chart: Chart): outcome.Outcome<text, text>`
