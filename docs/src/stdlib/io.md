# `io`

Standard input/output: print, read a line, and flush streams.

> Auto-generated from `stdlib/io.dn` by `tools/gen_stdlib_docs.py`.

### `fn write(message: text): outcome.Outcome<text, text>`

Write text to stdout without adding a newline.

### `fn writeln(message: text): outcome.Outcome<text, text>`

Write text to stdout followed by `\n`.

### `fn err_write(message: text): outcome.Outcome<text, text>`

Write text to stderr without adding a newline.

### `fn err_writeln(message: text): outcome.Outcome<text, text>`

Write text to stderr followed by `\n`.

### `fn flush(): outcome.Outcome<text, text>`

Flush stdout.

### `fn flush_err(): outcome.Outcome<text, text>`

Flush stderr.

### `fn read_line(): outcome.Outcome<text, text>`

Read one line from stdin without the trailing newline.

EOF is reported as `Failed("end of input")`; other stream errors are reported as `Failed("could not read from stdin")`.

### `fn print<T>(value: T): unit`

Print a printable value to stdout without newline, ignoring output errors for convenience.

### `fn println<T>(value: T): unit`

Print a printable value to stdout with a trailing newline, ignoring output errors.

### `fn eprint<T>(value: T): unit`

Print a printable value to stderr without newline, ignoring output errors.

### `fn eprintln<T>(value: T): unit`

Print a printable value to stderr with a trailing newline, ignoring output errors.

### `fn prompt(message: text): outcome.Outcome<text, text>`

Write a prompt to stdout, then read one line from stdin.
