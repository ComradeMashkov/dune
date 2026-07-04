# `autograd`

Scalar reverse-mode automatic differentiation.

`autograd` is a scalar reverse-mode automatic differentiation module. Each `Value` stores its forward `data`, accumulated `grad`, whether it `requires_grad`, and the parent links needed to propagate derivatives backward through a computation graph.

Create differentiable inputs with `variable` or `value`, constants with `constant`, then build expressions with arithmetic helpers such as `add`, `mul`, `div`, `pow`, `relu`, `tanh`, `exp`, `ln`, and `sqrt`. Calling `backward` on an output seeds its gradient with `1.0` and fills in the gradients of upstream variables.

```dn
import autograd;

x = autograd.variable(2.0);
y = autograd.variable(3.0);

loss = x.mul(y).add(x.pow(2.0)).add(1.0);
loss.backward();

print(loss.data);
print(x.grad);
print(y.grad);
```

> Auto-generated from `stdlib/autograd.dn` by `tools/gen_stdlib_docs.py`.

### `record Value`

A node in the autodiff graph: its numeric value plus the edges to its inputs.

**Fields:**

- `data: real64,          // the forward (computed) value`
- `grad: real64,          // accumulated gradient after backward()`
- `requires_grad: bool,   // whether gradient should flow through this node`

### `fn variable(data: real64): Value`

Create a leaf variable (participates in gradients, no parents).

### `fn constant(data: real64): Value`

Create a leaf constant (does not participate in gradients).

### `fn value(data: real64): Value`

Alias for `variable`: the default way to introduce a differentiable value.

### `fn data(value: Value): real64`

Read a node's forward value.

### `fn grad(value: Value): real64`

Read a node's accumulated gradient.

### `fn add(left: Value, right: Value): Value`

Addition: d/dleft = 1, d/dright = 1.

### `fn add(left: Value, right: real64): Value`

Addition with a plain scalar on the right (wrapped as a constant).

### `fn add(left: real64, right: Value): Value`

Addition with a plain scalar on the left.

### `fn sub(left: Value, right: Value): Value`

Subtraction: d/dleft = 1, d/dright = -1.

### `fn sub(left: Value, right: real64): Value`

Subtraction with a scalar right operand.

### `fn sub(left: real64, right: Value): Value`

Subtraction with a scalar left operand.

### `fn mul(left: Value, right: Value): Value`

Multiplication: d/dleft = right.data, d/dright = left.data (product rule).

### `fn mul(left: Value, right: real64): Value`

Multiplication with a scalar right operand.

### `fn mul(left: real64, right: Value): Value`

Multiplication with a scalar left operand.

### `fn div(left: Value, right: Value): Value`

Division: d/dleft = 1/right, d/dright = -left/right^2 (quotient rule).

### `fn div(left: Value, right: real64): Value`

Division with a scalar denominator.

### `fn div(left: real64, right: Value): Value`

Division with a scalar numerator.

### `fn neg(value: Value): Value`

Negation: d/dvalue = -1.

### `fn pow(base: Value, exponent: real64): Value`

Power with a constant exponent: d/dbase = exponent * base^(exponent-1).

### `fn relu(value: Value): Value`

ReLU: passes positives through (derivative 1) and clamps negatives to 0.

### `fn tanh(value: Value): Value`

Hyperbolic tangent computed from exp, with derivative 1 - tanh^2.

### `fn exp(value: Value): Value`

Exponential: d/dvalue = exp(value), which equals the forward result itself.

### `fn ln(value: Value): Value`

Natural log: d/dvalue = 1/value.

### `fn sqrt(value: Value): Value`

Square root: d/dvalue = 0.5 / sqrt(value).

### `fn zero_grad(value: Value): unit`

Reset gradients throughout the graph feeding into `value`.

### `fn backward(value: Value): unit`

Run a full backward pass: clear old gradients, then seed the output with 1.0
and propagate. After this, each input's `grad` holds d(value)/d(input).
