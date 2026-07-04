# `dict`

A hash-map style dictionary built in Dune.

`dict` is a generic dictionary from `text` keys to values of type `V`. It keeps keys and values in insertion order and stores them in parallel arrays, so the behavior is simple and deterministic across backends.

Use `Dict<V>` when you need a mutable string-keyed map with explicit optional lookups. `set` inserts or overwrites, `get` returns `Maybe<V>`, and `keys` and `values` return fresh arrays in insertion order.

```dn
import dict;
import maybe;

scores: dict.Dict<int> = dict.Dict.new();
scores.set("ada", 10);
scores.set("grace", 7);
scores.set("ada", 12);

print(scores.get("ada").value_or(0));
print(scores.contains("grace"));
print(scores.keys().len());
```

> Auto-generated from `stdlib/dict.dn` by `tools/gen_stdlib_docs.py`.

### `record Dict<V>`

A small associative collection mapping `text` keys to `V` values.

The first iteration keeps keys as `text` and stores entries in two parallel arrays, so lookups are linear. That is simple, predictable, and identical across backends; hashing can come later without changing this API. Generic over the value type `V`; keys are always text.

**Methods:**

- `fn new(): Dict<V>` — Construct an empty dictionary. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.len()  // 0`
- `fn set(key: text, value: V): unit` — Insert or overwrite the value for `key`. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.set("a", 1); d.get("a").value_or(0)  // 1`
- `fn get(key: text): maybe.Maybe<V>` — Look up `key`, returning `Present(value)` or `Absent`. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.get("missing").value_or(0)  // 0`
- `fn contains(key: text): bool` — True when `key` has an associated value. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.set("a", 1); d.contains("a")  // 1`
- `fn remove(key: text): bool` — Remove `key`; returns whether a value was actually removed. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.set("a", 1); d.remove("a")  // 1`
- `fn len(): int` — The number of key/value pairs stored.
- `fn is_empty(): bool` — True when the dictionary holds no entries.
- `fn clear(): unit` — Drop all entries, leaving an empty dictionary.
- `fn keys(): [text]` — A copy of the keys in insertion order. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.set("a", 1); d.set("b", 2); d.keys()`
- `fn values(): [V]` — A copy of the values in insertion order. — e.g. `d: dict.Dict<int> = dict.Dict.new(); d.set("a", 1); d.set("b", 2); d.values()`
