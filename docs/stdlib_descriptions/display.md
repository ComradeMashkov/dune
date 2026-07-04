`display` defines the `Display` contract: a type that promises a `to_text(): text` method. Records with `to_text` can already be printed and formatted directly, but declaring `with display.Display` lets generic functions require that capability explicitly.

Use `show` when you want the rendered text value instead of printing immediately, especially inside generic code bounded by `T is Display`.

```dn
import display;

record Label with display.Display {
    name: text,

    fn to_text(): text {
        return format("label: {}", this.name);
    }
}

label = Label { name: "core" };
print(display.show(label));
```
