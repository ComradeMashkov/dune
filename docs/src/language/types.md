# Types

## Scalar types

| Category | Types |
| --- | --- |
| Signed integers | `int`, `i8`, `i16`, `i32`, `i64`, `isize` |
| Unsigned integers | `u8`, `u16`, `u32`, `u64`, `usize`, `uint8`, `uint16`, `uint32`, `uint64` |
| Reals | `real`, `real32`, `real64` |
| Other | `bool`, `glyph`, `text`, `unit` |

`unit` is the type of an expression with no meaningful value (for example a
function that returns nothing).

## Arrays

An array type is written `[T]`. Arrays support literals, indexing, slicing,
`len`, `push`, and `pop`, plus a large set of stdlib helpers (see
[`array`](../stdlib/array.md)).

```dn
values: [int] = [1, 2, 3, 4];
values.push(5);
print(values.len());     // 5
print(values[0]);        // 1
middle: [int] = values[1:3];
```

## Tuples

Tuples group a fixed number of values of possibly different types and can be
destructured on assignment.

```dn
pair = (1, "one");
(number, label) = pair;
```

## Type aliases

Type aliases give a shorter name to an existing type. They are transparent
compile-time aliases, not new runtime types, and can target primitive, array,
tuple, record, choice, generic, and module-qualified types.

```dn
import matrix;

type Count = int;
type Counts = [Count];
type Vec = matrix.Vector<real64>;
```

Generic aliases (`type Vec<T> = ...`) are reserved for a later release.

## Explicit casts

Convert between numeric types with the `to` operator.

```dn
value = 17;
exact: real64 = value to real64;
```
