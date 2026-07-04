# Comments and doc-comments

Dune has single-line `//` comments and multi-line `/* ... */` block comments.

```dn
// a line comment
/* a block comment
   that spans several lines */
```

## Doc-comments

A comment block written **directly above** a declaration becomes its
documentation. The editor's LSP shows it in the hover for that symbol, including
symbols in other modules (hovering `math.square` pulls the comment from
`math.dn`). A blank line between the comment and the declaration detaches it, and
a comment trailing code on the same line never attaches.

Plain `//` comments are shown as prose, so existing comments document their
symbols with no extra syntax. Doc-comments may also use the `///` line form or the
`/** ... */` block form.

## Structured tags

Doc-comments may carry structured tags — `brief`, `param`, `returns`, and
`example` — which the editor renders as sections:

```dn
/// brief: Squares a value.
/// param value: the number to square
/// returns: value * value
fn square(value: int): int {
    return value * value;
}
```

Tags work on functions, records, record fields, and record methods:

```dn
/** brief: A point on the integer grid. */
record Point {
    // The horizontal coordinate.
    x: int,

    /// brief: The squared distance from the origin.
    fn magnitude_squared(): int { return this.x * this.x + this.y * this.y; }
}
```

The [`documented.dn`](../guides/examples.md) example demonstrates every form. The
standard-library reference in this book is generated from these same
doc-comments.
