`regex` provides a small, safe ASCII regular-expression engine written in Dune.

Use it for validation and text cleanup when a predictable subset is enough:
literals, `.`, `*`, `+`, `?`, character classes and ranges, anchors, and
capturing groups. `compile` returns `Outcome<Regex, text>` so invalid or
unsupported syntax stays explicit.

```dn
import regex;

ident = regex.compile(r"^[a-z_][a-z0-9_]*$").value_or(regex.never());
print(ident.is_match("hello_1"));

pairs = regex.compile(r"([a-z]+)([0-9]+)").value_or(regex.never());
print(pairs.replace_all("a1 b22", "$2:$1"));
```

Unsupported syntax returns a compile error: alternation, lookaround,
backreferences, counted repetition, flags, word-boundary escapes, absolute
anchor escapes, and quantified capture groups.
