`plot` builds deterministic chart specifications in pure Dune and renders them to
SVG or HTML — no external plotting library. It covers the common chart types (line, area, step, scatter, bar, grouped bars, histogram, and pie), multi-series
overlays, grids, subplot figures, file output via `fs.write_text`, and a
platform-native display window through `show()` where the VM supports it.

## Chart types

Every builder returns a `Chart` you refine with chained methods (`.title(...)`, `.x_label(...)`, `.grid(...)`, `.legend(true)`, `.size(w, h)`) and then render with `plot.svg(chart)`.

### Line chart

The default for a trend over time. `plot.line(xs, ys)` (or `plot.line(ys)` for index-based x). Chain `.grid(...)` / `.minor_grid(...)` for a background mesh.

```dn
import plot;

months = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0];
users  = [120.0, 135.0, 148.0, 172.0, 168.0, 205.0, 233.0, 258.0];

chart = plot.line(months, users)
    .title("Monthly active users").x_label("month").y_label("users")
    .grid(7).minor_grid(2);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Monthly active users</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid-minor" fill="none" stroke="#eef2f7" stroke-width="1">
<line x1="94" y1="48" x2="94" y2="256"/>
<line x1="64" y1="62.8571428571429" x2="484" y2="62.8571428571429"/>
<line x1="124" y1="48" x2="124" y2="256"/>
<line x1="64" y1="77.7142857142857" x2="484" y2="77.7142857142857"/>
<line x1="154" y1="48" x2="154" y2="256"/>
<line x1="64" y1="92.5714285714286" x2="484" y2="92.5714285714286"/>
<line x1="184" y1="48" x2="184" y2="256"/>
<line x1="64" y1="107.428571428571" x2="484" y2="107.428571428571"/>
<line x1="214" y1="48" x2="214" y2="256"/>
<line x1="64" y1="122.285714285714" x2="484" y2="122.285714285714"/>
<line x1="244" y1="48" x2="244" y2="256"/>
<line x1="64" y1="137.142857142857" x2="484" y2="137.142857142857"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="304" y1="48" x2="304" y2="256"/>
<line x1="64" y1="166.857142857143" x2="484" y2="166.857142857143"/>
<line x1="334" y1="48" x2="334" y2="256"/>
<line x1="64" y1="181.714285714286" x2="484" y2="181.714285714286"/>
<line x1="364" y1="48" x2="364" y2="256"/>
<line x1="64" y1="196.571428571429" x2="484" y2="196.571428571429"/>
<line x1="394" y1="48" x2="394" y2="256"/>
<line x1="64" y1="211.428571428571" x2="484" y2="211.428571428571"/>
<line x1="424" y1="48" x2="424" y2="256"/>
<line x1="64" y1="226.285714285714" x2="484" y2="226.285714285714"/>
<line x1="454" y1="48" x2="454" y2="256"/>
<line x1="64" y1="241.142857142857" x2="484" y2="241.142857142857"/>
</g>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="124" y1="48" x2="124" y2="256"/>
<line x1="64" y1="77.7142857142857" x2="484" y2="77.7142857142857"/>
<line x1="184" y1="48" x2="184" y2="256"/>
<line x1="64" y1="107.428571428571" x2="484" y2="107.428571428571"/>
<line x1="244" y1="48" x2="244" y2="256"/>
<line x1="64" y1="137.142857142857" x2="484" y2="137.142857142857"/>
<line x1="304" y1="48" x2="304" y2="256"/>
<line x1="64" y1="166.857142857143" x2="484" y2="166.857142857143"/>
<line x1="364" y1="48" x2="364" y2="256"/>
<line x1="64" y1="196.571428571429" x2="484" y2="196.571428571429"/>
<line x1="424" y1="48" x2="424" y2="256"/>
<line x1="64" y1="226.285714285714" x2="484" y2="226.285714285714"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">1</text>
<text x="484" y="274" text-anchor="middle">8</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">258</text>
</g>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,159 124,147 184,137 244,117 304,121 364,91 424,68 484,48"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Monthly active users</text>
<text x="260" y="304" text-anchor="middle" font-size="13">month</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">users</text>
</g>
</svg>
</figure>

### Multiple series

Overlay series by chaining `add_line` / `add_scatter` / `add_bar` on one chart. `.label(...)` names the most recently added series and `legend(true)` draws the key.

```dn
import plot;

