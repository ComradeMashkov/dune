# `assert`

Assertion helpers for tests.

Tiny assertion helpers used by tests. Each returns a bool the caller can
check; they do not abort on their own. They exist mostly to give tests
readable, intention-revealing names for common comparisons.

`assert` contains tiny boolean helpers for tests and examples. They do not stop the program or call `panic`; each helper returns `true` or `false` so the caller can decide how to report the result.

Use it for readable checks around booleans, integers, and text values when a full test framework would be too heavy.

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
