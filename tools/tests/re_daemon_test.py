import asyncio
import multiprocessing
import tempfile
from pathlib import Path
import unittest

from tools.re_daemon.broker import EventBroker
from tools.re_daemon.instance_lock import acquire_daemon_lock
from tools.re_daemon.normalize import normalize_pi_event
from tools.re_daemon.store import DaemonStore


def _attempt_daemon_lock(state_path: str, result_queue) -> None:
    try:
        with acquire_daemon_lock(Path(state_path)):
            result_queue.put("acquired")
    except RuntimeError:
        result_queue.put("blocked")


class DaemonStoreTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.store = DaemonStore(Path(self.temporary.name) / "state.sqlite3")
        self.store.initialize()

    def tearDown(self):
        self.temporary.cleanup()

    def test_jobs_agents_and_cursor_event_replay(self):
        job = self.store.create_job("Validate Building", "Validate 0x4343B0")
        agent = self.store.create_agent(job["id"], "validator", "Check Draw", "sessions/agent")
        first = self.store.record_event(agent["id"], "tool_started", {"toolName": "re_ghidra_query"})
        second = self.store.record_event(agent["id"], "assistant_delta", {"delta": "observed field +0x4c"})
        self.assertLess(first["sequence"], second["sequence"])
        replay = self.store.events_after(first["sequence"], agent["id"])
        self.assertEqual([event["sequence"] for event in replay], [second["sequence"]])
        self.assertEqual(self.store.snapshot()["agents"][0]["status"], "queued")

    def test_snapshot_orders_agents_by_latest_event(self):
        job = self.store.create_job("Activity", "Order agent selector")
        first = self.store.create_agent(job["id"], "investigator", "older", "sessions/first")
        second = self.store.create_agent(job["id"], "validator", "newer", "sessions/second")
        self.store.record_event(first["id"], "assistant_delta", {"delta": "most recent"})
        agents = self.store.snapshot()["agents"]
        self.assertEqual([agent["id"] for agent in agents[:2]], [first["id"], second["id"]])
        self.assertGreater(agents[0]["last_activity_sequence"], agents[1]["last_activity_sequence"])

    def test_daemon_state_lock_rejects_another_process(self):
        context = multiprocessing.get_context("fork")
        queue = context.Queue()
        with acquire_daemon_lock(Path(self.temporary.name) / "state.sqlite3"):
            child = context.Process(target=_attempt_daemon_lock, args=(str(Path(self.temporary.name) / "state.sqlite3"), queue))
            child.start()
            child.join(timeout=5)
        self.assertFalse(child.is_alive())
        self.assertEqual(queue.get(timeout=1), "blocked")

    def test_job_status_is_derived_from_task_lifecycle(self):
        job = self.store.create_job("Lifecycle", "Keep dashboard state honest")
        self.assertEqual(self.store.snapshot()["jobs"][0]["status"], "draft")
        task = self.store.create_task(job["id"], "Investigate", "Read evidence", "investigator")
        self.assertEqual(self.store.snapshot()["jobs"][0]["status"], "queued")
        self.store.transition_task(task["id"], "in_progress", "claimed")
        self.assertEqual(self.store.snapshot()["jobs"][0]["status"], "running")
        self.store.transition_task(task["id"], "completed", "evidence recorded")
        self.assertEqual(self.store.snapshot()["jobs"][0]["status"], "completed")

    def test_broker_publishes_durable_event(self):
        job = self.store.create_job("Test", "Goal")
        agent = self.store.create_agent(job["id"], "investigator", "Task", "sessions/agent")

        async def publish_and_read():
            broker = EventBroker(self.store)
            queue = broker.subscribe()
            recorded = await broker.publish(agent["id"], "agent_start", {"role": "investigator"})
            delivered = await queue.get()
            self.assertEqual(delivered["sequence"], recorded["sequence"])
            broker.unsubscribe(queue)

        asyncio.run(publish_and_read())


class NormalizeTests(unittest.TestCase):
    def test_message_updates_persist_only_delta(self):
        normalized = normalize_pi_event({
            "type": "message_update",
            "message": {"content": "an accumulated megabyte-scale snapshot"},
            "assistantMessageEvent": {"type": "text_delta", "contentIndex": 0, "delta": "new bytes"},
        })
        self.assertEqual(normalized, ("assistant_delta", {"deltaType": "text_delta", "contentIndex": 0, "delta": "new bytes"}))

    def test_tool_progress_drops_accumulated_partial_result(self):
        normalized = normalize_pi_event({
            "type": "tool_execution_update", "toolCallId": "call-1", "toolName": "bash",
            "partialResult": {"content": [{"type": "text", "text": "everything so far"}]},
        })
        self.assertEqual(normalized, ("tool_progress", {"toolCallId": "call-1", "toolName": "bash"}))


if __name__ == "__main__":
    unittest.main(verbosity=2)