chart = plot.line([30.0, 42.0, 39.0, 55.0, 61.0, 74.0]).label("revenue")
    .add_line([22.0, 28.0, 31.0, 36.0, 41.0, 48.0]).label("cost")
    .title("Revenue vs cost").x_label("quarter").y_label("$k")
    .legend(true).grid(6).minor_grid(2);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Revenue vs cost</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid-minor" fill="none" stroke="#eef2f7" stroke-width="1">
<line x1="99" y1="48" x2="99" y2="256"/>
<line x1="64" y1="65.3333333333333" x2="484" y2="65.3333333333333"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="169" y1="48" x2="169" y2="256"/>
<line x1="64" y1="100" x2="484" y2="100"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="239" y1="48" x2="239" y2="256"/>
<line x1="64" y1="134.666666666667" x2="484" y2="134.666666666667"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="309" y1="48" x2="309" y2="256"/>
<line x1="64" y1="169.333333333333" x2="484" y2="169.333333333333"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="379" y1="48" x2="379" y2="256"/>
<line x1="64" y1="204" x2="484" y2="204"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="449" y1="48" x2="449" y2="256"/>
<line x1="64" y1="238.666666666667" x2="484" y2="238.666666666667"/>
</g>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">1</text>
<text x="484" y="274" text-anchor="middle">6</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">74</text>
</g>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,172 148,138 232,146 316,101 400,85 484,48"/>
<polyline class="series line" fill="none" stroke="#dc2626" stroke-width="2" points="64,194 148,177 232,169 316,155 400,141 484,121"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Revenue vs cost</text>
<text x="260" y="304" text-anchor="middle" font-size="13">quarter</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">$k</text>
</g>
<g class="legend" font-family="sans-serif" font-size="12" fill="#0f172a">
<rect x="352" y="18" width="12" height="12" fill="#2563eb"/>
<text x="370" y="28">revenue</text>
<rect x="352" y="36" width="12" height="12" fill="#dc2626"/>
<text x="370" y="46">cost</text>
</g>
</svg>
</figure>

### Area chart

`plot.area(...)` is a line filled down to the baseline — good for showing a cumulative quantity or emphasising volume under a curve.

```dn
import plot;

chart = plot.area([4.0, 9.0, 7.0, 14.0, 20.0, 18.0, 27.0])
    .title("Daily downloads").x_label("day").y_label("thousands")
    .grid(6).minor_grid(2);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Daily downloads</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid-minor" fill="none" stroke="#eef2f7" stroke-width="1">
<line x1="99" y1="48" x2="99" y2="256"/>
<line x1="64" y1="65.3333333333333" x2="484" y2="65.3333333333333"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="169" y1="48" x2="169" y2="256"/>
<line x1="64" y1="100" x2="484" y2="100"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="239" y1="48" x2="239" y2="256"/>
<line x1="64" y1="134.666666666667" x2="484" y2="134.666666666667"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="309" y1="48" x2="309" y2="256"/>
<line x1="64" y1="169.333333333333" x2="484" y2="169.333333333333"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="379" y1="48" x2="379" y2="256"/>
<line x1="64" y1="204" x2="484" y2="204"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="449" y1="48" x2="449" y2="256"/>
<line x1="64" y1="238.666666666667" x2="484" y2="238.666666666667"/>
</g>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">1</text>
<text x="484" y="274" text-anchor="middle">7</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">27</text>
</g>
<polygon class="series area" fill="#2563eb" fill-opacity="0.25" stroke="none" points="64,256 64,225 134,187 204,202 274,148 344,102 414,117 484,48 484,256"/>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,225 134,187 204,202 274,148 344,102 414,117 484,48"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Daily downloads</text>
<text x="260" y="304" text-anchor="middle" font-size="13">day</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">thousands</text>
</g>
</svg>
</figure>

### Step chart

`plot.step(...)` holds each value constant until the next sample — the right shape for quantities that change in discrete jumps (counts, levels, states).

```dn
import plot;

