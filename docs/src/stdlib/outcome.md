# `outcome`

The result-style `Outcome<T, E>` choice and helpers.

> Auto-generated from `stdlib/outcome.dn` by `tools/gen_stdlib_docs.py`.

### `choice Outcome<T, E>`

`Outcome<T, E>` is a result type: either success carrying a `T` or failure
carrying an error `E`. It is the explicit error-handling type that the `?`
operator understands. The two variants are `Done(T)` and `Failed(E)`.

### `fn done<T, E>(value: T, error_default: E): Outcome<T, E>`

Build a success. `error_default` is only present to fix the error type E
(Dune needs a value of E to infer it); it is not stored.

### `fn failed<T, E>(value_default: T, error: E): Outcome<T, E>`

Build a failure. `value_default` is only present to fix the success type T;
it is not stored.

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

### `method<T, E> Outcome<T, E>.is_failed(): bool`

Method: true when this Outcome is a failure.

### `method<T, E> Outcome<T, E>.value_or(default: T): T`

Method: return the success value, or `default` if this is a failure.

### `method<T, E> Outcome<T, E>.failure_or(default: E): E`

Method: return the error value, or `default` if this is a success.
