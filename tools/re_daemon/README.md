# Autonomous RE daemon

Run the local dashboard from the project root:

```bash
nix develop . --command python3 -m tools.re_daemon
# browse http://127.0.0.1:8765/
```

The daemon binds to loopback by default. Its SQLite state, Pi sessions, and cached artifacts live under `.pi/re-daemon/` unless `--state` is supplied.

The current daemon supports durable jobs/tasks/agents/evidence, agent-driven bounded task-graph expansion, dependency-gated autonomous successor dispatch, live WebSocket tailing, per-agent Pi RPC process control, capability-protected extension calls, and bounded normalized event logging.

To enable Ghidra, supply a daemon-owned MCP stdio command and the raw binary. The existing Ghidra service uses `re-mcp-ghidra proxy`; use the absolute executable that owns that configured service (the project Nix shell does not add it automatically):

```bash
export RE_MCP_GHIDRA=/absolute/path/to/re-mcp-ghidra
nix develop . --command python3 -m tools.re_daemon \
  --ghidra-command "$RE_MCP_GHIDRA proxy" \
  --ghidra-binary "$PWD/lego-loco-unpacked/Exe/loco.exe"
```

The adapter opens the daemon database, waits for analysis, and allows only decompile/disassemble/function/xref/structure/name/string queries. It records each result as a content-addressed evidence revision, reuses identical evidence captures within the same job by default, and replaces a failed MCP worker once before surfacing an error. The direct proxy operations are unprefixed (`open_database`, `decompile_function`, etc.); Pi's `mcp.ghidra.*` namespace is not sent to the proxy. Ghidra mutation tools are not exposed.

For repeatable local launch configuration, create the untracked, non-secret `.pi/re-daemon-ghidra.json`:

```json
{
  "command": ["/absolute/path/to/re-mcp-ghidra", "proxy"],
  "binary": "lego-loco-unpacked/Exe/loco.exe",
  "databaseIdPrefix": "loco-daemon"
}
```

CLI Ghidra options override this file. Do not place credentials, tokens, or environment values in it.

Run the daemon tests:

```bash
PYTHONPATH=. python3 tools/tests/re_daemon_test.py
PYTHONPATH=. python3 tools/tests/re_daemon_task_test.py
PYTHONPATH=. python3 tools/tests/re_daemon_rpc_test.py
nix develop . --command python3 -m unittest tools.tests.re_daemon_web_test -v
```
