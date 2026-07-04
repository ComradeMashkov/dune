`matrix` provides dense numeric `Vector<T>` and `Matrix<T>` records backed by flat arrays. It covers construction, shape checks, indexing, copying, reshaping, slicing, transposition, elementwise arithmetic, reductions, dot products, matrix multiplication, norms, and small determinants.

Use vectors for one-dimensional numeric data and matrices for row-major two-dimensional data. Constructors such as `vector`, `from_rows`, `from_flat`, `zeros`, `ones`, `identity`, and `diagonal` keep setup explicit, while methods perform shape validation before operations that require compatible dimensions.

```dn
import io;
import matrix;

left = matrix.from_rows([[1, 2, 3], [4, 5, 6]]);
right = matrix.from_rows([[7, 8], [9, 10], [11, 12]]);

product = matrix.dot(left, right);
id: matrix.Matrix<int> = matrix.identity(2);

io.println(product.rows());
io.println(product.get(0, 0));
io.println(id.trace());
```

## Operators

`Vector` and `Matrix` implement the operator methods, so arithmetic reads
naturally (see [operator overloading](../language/records.md#operator-overloading)):

```dn
import io;
import matrix;

v = matrix.vector([1, 2, 3]);
w = matrix.vector([4, 5, 6]);

io.println((v + w).get(2));   // 9   -> Vector.add
io.println((v * w).get(1));   // 10  -> Vector.mul (element-wise)
io.println((v * 10).get(0));  // 10  -> Vector.mul (scalar)
```

`+`/`-` add and subtract same-shape vectors/matrices; `*` is element-wise (or a
scalar scale). Matrix multiplication stays explicit as `matrix.dot(a, b)` /
`a.matmul(b)` so it is never confused with element-wise `*`.
