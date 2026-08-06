`set` is a mutable collection of unique `text` values. It keeps values in insertion order and stores them in a simple array, giving deterministic behavior with a small API.

Use `Set` when you need membership checks, duplicate suppression, and removal for strings. `add` ignores duplicates, `contains` reports membership, `remove` tells you whether anything was removed, and `values` returns a fresh array of the stored values. `copy()` creates a set with an independent backing array.

```dn
import io;
import set;

seen: set.Set = set.Set.new();
seen.add("lexer");
seen.add("parser");
seen.add("lexer");

io.println(seen.len());
io.println(seen.contains("parser"));
io.println(seen.values().len());
```
