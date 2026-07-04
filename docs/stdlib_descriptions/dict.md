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
