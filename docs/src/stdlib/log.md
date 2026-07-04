# `log`

Levelled diagnostics with stderr output and filtering.

Structured diagnostics for Dune programs.

The first stage exposes levelled human logging with process-wide filtering.
Logs are written to stderr by the VM `__log_emit` intrinsic so normal program
output on stdout stays separate. The active level defaults to `info`, can be
set from `DUNE_LOG` or `DUNE_LOG_LEVEL`, and can be changed at runtime with
`set_level`.

`log` provides a small diagnostics API for command-line tools and longer-running programs. It keeps logs on stderr, separate from regular `print` output, and filters messages below the active level.

The default level is `info`. Set `DUNE_LOG` or `DUNE_LOG_LEVEL` to `trace`, `debug`, `info`, `warn`, `error`, or `off`, or call `log.set_level` from Dune code.

```dn
import log;

log.set_level(log.DEBUG);
log.info("building target");
log.warn("native backend disabled");
```

> Auto-generated from `stdlib/log.dn` by `tools/gen_stdlib_docs.py`.

### `const TRACE: int`

The most verbose level.

**Example:**
```dune
log.set_level(log.TRACE)
```

### `const DEBUG: int`

Debug-level diagnostics.

### `const INFO: int`

Informational diagnostics; this is the default active level.

### `const WARN: int`

Warnings for recoverable problems.

### `const ERROR: int`

Errors for failed operations that the program can still report.

### `const OFF: int`

Disable all log output.

### `fn set_level(level: int): unit`

Set the active minimum level. Messages below this level are suppressed.

**Example:**
```dune
log.set_level(log.WARN)
```

### `fn level(): int`

Return the active minimum level.

**Example:**
```dune
log.level()
```

### `fn enabled(level: int): bool`

True when a message at `level` would be emitted.

**Example:**
```dune
log.enabled(log.DEBUG)
```

### `fn trace(message: text): unit`

Emit a trace message.

**Example:**
```dune
log.trace("starting parser")
```

### `fn debug(message: text): unit`

Emit a debug message.

**Example:**
```dune
log.debug("cache miss")
```

### `fn info(message: text): unit`

Emit an informational message.

**Example:**
```dune
log.info("building target")
```

### `fn warn(message: text): unit`

Emit a warning message.

**Example:**
```dune
log.warn("native backend disabled")
```

### `fn error(message: text): unit`

Emit an error message.

**Example:**
```dune
log.error("failed")
```
