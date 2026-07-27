import tempfile
from pathlib import Path
import unittest

from tools.re_daemon.store import DaemonStore


class OperatorWorkflowStoreTests(unittest.TestCase):
    def test_evidence_cache_hypothesis_revisions_and_scope_approval(self):
        with tempfile.TemporaryDirectory() as temporary:
            store = DaemonStore(Path(temporary) / "state.sqlite3")
            store.initialize()
            job = store.create_job("Building", "validate draw")
            task = store.create_task(job["id"], "draw", "inspect draw", "investigator", ["game/Building.cpp"])
            agent = store.create_agent(job["id"], "investigator", "inspect draw", str(Path(temporary) / "session"), ["game/Building.cpp"])
            store.transition_task(task["id"], "in_progress", "assigned", agent["id"])
            evidence = store.record_evidence(
                job["id"], task["id"], agent["id"], "ghidra", "decompile_function", {"address": "0x4343B0"}, {"content": ["draw"]}
            )
            cached = store.find_evidence(job["id"], "ghidra", "decompile_function", {"address": "0x4343B0"})
            self.assertEqual(cached["id"], evidence["id"])
            other_job = store.create_job("Other", "separate objective")
            self.assertIsNone(store.find_evidence(other_job["id"], "ghidra", "decompile_function", {"address": "0x4343B0"}))
            first = store.record_hypothesis(job["id"], task["id"], agent["id"], "Building::Draw", "vtable slot 11", [evidence["id"]])
            second = store.record_hypothesis(
                job["id"], task["id"], agent["id"], "Building::Draw", "vtable slot 11 draws a sprite", [evidence["id"]], "supported", first["id"]
            )
            self.assertEqual((first["revision"], second["revision"]), (1, 2))
            self.assertEqual(second["supersedes_id"], first["id"])
            scope = store.request_write_scope(agent["id"], task["id"], "game/Building.h", "add named field")
            approved = store.resolve_write_scope_request(scope["id"], "approved", "field evidence is sufficient")
            self.assertEqual(approved["status"], "approved")
            self.assertIn("game/Building.h", store.get_task(task["id"])["write_scope"])
            self.assertIn("game/Building.h", store.get_agent(agent["id"])["write_scope"])
            rejected = store.request_write_scope(agent["id"], task["id"], "game/Train.h", "unrelated")
            with self.assertRaises(ValueError):
                store.resolve_write_scope_request(rejected["id"], "rejected")
            self.assertEqual(store.resolve_write_scope_request(rejected["id"], "rejected", "outside task")["status"], "rejected")


if __name__ == "__main__":
    unittest.main(verbosity=2)
