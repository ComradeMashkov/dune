# The `dune` command-line tool

The `dune` binary has a small set of commands.

## Run a program

```sh
dune path/to/program.dn [args...]
```

Runs a program on the bytecode VM. Any trailing arguments are exposed to the
program through the [`process`](../stdlib/process.md) module's `args()`.

## Type-check without running

```sh
dune check path/to/program.dn
```

Type-checks the program and reports diagnostics without executing it. Exits
non-zero if there are errors.

## Build a native binary

```sh
dune build path/to/program.dn -o path/to/output
```

Compiles the program ahead of time through the native LLVM backend. Requires a
build configured with `-D DUNE_ENABLE_NATIVE=ON`. Features the native backend does
not support are documented per feature in the language reference.

## Emit LLVM IR

```sh
dune llvm path/to/program.dn -o path/to/output.ll
```

Writes the generated LLVM IR for inspection.

## Language server

```sh
dune lsp
```

Starts the Language Server over stdio. Editors launch this for diagnostics,
completions, hover, and go-to-definition — see [Editor integration](editor.md).

## Version

```sh
dune --version
```
