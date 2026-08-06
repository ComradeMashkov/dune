# Values, copying, and mutation

This page defines what assignment, function calls, returns, `const`, and
`copy()` mean in Dune. These rules are part of the language contract and apply
to the bytecode VM and the type checker.

## The core rule

Every assignment, argument, and return **copies a Dune value**. It never
implicitly deep-copies an object and never moves from or invalidates the source.

There are two relevant kinds of runtime value:

- **Independent values** contain their complete observable value. Copying one
  gives an independent value.
- **Shared handles** identify a mutable runtime object. Copying the handle makes
  another name for the same object.

This distinction is fixed by the type; it is not inferred from whether a
binding is declared with `const`.

| Value kind | Copy result | Mutable through the value? |
| --- | --- | --- |
| integers and reals | independent value | no |
| `bool`, `glyph`, `unit` | independent value | no |
| `text` | independent immutable value | no |
| `fn(...)` | independent function handle | no |
| array `[T]` | handle to the same array | yes |
| record | handle to the same record | yes |
| tuple | immutable tuple value; nested handles remain shared | no outer mutation |
| choice | immutable tag/payload value; a handle in the payload remains shared | no outer mutation |

Standard-library structures such as `Dict`, `Set`, `Vector`, and `Matrix` are
records, so ordinary assignment follows the record row of the table.

## Assignment

Scalar and immutable values are independent after assignment:

```dn
x = 1;
y = x;
y = 2;
print(x); // 1

first = "Dune";
second = first;
second = "VM";
print(first); // Dune
```

Arrays and records share their mutable object:

```dn
record Counter { value: int }

values = [1];
alias = values;
alias[0] = 2;
print(values[0]); // 2

counter = Counter { value: 3 };
counter_alias = counter;
counter_alias.value = 4;
print(counter.value); // 4
```

Reassigning a handle binding changes only that binding. It does not change the
object selected by another binding:

```dn
left = [1];
right = left;
right = [9];
print(left[0]); // 1
```

There is no implicit copy-on-write behavior.

## Construction and slicing

Each evaluation of an array literal, array comprehension, record literal,
tuple literal, or choice constructor creates a new outer value. Record field
defaults are evaluated for each record construction, so a default array is not
accidentally shared between separately constructed records.

Array slicing (`values[start:end]`) creates a fresh outer array and copies the
selected elements using the same shallow value rules. A nested array or record
inside a slice therefore remains shared. Text slicing creates another immutable
`text` value.

Some wrapper constructors intentionally retain a supplied array. In the
`matrix` module, `vector(data)`, `Vector.new(data)`, `from_flat(rows, cols,
data)`, and `Matrix.new(rows, cols, data)` use `data` as their backing array
without copying it. Mutating either view is visible through the other.
`from_rows(rows)` is different: it flattens the cells into a fresh backing
array.

## `const` freezes a binding, not an object

`const` prevents the binding from being assigned again or shadowed. It does not
turn the referenced object into a deeply immutable object.

```dn
const values: [int] = [1];
values[0] = 2;  // valid: mutate the shared array
values.push(3); // valid: mutate the shared array
values = [4];   // error: cannot assign to constant 'values'
```

The same rule applies to records and receiver calls:

```dn
record Counter {
    value: int,
    fn increment(): unit { this.value = this.value + 1; }
}

const counter = Counter { value: 0 };
counter.value = 1;  // valid
counter.increment(); // valid
counter = Counter { value: 2 }; // error
```

This is binding immutability, similar to JavaScript's `const`. Dune currently
has no deep-`const`, frozen-array, or read-only-record type.

## Function arguments

A parameter receives a copy of the argument value. Therefore:

- changing a scalar parameter cannot change the caller's scalar;
- mutating an array or record parameter changes the shared caller-visible
  object;
- assigning a different value to the parameter is local to the call;
- the argument remains valid after the call.

```dn
fn update(values: [int]): unit {
    values[0] = 2; // visible to the caller
}

fn replace_locally(values: [int]): unit {
    values = [9];  // only this parameter now names the new array
}

source = [1];
update(source);
replace_locally(source);
print(source[0]); // 2
source.push(3);   // source was not moved and is still usable
```

The rules do not depend on whether a function is generic or overloaded.
`Dict`, `Set`, `Vector`, and `Matrix` are records, so they follow the same rule.
For example, mutating a dictionary parameter changes the caller's dictionary;
assigning `Dict.new()` to the parameter changes only that local parameter.

## Return values

Returning also copies a Dune value. Returning an array or record therefore
returns another handle to the same object:

```dn
fn identity(values: [int]): [int] {
    return values;
}

source = [1];
result = identity(source);
result[0] = 2;
print(source[0]); // 2
```

Returning a freshly constructed array or record returns a handle to that new
object. Return does not consume any local or argument at the language level.

## Method receivers

