# Autonomous RE daemon

Run the local dashboard from the project root:

```bash
nix develop . --command python3 -m tools.re_daemon
# browse http://127.0.0.1:8765/
```

The daemon binds to loopback by default. Its SQLite state, Pi sessions, and cached artifacts live under `.pi/re-daemon/` unless `--state` is supplied.

The current vertical slice supports durable jobs/agents/events, live WebSocket tailing, per-agent Pi RPC process control, capability-protected extension calls, and bounded normalized event logging. It intentionally does **not** launch real Ghidra work until the read-only Ghidra adapter is connected.

Run the daemon tests:

```bash
PYTHONPATH=. python3 tools/tests/re_daemon_test.py
PYTHONPATH=. python3 tools/tests/re_daemon_rpc_test.py
nix develop . --command python3 -m unittest tools.tests.re_daemon_web_test -v
```
