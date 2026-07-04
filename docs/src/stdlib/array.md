# `array`

Generic helpers and higher-order pipelines for arrays.

Generic array utilities exposed both as methods on `[T]` and as free
functions. Most are written directly on top of indexing, `len()`, and
`push()`; nothing here needs native support. Methods with a `<T>` are generic
over the element type; some carry a `T is numeric` bound for arithmetic.

Use `array` when you want the built-in `[T]` type to feel like a small collection library. It adds copying, slicing, concatenation, searching, and value-counting helpers, plus higher-order pipelines such as `map`, `filter`, `fold`, `reduce`, `any`, and `all`.

The module also provides numeric reductions and constructors: `range`, `repeat`, `zeros`, `ones`, `full`, `sum`, `product`, `min`, `max`, `argmin`, and `argmax`. Importing the module enables both free-function calls and receiver-style calls on arrays.

```dn
import array;

fn square(value: int): int { return value * value; }
fn is_even(value: int): bool { return value % 2 == 0; }

values = array.range(1, 6);
evens_squared = values
    .filter(is_even)
    .map(square);

print(evens_squared.sum());
print(values.take(3).last());
```

> Auto-generated from `stdlib/array.dn` by `tools/gen_stdlib_docs.py`.

### `method<T> [T].copy(): [T]`

A shallow copy of this array (new backing array, same elements).

### `method<T> [T].reverse(): [T]`

A new array with the elements in reverse order.

### `method<T> [T].contains(needle: T): bool`

True when `needle` appears in the array (delegates to index_of).

### `method<T> [T].index_of(needle: T): int`

The index of the first element equal to `needle`, or -1 if none.

### `method<T> [T].first(): T`

The first element (indexing panics if the array is empty).

### `method<T> [T].last(): T`

The last element (index len-1).

### `method<T> [T].append(value: T): [T]`

A copy of this array with `value` appended at the end.

### `method<T> [T].prepend(value: T): [T]`

A copy of this array with `value` inserted at the front.

### `method<T> [T].concat(other: [T]): [T]`

A new array made of this array followed by `other`.

### `method<T> [T].slice(start: int, end: int): [T]`

The sub-array from `start` (inclusive) to `end` (exclusive).

### `method<T> [T].take(count: int): [T]`

The first `count` elements (a prefix slice).

### `method<T> [T].drop(count: int): [T]`

Everything after the first `count` elements (a suffix slice).

### `method<T> [T].count_value(needle: T): int`

How many elements equal `needle`.

### `method<T> [T].equals(other: [T]): bool`

True when this array and `other` have equal length and equal elements.

### `method<T> [T].fill(value: T): unit`

Overwrite every slot with `value` in place (returns unit, mutates self).

### `method<T, U> [T].map(transform: fn(T): U): [U]`

A new array with `transform` applied to every element.

### `method<T> [T].filter(keep: fn(T): bool): [T]`

A new array holding only the elements for which `keep` returns true.

### `method<T, Acc> [T].fold(initial: Acc, combine: fn(Acc, T): Acc): Acc`

Left fold: seed an accumulator with `initial`, then combine it with each element in order. `combine` receives (accumulator, element).

### `method<T> [T].reduce(combine: fn(T, T): T): T`

Reduce with the first element as the seed (panics on an empty array, since there is no element to start from). `combine` receives (accumulator, element).

### `method<T> [T].any(predicate: fn(T): bool): bool`

True when `predicate` holds for at least one element (short-circuits).

### `method<T> [T].all(predicate: fn(T): bool): bool`

True when `predicate` holds for every element (short-circuits on the first failure; vacuously true for an empty array).

### `method<T> [T].count_where(predicate: fn(T): bool): int`

How many elements satisfy `predicate`.

### `fn range(start: int, end: int): [int]`

The half-open integer range [start, end) as an array.

**Example:**
```dune
array.range(2, 6).sum()  // 14
```

### `fn range(end: int): [int]`

The range [0, end); an overload that defaults the start to 0.

### `fn range(start: int, end: int, step: int): [int]`

The range [start, end) advancing by `step` (supports negative steps).

### `fn repeat<T>(value: T, count: int): [T]`

An array containing `value` repeated `count` times (generic element type).

**Example:**
```dune
array.repeat(4, 3).sum()  // 12
```

### `fn zeros<T is numeric>(count: int): [T]`

`count` numeric zeros. The literal 0 takes on the requested numeric type T.

### `fn ones<T is numeric>(count: int): [T]`

`count` numeric ones (the literal 1 takes on the numeric type T).

### `fn full<T>(count: int, value: T): [T]`

`count` copies of `value`; a named alias for `repeat`.

### `fn sum(values: [int]): int`

Sum of an int array (free-function form).

### `method<T is numeric> [T].sum(): T`

Sum of a numeric array as a method. `result` starts at 0 typed as T.

**Example:**
```dune
[1, 2, 3].sum()  // 6
```

### `method<T is numeric> [T].product(): T`

Product of all elements (starts from the multiplicative identity 1).

**Example:**
```dune
[1, 2, 3, 4].product()  // 24
```

### `method<T is numeric> [T].min(): T`

The minimum element (assumes at least one element; seeds with index 0).

**Example:**
```dune
[3, 1, 4, 1, 5].min()  // 1
```

### `method<T is numeric> [T].max(): T`

The maximum element (seeds with the first element, then scans the rest).

**Example:**
```dune
[3, 1, 4, 1, 5].max()  // 5
```

### `method<T is numeric> [T].argmin(): int`

The index of the minimum element (argmin).

### `method<T is numeric> [T].argmax(): int`

The index of the maximum element (argmax).

### `method [bool].all(): bool`

True when every element of a bool array is true (logical AND-reduce).

**Example:**
```dune
[true, true, false].all()  // 0
```

### `method [bool].any(): bool`

True when any element of a bool array is true (logical OR-reduce).

**Example:**
```dune
[false, false, true].any()  // 1
```

### `fn sum(values: [real64]): real64`

Sum of a real64 array (free-function overload; result seeded as 0.0).
