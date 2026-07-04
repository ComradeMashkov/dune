# `collections`

Shared collection utilities.

Small convenience constructors for building short arrays of ints and text.
These are handy shorthands so callers do not have to write array literals
by hand for the most common one- and two-element cases.

> Auto-generated from `stdlib/collections.dn` by `tools/gen_stdlib_docs.py`.

### `fn singleton_int(value: int): [int]`

A one-element int array holding `value`.

### `fn pair_int(left: int, right: int): [int]`

A two-element int array holding `left` then `right`.

### `fn singleton_text(value: text): [text]`

A one-element text array holding `value`.

### `fn pair_text(left: text, right: text): [text]`

A two-element text array holding `left` then `right`.

### `fn repeat_int(value: int, count: int): [int]`

Build an int array that repeats `value` exactly `count` times.
