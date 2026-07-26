This project seeks to fully reverse engineer and document the game Lego Loco.

A Ghidra database can be found in `./lego-loco-unpacked/Exe/ghidra_projects/`

## Opening the Ghidra database

The `.gpr` project file fails with "No load spec found" — open the raw binary instead:

```
mcp__plugin_claude-code-home-manager_ghidra__open_database:
  file_path: "/home/user/projects/v43/jenrik/lego-loco-rev-eng/lego-loco-unpacked/Exe/loco.exe"
  database_id: "loco2"   // use a fresh ID each session; stale IDs from failed opens block reuse
```

Then wait for analysis:

```
mcp__plugin_claude-code-home-manager_ghidra__wait_for_analysis:
  database: "loco2"
```

**Important:** Never use `force_new: true` — it's blocked by the MCP server. Never use the `.gpr` project file path. Always use a unique `database_id` (e.g. `loco2`, `loco3`, etc.) since the ID stays reserved after a failed open.
