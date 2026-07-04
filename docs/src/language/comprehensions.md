# Loops, ranges, and comprehensions

## Ranges

`start..end` is a half-open integer range used by `for` loops and stdlib helpers.

```dn
for i in 0..4 {
    print(i);   // 0 1 2 3
}
```

## `for` and `while`

`for ... in` iterates an array or a range. `while` loops on a condition. Both
support `break` and `continue`.

```dn
values: [int] = [10, 20, 30];
for value in values {
    print(value);
}

x = 3;
while x > 0 {
    x = x - 1;
}
```

## Array comprehensions

An array comprehension builds a new array from an iterable, with an optional
filter.

```dn
squares = [x * x for x in 0..5];             // [0, 1, 4, 9, 16]
evens = [x for x in 0..10 if x % 2 == 0];    // [0, 2, 4, 6, 8]
```

For a functional style over existing arrays, see the higher-order pipeline in
[Functions](functions.md#function-values-and-method-chaining) and the
[`array`](../stdlib/array.md) module.
