"""Read-only Ghidra adapter over a daemon-owned MCP stdio process."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
from typing import Any


class McpError(RuntimeError):
    """An MCP transport, protocol, or server error."""


class McpStdioClient:
    """Minimal JSON-RPC 2.0 stdio client for a single MCP server.

    MCP operations are serialized by ``GhidraAdapter``. This client still
    correlates request IDs so notifications cannot be mistaken for responses.
    """

    def __init__(self, command: list[str], timeout_seconds: float = 120.0):
        if not command:
            raise ValueError("MCP command is required")
        self.command = command
        self.timeout_seconds = timeout_seconds
        self.process: asyncio.subprocess.Process | None = None
        self._next_id = 1
        self._pending: dict[int, asyncio.Future[dict[str, Any]]] = {}
        self._write_lock = asyncio.Lock()
        self._reader_task: asyncio.Task[None] | None = None
        self._stderr_task: asyncio.Task[None] | None = None
        self.stderr_tail: list[str] = []

    async def start(self) -> None:
        if self.process is not None and self.process.returncode is None:
            return
        self.process = await asyncio.create_subprocess_exec(
            *self.command,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        self._reader_task = asyncio.create_task(self._read_stdout())
        self._stderr_task = asyncio.create_task(self._read_stderr())
        await self.request(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "lego-loco-re-daemon", "version": "0.1"},
            },
        )
        await self.notify("notifications/initialized", {})

    async def close(self) -> None:
        if self.process is None:
            return
        if self.process.returncode is None:
            self.process.terminate()
            try:
                await asyncio.wait_for(self.process.wait(), timeout=5)
            except TimeoutError:
                self.process.kill()
                await self.process.wait()
        for task in (self._reader_task, self._stderr_task):
            if task is not None:
                task.cancel()
        self.process = None

    async def notify(self, method: str, params: dict[str, Any]) -> None:
        await self._write({"jsonrpc": "2.0", "method": method, "params": params})

    async def request(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        await self.start_if_needed_for_request()
        request_id = self._next_id
        self._next_id += 1
        future: asyncio.Future[dict[str, Any]] = asyncio.get_running_loop().create_future()
        self._pending[request_id] = future
        try:
            await self._write({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
            response = await asyncio.wait_for(future, timeout=self.timeout_seconds)
        except TimeoutError as error:
            raise McpError(f"MCP request timed out: {method}") from error
        finally:
            self._pending.pop(request_id, None)
        if "error" in response:
            raise McpError(f"MCP {method} failed: {response['error']}")
        result = response.get("result")
        if not isinstance(result, dict):
            raise McpError(f"MCP {method} returned a non-object result")
        return result

    async def start_if_needed_for_request(self) -> None:
        if self.process is None or self.process.returncode is not None:
            await self.start()

    async def call_tool(self, name: str, arguments: dict[str, Any]) -> dict[str, Any]:
        return await self.request("tools/call", {"name": name, "arguments": arguments})

    async def _write(self, payload: dict[str, Any]) -> None:
        if self.process is None or self.process.stdin is None or self.process.returncode is not None:
            raise McpError("MCP server is not running")
        encoded = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        async with self._write_lock:
            self.process.stdin.write(encoded)
            await self.process.stdin.drain()

    async def _read_stdout(self) -> None:
        assert self.process is not None and self.process.stdout is not None
        while line := await self.process.stdout.readline():
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                self._fail_all(McpError("MCP server emitted invalid JSON"))
                continue
            request_id = message.get("id")
            if isinstance(request_id, int) and request_id in self._pending:
                future = self._pending[request_id]
                if not future.done():
                    future.set_result(message)
        self._fail_all(McpError("MCP server terminated unexpectedly"))

    async def _read_stderr(self) -> None:
        assert self.process is not None and self.process.stderr is not None
        while line := await self.process.stderr.readline():
            self.stderr_tail.append(line.decode("utf-8", errors="replace").rstrip())
            del self.stderr_tail[:-20]

    def _fail_all(self, error: Exception) -> None:
        for future in self._pending.values():
            if not future.done():
                future.set_exception(error)


@dataclass(frozen=True)
class GhidraConfig:
    command: tuple[str, ...]
    binary_path: Path
    database_id: str


class GhidraAdapter:
    """Allowlisted, read-only Ghidra operations for one raw binary database."""

    OPERATIONS = {
        "decompile_function": "ghidra_decompile_function",
        "disassemble_function": "ghidra_disassemble_function",
        "list_functions": "ghidra_list_functions",
        "get_xrefs_to": "ghidra_get_xrefs_to",
        "get_xrefs_from": "ghidra_get_xrefs_from",
        "list_structures": "ghidra_list_structures",
        "get_structure": "ghidra_get_structure",
        "list_names": "ghidra_list_names",
        "get_strings": "ghidra_get_strings",
        "find_code_by_string": "ghidra_find_code_by_string",
    }

    def __init__(self, config: GhidraConfig | None):
        self.config = config
        self._client = McpStdioClient(list(config.command)) if config is not None else None
        self._lock = asyncio.Lock()
        self._opened = False
        self.last_error: str | None = None
        self.restart_count = 0
        self.binary_digest = self._digest_binary(config.binary_path) if config is not None and config.binary_path.is_file() else None

    def status(self) -> dict[str, Any]:
        return {
            "configured": self.config is not None,
            "databaseId": self.config.database_id if self.config else None,
            "binaryPath": str(self.config.binary_path) if self.config else None,
            "opened": self._opened,
            "lastError": self.last_error,
            "restartCount": self.restart_count,
            "binaryDigest": self.binary_digest,
        }

    async def close(self) -> None:
        if self._client is not None:
            if self._opened and self.config is not None:
                try:
                    await self._client.call_tool("ghidra_close_database", {"database": self.config.database_id})
                except McpError:
                    # Shutdown must release the process even if the worker already died.
                    pass
            await self._client.close()
        self._opened = False

    async def query(self, operation: str, arguments: dict[str, Any]) -> dict[str, Any]:
        if self.config is None or self._client is None:
            raise McpError("Ghidra is not configured; start the daemon with --ghidra-command and --ghidra-binary")
        if operation not in self.OPERATIONS:
            raise McpError(f"operation is not in the read-only Ghidra allowlist: {operation}")
        if not isinstance(arguments, dict):
            raise McpError("Ghidra arguments must be an object")
        request = dict(arguments)
        supplied_database = request.get("database")
        if supplied_database is not None and supplied_database != self.config.database_id:
            raise McpError("requested database differs from the daemon-owned database")
        request["database"] = self.config.database_id
        async with self._lock:
            for attempt in range(2):
                try:
                    await self._ensure_open()
                    result = await self._client.call_tool(self.OPERATIONS[operation], request)
                    self.last_error = None
                    return result
                except McpError as error:
                    self.last_error = str(error)
                    if attempt:
                        raise
                    await self._restart()
        raise AssertionError("unreachable")

    @staticmethod
    def _digest_binary(binary_path: Path) -> str:
        digest = hashlib.sha256()
        with binary_path.open("rb") as binary:
            for chunk in iter(lambda: binary.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()

    async def _restart(self) -> None:
        """Discard a failed worker and reopen the daemon-owned database once."""
        if self._client is not None:
            await self._client.close()
        assert self.config is not None
        self._client = McpStdioClient(list(self.config.command))
        self._opened = False
        self.restart_count += 1
    async def _ensure_open(self) -> None:
        assert self.config is not None and self._client is not None
        if self._client.process is None or self._client.process.returncode is not None:
            self._opened = False
        if self._opened:
            return
        if not self.config.binary_path.is_file():
            raise McpError(f"Ghidra binary does not exist: {self.config.binary_path}")
        await self._client.call_tool("ghidra_open_database", {
            "file_path": str(self.config.binary_path),
            "database_id": self.config.database_id,
        })
        await self._client.call_tool("ghidra_wait_for_analysis", {"database": self.config.database_id})
        self._opened = True
