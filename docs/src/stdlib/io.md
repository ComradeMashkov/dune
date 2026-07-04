# `io`

Standard input/output: print, read a line, and flush streams.

> Auto-generated from `stdlib/io.dn` by `tools/gen_stdlib_docs.py`.

### `fn write(message: text): outcome.Outcome<text, text>`

Write text to stdout without adding a newline.

**Example:**
```dune
io.write("hi")  // prints: hi (no trailing newline)
```

### `fn writeln(message: text): outcome.Outcome<text, text>`

Write text to stdout followed by `\n`.

**Example:**
```dune
io.writeln("hi")  // prints: hi
```

### `fn err_write(message: text): outcome.Outcome<text, text>`

Write text to stderr without adding a newline.

**Example:**
```dune
io.err_write("oops")  // prints to stderr: oops (no trailing newline)
```

### `fn err_writeln(message: text): outcome.Outcome<text, text>`

Write text to stderr followed by `\n`.

**Example:**
```dune
io.err_writeln("oops")  // prints to stderr: oops
```

### `fn flush(): outcome.Outcome<text, text>`

Flush stdout.

**Example:**
```dune
io.flush()  // flushes buffered stdout; returns Done("")
```

### `fn flush_err(): outcome.Outcome<text, text>`

Flush stderr.

**Example:**
```dune
io.flush_err()  // flushes buffered stderr; returns Done("")
```

### `fn read_line(): outcome.Outcome<text, text>`

Read one line from stdin without the trailing newline.

EOF is reported as `Failed("end of input")`; other stream errors are reported as `Failed("could not read from stdin")`.

**Example:**
```dune
io.read_line()  // reads one line from stdin, e.g. Done("hi there")
```

### `fn print<T>(value: T): unit`

Print a printable value to stdout without newline, ignoring output errors for convenience.

**Example:**
```dune
io.print("hi")  // prints: hi (no trailing newline)
```

### `fn println<T>(value: T): unit`

Print a printable value to stdout with a trailing newline, ignoring output errors.

**Example:**
```dune
io.println("hello, world")  // prints: hello, world
```

### `fn eprint<T>(value: T): unit`

Print a printable value to stderr without newline, ignoring output errors.

**Example:**
```dune
io.eprint("oops")  // prints to stderr: oops (no trailing newline)
```

### `fn eprintln<T>(value: T): unit`

Print a printable value to stderr with a trailing newline, ignoring output errors.

**Example:**
```dune
io.eprintln("oops")  // prints to stderr: oops
```

### `fn prompt(message: text): outcome.Outcome<text, text>`

Write a prompt to stdout, then read one line from stdin.

**Example:**
```dune
io.prompt("name? ")  // prints "name? ", then reads a line from stdin
```
