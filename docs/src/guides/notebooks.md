# Dune notebooks

Dune notebooks are interactive documents stored in the versioned `.dnb`
format. A notebook contains Markdown cells, Dune code cells, execution counts,
and captured output. Code runs through the same lexer, parser, type checker,
compiler, and bytecode VM as a `.dn` program.

## Create and open a notebook

```sh
dune notebook new notebooks/tutorial.dnb --title "Dune tutorial"
dune notebook serve notebooks/tutorial.dnb
```

`serve` starts Dune's dependency-free HTTP server and normally opens the
browser workspace. The workspace includes:

- a `.dnb` file browser rooted at the selected directory;
- a classic Jupyter-style menu, toolbar, prompt gutter, and cell selection;
- persistent light and dark themes that follow the system on first launch;
- Markdown and Dune code cell editing;
- adding, moving, and deleting cells;
- toolbar and keyboard cell-type switching (`Y` for code, `M` for Markdown);
- `Shift+Enter` runs a cell and selects the next one, creating a Code cell at
  the end; `Cmd/Ctrl+Enter` runs without moving;
- Run Cell and Run All actions;
- persistent kernel sessions with Restart Kernel;
- structured stdout and stderr output;
- saving and standalone HTML export.

The server listens only on `127.0.0.1:8888` by default. It generates a random
token and includes it in the printed browser URL. Every workspace API request
must provide that token. Paths are confined to the selected root, and only
`.dnb` files can be read or written.

Server options:

```sh
dune notebook serve notebooks/ --port 9000
dune notebook serve tutorial.dnb --no-open
dune notebook serve notebooks/ --token private_token
dune notebook serve notebooks/ --host 0.0.0.0
```

Binding to `0.0.0.0` exposes the server to the network and prints a warning.
Keep the token private. Explicit tokens may contain letters, digits, `-`, and
`_` (up to 128 characters). The server intentionally has no package, Jupyter,
Node.js, or browser-framework dependency.

## The `.dnb` format

`.dnb` is JSON with an explicit format version. Its structure follows the
useful parts of `.ipynb` while keeping the Dune schema small:

```json
{
  "dune_notebook": 1,
  "metadata": {
    "title": "A tiny notebook"
  },
  "cells": [
    {
      "id": "intro",
      "cell_type": "markdown",
      "source": "# Hello\n\nThis is **Markdown**."
    },
    {
      "id": "answer",
      "cell_type": "code",
      "source": "x = 40 + 2;\nx",
      "execution_count": 1,
      "outputs": [
        {
          "output_type": "stream",
          "name": "stdout",
          "text": "42\n"
        }
      ]
    }
  ]
}
```

Cell IDs are stable across edits. `source` may be a single string or an array
of strings when importing data from ipynb-style tooling. Code outputs use
`stdout` and `stderr` streams, so saved notebooks remain deterministic and
easy to diff.

Unknown object fields are ignored for forward compatibility. A newer
`dune_notebook` version is rejected with a clear error instead of being
silently misread.

## Kernel and cell execution

Cells execute in document order and share state: bindings, imports, functions,
records, choices, aliases, and mutations from earlier successful cells are
available later. A final bare expression is printed automatically, using
`to_text()` for displayable records.

The kernel continues an unchanged prefix. Editing or rerunning an earlier cell
rebuilds its dependent prefix so declarations do not become duplicated.
Parser, type-checker, and runtime failures stop Run All at the failed cell.
Front-end diagnostics include the notebook path and stable cell ID:

```text
error: expected type 'int' but got 'bool'
  --> tutorial.dnb#cell-types:1:12
```

The current kernel shares the REPL's accumulated-source implementation. Stable
repeated console output is hidden, but external side effects such as file
writes may run again when an edited prefix is rebuilt. Program stdin is not
available inside notebook cells.

## CLI and CI

Run a notebook and print each cell's captured output:

```sh
dune notebook run notebooks/tutorial.dnb
```

Refresh outputs and execution counts in the file:

```sh
dune notebook run notebooks/tutorial.dnb --update
```

Check saved outputs without modifying the notebook:

```sh
dune notebook check notebooks/tutorial.dnb
```

`check` exits non-zero when a cell fails or its saved stdout/stderr differs
from a fresh run, making notebooks reproducible CI artifacts.

Export the saved document as standalone HTML:

```sh
dune notebook export notebooks/tutorial.dnb --html
dune notebook export notebooks/tutorial.dnb --html -o reports/tutorial.html
```

The export embeds its responsive Jupyter-style layout, follows the reader's
light/dark system preference, escapes notebook content, and needs no running
server or external assets.

See [`examples/notebooks/scientific_workflow.dnb`](../../../examples/notebooks/scientific_workflow.dnb)
for a stateful matrix and automatic-differentiation notebook.
