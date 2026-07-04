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
| `collection_pipeline.dn` | The higher-order `filter`/`map`/`sum` pipeline with [function values](../language/functions.md#function-values-and-method-chaining). |
| `geometry.dn` + `geometry_demo.dn` | A two-file program showing [module](../language/modules.md) declarations, aliases, and selective imports. |
| `documented.dn` | Every comment form plus `brief`/`param`/`returns` [doc-comments](../language/comments.md) on a function, record fields, and a method. |

The `documented.dn` example is a good starting point for seeing how
doc-comments render on hover in an editor — open it with the
[Zed extension](editor.md) and hover the documented names.
