# `text`

Text and glyph helpers.

String utilities implemented as methods on the built-in `text` type and a
few free predicate functions on `glyph` (a single character). Several
methods simply forward to the VM's native text operations of the same name;
others are written in Dune on top of indexing and slicing.

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

> Auto-generated from `stdlib/text.dn` by `tools/gen_stdlib_docs.py`.

### `method text.len(): int`

The length in glyphs of this text (forwards to the native operation).

### `method text.is_empty(): bool`

True when the text has zero length (forwards to the native operation).

### `method text.contains(needle: text): bool`

True when `needle` occurs somewhere in this text.

**Example:**
```dune
"hello world".contains("wor")  // 1
```

### `method text.starts_with(prefix: text): bool`

True when this text begins with `prefix`.

**Example:**
```dune
"hello".starts_with("he")  // 1
```

### `method text.ends_with(suffix: text): bool`

True when this text ends with `suffix`.

### `method text.char_at(index: int): glyph`

The glyph at `index` (0-based) via indexing.

### `method text.slice(start: int, end: int): text`

The substring from `start` (inclusive) to `end` (exclusive).

**Example:**
```dune
"hello world".slice(0, 5)  // hello
```

### `method text.prefix(end: int): text`

The first `end` glyphs of the text.

### `method text.suffix(start: int): text`

Everything from `start` to the end of the text.

### `method text.index_of(needle: glyph): int`

The index of the first occurrence of glyph `needle`, or -1 if absent.

**Example:**
```dune
"hello".index_of('l')  // 2
```

### `method text.count(needle: glyph): int`

How many times glyph `needle` occurs in the text.

### `method text.trim_start(): text`

Drop leading whitespace and return the remainder.

### `method text.trim_end(): text`

Drop trailing whitespace and return the remainder.

### `method text.trim(): text`

Drop whitespace from both ends by composing the two trims.

**Example:**
```dune
"  hi  ".trim()  // hi
```

### `fn is_space(value: glyph): bool`

True when `value` is a space, newline, carriage return, or tab.

### `fn is_digit(value: glyph): bool`

True when `value` is an ASCII decimal digit '0'..'9'.

**Example:**
```dune
text.is_digit('5')  // 1
```

### `fn is_lower(value: glyph): bool`

True when `value` is a lowercase ASCII letter 'a'..'z'.

### `fn is_upper(value: glyph): bool`

True when `value` is an uppercase ASCII letter 'A'..'Z'.

### `fn is_alpha(value: glyph): bool`

True when `value` is any ASCII letter (upper or lower case).

**Example:**
```dune
text.is_alpha('a')  // 1
```

### `method text.concat(other: text): text`

This text followed by `other` (the method form of `this + other`).

**Example:**
```dune
"foo".concat("bar")  // foobar
```

### `method text.repeat(count: int): text`

This text repeated `count` times ("" for count <= 0).

**Example:**
```dune
"ab".repeat(3)  // ababab
```

### `method text.reverse(): text`

A new text with the glyphs in reverse order.

**Example:**
```dune
"abc".reverse()  // cba
```

### `method text.to_upper(): text`

A copy of this text with every ASCII lowercase letter upper-cased.

**Example:**
```dune
"Hello, World!".to_upper()  // HELLO, WORLD!
```

### `method text.to_lower(): text`

A copy of this text with every ASCII uppercase letter lower-cased.

**Example:**
```dune
"Hello, World!".to_lower()  // hello, world!
```

### `method text.replace(target: text, replacement: text): text`

Every occurrence of `target` replaced by `replacement`. Returns the text unchanged when `target` is empty (which would otherwise never advance).

**Example:**
```dune
"a-b-c".replace("-", "+")  // a+b+c
```

### `method text.split(separator: glyph): [text]`

Split into the pieces separated by glyph `separator`. Adjacent separators yield empty pieces, and the result always has one more piece than the number of separators.

**Example:**
```dune
"a,b,c".split(',').len()  // 3
```

### `method text.pad_start(width: int, fill: glyph): text`

Left-pad with `fill` until the text is at least `width` glyphs wide.

**Example:**
```dune
"42".pad_start(5, '0')  // 00042
```

### `method text.pad_end(width: int, fill: glyph): text`

Right-pad with `fill` until the text is at least `width` glyphs wide.

**Example:**
```dune
"42".pad_end(5, '.')  // 42...
```

### `fn join(parts: [text], separator: text): text`

Join `parts` into a single text, inserting `separator` between them.

**Example:**
```dune
join(["a", "b", "c"], ", ")  // a, b, c
```
