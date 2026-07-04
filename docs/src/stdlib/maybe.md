# `maybe`

The optional `Maybe<T>` choice and helpers.

> Auto-generated from `stdlib/maybe.dn` by `tools/gen_stdlib_docs.py`.

### `choice Maybe<T>`

`Maybe<T>` is an optional value: either a present value or nothing.
A `choice` is a tagged union; here it has two variants:
  `Present(T)` wraps a value of type T, and `Absent` carries no payload.

### `fn present<T>(value: T): Maybe<T>`

Wrap a concrete value as `Present`.

### `fn absent<T>(default: T): Maybe<T>`

Produce an `Absent`. The `default` argument is only there to pin down the
generic type T (Dune infers T from it); the value itself is discarded.

### `fn absent_int(): Maybe<int>`

An `Absent` specialised to `Maybe<int>` (uses 0 just to fix T = int).

### `fn absent_text(): Maybe<text>`

An `Absent` specialised to `Maybe<text>` (uses "" just to fix T = text).

### `method<T> Maybe<T>.has_value(): bool`

Method: true when this Maybe holds a value.
`when this { ... }` pattern-matches on the choice's variant.

### `method<T> Maybe<T>.is_absent(): bool`

Method: true when this Maybe is empty (the inverse of has_value).

### `method<T> Maybe<T>.value_or(default: T): T`

Method: return the contained value, or `default` when Absent.
