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
import maybe;
import process;

print(process.arg_count());
print(process.arg(0).value_or("no argument"));
print(process.env_or("DUNE_PROFILE", "dev"));
print(process.cwd().has_value());
```

> Auto-generated from `stdlib/process.dn` by `tools/gen_stdlib_docs.py`.

### `fn args(): [text]`

The arguments passed after the script path (`dune script.dn a b c`).

### `fn arg_count(): int`

The number of command-line arguments.

### `fn arg(index: int): maybe.Maybe<text>`

The argument at `index`, or `Absent` when out of range.

### `fn env(name: text): maybe.Maybe<text>`

The value of environment variable `name`, or `Absent` when unset.

### `fn env_or(name: text, default: text): text`

The value of environment variable `name`, or `default` when unset.

### `fn cwd(): maybe.Maybe<text>`

The current working directory, or `Absent` when it cannot be determined.
