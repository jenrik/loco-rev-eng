"""Normalize Pi RPC events before durable storage.

Pi's message_update and tool_execution_update events can contain accumulated
snapshots. Persisting them verbatim causes quadratic log growth. This module
retains deltas and terminal records only.
"""

from __future__ import annotations

from typing import Any

MAX_TEXT_BYTES = 256 * 1024


def _clip(value: Any, budget: int = MAX_TEXT_BYTES) -> Any:
    if isinstance(value, str):
        encoded = value.encode("utf-8")
        if len(encoded) <= budget:
            return value
        clipped = encoded[:budget].decode("utf-8", errors="ignore")
        return {"text": clipped, "truncated": True, "omittedBytes": len(encoded) - len(clipped.encode("utf-8"))}
    if isinstance(value, list):
        return [_clip(item, budget) for item in value]
    if isinstance(value, dict):
        return {str(key): _clip(item, budget) for key, item in value.items()}
    return value


def normalize_pi_event(event: dict[str, Any]) -> tuple[str, dict[str, Any]] | None:
    event_type = event.get("type")
    if event_type == "message_update":
        delta = event.get("assistantMessageEvent") or {}
        delta_type = delta.get("type")
        if delta_type in {"text_delta", "thinking_delta", "toolcall_delta"}:
            return "assistant_delta", {
                "deltaType": delta_type,
                "contentIndex": delta.get("contentIndex"),
                "delta": _clip(delta.get("delta", "")),
            }
        if delta_type in {"toolcall_start", "toolcall_end", "done", "error"}:
            return "assistant_stream_state", _clip({
                "deltaType": delta_type,
                "contentIndex": delta.get("contentIndex"),
                "toolCall": delta.get("toolCall"),
                "reason": delta.get("reason"),
            })
        return None
    if event_type == "tool_execution_start":
        return "tool_started", _clip({
            "toolCallId": event.get("toolCallId"), "toolName": event.get("toolName"), "args": event.get("args", {}),
        })
    if event_type == "tool_execution_update":
        # Progress is a replaceable live snapshot. Do not append accumulated output.
        return "tool_progress", {"toolCallId": event.get("toolCallId"), "toolName": event.get("toolName")}
    if event_type == "tool_execution_end":
        return "tool_finished", _clip({
            "toolCallId": event.get("toolCallId"), "toolName": event.get("toolName"),
            "isError": bool(event.get("isError")), "result": event.get("result"),
        })
    if event_type == "message_end":
        message = event.get("message", {})
        return "message_finished", _clip({"role": message.get("role"), "content": message.get("content"), "stopReason": message.get("stopReason")})
    if event_type == "agent_start":
        return event_type, {}
    if event_type == "agent_end":
        messages = event.get("messages", [])
        return event_type, _clip({
            "willRetry": bool(event.get("willRetry")),
            "messageCount": len(messages) if isinstance(messages, list) else 0,
        })
    if event_type == "agent_settled":
        return event_type, {}
    if event_type in {"turn_start", "turn_end"}:
        return event_type, {}
    if event_type in {"auto_retry_start", "auto_retry_end", "compaction_start", "compaction_end", "extension_error"}:
        return event_type, _clip({key: value for key, value in event.items() if key != "message"})
    return None
