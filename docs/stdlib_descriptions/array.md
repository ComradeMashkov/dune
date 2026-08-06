Use `array` when you want the built-in `[T]` type to feel like a small collection library. It adds copying, slicing, concatenation, searching, and value-counting helpers, plus higher-order pipelines such as `map`, `filter`, `fold`, `reduce`, `any`, and `all`.

`copy()` creates a fresh outer array and copies elements shallowly. Nested
arrays and records remain shared according to Dune's
[value semantics](../language/value-semantics.md#explicit-copies).

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
