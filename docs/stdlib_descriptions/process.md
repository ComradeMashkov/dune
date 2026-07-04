`process` exposes the current program's command-line arguments, environment variables, and working directory. It wraps VM process intrinsics in ordinary Dune values so missing arguments and unavailable environment values are represented with `Maybe<text>`.

Use `args`, `arg_count`, and `arg` for CLI programs, `env` or `env_or` for environment-driven configuration, and `cwd` when code needs to report or resolve paths relative to the current directory.

```dn
import io;
import maybe;
import process;

io.println(process.arg_count());
io.println(process.arg(0).value_or("no argument"));
io.println(process.env_or("DUNE_PROFILE", "dev"));
io.println(process.cwd().has_value());
```