hours    = [0.0, 3.0, 6.0, 9.0, 12.0, 15.0, 18.0, 21.0];
replicas = [2.0, 2.0, 4.0, 8.0, 8.0, 6.0, 3.0, 2.0];

chart = plot.step(hours, replicas)
    .title("Autoscaled replicas").x_label("hour").y_label("pods").grid(7);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Autoscaled replicas</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="124" y1="48" x2="124" y2="256"/>
<line x1="64" y1="77.7142857142857" x2="484" y2="77.7142857142857"/>
<line x1="184" y1="48" x2="184" y2="256"/>
<line x1="64" y1="107.428571428571" x2="484" y2="107.428571428571"/>
<line x1="244" y1="48" x2="244" y2="256"/>
<line x1="64" y1="137.142857142857" x2="484" y2="137.142857142857"/>
<line x1="304" y1="48" x2="304" y2="256"/>
<line x1="64" y1="166.857142857143" x2="484" y2="166.857142857143"/>
<line x1="364" y1="48" x2="364" y2="256"/>
<line x1="64" y1="196.571428571429" x2="484" y2="196.571428571429"/>
<line x1="424" y1="48" x2="424" y2="256"/>
<line x1="64" y1="226.285714285714" x2="484" y2="226.285714285714"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">0</text>
<text x="484" y="274" text-anchor="middle">21</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">8</text>
</g>
<polyline class="series step" fill="none" stroke="#2563eb" stroke-width="2" points="64,204 124,204 124,204 184,204 184,152 244,152 244,48 304,48 304,48 364,48 364,100 424,100 424,178 484,178 484,204"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Autoscaled replicas</text>
<text x="260" y="304" text-anchor="middle" font-size="13">hour</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">pods</text>
</g>
</svg>
</figure>

### Scatter plot

`plot.scatter(...)` draws points instead of a connecting line — use it to show the relationship between two variables.

```dn
import plot;

chart = plot.scatter(study_hours, exam_scores)
    .title("Study hours vs score").x_label("hours").y_label("score")
    .grid(6).minor_grid(2);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Study hours vs score</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid-minor" fill="none" stroke="#eef2f7" stroke-width="1">
<line x1="99" y1="48" x2="99" y2="256"/>
<line x1="64" y1="65.3333333333333" x2="484" y2="65.3333333333333"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="169" y1="48" x2="169" y2="256"/>
<line x1="64" y1="100" x2="484" y2="100"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="239" y1="48" x2="239" y2="256"/>
<line x1="64" y1="134.666666666667" x2="484" y2="134.666666666667"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="309" y1="48" x2="309" y2="256"/>
<line x1="64" y1="169.333333333333" x2="484" y2="169.333333333333"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="379" y1="48" x2="379" y2="256"/>
<line x1="64" y1="204" x2="484" y2="204"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="449" y1="48" x2="449" y2="256"/>
<line x1="64" y1="238.666666666667" x2="484" y2="238.666666666667"/>
</g>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">1</text>
<text x="484" y="274" text-anchor="middle">6.5</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">92</text>
</g>
<g class="series scatter" fill="#2563eb">
<circle cx="64" cy="138" r="3"/>
<circle cx="102" cy="132" r="3"/>
<circle cx="140" cy="118" r="3"/>
<circle cx="179" cy="125" r="3"/>
<circle cx="217" cy="105" r="3"/>
<circle cx="255" cy="93" r="3"/>
<circle cx="293" cy="98" r="3"/>
<circle cx="331" cy="80" r="3"/>
<circle cx="369" cy="68" r="3"/>
<circle cx="408" cy="75" r="3"/>
<circle cx="446" cy="57" r="3"/>
<circle cx="484" cy="48" r="3"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Study hours vs score</text>
<text x="260" y="304" text-anchor="middle" font-size="13">hours</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">score</text>
</g>
</svg>
</figure>

### Bar chart

`plot.bar(...)` for categorical comparisons. Bars are centred on their x value and the x-domain is padded so the outer bars stay inside the axes.

```dn
import plot;

chart = plot.bar([1.0, 2.0, 3.0, 4.0, 5.0], [42.0, 58.0, 33.0, 71.0, 25.0])
    .title("Units sold by product").x_label("product").y_label("units").grid(6);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Units sold by product</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">0.5</text>
