# `assert`

Assertion helpers for tests.

Tiny assertion helpers used by tests. Each returns a bool the caller can
check; they do not abort on their own. They exist mostly to give tests
readable, intention-revealing names for common comparisons.

> Auto-generated from `stdlib/assert.dn` by `tools/gen_stdlib_docs.py`.

### `fn is_true(value: bool): bool`

True when `value` is already true (identity predicate on a bool).

### `fn is_false(value: bool): bool`

True when `value` is false (logical negation of the input).

### `fn equals_int(actual: int, expected: int): bool`

True when the observed integer `actual` matches the `expected` integer.

### `fn equals_text(actual: text, expected: text): bool`

True when the observed text `actual` matches the `expected` text.
