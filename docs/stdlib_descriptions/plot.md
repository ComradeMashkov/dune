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
