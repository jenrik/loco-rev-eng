"""Non-secret project-local daemon configuration."""

from __future__ import annotations

import json
from pathlib import Path
import shlex
from typing import Any

from .mcp import GhidraConfig


class SettingsError(ValueError):
    """The project-local daemon configuration is malformed."""


def load_ghidra_config(
    project_root: Path,
    config_path: Path | None,
    command_text: str | None,
    binary_path: Path | None,
    database_id: str | None,
    process_id: int,
) -> GhidraConfig | None:
    """Resolve explicit CLI settings or `.pi/re-daemon-ghidra.json`.

    The JSON file intentionally permits only a command and a binary path; it is
    not an MCP credential import and must not contain secrets.
    """
    if bool(command_text) != bool(binary_path):
        raise SettingsError("--ghidra-command and --ghidra-binary must be supplied together")
    if command_text:
        command = tuple(shlex.split(command_text))
        assert binary_path is not None
        return GhidraConfig(command, binary_path.resolve(), database_id or f"re-daemon-{process_id}")

    candidate = config_path or project_root / ".pi" / "re-daemon-ghidra.json"
    if not candidate.is_file():
        return None
    try:
        raw: Any = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SettingsError(f"invalid Ghidra configuration at {candidate}: {error}") from error
    if not isinstance(raw, dict):
        raise SettingsError("Ghidra configuration must be a JSON object")
    command_value = raw.get("command")
    binary_value = raw.get("binary")
    prefix = raw.get("databaseIdPrefix", "re-daemon")
    if not isinstance(command_value, list) or not command_value or not all(isinstance(item, str) and item for item in command_value):
        raise SettingsError("Ghidra configuration command must be a non-empty string array")
    if not isinstance(binary_value, str) or not binary_value:
        raise SettingsError("Ghidra configuration binary must be a non-empty string")
    if not isinstance(prefix, str) or not prefix or any(character.isspace() for character in prefix):
        raise SettingsError("databaseIdPrefix must be a non-empty identifier without whitespace")
    binary = Path(binary_value)
    if not binary.is_absolute():
        binary = project_root / binary
    return GhidraConfig(tuple(command_value), binary.resolve(), database_id or f"{prefix}-{process_id}")
