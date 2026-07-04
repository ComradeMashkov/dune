# `math`

Numeric constants and generic math functions.

Pure-Dune math: named constants plus elementary functions approximated with
Taylor/Maclaurin series, Newton iteration, and range reduction. Nothing here
calls native code, so results are identical across backends.

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

### `fn cube<T is numeric>(value: T): T`

Cube of `value`.

### `fn abs<T is numeric>(value: T): T`

Absolute value: negate when the input is negative.

### `fn min<T is numeric>(left: T, right: T): T`

The smaller of two numbers.

### `fn max<T is numeric>(left: T, right: T): T`

The larger of two numbers.

### `fn clamp<T is numeric>(value: T, lower: T, upper: T): T`

Constrain `value` to the inclusive range [lower, upper].

### `fn sqrt<T is real>(value: T): T`

Square root via Newton's method (real types only).

### `fn normalize_radians<T is real>(value: T): T`

Reduce an angle into (-pi, pi] so the sin/cos series converge quickly.

### `fn sin<T is real>(value: T): T`

Sine via the Maclaurin series after range reduction.

### `fn cos<T is real>(value: T): T`

Cosine via the Maclaurin series after range reduction.

### `fn tan<T is real>(value: T): T`

Tangent as sine over cosine.

### `fn exp<T is real>(value: T): T`

Exponential e^value using range reduction plus the Taylor series.

### `fn ln<T is real>(value: T): T`

Natural logarithm via range reduction and the artanh series.

### `fn pow<T is real>(base: T, exponent: int): T`

Integer power: base raised to an integer exponent by repeated multiplication.

### `fn pow<T is real>(base: T, exponent: T): T`

Real power: base^exponent for a real exponent via exp(exponent * ln(base)).
This overload is chosen when the exponent has the same real type as the base.

### `fn floor<T is real>(value: T): T`

Largest whole number not greater than `value`.

### `fn ceil<T is real>(value: T): T`

Smallest whole number not less than `value`.

### `fn round<T is real>(value: T): T`

Round to the nearest whole number (halves round away from zero).
