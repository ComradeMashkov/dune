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

The compiler currently defines eight attributes. Unknown attributes, including
qualified names, are errors so that a misspelling cannot silently change a
build.

| Attribute | Purpose |
| --- | --- |
| `@deprecated("message")` | Warn whenever an old API is used. |
| `@experimental("message")` | Warn that an API contract may still change. |
| `@since("version")` | Record the release that introduced a declaration. |
| `@must_use` / `@must_use("message")` | Warn when a function result is discarded. |
| `@test` | Register a zero-argument `unit` function with `dune test`. |
| `@ignore` / `@ignore("reason")` | Discover but skip an `@test` function. |
| `@should_panic` / `@should_panic("message")` | Require an `@test` function to panic. |
| `@should_fail` / `@should_fail("message")` | Require any typed runtime failure. |

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

## `@experimental(message)`

`@experimental` has the same declaration targets and module behavior as
`@deprecated`, but communicates that an API is available for evaluation while
its contract may still change:

```dune
@experimental("the shape API may change before 1.0")
export fn infer_shape(): int {
    return 2;
}
```

Every use produces a source-mapped warning. `@experimental` requires one
non-empty text message and cannot be combined with `@deprecated` on the same
declaration.

## `@since(version)`

`@since` records the release in which a declaration became available. It
accepts exactly one non-empty text value and works on every supported top-level
declaration:

```dune
@since("0.14.0")
export fn stable_api(): int {
    return 42;
}
```

It does not change runtime behavior. The metadata is preserved in the AST and
modules and is rendered as an “Available since” callout by LSP hover and
`dune doc`.

## `@must_use(message?)`

`@must_use` is valid on functions that explicitly return a non-`unit` value.
It warns when a direct call is used as a standalone statement and its result is
discarded:

```dune
@must_use("check whether the operation succeeded")
fn save(): bool {
    return true;
}

save();                 // warning
saved: bool = save();   // no warning
```

The explanatory text is optional, but when present it must be a non-empty text
literal. The rule also works for exported, imported, overloaded, and generic
functions because the metadata follows the resolved overload.

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

### `@ignore(reason?)`

`@ignore` skips an `@test` function without compiling out the declaration. The
optional reason is printed in the test report, and ignored tests are counted
separately:

```dune
@test
@ignore("requires a local service")
fn integration_test(): unit { }
```

### `@should_panic(message?)`

`@should_panic` makes a panic the successful outcome of an `@test` function.
With no argument, any runtime panic is accepted. With a text argument, the
panic message must contain that text:

```dune
import runtime;

@test
@should_panic("index out of bounds")
fn rejects_bad_index(): unit {
    runtime.panic("index out of bounds");
}
```

A test fails if it returns normally or panics with a different message.

### `@should_fail(message?)`

`@should_fail` is the broader counterpart to `@should_panic`: any typed runtime
failure (`panic`, bounds, arithmetic, type, I/O, or foreign error) satisfies the
expectation. Its optional text argument must occur in the primary error message:

```dune
@test
@should_fail("array index out of bounds")
fn rejects_bad_index(): unit {
    values: [int] = [1];
    io.println(values[4]);
}
```

`@ignore`, `@should_panic`, and `@should_fail` require `@test`. They are
mutually exclusive so every test has one unambiguous execution policy.

## Placement and validation

Attributes are currently supported only on top-level declarations. They cannot
be attached to local bindings, statements, parameters, fields, variants, or
record methods. The compiler reports duplicate attributes, unknown attributes,
invalid targets, invalid arguments, conflicting combinations, and invalid test
signatures at the attribute's source location.
