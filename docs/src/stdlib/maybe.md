# `maybe`

The optional `Maybe<T>` choice and helpers.

`maybe` defines `Maybe<T>`, the standard optional value type. A value is either `Present(T)` or `Absent`, which makes missing data explicit without choosing a sentinel value such as `0` or an empty string.

Use `Maybe<T>` for lookups and operations that may not return a value. Constructors create present or absent values, while `has_value`, `is_absent`, and `value_or` cover the common checks and fallback path.

```dn
import maybe;

found: maybe.Maybe<int> = maybe.present(42);
missing: maybe.Maybe<int> = maybe.absent(0);

print(found.has_value());
print(found.value_or(0));
print(missing.is_absent());
print(missing.value_or(7));
```

> Auto-generated from `stdlib/maybe.dn` by `tools/gen_stdlib_docs.py`.

### `choice Maybe<T>`

`Maybe<T>` is an optional value: either a present value or nothing. A `choice` is a tagged union; here it has two variants: `Present(T)` wraps a value of type T, and `Absent` carries no payload.

### `fn present<T>(value: T): Maybe<T>`

Wrap a concrete value as `Present`.

**Example:**
```dune
maybe.present(7).has_value()  // 1
```

### `fn absent<T>(default: T): Maybe<T>`

Produce an `Absent`. The `default` argument is only there to pin down the generic type T (Dune infers T from it); the value itself is discarded.

**Example:**
```dune
maybe.absent(0).has_value()  // 0
```

### `fn absent_int(): Maybe<int>`

An `Absent` specialised to `Maybe<int>` (uses 0 just to fix T = int).

### `fn absent_text(): Maybe<text>`

An `Absent` specialised to `Maybe<text>` (uses "" just to fix T = text).

### `method<T> Maybe<T>.has_value(): bool`

Method: true when this Maybe holds a value. `when this { ... }` pattern-matches on the choice's variant.

**Example:**
```dune
maybe.present("hi").has_value()  // 1
```

### `method<T> Maybe<T>.is_absent(): bool`

Method: true when this Maybe is empty (the inverse of has_value).

**Example:**
```dune
maybe.absent(0).is_absent()  // 1
```

### `method<T> Maybe<T>.value_or(default: T): T`

Method: return the contained value, or `default` when Absent.

**Example:**
```dune
maybe.absent(0).value_or(9)  // 9
```
