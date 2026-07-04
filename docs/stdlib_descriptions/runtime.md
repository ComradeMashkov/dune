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
