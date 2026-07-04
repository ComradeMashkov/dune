`assert` provides two families of helpers. The `is_*`/`equals_*` predicates return a `bool` so the caller can decide how to report the result — handy in examples or ad-hoc checks. The `assert_*` helpers are meant for [`test` blocks](../guides/testing.md): each one calls `runtime.panic` when its check fails, which aborts the current test and marks it failed under `dune test`.

Import the assertions with `from assert import assert_eq, assert_true, assert_false;` to call them unqualified inside tests, or `import assert;` to reach every helper through the module name.

```dn
from assert import assert_eq, assert_true, assert_false;

test "arithmetic" {
    assert_eq(6 * 7, 42);         // any comparable type
    assert_true(2 + 2 == 4);
    assert_false(2 + 2 == 5);
}
```

The boolean predicates are still available when a returned value is more convenient than a panic:

```dn
import assert;

print(assert.is_true(2 + 2 == 4));
print(assert.equals_int(6 * 7, 42));
print(assert.equals_text("dune", "dune"));
```
