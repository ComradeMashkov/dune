# `outcome`

The result-style `Outcome<T, E>` choice and helpers.

`outcome` defines `Outcome<T, E>`, the standard success-or-failure type. `Done(T)` carries a successful value and `Failed(E)` carries an error value; the `?` operator understands this type and short-circuits failures from functions that also return an `Outcome`.

Use it for recoverable errors such as parsing, file I/O, and validation. The specialized helpers for `int` and `text` reduce boilerplate in common cases, and `value_or` and `failure_or` make simple fallback handling concise.

```dn
import outcome;

fn checked(value: int): outcome.Outcome<int, text> {
    if value > 0 {
        return outcome.done_int(value);
    }

    return outcome.failed_int("not positive");
}

fn doubled(value: int): outcome.Outcome<int, text> {
    number: int = checked(value)?;
    return outcome.done_int(number * 2);
}

print(doubled(21).value_or(0));
print(doubled(0).failure_or("ok"));
```

> Auto-generated from `stdlib/outcome.dn` by `tools/gen_stdlib_docs.py`.

### `choice Outcome<T, E>`

`Outcome<T, E>` is a result type: either success carrying a `T` or failure carrying an error `E`. It is the explicit error-handling type that the `?` operator understands. The two variants are `Done(T)` and `Failed(E)`.

### `fn done<T, E>(value: T, error_default: E): Outcome<T, E>`

Build a success. `error_default` is only present to fix the error type E (Dune needs a value of E to infer it); it is not stored.

**Example:**
```dune
outcome.done(7, "").is_done()  // 1
```

### `fn failed<T, E>(value_default: T, error: E): Outcome<T, E>`

Build a failure. `value_default` is only present to fix the success type T; it is not stored.

**Example:**
```dune
outcome.failed(0, "bad").is_failed()  // 1
```

### `fn done_int(value: int): Outcome<int, text>`

A success specialised to `Outcome<int, text>` (empty text pins E = text).

### `fn failed_int(error: text): Outcome<int, text>`

A failure specialised to `Outcome<int, text>` (0 pins T = int).

### `fn done_text(value: text): Outcome<text, text>`

A success specialised to `Outcome<text, text>`.

### `fn failed_text(error: text): Outcome<text, text>`

A failure specialised to `Outcome<text, text>`.

### `method<T, E> Outcome<T, E>.is_done(): bool`

Method: true when this Outcome is a success.

**Example:**
```dune
outcome.failed(0, "bad").is_done()  // 0
```

### `method<T, E> Outcome<T, E>.is_failed(): bool`

Method: true when this Outcome is a failure.

**Example:**
```dune
outcome.done(7, "").is_failed()  // 0
```

### `method<T, E> Outcome<T, E>.value_or(default: T): T`

Method: return the success value, or `default` if this is a failure.

**Example:**
```dune
outcome.failed(0, "bad").value_or(9)  // 9
```

### `method<T, E> Outcome<T, E>.failure_or(default: E): E`

Method: return the error value, or `default` if this is a success.

**Example:**
```dune
outcome.done(7, "").failure_or("none")  // none
```