An instance or receiver method receives the caller's value as `this`. The
receiver follows the same copy rule as an ordinary argument. For records and
arrays it is a copied handle to the same mutable object, so field, element, and
nested-container mutation is visible to the caller. Immutable receiver types
remain immutable.

`this` is a non-reassignable receiver binding:

```dn
record Counter {
    value: int,

    fn increment(): unit {
        this.value = this.value + 1; // valid
    }

    fn invalid(): unit {
        this = Counter { value: 0 };
        // error: cannot reassign method receiver 'this'
    }
}
```

Non-reassignability avoids the misleading impression that replacing `this`
could replace the caller's binding. It applies to both record methods and
extension methods; it does not make the receiver object immutable.

## Mutating and value-returning methods

The method syntax does not itself promise mutation or purity. A method mutates
the caller-visible object when its body writes through `this`; a method can
instead construct and return a new value. The return type alone is not enough
to classify it: `pop()` both mutates an array and returns an element.

The standard collection APIs use these concrete rules:

| Type | Mutates existing object | Returns fresh structure |
| --- | --- | --- |
| array | indexed assignment, `push`, `pop`, `clear` | `copy`, slicing, `append`, `prepend`, `concat` |
| `Dict<V>` | `set`, `remove`, `clear` | `copy`, `keys`, `values` |
| `Set` | `add`, `remove`, `clear` | `copy`, `values` |
| `Vector<T>` | `set`, `fill` | `copy`, `slice`, `concat`, arithmetic, `reshape` |
| `Matrix<T>` | `set`, `fill` | `copy`, row/column extraction, `flatten`, `reshape`, arithmetic, transpose and products |

"Fresh structure" still uses ordinary shallow element semantics. For example,
`Dict<V>.values()` returns a new outer array, but a record or array stored as a
`V` remains shared. Custom record methods must document which operations they
perform; Dune does not infer an effect annotation from the method name.

## Tuples and choices

A tuple's element slots and a choice's tag/payload cannot be assigned through
the tuple or choice. However, an array or record stored inside them remains a
shared handle:

```dn
choice Payload { Values([int]), Empty }

source = [1];
(unpacked, marker) = (source, 0);
unpacked[0] = 2;
print(source[0]); // 2

payload: Payload = Values(source);
// Binding the Values payload obtains another handle to source.
```

Immutability of the outer tuple/choice is not recursive deep immutability.

## Explicit copies

Dune has no implicit deep-copy operation. Copying structure is requested by an
ordinary API named `copy()`:

- `[T].copy()` creates a new outer array and copies its elements using normal
  Dune value semantics;
- `record Name derive copy` generates a new outer record and copies each field
  using normal Dune value semantics;
- `Dict<V>.copy()` creates fresh key/value arrays, while `V` values are copied
  shallowly;
- `Set.copy()` creates a fresh backing array (its elements are immutable
  `text`);
- `Vector.copy()` and `Matrix.copy()` create fresh numeric backing arrays.

For example, array copy separates the outer array but shares a nested array:

```dn
import array;

nested: [[int]] = [[1]];
clone = nested.copy();
clone.push([2]);       // changes only clone's outer array
clone[0][0] = 9;      // changes the shared inner array
print(nested.len());  // 1
print(nested[0][0]); // 9
```

Likewise, derived record copy is shallow:

```dn
record Bag derive copy {
    name: text,
    values: [int],
}

original = Bag { name: "a", values: [1] };
clone = original.copy();
clone.name = "b";       // independent outer record field
clone.values[0] = 2;    // shared nested array
print(original.name);   // a
print(original.values[0]); // 2
```

An API that needs recursive independence must construct it explicitly by
copying each nested array/record at the desired depth. The name `copy()` alone
never promises recursive copying.

## No source-level move semantics

Ordinary Dune values are never moved at the language level. There is currently:

- no `move` expression;
- no consumed or moved-from state;
- no use-after-move diagnostic;
- no ownership transfer caused by assignment, calls, or returns.

The VM implementation may use C++ moves internally as an optimization only
when this is unobservable. Such implementation details cannot change the rules
on this page.

## Reserved rules for closures and resources

Closures and resource-owning values are not part of the current release. Their
future implementations must extend this model without silently changing
ordinary values:

- a closure capture copies the captured Dune value at closure creation;
  scalars/text are snapshots, while captured array/record handles continue to
  share their object;
- rebinding the original local after capture does not retarget the captured
  value;
- mutable captured bindings, if introduced, require explicit shared capture
  state rather than changing ordinary assignment semantics;
- resource-owning types must be explicitly marked move-only and use an explicit
  move/consume operation;
- the type checker must reject copying a move-only resource and using it after
  transfer;
- adding resources must not make today's arrays, records, arguments, or returns
  implicitly move-only.

These are compatibility constraints for the planned closure and resource work,
not syntax that is accepted today.
