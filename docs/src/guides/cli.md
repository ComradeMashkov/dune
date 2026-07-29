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

## Interactive REPL

```sh
dune repl
```

Starts an interactive session on the bytecode VM. Successful input remains in
scope for later entries, including bindings, imports, functions, records,
choices, and type aliases. Bare expressions print their result automatically:

```text
> value = 40 + 2;
> value
42
> import math;
> math.square(9)
81
```

Blocks and declarations may span multiple lines; the prompt changes to `...`
until the entry is complete. Parser, type-checker, and runtime errors are
reported without ending the session.

The built-in commands are:

- `:help` — show the command list.
- `:reset` — clear all accumulated source and values.
- `:quit` — exit successfully.

The first implementation recompiles and re-executes accumulated successful
source for every entry. Stable repeated console output is hidden, but external
side effects such as file writes can run again. Program reads from stdin are
not available because stdin belongs to the REPL command loop. True incremental
compiler and VM state can replace this model later without changing the command
surface.

## Notebooks

```sh
dune notebook new tutorial.dnb --title "Tutorial"
dune notebook serve tutorial.dnb
```

Notebooks are versioned `.dnb` JSON documents with Markdown cells, Dune code
cells, execution counts, and structured outputs. The local server opens a
token-protected browser workspace for editing and executing cells. The same
files can be run, checked, and exported without starting the server:

```sh
dune notebook run tutorial.dnb
dune notebook run tutorial.dnb --update
dune notebook check tutorial.dnb
dune notebook export tutorial.dnb --html -o tutorial.html
```

See the [notebook guide](notebooks.md) for the file format, server options,
kernel behavior, and CI workflow.

## Run tests

```sh
dune test path/to/program.dn
```

Runs every [`test "..." { ... }`](testing.md) block in the file and prints a
per-test `ok`/`FAILED` line plus a summary. Each block runs in isolation — the
file's top-level code is skipped, so only the tests execute — while top-level
functions, constants, and imports remain in scope. A failed assertion aborts
just that test; the command exits non-zero if any test fails.

## Language server

```sh
dune lsp
```

Starts the Language Server over stdio. Editors launch this for diagnostics,
completions, hover, and go-to-definition — see [Editor integration](editor.md).

## Generate API documentation

```sh
dune doc path/to/module.dn            # print Markdown to stdout
dune doc path/to/module.dn -o out.md  # write one page
dune doc path/to/modules -o out/      # a page per module, plus index.md
dune doc path/to/modules -o out/ --check   # fail if out/ is out of date
```

Renders a module's public API — functions, constants, type aliases, records
(with their fields and methods), choices, and contracts — to Markdown, using the
real parser so signatures and [doc-comments](../language/comments.md) match the
source exactly. Only exported declarations appear (a module with no `export` is
treated as fully public). `--check` regenerates in memory and exits non-zero on
any drift, which keeps generated docs current in CI.

## Diagnostics

When a command fails to lex, parse, or type-check the main file, `dune` prints a
source snippet that points at the exact span the error refers to:

```text
error: expected type 'int' but got 'text'
  --> program.dn:1:10
  |
1 | x: int = "hello";
  |          ^^^^^^^
```

The `-->` line gives `file:line:column`, and the caret underline marks the
offending token or expression. Lexer, parser, and type-check errors all use this
format; `dune check` shows it beneath its per-stage progress trace. Errors from imported modules and runtime failures fall
back to a single-line message (source snippets for other files are a follow-up).
The same locations are sent to editors over the [language server](editor.md), so
squiggles land on the right span.

## Version

```sh
dune --version
```
