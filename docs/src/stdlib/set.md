# `set`

A hash-set built in Dune.

`set` is a mutable collection of unique `text` values. It keeps values in insertion order and stores them in a simple array, giving deterministic behavior with a small API.

Use `Set` when you need membership checks, duplicate suppression, and removal for strings. `add` ignores duplicates, `contains` reports membership, `remove` tells you whether anything was removed, and `values` returns a fresh array of the stored values.

```dn
import set;

seen: set.Set = set.Set.new();
seen.add("lexer");
seen.add("parser");
seen.add("lexer");

print(seen.len());
print(seen.contains("parser"));
print(seen.values().len());
```

> Auto-generated from `stdlib/set.dn` by `tools/gen_stdlib_docs.py`.

### `record Set`

A collection of unique `text` values.

Like `dict`, the first iteration stores items in a single array and scans linearly, which keeps behaviour simple and identical across backends. Insertion order is preserved. A `record` bundles data (the `items` field) with methods that operate on it.

**Methods:**

- `fn new(): Set` — Construct an empty set. — e.g. `set.Set.new().len()  // 0`
- `fn add(value: text): unit` — Add `value`; duplicates are ignored. — e.g. `s = set.Set.new(); s.add("a"); s.contains("a")  // 1`
- `fn contains(value: text): bool` — True when `value` is a member of the set. — e.g. `s = set.Set.new(); s.add("a"); s.contains("b")  // 0`
- `fn remove(value: text): bool` — Remove `value`; returns whether it was present. — e.g. `s = set.Set.new(); s.add("a"); s.remove("a")  // 1`
- `fn len(): int` — The number of elements currently in the set.
- `fn is_empty(): bool` — True when the set holds no elements.
- `fn clear(): unit` — Drop all elements, leaving an empty set. — e.g. `s = set.Set.new(); s.add("a"); s.clear(); s.is_empty()  // 1`
- `fn values(): [text]` — A copy of the values in insertion order. — e.g. `s = set.Set.new(); s.add("a"); s.add("b"); s.values()`
