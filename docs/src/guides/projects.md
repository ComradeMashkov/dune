# Projects and packages

Dune keeps the single-file workflow, but a directory containing `dune.toml`
becomes a project. The project manifest gives every tool the same package root
and module search paths.

## Layout

A conventional project looks like this:

```text
hello/
├── dune.toml
├── src/
│   ├── main.dn
│   └── greeting.dn
└── tests/
    └── greeting_test.dn
```

```toml
name = "hello"
version = "0.1.0"
sources = ["src"]
tests = ["tests"]
```

`name` is required and cannot be empty. `version` is optional. When omitted,
`sources` defaults to `["src"]` and `tests` defaults to `["tests"]`. Set
`tests = []` when a project deliberately has no test roots; `sources` must
always contain at least one directory.

The project manifest intentionally supports a small, dependency-free TOML
subset until the general serialization layer is available: one assignment per
line, double-quoted strings, arrays of double-quoted strings, `#` comments, and
an optional trailing array comma. Unknown or duplicate keys are errors.

All source and test paths are relative to the manifest directory. Absolute
paths, `..` components, duplicate paths, and paths that resolve outside the
project through a symbolic link are rejected.

## Discovery

Dune starts at the source file or workspace directory and walks toward the
filesystem root. The nearest `dune.toml` wins, so a nested package does not
inherit the outer package accidentally.

This discovery is shared by:

- `dune file.dn`, `dune check file.dn`, and `dune test file.dn`;
- `dune repl`, using its current working directory;
- notebook kernels, using the notebook directory;
- diagnostics, completion, hover, and definitions in `dune lsp`.

Project-wide targets and commands such as `dune build` belong to the build
system. The project model here supplies the root and module paths those commands
will consume.

## Module lookup

For a source file, imports are resolved in this order:

1. beside the importing file;
2. through every configured `sources` root;
3. through the standard library.

A file located under a configured test root also searches all `tests` roots,
after the production source roots. Production code cannot import test-only
helpers merely because they are part of the same project.

If the same module exists in multiple configured roots, Dune reports every
conflicting path instead of choosing one silently. Standard-library module
names are reserved: a local `math.dn`, for example, produces a shadowing error.

## Single-file compatibility

Without a `dune.toml`, Dune behaves as before: it looks beside the importing
file, then in the current working directory, then in the standard library.
Existing scripts do not need a manifest.
