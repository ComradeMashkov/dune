# `runtime`

Runtime helpers such as `panic`.

`runtime` contains the standard library's single sanctioned native primitive: `panic`. It aborts execution with a message and exists because that behavior cannot be implemented in pure Dune.

Use it for unrecoverable internal errors and argument validation where returning `Maybe` or `Outcome` would hide a programming mistake. Other standard-library modules should remain pure Dune and should not add new foreign declarations.

```dn
import runtime;

fn require_positive(value: int): int {
    if value <= 0 {
        runtime.panic("expected a positive value");
    }

    return value;
}

print(require_positive(3));
```

> Auto-generated from `stdlib/runtime.dn` by `tools/gen_stdlib_docs.py`.

### `foreign fn panic(message: text): unit`

The one sanctioned native primitive in the standard library. `panic` aborts execution with a message and cannot be expressed in Dune, so it is bound to the C runtime symbol "dune_panic". Everything else in the stdlib is pure Dune; new `foreign fn` declarations are forbidden here. `export` makes it visible to other modules; it takes a text message and

**Returns:** `unit` (no meaningful value) because it never returns normally.

**Example:**
```dune
runtime.panic("index out of range")
```
