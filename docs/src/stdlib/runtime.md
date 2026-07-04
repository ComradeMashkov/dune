# `runtime`

Runtime helpers such as `panic`.

> Auto-generated from `stdlib/runtime.dn` by `tools/gen_stdlib_docs.py`.

### `foreign fn panic(message: text): unit`

The one sanctioned native primitive in the standard library.
`panic` aborts execution with a message and cannot be expressed in Dune,
so it is bound to the C runtime symbol "dune_panic". Everything else in the
stdlib is pure Dune; new `foreign fn` declarations are forbidden here.
`export` makes it visible to other modules; it takes a text message and
returns `unit` (no meaningful value) because it never returns normally.
