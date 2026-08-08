# Deterministic cleanup with `defer`

`defer` registers synchronous cleanup for the current lexical scope. The
cleanup runs exactly once when that scope is left, whether control reaches the
closing brace or leaves through `break`, `continue`, `return`, `?`, or a VM
runtime error such as `runtime.panic`.

Use the expression form for one operation. Its result must be `unit`:

```dn
resource = open_resource("events.log");
defer resource.close();
```

Use the block form for several operations or conditional cleanup:

```dn
defer {
    resource.flush();
    resource.close();
}
```

A semicolon is required after the expression form and optional after the block
form.

## Order and scope

Each `defer` belongs to the innermost enclosing statement scope. Cleanups in
one scope run in reverse registration order (LIFO):

```dn
defer io.println("last");
defer io.println("first");
// prints "first", then "last"
```

A loop body is a fresh scope on every iteration. Therefore a deferred cleanup
inside the body runs before the next iteration on `continue` and before leaving
the loop on `break`. Function, lambda, test, and top-level bodies also own their
registered cleanups. When a runtime error crosses several function calls, Dune
cleans the innermost frame first and proceeds outward.

The value of a `return` is saved before cleanup starts, so cleanups cannot
replace it. The `?` operator uses the same return path: propagating a `Failed`
`outcome.Outcome` still runs every pending cleanup in the function.

## Capture and evaluation

Registration creates a zero-argument closure; the cleanup body itself executes
only when the scope exits. Captures follow ordinary Dune value semantics:

- numbers, booleans, glyphs, text, and callable values are snapshots taken at
  registration;
- arrays and records are captured as shared handles, so later element or field
  mutation is visible to cleanup;
- a captured binding itself cannot be reassigned inside cleanup, just as in an
  ordinary closure;
- names declared after `defer` are not in scope and produce a type error.

Calls and other operations written inside the deferred expression or block are
delayed until cleanup. If they need a value computed immediately, bind it
before `defer`; the scalar binding is then captured as a snapshot.

```dn
path: text = current_path(); // runs now
defer remove_path(path);     // remove_path runs on exit
```

## Failures during cleanup

The expression form accepts only `unit`, so a fallible close operation returning
`outcome.Outcome` cannot be accidentally discarded. Handle that result
explicitly in a block (for example by logging it or converting it to a panic):

```dn
defer {
    closed = resource.try_close();
    if closed.is_failed() {
        log.error(closed.failure_or("close failed"));
    }
}
```

`?` cannot propagate from such a block because cleanup itself returns `unit`;
the block must choose its failure policy explicitly.

Cleanup is best-effort and deterministic. If one cleanup fails, Dune still
runs the remaining pending cleanups in LIFO order. When another runtime error
is already being handled, it remains the primary error; cleanup failures are
appended as `while running deferred cleanup` context. If cleanup is the first
operation to fail during an otherwise normal exit, that failure becomes the
primary error.

A `return` inside a deferred block returns from that cleanup block, not from the
surrounding function. `break` and `continue` cannot target an outer loop from a
cleanup block. A cleanup may register its own nested `defer`; those nested
cleanups finish before the outer cleanup returns.

## Resource API convention

There is deliberately no mandatory `Dispose` contract. Any function or method
returning `unit` can be deferred, which keeps pure-Dune modules and user-defined
FFI wrappers on the same path. Resource-owning APIs should:

1. provide an idempotent `close()`, `release()`, or similarly explicit `unit`
   operation;
2. show `defer resource.close();` immediately after successful acquisition in
   their documentation;
3. use an `outcome.Outcome` result when acquisition can fail, then register
   cleanup only after unwrapping the resource.

`defer` is synchronous: a cleanup finishes before execution continues outside
the scope. A future asynchronous resource model will need a separate contract;
this statement does not detach or schedule background work.

See the runnable [`defer_cleanup.dn`](../../../examples/defer_cleanup.dn) example
for normal return, early return, `?`, loop exits, capture behavior, and LIFO
ordering.
