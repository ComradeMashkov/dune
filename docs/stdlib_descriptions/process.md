`process` exposes the current program's command-line arguments, environment variables, and working directory. It wraps VM process intrinsics in ordinary Dune values so missing arguments and unavailable environment values are represented with `Maybe<text>`.

Use `args`, `arg_count`, and `arg` for CLI programs, `env` or `env_or` for environment-driven configuration, and `cwd` when code needs to report or resolve paths relative to the current directory.

```dn
import maybe;
import process;

print(process.arg_count());
print(process.arg(0).value_or("no argument"));
print(process.env_or("DUNE_PROFILE", "dev"));
print(process.cwd().has_value());
```
