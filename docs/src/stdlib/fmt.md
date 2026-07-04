# `fmt`

String formatting with `{}` placeholders.

> Auto-generated from `stdlib/fmt.dn` by `tools/gen_stdlib_docs.py`.

### `const __fmt_module_marker: int`

Formatting is compiler-backed for now because Dune does not have variadic user functions yet. Import this module and call `fmt.format(...)` to use it.

`fmt.format(template, ...values)` returns a `text` value: each `{}` placeholder is filled left-to-right by the next argument. Any value type is accepted — int, real, bool (rendered as `1`/`0`), text, and glyph — and text with no `{}` is returned unchanged.

**Example:**
```dune
fmt.format("{}-{}", 4, 2)  // "4-2"
fmt.format("{} + {} = {}", 2, 3, 5)  // "2 + 3 = 5"
fmt.format("Hello, {}!", "world")  // "Hello, world!"
fmt.format("half is {}", 1.5)  // "half is 1.5"
fmt.format("flag is {}", true)  // "flag is 1"
fmt.format("letter {}", 'A')  // "letter A"
```
