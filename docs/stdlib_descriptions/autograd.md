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
