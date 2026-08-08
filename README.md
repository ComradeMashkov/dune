# Dune

> A small, statically-typed language that compiles through a real pipeline —
> **lexer → parser → AST → type checker → bytecode VM** — with a standard
> library increasingly written in Dune itself.

[![CI](https://github.com/ComradeMashkov/dune/actions/workflows/ci.yml/badge.svg)](https://github.com/ComradeMashkov/dune/actions/workflows/ci.yml)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-2563eb)](https://comrademashkov.github.io/dune/)
[![Language](https://img.shields.io/badge/C%2B%2B-23-00599c)](CMakeLists.txt)

Dune keeps systems-style code small, readable, and predictable: explicit types
where they matter, overloads and generics, records with methods, choices matched
by `when`, deterministic cleanup with `defer`, operator overloading, and modules
loaded from `.dn` files. The type
checker rejects mismatched assignments, calls, returns, and operators before
execution; the VM adds runtime checks for things like invalid indexes and slices.

📖 **[Documentation site](https://comrademashkov.github.io/dune/)** — the language
reference, the standard-library reference (generated from source doc-comments,
including a [chart-type gallery](https://comrademashkov.github.io/dune/stdlib/plot.html)),
and guides. This README is only a quick tour; the site is the full story.

## Example

```dn
import io;
import fmt;

// Records bundle data with methods. `+` dispatches to a record's `add` method,
// and a `to_text` method makes a value printable.
record Vec2 {
    x: int,
    y: int,
    fn add(other: Vec2): Vec2 { return Vec2 { x: this.x + other.x, y: this.y + other.y }; }
    fn to_text(): text { return fmt.format("({}, {})", this.x, this.y); }
}

// Choices are tagged unions matched with `when`.
choice Shape { Dot, Line(Vec2) }

fn describe(s: Shape): text {
    return when s {
        Dot => "a dot",
        Line(v) => fmt.format("line to {}", v),
    };
}

// A generic function constrained to ordered types.
fn largest<T is ordered>(values: [T]): T {
    best = values[0];
    for v in values {
        if v > best { best = v; }
    }
    return best;
}

a: Vec2 = Vec2 { x: 1, y: 2 };
b: Vec2 = Vec2 { x: 3, y: 4 };
edge: Shape = Line(a + b);

io.println(a + b);                 // (4, 6)
io.println(largest([3, 9, 2, 7])); // 9
io.println(describe(edge));        // line to (4, 6)
```

## Quick start

```bash
cmake -S . -B build
cmake --build build -j
./build/dune examples/hello.dn        # run a program
ctest --test-dir build                # run the test suite
```

The build runs `clang-format` and `clang-tidy` before compiling. If those tools
are not installed, configure with `-D DUNE_ENABLE_LINT=OFF`.

Arguments after the script are exposed through `process.args()`. The standard
library is resolved from `./stdlib` by default; set `DUNE_STDLIB_PATH` to point
elsewhere.

## The `dune` command

| Command | Purpose |
| --- | --- |
| `dune <file.dn>` | Type-check and run a program on the VM. |
| `dune check <file.dn>` | Type-check only, printing a short pipeline trace. |
| `dune repl` | Start an interactive VM session with persistent definitions. |
| `dune notebook <command>` | Create, run, check, export, or serve `.dnb` notebooks. |
| `dune test <file.dn>` | Run every `test "..." { ... }` block and report results. |
| `dune doc <path> [-o dir]` | Generate Markdown API docs from source doc-comments. |
| `dune lsp` | Start the editor language server (diagnostics, hover, completion). |

Color is automatic on terminals; force it with `DUNE_COLOR=always` or disable it
with `DUNE_COLOR=never` / `NO_COLOR=1`.

Start an interactive session with `dune repl`. Bindings, imports, functions,
records, choices, and aliases remain available between entries; a bare
expression prints its value:

```text
$ dune repl
Dune 0.13.1
Type :help for help.
> x = 40 + 2;
> x
42
> import math;
> math.square(9)
81
```

Use `:help`, `:reset`, and `:quit` for session control. The initial REPL
recompiles and re-executes accumulated successful input, so file writes and
other external side effects may repeat. Interactive program stdin is reserved
for REPL commands in this first version.

Create a versioned Dune notebook and launch the local browser workspace:

```sh
dune notebook new notebooks/demo.dnb --title "Dune demo"
dune notebook serve notebooks/demo.dnb
```

The `.dnb` format is a JSON document similar to `.ipynb`: it stores Markdown
and Dune code cells, stable cell IDs, execution counts, and structured
stdout/stderr outputs. The built-in server provides a file browser, cell
editing, a Jupyter-style menu and toolbar, persistent light/dark themes, Run
Cell/Run All, live Dune syntax highlighting, offline LaTeX formulas in Markdown
cells, inline SVG plots, kernel restart, per-cell/all-output clearing, saving,
and standalone HTML export. It listens on `127.0.0.1` by default and
protects its API with a random token.

Notebook files are also first-class in scripts and CI:

```sh
dune notebook run notebooks/demo.dnb --update
dune notebook check notebooks/demo.dnb
dune notebook export notebooks/demo.dnb --html
```

See the runnable [`scientific_workflow.dnb`](examples/notebooks/scientific_workflow.dnb),
[`plot_gallery.dnb`](examples/notebooks/plot_gallery.dnb), and
[`statistical_analysis.dnb`](examples/notebooks/statistical_analysis.dnb), and
[`functions_and_closures.dnb`](examples/notebooks/functions_and_closures.dnb)
examples, plus the
[notebook guide](https://comrademashkov.github.io/dune/guides/notebooks.html).

For the same feature in a normal source file, run
[`examples/functions_and_closures.dn`](examples/functions_and_closures.dn).
For resource lifetime and every `defer` exit path, run
[`examples/defer_cleanup.dn`](examples/defer_cleanup.dn).

## Standard library

The standard library lives in [`stdlib/`](stdlib/) as plain `.dn` files: `io`,
`fmt`, `math`, `matrix`, `stats`, `text`, `array`, `dict`, `set`, `random`, `fs`,
`process`, `csv`, `regex`, `cli`, `log`, `plot`, `canvas`, `maybe`, `outcome`,
and more. Each module's reference page on the
[documentation site](https://comrademashkov.github.io/dune/) is generated from
its doc-comments, with runnable examples.

## Editor support

A Zed extension in [`editors/zed/`](editors/zed/) provides Tree-sitter
highlighting plus semantic highlighting, completion, hover, symbol outline,
go-to-definition, and diagnostics through `dune lsp`. Build Dune, then in Zed run
`Extensions: Install Dev Extension` and select the `editors/zed` directory (not
the repository root). See the [editor guide](https://comrademashkov.github.io/dune/guides/editor.html).

## Project layout

| Path | Contents |
| --- | --- |
| `src/` | Compiler and VM: `lexer`, `parser`, `ast`, `typechecker`, `compiler`, `vm`, `repl`, `notebook`, `lsp`, `doc`, `diagnostics`. |
| `stdlib/` | Standard-library modules written in Dune. |
| `examples/` | Runnable example programs. |
| `tests/` | Unit tests and `.dn` fixtures driven by CTest. |
| `docs/` | mdBook sources for the documentation site. |
| `editors/zed/` | Zed language extension and Tree-sitter grammar. |

## Status

Dune implements a compact but real language: a static type checker with
overloads, generics and bounds, contracts, records, choices, tuples, and type
aliases; first-class function values, lambdas, and capturing closures; array
comprehensions; deterministic `defer` cleanup; operator overloading; a bytecode
compiler and VM with runtime bounds checks; and the CLI,
LSP, doc generator, and test runner above. The
[language reference](https://comrademashkov.github.io/dune/) covers each feature
in detail.
