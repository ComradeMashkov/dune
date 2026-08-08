# Examples

Runnable examples live in the `examples/` directory and are covered by golden
output tests. Run any of them with the `dune` binary:

```sh
dune examples/matrix_basics.dn
```

| Example | What it shows |
| --- | --- |
| `matrix_basics.dn` | Vectors and matrices from the [`matrix`](../stdlib/matrix.md) module. |
| `vector_stats.dn` | Reductions and statistics over a vector. |
| `linear_regression.dn` | A small numerical program: least-squares fit. |
| `statistical_analysis.dn` | Descriptive statistics, regression, histograms, probability helpers, and [`matrix`](../stdlib/matrix.md)/[`plot`](../stdlib/plot.md) integration. |
| `collection_pipeline.dn` | The higher-order `filter`/`map`/`sum` pipeline with [function values](../language/functions.md#function-values). |
| `functions_and_closures.dn` | Typed lambdas, capture snapshots, shared aggregate handles, nested/generic closures, stored closures, composition, callbacks, and callable-expression chains. |
| `defer_cleanup.dn` | Deterministic [resource cleanup with `defer`](../language/resource-cleanup.md): LIFO, captures, early exits, `?`, and loop scopes. |
| `geometry.dn` + `geometry_demo.dn` | A two-file program showing [module](../language/modules.md) declarations, aliases, and selective imports. |
| `documented.dn` | Every comment form plus `brief`/`param`/`returns` [doc-comments](../language/comments.md) on a function, record fields, and a method. |

The `documented.dn` example is a good starting point for seeing how
doc-comments render on hover in an editor — open it with the
[Zed extension](editor.md) and hover the documented names.

Interactive examples live under `examples/notebooks/`. Open
`functions_and_closures.dnb` for an unexecuted, cell-by-cell tour of lambdas,
capture semantics, nested and generic closures, callback pipelines, and plot
integration.
