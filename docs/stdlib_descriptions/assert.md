`assert` contains tiny boolean helpers for tests and examples. They do not stop the program or call `panic`; each helper returns `true` or `false` so the caller can decide how to report the result.

Use it for readable checks around booleans, integers, and text values when a full test framework would be too heavy.

```dn
import assert;

print(assert.is_true(2 + 2 == 4));
print(assert.equals_int(6 * 7, 42));
print(assert.equals_text("dune", "dune"));
```
