# `math`

Numeric constants and generic math functions.

Pure-Dune math: named constants plus elementary functions approximated with
Taylor/Maclaurin series, Newton iteration, and range reduction. Nothing here
calls native code, so results are identical across backends.

`math` is a pure-Dune numeric module. It exposes real constants such as `PI`, `TAU`, and `E`, generic helpers such as `square`, `cube`, `abs`, `min`, `max`, and `clamp`, and elementary real functions implemented with series expansion, Newton iteration, and range reduction.

Use it when you need portable numeric behavior in both the VM and native backend. The functions are intentionally small and deterministic; they do not call a native math library.

```dn
import math;

print(math.square(7));
print(math.clamp(15, 0, 10));
print(math.sqrt(81.0));
print(math.round(math.PI));
```

> Auto-generated from `stdlib/math.dn` by `tools/gen_stdlib_docs.py`.

### `const PI: real64`

High-precision real64 constants.

### `const TAU: real64`

### `const E: real64`

### `const INVERSE_E: real64`

### `const PI32: real32`

Lower-precision real32 mirrors of the same constants.

### `const TAU32: real32`

### `const E32: real32`

### `const INVERSE_E32: real32`

### `fn square<T is numeric>(value: T): T`

Square of `value` (generic over any numeric type T).

**Example:**
```dune
math.square(7)  // 49
```

### `fn cube<T is numeric>(value: T): T`

Cube of `value`.

**Example:**
```dune
math.cube(3)  // 27
```

### `fn abs<T is numeric>(value: T): T`

Absolute value: negate when the input is negative.

**Example:**
```dune
math.abs(0 - 5)  // 5
```

### `fn min<T is numeric>(left: T, right: T): T`

The smaller of two numbers.

**Example:**
```dune
math.min(3, 8)  // 3
```

### `fn max<T is numeric>(left: T, right: T): T`

The larger of two numbers.

**Example:**
```dune
math.max(3, 8)  // 8
```

### `fn clamp<T is numeric>(value: T, lower: T, upper: T): T`

Constrain `value` to the inclusive range [lower, upper].

**Example:**
```dune
math.clamp(15, 0, 10)  // 10
```

### `fn sqrt<T is real>(value: T): T`

Square root via Newton's method (real types only).

**Example:**
```dune
math.sqrt(81.0)  // 9
```

### `fn normalize_radians<T is real>(value: T): T`

Reduce an angle into (-pi, pi] so the sin/cos series converge quickly.

### `fn sin<T is real>(value: T): T`

Sine via the Maclaurin series after range reduction.

**Example:**
```dune
math.sin(0.0)  // 0
```

### `fn cos<T is real>(value: T): T`

Cosine via the Maclaurin series after range reduction.

**Example:**
```dune
math.cos(0.0)  // 1
```

### `fn tan<T is real>(value: T): T`

Tangent as sine over cosine.

### `fn exp<T is real>(value: T): T`

Exponential e^value using range reduction plus the Taylor series.

**Example:**
```dune
math.exp(0.0)  // 1
```

### `fn ln<T is real>(value: T): T`

Natural logarithm via range reduction and the artanh series.

**Example:**
```dune
math.ln(1.0)  // 0
```

### `fn pow<T is real>(base: T, exponent: int): T`

Integer power: base raised to an integer exponent by repeated multiplication.

**Example:**
```dune
math.pow(2.0, 10)  // 1024
```

### `fn pow<T is real>(base: T, exponent: T): T`

Real power: base^exponent for a real exponent via exp(exponent * ln(base)). This overload is chosen when the exponent has the same real type as the base.

### `fn floor<T is real>(value: T): T`

Largest whole number not greater than `value`.

**Example:**
```dune
math.floor(3.7)  // 3
```

### `fn ceil<T is real>(value: T): T`

Smallest whole number not less than `value`.

**Example:**
```dune
math.ceil(3.2)  // 4
```

### `fn round<T is real>(value: T): T`

Round to the nearest whole number (halves round away from zero).

**Example:**
```dune
math.round(2.5)  // 3
```
