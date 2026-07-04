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
- `fn capture_text(index: int, default: text): text` — The text of capture group `index`, or `default` when it did not match. — e.g. `regex.compile(r"([a-z]+)([0-9]+)").value_or(regex.never()).find("id42").value_or(regex.empty_match()).capture_text(1, "")  // id`

### `record Regex`

A compiled regular expression. The engine is ASCII-oriented and supports a bounded, safe subset: literals, '.', '*', '+', '?', classes, anchors, and capturing groups. It intentionally rejects alternation, lookaround, backreferences, counted repetition, flags, and quantified groups.

**Fields:**

- `pattern: text`

**Methods:**

- `fn is_match(input: text): bool` — True when the pattern matches anywhere in the input. — e.g. `regex.compile(r"\d+").value_or(regex.never()).is_match("abc123")  // 1`
- `fn match(input: text): maybe.Maybe<Match>` — Alias for find: the first match as a Maybe<Match>. — e.g. `regex.compile(r"\d+").value_or(regex.never()).match("abc123").value_or(regex.empty_match()).value  // 123`
- `fn find(input: text): maybe.Maybe<Match>` — The leftmost match as a Maybe<Match>, or absent when there is none. — e.g. `regex.compile(r"\d+").value_or(regex.never()).find("abc123def").value_or(regex.empty_match()).value  // 123`
- `fn find_all(input: text): [Match]` — Every non-overlapping match, from left to right. — e.g. `regex.compile(r"\d+").value_or(regex.never()).find_all("a1 b22 c333").len()  // 3`
- `fn split(input: text): [text]` — Split the input around each match, returning the pieces in between. — e.g. `regex.compile(r"\s+").value_or(regex.never()).split("a b c").len()  // 3`
- `fn replace(input: text, replacement: text): text` — Replace the first match; `$1`..`$9` in `replacement` expand captures. — e.g. `regex.compile(r"\d+").value_or(regex.never()).replace("abc123def456", "#")  // abc#def456`
- `fn replace_all(input: text, replacement: text): text` — Replace every match; `$1`..`$9` in `replacement` expand captures. — e.g. `regex.compile(r"\d+").value_or(regex.never()).replace_all("abc123def456", "#")  // abc#def#`

### `fn empty_match(): Match`

A default empty match, useful for Maybe.value_or.

**Example:**
```dune
regex.empty_match().is_empty()  // 1
```

### `fn never(): Regex`

A regex that never matches. Useful with Outcome.value_or after compile.

**Example:**
```dune
regex.never().is_match("anything")  // 0
```

### `fn compile(pattern: text): outcome.Outcome<Regex, text>`

Compile a pattern into a Regex. Errors are explicit Outcome failures rather than panics.

**Example:**
```dune
regex.compile(r"\d+").is_done()  // 1
```
