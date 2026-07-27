"""Run the local autonomous reverse-engineering dashboard daemon."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import secrets
import shlex

import uvicorn

from .app import create_app
from .mcp import GhidraConfig


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", type=Path, default=Path(".pi/re-daemon/state.sqlite3"))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--pi-binary", default="pi")
    parser.add_argument("--ghidra-command", help="Shell-style command for the read-only Ghidra MCP stdio proxy")
    parser.add_argument("--ghidra-binary", type=Path, help="Raw binary opened by the daemon-owned Ghidra database")
    parser.add_argument("--ghidra-database", help="Unique Ghidra database ID for this daemon process")
    args = parser.parse_args()
    if bool(args.ghidra_command) != bool(args.ghidra_binary):
        parser.error("--ghidra-command and --ghidra-binary must be supplied together")
    ghidra = None
    if args.ghidra_command:
        ghidra = GhidraConfig(
            tuple(shlex.split(args.ghidra_command)),
            args.ghidra_binary.resolve(),
            args.ghidra_database or f"re-daemon-{os.getpid()}",
        )
    token = secrets.token_urlsafe(32)
    daemon_url = f"http://{args.host}:{args.port}"
    app = create_app(
        args.state, project_root=Path.cwd(), daemon_url=daemon_url, daemon_token=token,
        pi_binary=args.pi_binary, ghidra_config=ghidra,
    )
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
