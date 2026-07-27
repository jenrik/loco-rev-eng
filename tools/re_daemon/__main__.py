"""Run the local autonomous reverse-engineering dashboard daemon."""

from __future__ import annotations

import argparse
from pathlib import Path
import secrets

import uvicorn

from .app import create_app


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", type=Path, default=Path(".pi/re-daemon/state.sqlite3"))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--pi-binary", default="pi")
    args = parser.parse_args()
    token = secrets.token_urlsafe(32)
    daemon_url = f"http://{args.host}:{args.port}"
    app = create_app(args.state, project_root=Path.cwd(), daemon_url=daemon_url, daemon_token=token, pi_binary=args.pi_binary)
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
