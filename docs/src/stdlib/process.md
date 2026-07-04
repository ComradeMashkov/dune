# `process`

Process access: arguments and environment.

Access to command-line arguments and environment variables.

The heavy lifting is done by the `__process_args`, `__env_get`, and
`__process_cwd` VM intrinsics (dedicated opcodes, not C/C++ foreign
functions). This module is pure Dune: it only shapes the primitive
results into the `Maybe` type.

`process` exposes the current program's command-line arguments, environment variables, and working directory. It wraps VM process intrinsics in ordinary Dune values so missing arguments and unavailable environment values are represented with `Maybe<text>`.

Use `args`, `arg_count`, and `arg` for CLI programs, `env` or `env_or` for environment-driven configuration, and `cwd` when code needs to report or resolve paths relative to the current directory.

```dn
import io;
import maybe;
import process;

io.println(process.arg_count());
io.println(process.arg(0).value_or("no argument"));
io.println(process.env_or("DUNE_PROFILE", "dev"));
io.println(process.cwd().has_value());
```

> Auto-generated from `stdlib/process.dn` by `tools/gen_stdlib_docs.py`.

### `fn args(): [text]`

The arguments passed after the script path (`dune script.dn a b c`).

**Example:**
```dune
process.args()
```

### `fn arg_count(): int`

The number of command-line arguments.

**Example:**
```dune
process.arg_count()
```

### `fn arg(index: int): maybe.Maybe<text>`

The argument at `index`, or `Absent` when out of range.

**Example:**
```dune
process.arg(0)
```

### `fn env(name: text): maybe.Maybe<text>`

The value of environment variable `name`, or `Absent` when unset.

**Example:**
```dune
process.env("PATH")
```

### `fn env_or(name: text, default: text): text`

The value of environment variable `name`, or `default` when unset.

**Example:**
```dune
process.env_or("DUNE_EXAMPLE_UNSET_VAR", "fallback")
```

### `fn cwd(): maybe.Maybe<text>`

The current working directory, or `Absent` when it cannot be determined.

**Example:**
```dune
process.cwd()
```
