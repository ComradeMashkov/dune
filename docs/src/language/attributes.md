# Source-level attributes

Attributes attach compile-time metadata to a top-level declaration. They are
written before the declaration (and before `export`, `foreign`, or `foreknown`)
and become part of the AST:

```dune
@deprecated("use matrix.dot")
export fn dot_old(left: int, right: int): int {
    return left * right;
}

@test
fn vector_sum(): unit {
    return;
}
```

Several attributes can be stacked. Attribute names may be qualified with dots,
and the syntax accepts an optional comma-separated list of literal arguments:

```dune
@tool.flags(-2, 3.5, 'x', true, "stable",)
fn configured(): unit { }
```

Integer, real, glyph, text, raw-text, and boolean literals are represented in
the AST without becoming runtime expressions. A trailing comma is accepted.
Calls, identifiers, arrays, and other expressions are rejected as attribute
arguments.

The compiler currently defines two attributes. Unknown attributes, including
qualified names, are errors so that a misspelling cannot silently change a
build.

## `@deprecated(message)`

`@deprecated` accepts exactly one non-empty text argument. It is valid on
functions, constants, records, choices, contracts, and type aliases:

```dune
@deprecated("use MAX_RETRIES")
const OLD_RETRIES: int = 3;

@deprecated("use Result")
choice LegacyResult { Ok(int), Error(text) }
```

Using a deprecated function, function value, constant, record constructor or
literal, record member, choice variant, type annotation, type alias, or contract
bound produces a source-mapped warning. A warning does not fail `dune <file>`,
`dune check`, or `dune test`. Metadata is
preserved when an exported declaration is qualified and loaded from another
module, so clients of that module receive the same warning.

The language server publishes these diagnostics with LSP warning severity and
shows the attribute plus a deprecation callout in hover. `dune doc` includes the
deprecation message in generated API documentation.

## `@test`

`@test` turns a named function into a test discovered by `dune test`:

```dune
from assert import assert_eq;

@test
fn addition(): unit {
    assert_eq(20 + 22, 42);
}
```

A test function must:

- be top-level;
- take no arguments;
- explicitly return `unit`;
- have a body (it cannot be `foreign`);
- be non-generic and non-`foreknown`.

Attributed test functions and `test "name" { ... }` blocks can coexist and run
in declaration order. The function name is used in the test report. Like test
blocks, they are not executed automatically during an ordinary run. Because the
`@test` form is still a named function, user code can call it explicitly.

## Placement and validation

Attributes are currently supported only on top-level declarations. They cannot
be attached to local bindings, statements, parameters, fields, variants, or
record methods. The compiler reports duplicate attributes, unknown attributes,
invalid targets, invalid arguments, and invalid `@test` signatures at the
attribute's source location.
