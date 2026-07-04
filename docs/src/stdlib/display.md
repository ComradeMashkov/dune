# `display`

The `Display` contract and `show` helper.

> Auto-generated from `stdlib/display.dn` by `tools/gen_stdlib_docs.py`.

### `fn show<T is Display>(value: T): text`

Render any Display value to text. Useful when a function wants the text
instead of printing it directly.
The bound `T is Display` guarantees `value` has a `to_text` method to call.
