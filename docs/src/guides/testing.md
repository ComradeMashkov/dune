# Writing tests

Dune has tests built into the language. A `test` block is a top-level construct
that names a small piece of code to run, and the [`dune test`](cli.md#run-tests)
command runs every block in a file and reports the results.

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

- **Isolation.** `dune test` runs *only* the `test` blocks. A file's top-level
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

A failing run looks like this:

```text
running 2 tests
test "this assertion holds" ... ok
test "this assertion fails" ... FAILED
    assertion failed: values are not equal

test result: FAILED. 1 passed; 1 failed
```

## Rules

- `test` blocks are only allowed at the top level of a file. A `test` inside a
  function or another block is a compile-time error.
- Test blocks are ignored when a file is run normally with `dune path/to/file.dn`;
  they exist purely for `dune test`.
