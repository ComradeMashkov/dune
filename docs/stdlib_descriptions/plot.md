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
