# `regex`

Safe ASCII regular expressions for validation and text cleanup.

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

> Auto-generated from `stdlib/regex.dn` by `tools/gen_stdlib_docs.py`.

### `record Capture`

One captured subgroup. Captures that did not participate are omitted from Match.captures; use Match.capture(index) to get a Maybe<Capture>.

**Fields:**

- `index: int`
- `start: int`
- `end: int`
- `value: text`

### `record Match`

A single match with byte/glyph offsets into the searched text.

**Fields:**

- `start: int`
- `end: int`
- `value: text`
- `captures: [Capture]`

**Methods:**

- `fn len(): int`
- `fn is_empty(): bool`
- `fn capture(index: int): maybe.Maybe<Capture>`
- `fn capture_text(index: int, default: text): text`

### `record Regex`

A compiled regular expression. The engine is ASCII-oriented and supports a bounded, safe subset: literals, '.', '*', '+', '?', classes, anchors, and capturing groups. It intentionally rejects alternation, lookaround, backreferences, counted repetition, flags, and quantified groups.

**Fields:**

- `pattern: text`

**Methods:**

- `fn is_match(input: text): bool`
- `fn match(input: text): maybe.Maybe<Match>`
- `fn find(input: text): maybe.Maybe<Match>`
- `fn find_all(input: text): [Match]`
- `fn split(input: text): [text]`
- `fn replace(input: text, replacement: text): text`
- `fn replace_all(input: text, replacement: text): text`

### `fn empty_match(): Match`

A default empty match, useful for Maybe.value_or.

### `fn never(): Regex`

A regex that never matches. Useful with Outcome.value_or after compile.

### `fn compile(pattern: text): outcome.Outcome<Regex, text>`

Compile a pattern into a Regex. Errors are explicit Outcome failures rather than panics.
