# Runtime errors and stack traces

Dune distinguishes recoverable values from failures that abort the current VM
execution. A recoverable operation returns a choice such as
`outcome.Outcome<T, E>`; a panic is reserved for violated invariants and
operations that cannot continue, such as division by zero or indexing outside
an array.

## Explicit panic

The `runtime` module exposes the only native primitive used by Dune's standard
library:

```dn
import runtime;

fn require_positive(value: int): unit {
    if value <= 0 {
        runtime.panic("value must be positive");
    }
}
```

`runtime.panic(message)` has the `panic` category and preserves `message`
verbatim. It aborts the current script, test, REPL entry, or notebook cell after
running all pending `defer` cleanups.

## Categories

Every VM failure has a stable category:

| Category | Meaning |
| --- | --- |
| `panic` | An explicit `runtime.panic` call. |
| `bounds error` | Invalid array, tuple, record, text, or slice position. |
| `arithmetic error` | An arithmetic operation cannot produce a value, such as division by zero. |
| `runtime type error` | Invalid operands reached the VM or a dynamic callable had the wrong shape. |
| `I/O error` | A non-recoverable stream/runtime I/O failure. Normal filesystem APIs return `Outcome` values. |
| `foreign function error` | A C FFI symbol or invocation is unsupported or invalid. |
| `VM error` | A bytecode invariant failed. This normally indicates a compiler or VM bug. |

The category is available to C++ embedders as `RuntimeErrorKind`; the original
message, frames, and deferred-cleanup failures remain structured on
`RuntimeError` instead of requiring consumers to parse `what()`.

## Stack trace format

The compiler records the source span of every bytecode instruction. When an
operation fails, the VM snapshots all live Dune frames before unwinding them:

```text
panic: value must be positive
stack trace:
  0: require_positive
      at examples/check.dn:5:9
  1: validate
      at examples/check.dn:10:5
  2: <top-level>
      at examples/check.dn:13:1
```

Frames are ordered from the failing function to the outermost caller. Imported
functions use their module-qualified names and point to the imported `.dn`
file, including pure-Dune standard-library modules. Lambdas use a generated
`<lambda@line:column>` name.

`dune test` names the outer frame as `test "name"`. REPL entries use `<repl>`.
Notebook frames use `<path>.dnb#cell-<id>` and report lines relative to that
cell, so a function defined in one cell and called in another points to both
cells correctly.

## Panic during `defer`

The first failure remains primary. Dune still runs every pending cleanup in
LIFO order. Each cleanup failure is appended as `while running deferred
cleanup: ...` and keeps its own stack trace; it does not replace the original
category, message, or frames.

## Recoverable errors remain values

Returning `Failed(error)` from `outcome.Outcome<T, E>` does not create a stack
trace and is not caught by the panic machinery. It remains a normal choice
value that can be matched, inspected, or propagated with `?`. Use `Outcome`
for expected failures and `runtime.panic` for conditions the current execution
cannot safely continue from.
