# Writing tests

Dune has tests built into the language. Tests can be written as a named `test`
block or as a zero-argument `unit` function marked with
[`@test`](../language/attributes.md#test). The [`dune test`](cli.md#run-tests)
command discovers both forms and reports the results.

## A first test

```dune
from assert import assert_eq;

fn double(x: int): int {
    return x * 2;
}

test "double multiplies by two" {
    assert_eq(double(2), 4);
    assert_eq(double(0), 0);
}
```

Run it with:

```sh
dune test path/to/file.dn
```

```text
running 1 test
test "double multiplies by two" ... ok

test result: ok. 1 passed; 0 failed
```

The name after `test` is an ordinary string literal, so it can contain spaces
and punctuation. The body is a normal block: it can declare bindings, call
functions, and loop, just like a function body.

The function form is useful when a test should also have an ordinary
declaration name:

```dune
@test
fn double_handles_zero(): unit {
    assert_eq(double(0), 0);
}
```

An attributed test function must take no parameters, explicitly return `unit`,
and cannot be generic, `foreign`, or `foreknown`.

Use `@ignore` to keep a temporarily disabled test discoverable, optionally with
a reason:

```dune
@test
@ignore("requires a local database")
fn database_round_trip(): unit { }
```

Use `@should_panic` when failure is the behavior under test. An optional text
argument requires the panic report to contain that text:

```dune
import runtime;

@test
@should_panic("index out of bounds")
fn rejects_invalid_index(): unit {
    runtime.panic("index out of bounds");
}
```

An expected-panic test fails if it returns normally or produces a different
message. Use `@should_fail("message")` instead when any typed runtime failure,
such as a bounds or arithmetic error, is acceptable. `@ignore`,
`@should_panic`, and `@should_fail` are mutually exclusive.

## Assertions

The [`assert`](../stdlib/assert.md) module provides the helpers that fail a
test. Each one calls `runtime.panic` when its check does not hold, which aborts
the current test and marks it as failed:

```dune
from assert import assert_eq, assert_true, assert_false;

test "assertions" {
    assert_eq(1 + 1, 2);          // any comparable type
    assert_true(1 < 2);
    assert_false(2 < 1);
}
```

`assert_eq<T>` works for any type whose values can be compared with `==`, so it
handles integers, reals, text, and booleans alike. Import the helpers with
`from assert import ...` to call them unqualified, or `import assert;` and write
`assert.assert_eq(...)`.

## How tests run

- **Isolation.** `dune test` runs *only* attributed test functions and `test`
  blocks. A file's top-level
  code (statements outside any function or test) does not run, so a script and
  its tests can live in the same file.
- **Shared declarations.** Top-level functions, constants, records, and imports
  are all in scope inside every test, so tests exercise the same code the rest
  of the file uses.
- **Independent failures.** A failing assertion aborts only the test it is in.
  The remaining tests still run, and the final line summarises how many passed
  and failed.
- **Exit code.** `dune test` exits non-zero if any test fails, which lets it
  gate a CI pipeline.
- **Ignored tests.** `@ignore` tests are not executed and are reported in a
  separate ignored count; they do not make the command fail.

A failing run looks like this:

```text
running 2 tests
test "this assertion holds" ... ok
test "this assertion fails" ... FAILED
    panic: assertion failed: values are not equal
    stack trace:
      0: assert.assert_eq
          at stdlib/assert.dn:54:9
      1: test "this assertion fails"
          at tests/example.dn:8:5

test result: FAILED. 1 passed; 1 failed
```

The named `test "..."` block is a real outer stack frame, so failures retain
the assertion helper, user functions, imported module files, and the exact line
inside the test. See [Runtime errors and stack traces](../language/runtime-errors.md).

## Rules

- `test` blocks and `@test` functions are only allowed at the top level of a
  file. A test inside a function or another block is a compile-time error.
- `test` blocks are ignored when a file is run normally. An `@test` function is
  not run automatically, but remains callable like any other named function.
