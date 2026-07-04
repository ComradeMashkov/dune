`text` adds string-style helpers to the built-in `text` type and predicate helpers for `glyph` values. Some methods forward to VM text operations, while others are implemented in Dune using indexing and slicing.

Use it for length checks, substring tests, slicing, trimming whitespace, glyph search/counting, and ASCII character classification. Importing the module enables receiver-style calls such as `value.trim()` and `value.starts_with(...)`.

```dn
import io;
import text;

raw = "  dune language  ";
clean = raw.trim();

io.println(clean.starts_with("dune"));
io.println(clean.index_of('g'));
io.println(text.is_alpha(clean.char_at(0)));
```
