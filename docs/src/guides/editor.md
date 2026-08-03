# Editor integration

Dune ships a Language Server (`dune lsp`) and a Zed extension.

## What the language server provides

- **Diagnostics** — type errors with precise source ranges.
- **Completions** — keywords, local symbols, imported module members, and typed
  receiver methods.
- **Hover** — the signature of a symbol plus its [doc-comment](../language/comments.md),
  including symbols from other modules.
- **Go-to-definition** — jumps to a local declaration or into the module file for
  imported symbols, aliases, and `from ... import` symbols.
- **Semantic highlighting** — distinguishes functions, methods, types, generic
  parameters, constants, variables, fields, modules, literals, operators, and
  doc comments. The server reports standard LSP token types and modifiers such
  as `declaration`, `readonly`, `static`, and `defaultLibrary`.

Any editor that speaks LSP can talk to `dune lsp` over stdio.

## Zed extension

The Zed extension lives in `editors/zed/`. It provides tree-sitter syntax
highlighting, a symbol outline, and wires up the `dune lsp` server. Semantic
tokens from the language server refine symbol roles when the server is running;
tree-sitter remains the lexical fallback while the server starts or is
unavailable. The extension pins the tree-sitter grammar to a commit of this
repository, so fallback highlighting always matches the language version.

To use it, install the extension as a Zed dev extension pointing at
`editors/zed/`, and make sure the `dune` binary (which also serves `dune lsp`) is
built and on your `PATH`. After pulling changes that touch the LSP, rebuild the
`dune` binary so the editor picks up new behavior.