<text x="484" y="274" text-anchor="middle">5.5</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">71</text>
</g>
<g class="series bar" fill="#2563eb">
<rect x="76.6" y="133" width="59" height="123"/>
<rect x="160.6" y="86" width="59" height="170"/>
<rect x="244.6" y="159" width="59" height="97"/>
<rect x="328.6" y="48" width="59" height="208"/>
<rect x="412.6" y="183" width="59" height="73"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Units sold by product</text>
<text x="260" y="304" text-anchor="middle" font-size="13">product</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">units</text>
</g>
</svg>
</figure>

### Grouped bars

Add more than one bar series and they are drawn side by side within each slot, so you can compare categories across groups.

```dn
import plot;

qs = [1.0, 2.0, 3.0, 4.0];

chart = plot.bar(qs, [18.0, 24.0, 21.0, 30.0]).label("2024")
    .add_bar(qs, [22.0, 20.0, 27.0, 34.0]).label("2025")
    .title("Quarterly sales").x_label("quarter").y_label("units")
    .legend(true).grid(6);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Quarterly sales</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">0.5</text>
<text x="484" y="274" text-anchor="middle">4.5</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">34</text>
</g>
<g class="series bar" fill="#2563eb">
<rect x="80.25" y="146" width="37" height="110"/>
<rect x="185.25" y="109" width="37" height="147"/>
<rect x="290.25" y="128" width="37" height="128"/>
<rect x="395.25" y="72" width="37" height="184"/>
</g>
<g class="series bar" fill="#dc2626">
<rect x="117" y="121" width="37" height="135"/>
<rect x="222" y="134" width="37" height="122"/>
<rect x="327" y="91" width="37" height="165"/>
<rect x="432" y="48" width="37" height="208"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Quarterly sales</text>
<text x="260" y="304" text-anchor="middle" font-size="13">quarter</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">units</text>
</g>
<g class="legend" font-family="sans-serif" font-size="12" fill="#0f172a">
<rect x="352" y="18" width="12" height="12" fill="#2563eb"/>
<text x="370" y="28">2024</text>
<rect x="352" y="36" width="12" height="12" fill="#dc2626"/>
<text x="370" y="46">2025</text>
</g>
</svg>
</figure>

### Histogram

`plot.histogram(values, bins)` buckets raw samples into equal-width bars to show a distribution.

```dn
import plot;

samples = [ /* 30 response times in ms */ ];

chart = plot.histogram(samples, 8)
    .title("Response-time distribution").x_label("ms").y_label("count").grid(6);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="320" viewBox="0 0 520 320">
<title>Response-time distribution</title>
<rect width="520" height="320" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="256"/>
<line x1="64" y1="48" x2="484" y2="48"/>
<line x1="134" y1="48" x2="134" y2="256"/>
<line x1="64" y1="82.6666666666667" x2="484" y2="82.6666666666667"/>
<line x1="204" y1="48" x2="204" y2="256"/>
<line x1="64" y1="117.333333333333" x2="484" y2="117.333333333333"/>
<line x1="274" y1="48" x2="274" y2="256"/>
<line x1="64" y1="152" x2="484" y2="152"/>
<line x1="344" y1="48" x2="344" y2="256"/>
<line x1="64" y1="186.666666666667" x2="484" y2="186.666666666667"/>
<line x1="414" y1="48" x2="414" y2="256"/>
<line x1="64" y1="221.333333333333" x2="484" y2="221.333333333333"/>
<line x1="484" y1="48" x2="484" y2="256"/>
<line x1="64" y1="256" x2="484" y2="256"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="256" x2="484" y2="256"/>
<line x1="64" y1="48" x2="64" y2="256"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="274" text-anchor="middle">12</text>
<text x="484" y="274" text-anchor="middle">30</text>
<text x="56" y="260" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">8</text>
</g>
<g class="series bar" fill="#2563eb">
<rect x="71.625" y="204" width="37" height="52"/>
<rect x="124.625" y="204" width="37" height="52"/>
<rect x="176.625" y="152" width="37" height="104"/>
<rect x="229.625" y="100" width="37" height="156"/>
<rect x="281.625" y="48" width="37" height="208"/>
<rect x="334.625" y="152" width="37" height="104"/>
<rect x="386.625" y="204" width="37" height="52"/>
<rect x="439.625" y="204" width="37" height="52"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Response-time distribution</text>
<text x="260" y="304" text-anchor="middle" font-size="13">ms</text>
<text x="18" y="160" text-anchor="middle" font-size="13" transform="rotate(-90 18 160)">count</text>
</g>
</svg>
</figure>

