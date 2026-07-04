# `process`

Process access: arguments and environment.

Access to command-line arguments and environment variables.

The heavy lifting is done by the `__process_args`, `__env_get`, and
`__process_cwd` VM intrinsics (dedicated opcodes, not C/C++ foreign
functions). This module is pure Dune: it only shapes the primitive
results into the `Maybe` type.

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
