`matrix` provides dense numeric `Vector<T>` and `Matrix<T>` records backed by flat arrays. It covers construction, shape checks, indexing, copying, reshaping, slicing, transposition, elementwise arithmetic, reductions, dot products, matrix multiplication, norms, and small determinants.

Use vectors for one-dimensional numeric data and matrices for row-major two-dimensional data. Constructors such as `vector`, `from_rows`, `from_flat`, `zeros`, `ones`, `identity`, and `diagonal` keep setup explicit, while methods perform shape validation before operations that require compatible dimensions.

```dn
import matrix;

left = matrix.from_rows([[1, 2, 3], [4, 5, 6]]);
right = matrix.from_rows([[7, 8], [9, 10], [11, 12]]);

product = matrix.dot(left, right);
id: matrix.Matrix<int> = matrix.identity(2);

print(product.rows());
print(product.get(0, 0));
print(id.trace());
```
