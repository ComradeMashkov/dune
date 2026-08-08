# The Dune Programming Language

Dune is a small, statically typed, compiled language with a clean C-family
syntax. It has function overloading, generics with bounds, records, choices
(tagged unions) with `when` expressions, contracts, first-class function values,
lambdas and value-capturing closures, deterministic cleanup with `defer`, and a
module system with a standard library written in Dune itself.

```dn
// Doc-comments above a declaration are shown on hover in the editor.
/// brief: Squares a value.
/// returns: value * value
fn square<T is numeric>(value: T): T {
    return value * value;
}

values: [int] = [1, 2, 3, 4];
total = values.filter(is_positive).map(square).sum();
print(total);
```

## One front end, one backend

Dune shares a single front end — lexer, parser, type checker — feeding a single
execution backend:

- A **bytecode virtual machine** that runs programs directly. It supports every
  language feature and needs no external toolchain.

## How to read this book

- **[Language](language/syntax.md)** — the reference for the language itself:
  syntax, types, functions and generics, records, choices, modules, and comments.
- **[Standard library](stdlib/index.md)** — one page per stdlib module, generated
  from the source doc-comments.
- **[Guides](guides/installation.md)** — building Dune, the command-line tool, the
  editor integration, and a tour of the runnable examples.

New to Dune? Start with [Installation](guides/installation.md) to build the
`dune` binary, then read [Syntax](language/syntax.md).
