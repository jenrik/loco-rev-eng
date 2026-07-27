import asyncio
import os
from pathlib import Path
import sys
import tempfile
import textwrap
import unittest

from tools.re_daemon.broker import EventBroker
from tools.re_daemon.pi_rpc import AgentManager
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
