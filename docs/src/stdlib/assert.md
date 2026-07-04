# `assert`

Assertion helpers for tests.

Tiny assertion helpers used by tests. The `is_*`/`equals_*` helpers return a
bool the caller can check; the `assert_*` helpers abort the current test via
`panic` when they fail, and are meant for `test "..." { ... }` blocks run by
`dune test`. Import them with `from assert import assert_eq, assert_true,
assert_false;` to call them unqualified inside tests.

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

> Auto-generated from `stdlib/assert.dn` by `tools/gen_stdlib_docs.py`.

### `fn is_true(value: bool): bool`

True when `value` is already true (identity predicate on a bool).

**Example:**
```dune
assert.is_true(true)  // 1
```

### `fn is_false(value: bool): bool`

True when `value` is false (logical negation of the input).

**Example:**
```dune
assert.is_false(false)  // 1
```

### `fn equals_int(actual: int, expected: int): bool`

True when the observed integer `actual` matches the `expected` integer.

**Example:**
```dune
assert.equals_int(2, 2)  // 1
```

### `fn equals_text(actual: text, expected: text): bool`

True when the observed text `actual` matches the `expected` text.

**Example:**
```dune
assert.equals_text("a", "a")  // 1
```

### `fn assert_true(value: bool): unit`

Fail the current test unless `value` is true; a failure aborts the test.

**Example:**
```dune
assert.assert_true(2 + 2 == 4)
```

### `fn assert_false(value: bool): unit`

Fail the current test unless `value` is false; a failure aborts the test.

**Example:**
```dune
assert.assert_false(2 + 2 == 5)
```

### `fn assert_eq<T>(actual: T, expected: T): unit`

Fail the current test unless `actual` equals `expected`, for any comparable type `T`; a failure aborts the test.

**Example:**
```dune
assert.assert_eq(2 + 2, 4)
```
