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
