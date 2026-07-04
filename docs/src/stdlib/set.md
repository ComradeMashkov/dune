# `set`

A hash-set built in Dune.

> Auto-generated from `stdlib/set.dn` by `tools/gen_stdlib_docs.py`.

### `record Set`

A collection of unique `text` values.

Like `dict`, the first iteration stores items in a single array and
scans linearly, which keeps behaviour simple and identical across
backends. Insertion order is preserved.
A `record` bundles data (the `items` field) with methods that operate on it.

**Methods:**

- `fn new(): Set` — Construct an empty set.
- `fn add(value: text): unit` — Add `value`; duplicates are ignored.
- `fn contains(value: text): bool` — True when `value` is a member of the set.
- `fn remove(value: text): bool` — Remove `value`; returns whether it was present.
- `fn len(): int` — The number of elements currently in the set.
- `fn is_empty(): bool` — True when the set holds no elements.
- `fn clear(): unit` — Drop all elements, leaving an empty set.
- `fn values(): [text]` — A copy of the values in insertion order.
