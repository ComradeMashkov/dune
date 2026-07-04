`log` provides a small diagnostics API for command-line tools and longer-running programs. It keeps logs on stderr, separate from regular `print` output, and filters messages below the active level.

The default level is `info`. Set `DUNE_LOG` or `DUNE_LOG_LEVEL` to `trace`, `debug`, `info`, `warn`, `error`, or `off`, or call `log.set_level` from Dune code.

```dn
import log;

log.set_level(log.DEBUG);
log.info("building target");
log.warn("native backend disabled");
```