### Pie chart

`plot.pie(values)` — or `plot.pie(values, labels)` — shows parts of a whole. Each wedge is sized by its share of the total and labelled with its percentage; pass labels and `legend(true)` to name the categories.

```dn
import plot;

chart = plot.pie([35.0, 25.0, 20.0, 15.0, 5.0],
    ["Rent", "Food", "Transport", "Savings", "Other"])
    .title("Monthly budget").legend(true);
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="520" height="340" viewBox="0 0 520 340">
<title>Monthly budget</title>
<rect width="520" height="340" fill="#ffffff"/>
<g class="series pie" stroke="#ffffff" stroke-width="2">
<path fill="#2563eb" d="M 260 178 L 260 64 A 114 114 0 0 1 352 245 Z"/>
<text x="321" y="147" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">35%</text>
<path fill="#dc2626" d="M 260 178 L 352 245 A 114 114 0 0 1 193 270 Z"/>
<text x="271" y="246" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">25%</text>
<path fill="#16a34a" d="M 260 178 L 193 270 A 114 114 0 0 1 152 143 Z"/>
<text x="195" y="199" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">20%</text>
<path fill="#9333ea" d="M 260 178 L 152 143 A 114 114 0 0 1 225 70 Z"/>
<text x="212" y="130" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">15%</text>
<path fill="#ea580c" d="M 260 178 L 225 70 A 114 114 0 0 1 260 64 Z"/>
<text x="249" y="110" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">5%</text>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="260" y="28" text-anchor="middle" font-size="18" font-weight="700">Monthly budget</text>
</g>
<g class="legend" font-family="sans-serif" font-size="12" fill="#0f172a">
<rect x="352" y="18" width="12" height="12" fill="#2563eb"/>
<text x="370" y="28">Rent</text>
<rect x="352" y="36" width="12" height="12" fill="#dc2626"/>
<text x="370" y="46">Food</text>
<rect x="352" y="54" width="12" height="12" fill="#16a34a"/>
<text x="370" y="64">Transport</text>
<rect x="352" y="72" width="12" height="12" fill="#9333ea"/>
<text x="370" y="82">Savings</text>
<rect x="352" y="90" width="12" height="12" fill="#ea580c"/>
<text x="370" y="100">Other</text>
</g>
</svg>
</figure>

## Subplots: one figure, many charts

`plot.subplots(charts, cols)` composes several charts into a single figure laid out as a grid of `cols` columns. Each chart keeps its own type, title, grid, and axes and is scaled into an equal cell. The result is a normal SVG, so it works with every output path — `save_subplots_svg` / `save_subplots_html` to a file, or `show_subplots_native` to open it in the native window.

```dn
import plot;

users    = [120.0, 135.0, 148.0, 172.0, 168.0, 205.0, 233.0, 258.0];
visits   = [4.0, 9.0, 7.0, 14.0, 20.0, 18.0, 27.0];
sold     = [42.0, 58.0, 33.0, 71.0, 25.0];
replicas = [2.0, 2.0, 4.0, 8.0, 8.0, 6.0, 3.0, 2.0];

