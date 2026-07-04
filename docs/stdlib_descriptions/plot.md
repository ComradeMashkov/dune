`plot` builds deterministic chart specs in pure Dune and renders them to SVG or
HTML. The first backend set covers line, scatter, bar, and histogram charts,
file output through `fs.write_text`, and headless-safe `show()` behavior via
`Outcome<text, text>`.
