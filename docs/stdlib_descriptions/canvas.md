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