figure = plot.subplots([
    plot.line(users).title("line").grid(4).size(300, 200),
    plot.area(visits).title("area").grid(4).size(300, 200),
    plot.scatter(study_hours, exam_scores).title("scatter").grid(4).size(300, 200),
    plot.bar(sold).title("bar").grid(4).size(300, 200),
    plot.step(replicas).title("step").grid(4).size(300, 200),
    plot.pie([35.0, 25.0, 20.0, 15.0, 5.0]).title("pie").size(300, 200),
], 3); // 3 columns -> a 3x2 grid, one SVG
```

<figure style="text-align:center;margin:1rem 0;">
<svg xmlns="http://www.w3.org/2000/svg" width="900" height="400" viewBox="0 0 900 400">
<rect width="900" height="400" fill="#ffffff"/>
<svg x="0" y="0" width="300" height="200" viewBox="0 0 300 200">
<title>line</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="136"/>
<line x1="64" y1="48" x2="264" y2="48"/>
<line x1="114" y1="48" x2="114" y2="136"/>
<line x1="64" y1="70" x2="264" y2="70"/>
<line x1="164" y1="48" x2="164" y2="136"/>
<line x1="64" y1="92" x2="264" y2="92"/>
<line x1="214" y1="48" x2="214" y2="136"/>
<line x1="64" y1="114" x2="264" y2="114"/>
<line x1="264" y1="48" x2="264" y2="136"/>
<line x1="64" y1="136" x2="264" y2="136"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="136" x2="264" y2="136"/>
<line x1="64" y1="48" x2="64" y2="136"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="154" text-anchor="middle">1</text>
<text x="264" y="154" text-anchor="middle">8</text>
<text x="56" y="140" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">258</text>
</g>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,95 93,90 121,86 150,77 178,79 207,66 235,57 264,48"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">line</text>
</g>
</svg>
<svg x="300" y="0" width="300" height="200" viewBox="0 0 300 200">
<title>area</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="136"/>
<line x1="64" y1="48" x2="264" y2="48"/>
<line x1="114" y1="48" x2="114" y2="136"/>
<line x1="64" y1="70" x2="264" y2="70"/>
<line x1="164" y1="48" x2="164" y2="136"/>
<line x1="64" y1="92" x2="264" y2="92"/>
<line x1="214" y1="48" x2="214" y2="136"/>
<line x1="64" y1="114" x2="264" y2="114"/>
<line x1="264" y1="48" x2="264" y2="136"/>
<line x1="64" y1="136" x2="264" y2="136"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="136" x2="264" y2="136"/>
<line x1="64" y1="48" x2="64" y2="136"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="154" text-anchor="middle">1</text>
<text x="264" y="154" text-anchor="middle">7</text>
<text x="56" y="140" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">27</text>
</g>
<polygon class="series area" fill="#2563eb" fill-opacity="0.25" stroke="none" points="64,136 64,123 97,107 131,113 164,90 197,71 231,77 264,48 264,136"/>
<polyline class="series line" fill="none" stroke="#2563eb" stroke-width="2" points="64,123 97,107 131,113 164,90 197,71 231,77 264,48"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">area</text>
</g>
</svg>
<svg x="600" y="0" width="300" height="200" viewBox="0 0 300 200">
<title>scatter</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="136"/>
<line x1="64" y1="48" x2="264" y2="48"/>
<line x1="114" y1="48" x2="114" y2="136"/>
<line x1="64" y1="70" x2="264" y2="70"/>
<line x1="164" y1="48" x2="164" y2="136"/>
<line x1="64" y1="92" x2="264" y2="92"/>
<line x1="214" y1="48" x2="214" y2="136"/>
<line x1="64" y1="114" x2="264" y2="114"/>
<line x1="264" y1="48" x2="264" y2="136"/>
<line x1="64" y1="136" x2="264" y2="136"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="136" x2="264" y2="136"/>
<line x1="64" y1="48" x2="64" y2="136"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="154" text-anchor="middle">1</text>
<text x="264" y="154" text-anchor="middle">6.5</text>
<text x="56" y="140" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">92</text>
</g>
<g class="series scatter" fill="#2563eb">
<circle cx="64" cy="86" r="3"/>
<circle cx="82" cy="83" r="3"/>
<circle cx="100" cy="78" r="3"/>
<circle cx="119" cy="81" r="3"/>
<circle cx="137" cy="72" r="3"/>
<circle cx="155" cy="67" r="3"/>
<circle cx="173" cy="69" r="3"/>
<circle cx="191" cy="61" r="3"/>
<circle cx="209" cy="57" r="3"/>
<circle cx="228" cy="59" r="3"/>
<circle cx="246" cy="52" r="3"/>
<circle cx="264" cy="48" r="3"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">scatter</text>
</g>
</svg>
<svg x="0" y="200" width="300" height="200" viewBox="0 0 300 200">
<title>bar</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="136"/>
<line x1="64" y1="48" x2="264" y2="48"/>
<line x1="114" y1="48" x2="114" y2="136"/>
<line x1="64" y1="70" x2="264" y2="70"/>
<line x1="164" y1="48" x2="164" y2="136"/>
<line x1="64" y1="92" x2="264" y2="92"/>
<line x1="214" y1="48" x2="214" y2="136"/>
<line x1="64" y1="114" x2="264" y2="114"/>
<line x1="264" y1="48" x2="264" y2="136"/>
<line x1="64" y1="136" x2="264" y2="136"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="136" x2="264" y2="136"/>
<line x1="64" y1="48" x2="64" y2="136"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="154" text-anchor="middle">0.5</text>
<text x="264" y="154" text-anchor="middle">5.5</text>
<text x="56" y="140" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">71</text>
</g>
<g class="series bar" fill="#2563eb">
<rect x="70" y="84" width="28" height="52"/>
<rect x="110" y="64" width="28" height="72"/>
<rect x="150" y="95" width="28" height="41"/>
<rect x="190" y="48" width="28" height="88"/>
<rect x="230" y="105" width="28" height="31"/>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">bar</text>
</g>
</svg>
<svg x="300" y="200" width="300" height="200" viewBox="0 0 300 200">
<title>step</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="plot-grid" fill="none" stroke="#d7dee7" stroke-width="1">
<line x1="64" y1="48" x2="64" y2="136"/>
<line x1="64" y1="48" x2="264" y2="48"/>
<line x1="114" y1="48" x2="114" y2="136"/>
<line x1="64" y1="70" x2="264" y2="70"/>
<line x1="164" y1="48" x2="164" y2="136"/>
<line x1="64" y1="92" x2="264" y2="92"/>
<line x1="214" y1="48" x2="214" y2="136"/>
<line x1="64" y1="114" x2="264" y2="114"/>
<line x1="264" y1="48" x2="264" y2="136"/>
<line x1="64" y1="136" x2="264" y2="136"/>
</g>
<g class="axes" fill="none" stroke="#334155" stroke-width="1">
<line x1="64" y1="136" x2="264" y2="136"/>
<line x1="64" y1="48" x2="64" y2="136"/>
</g>
<g class="ticks" fill="#475569" font-family="sans-serif" font-size="11">
<text x="64" y="154" text-anchor="middle">1</text>
<text x="264" y="154" text-anchor="middle">8</text>
<text x="56" y="140" text-anchor="end">0</text>
<text x="56" y="52" text-anchor="end">8</text>
</g>
<polyline class="series step" fill="none" stroke="#2563eb" stroke-width="2" points="64,114 93,114 93,114 121,114 121,92 150,92 150,48 178,48 178,48 207,48 207,70 235,70 235,103 264,103 264,114"/>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">step</text>
</g>
</svg>
<svg x="600" y="200" width="300" height="200" viewBox="0 0 300 200">
<title>pie</title>
<rect width="300" height="200" fill="#ffffff"/>
<g class="series pie" stroke="#ffffff" stroke-width="2">
<path fill="#2563eb" d="M 150 108 L 150 64 A 44 44 0 0 1 186 134 Z"/>
<text x="174" y="96" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">35%</text>
<path fill="#dc2626" d="M 150 108 L 186 134 A 44 44 0 0 1 124 144 Z"/>
<text x="154" y="134" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">25%</text>
<path fill="#16a34a" d="M 150 108 L 124 144 A 44 44 0 0 1 108 94 Z"/>
<text x="125" y="116" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">20%</text>
<path fill="#9333ea" d="M 150 108 L 108 94 A 44 44 0 0 1 136 66 Z"/>
<text x="131" y="89" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">15%</text>
<path fill="#ea580c" d="M 150 108 L 136 66 A 44 44 0 0 1 150 64 Z"/>
<text x="146" y="82" text-anchor="middle" fill="#ffffff" font-family="sans-serif" font-size="12" font-weight="700">5%</text>
</g>
<g class="labels" fill="#0f172a" font-family="sans-serif">
<text x="150" y="28" text-anchor="middle" font-size="18" font-weight="700">pie</text>
</g>
</svg>
</svg>
</figure>

