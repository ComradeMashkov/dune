# `fs`

File-system access (read, write, list).

Minimal file-system access built on the `__read_file` / `__write_file` VM
intrinsics (dedicated opcodes, not C/C++ foreign functions). This module is
pure Dune: it only shapes the primitive `(ok, payload)` results into the
`Outcome` type so errors stay explicit and compose with `?`.

`fs` provides minimal whole-file text I/O. It wraps the VM file intrinsics in `Outcome<text, text>` so successful reads and writes are explicit and filesystem errors stay in normal Dune values.

Use `read_text` to load a file as one text value and `write_text` to replace a file's contents. Both functions return `Done(...)` on success and `Failed(message)` on error, so they compose with `Outcome` helpers and the `?` operator.

```dn
import fs;
import outcome;

path = "dune_stdlib_example.txt";
written = fs.write_text(path, "hello from dune\n");

if written.is_done() {
    read_back = fs.read_text(written.value_or(""));
    print(read_back.value_or(""));
} else {
    print(written.failure_or("write failed"));
}
```

> Auto-generated from `stdlib/fs.dn` by `tools/gen_stdlib_docs.py`.

### `fn read_text(path: text): outcome.Outcome<text, text>`

Read the whole file at `path` as text.
On success returns `Done(contents)`, otherwise `Failed(message)`.

### `fn write_text(path: text, content: text): outcome.Outcome<text, text>`

Write `content` to `path`, replacing any existing file.
On success returns `Done(path)`, otherwise `Failed(message)`.
