import asyncio
import os
from pathlib import Path
import sys
import tempfile
import textwrap
import unittest

from tools.re_daemon.broker import EventBroker
from tools.re_daemon.pi_rpc import AgentManager
from tools.re_daemon.scheduler import AutonomousScheduler
from tools.re_daemon.store import DaemonStore


FAKE_PI = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        events = [
            {'type': 'agent_start'},
            {'type': 'message_update', 'message': {'content': 'accumulated'}, 'assistantMessageEvent': {'type': 'text_delta', 'contentIndex': 0, 'delta': 'hello'}},
            {'type': 'tool_execution_start', 'toolCallId': 'call-1', 'toolName': 'read', 'args': {'path': 'x'}},
            {'type': 'tool_execution_end', 'toolCallId': 'call-1', 'toolName': 'read', 'result': {'content': [{'type': 'text', 'text': 'ok'}]}, 'isError': False},
            {'type': 'agent_settled'},
        ]
        for event in events:
            print(json.dumps(event), flush=True)
        break
"""


FAKE_STALLED_PI = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        print(json.dumps({'type': 'agent_start'}), flush=True)
        print(json.dumps({'type': 'tool_execution_start', 'toolCallId': 'stuck-read', 'toolName': 'read', 'args': {'path': 'game/Building.cpp'}}), flush=True)
    elif command.get('type') == 'abort':
        print(json.dumps({'type': 'agent_settled'}), flush=True)
        break
"""

class PiRpcManagerTests(unittest.TestCase):
    def test_launch_normalizes_live_rpc_events_and_retains_status(self):
        async def scenario():
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fake_pi = root / "fake-pi"
                fake_pi.write_text(FAKE_PI.replace("__PYTHON__", sys.executable), encoding="utf-8")
                fake_pi.chmod(0o755)
                project = Path.cwd()
                store = DaemonStore(root / "state.sqlite3")
                store.initialize()
                job = store.create_job("test", "test pi rpc")
                agent = store.create_agent(job["id"], "investigator", "inspect one function", str(root / "session"), ["PROGRESS.md"])
                broker = EventBroker(store)
                manager = AgentManager(store, broker, project, "http://127.0.0.1:8765", "not-logged", str(fake_pi))
                await manager.launch(agent["id"])
                for _ in range(100):
                    if store.get_agent(agent["id"])["status"] == "settled":
                        break
                    await asyncio.sleep(0.01)
                self.assertEqual(store.get_agent(agent["id"])["status"], "settled")
                events = store.events_after(agent_id=agent["id"], limit=100)
                self.assertIn("assistant_delta", [event["kind"] for event in events])
                self.assertIn("tool_started", [event["kind"] for event in events])
                self.assertIn("tool_finished", [event["kind"] for event in events])
                self.assertNotIn("message_update", [event["kind"] for event in events])

        asyncio.run(scenario())

    def test_scheduler_launches_only_ready_tasks_then_unblocks_dependents(self):
        async def scenario():
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fake_pi = root / "fake-pi"
                fake_pi.write_text(FAKE_PI.replace("__PYTHON__", sys.executable), encoding="utf-8")
                fake_pi.chmod(0o755)
                store = DaemonStore(root / "state.sqlite3")
                store.initialize()
                job = store.create_job("task graph", "validate")
                prerequisite = store.create_task(job["id"], "inspect", "inspect assembly", "investigator")
                dependent = store.create_task(job["id"], "validate", "validate C++", "validator")
                store.add_task_dependency(dependent["id"], prerequisite["id"])
                broker = EventBroker(store)
                manager = AgentManager(store, broker, Path.cwd(), "http://127.0.0.1:8765", "not-logged", str(fake_pi))
                scheduler = AutonomousScheduler(store, broker, manager, root / "state.sqlite3")
                first = await scheduler.schedule(job["id"], 2)
                self.assertEqual(len(first), 1)
                self.assertEqual(first[0]["task"]["id"], prerequisite["id"])
                self.assertIsNotNone(store.task_for_agent(first[0]["agent"]["id"]))
                store.transition_task(prerequisite["id"], "completed", "done")
                second = await scheduler.schedule(job["id"], 2)
                self.assertEqual(len(second), 1)
                self.assertEqual(second[0]["task"]["id"], dependent["id"])

        asyncio.run(scenario())

    def test_tool_watchdog_fails_and_reaps_stalled_attempt(self):
        async def scenario():
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fake_pi = root / "stalled-pi"
                fake_pi.write_text(FAKE_STALLED_PI.replace("__PYTHON__", sys.executable), encoding="utf-8")
                fake_pi.chmod(0o755)
                store = DaemonStore(root / "state.sqlite3")
                store.initialize()
                job = store.create_job("watchdog", "recover stalled tool")
                task = store.create_task(job["id"], "stalled", "wait", "investigator")
                agent = store.create_agent(job["id"], "investigator", "wait", str(root / "session"))
                store.transition_task(task["id"], "in_progress", "launched", agent["id"])
                broker = EventBroker(store)
                manager = AgentManager(
                    store, broker, Path.cwd(), "http://127.0.0.1:8765", "not-logged", str(fake_pi),
                    tool_timeout_seconds=0.05, watchdog_poll_seconds=0.01,
                )
                await manager.launch(agent["id"])
                for _ in range(200):
                    if store.get_task(task["id"])["status"] == "failed":
                        break
                    await asyncio.sleep(0.01)
                self.assertEqual(store.get_task(task["id"])["status"], "failed")
                for _ in range(200):
                    if agent["id"] not in manager._agents:
                        break
                    await asyncio.sleep(0.01)
                self.assertNotIn(agent["id"], manager._agents)
                kinds = [event["kind"] for event in store.events_after(agent_id=agent["id"], limit=100)]
                self.assertIn("agent_tool_timeout", kinds)
                self.assertIn("task_attempt_failed", kinds)

        asyncio.run(scenario())

    def test_startup_recovers_task_with_dead_assigned_pid(self):
        async def scenario():
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                store = DaemonStore(root / "state.sqlite3")
                store.initialize()
                job = store.create_job("restart", "recover orphan")
                task = store.create_task(job["id"], "orphan", "recover", "investigator")
                agent = store.create_agent(job["id"], "investigator", "orphan", str(root / "session"))
                store.transition_task(task["id"], "in_progress", "launched", agent["id"])
                manager = AgentManager(store, EventBroker(store), Path.cwd(), "http://127.0.0.1:8765", "not-logged")
                self.assertEqual(await manager.recover_orphaned_tasks(), [task["id"]])
                self.assertEqual(store.get_task(task["id"])["status"], "failed")
                self.assertEqual(store.get_agent(agent["id"])["status"], "failed")

        asyncio.run(scenario())


if __name__ == "__main__":
    unittest.main(verbosity=2)
