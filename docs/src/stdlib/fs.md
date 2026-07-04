# `fs`

File-system access (read, write, list).

Minimal file-system access built on the `__read_file` / `__write_file` VM
intrinsics (dedicated opcodes, not C/C++ foreign functions). This module is
pure Dune: it only shapes the primitive `(ok, payload)` results into the
`Outcome` type so errors stay explicit and compose with `?`.

> Auto-generated from `stdlib/fs.dn` by `tools/gen_stdlib_docs.py`.

### `fn read_text(path: text): outcome.Outcome<text, text>`

Read the whole file at `path` as text.
On success returns `Done(contents)`, otherwise `Failed(message)`.

### `fn write_text(path: text, content: text): outcome.Outcome<text, text>`

Write `content` to `path`, replacing any existing file.
On success returns `Done(path)`, otherwise `Failed(message)`.
