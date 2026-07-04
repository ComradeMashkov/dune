# `collections`

Shared collection utilities.

Small convenience constructors for building short arrays of ints and text.
These are handy shorthands so callers do not have to write array literals
by hand for the most common one- and two-element cases.

`collections` provides a few small constructors for common array shapes. It is intentionally narrow: one- and two-element arrays for `int` and `text`, plus a simple `repeat_int` helper.

Use these helpers in tests, examples, and small programs when named construction is clearer than spelling out an array literal or a loop.

```dn
import collections;

pair = collections.pair_int(2, 3);
words = collections.singleton_text("dune");
repeated = collections.repeat_int(4, 3);

print(pair[0] + pair[1]);
print(words[0]);
print(repeated.len());
```

> Auto-generated from `stdlib/collections.dn` by `tools/gen_stdlib_docs.py`.

### `fn singleton_int(value: int): [int]`

A one-element int array holding `value`.

**Example:**
```dune
collections.singleton_int(5)[0]  // 5
```

### `fn pair_int(left: int, right: int): [int]`

A two-element int array holding `left` then `right`.

**Example:**
```dune
collections.pair_int(1, 2)[1]  // 2
```

### `fn singleton_text(value: text): [text]`

A one-element text array holding `value`.

**Example:**
```dune
collections.singleton_text("hi")[0]  // hi
```

### `fn pair_text(left: text, right: text): [text]`

A two-element text array holding `left` then `right`.

**Example:**
```dune
collections.pair_text("a", "b")[1]  // b
```

### `fn repeat_int(value: int, count: int): [int]`

Build an int array that repeats `value` exactly `count` times.

**Example:**
```dune
collections.repeat_int(9, 3)[2]  // 9
```
