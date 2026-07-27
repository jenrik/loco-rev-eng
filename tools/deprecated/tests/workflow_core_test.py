#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "tools" / "workflow_core.py"


class WorkflowCoreTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.state = Path(self.temporary.name) / "state.json"
        self.call("init", {"binary": {"sha256": "test-binary", "imageBase": "0x400000"}})

    def tearDown(self):
        self.temporary.cleanup()

    def call(self, command, payload):
        request = Path(self.temporary.name) / f"{command}.json"
        request.write_text(json.dumps(payload), encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(CORE), command, "--state", str(self.state), "--input", str(request)],
            capture_output=True,
            text=True,
            check=False,
        )
        response = json.loads(result.stdout)
        return result.returncode, response

    def task(self, task_id, status="open", **extra):
        payload = {"task": {"id": task_id, "phase": "validate", "status": status, **extra}}
        code, response = self.call("upsert-task", payload)
        self.assertEqual(code, 0, response)
        return response["result"]

    def test_evidence_is_revisioned_and_deduplicated(self):
        first = {"decompiler": "return 0;", "address": "0x401000"}
        code, response = self.call("record-evidence", {
            "key": "loco:test:function:0x401000",
            "source": {"tool": "mcp.ghidra.decompile_function"},
            "artifact": first,
            "observations": [{"kind": "return", "confidence": "observed"}],
            "hypotheses": [],
        })
        self.assertEqual(code, 0)
        self.assertEqual(response["result"], {"key": "loco:test:function:0x401000", "revision": 1, "deduplicated": False})

        code, response = self.call("record-evidence", {
            "key": "loco:test:function:0x401000", "artifact": first,
        })
        self.assertEqual(code, 0)
        self.assertTrue(response["result"]["deduplicated"])
        self.assertEqual(response["result"]["revision"], 1)

        code, response = self.call("record-evidence", {
            "key": "loco:test:function:0x401000", "artifact": {"decompiler": "return 1;"},
        })
        self.assertEqual(code, 0)
        self.assertEqual(response["result"]["revision"], 2)

        code, response = self.call("get-evidence", {"key": "loco:test:function:0x401000"})
        self.assertEqual(code, 0)
        self.assertEqual(len(response["result"]["evidence"]["revisions"]), 2)
        self.assertEqual(response["result"]["evidence"]["revisions"][0]["artifact"], first)

    def test_requires_edges_and_deferred_ledger_drive_ready_set(self):
        self.task("fn:base:integrate")
        self.task("fn:child:validate")
        code, response = self.call("add-edge", {
            "from": "fn:child:validate", "to": "fn:base:integrate",
            "kind": "requires", "confidence": "observed",
            "provenance": {"address": "0x401234"},
        })
        self.assertEqual(code, 0, response)

        code, response = self.call("ready", {})
        self.assertEqual(code, 0)
        self.assertEqual([task["id"] for task in response["result"]["ready"]], ["fn:base:integrate"])
        self.assertEqual(response["result"]["waiting"][0]["unmetPrerequisites"], ["fn:base:integrate"])

        code, response = self.call("transition", {
            "taskId": "fn:base:integrate", "status": "integrated", "reason": "assembly review complete",
        })
        self.assertEqual(code, 0, response)
        code, response = self.call("ready", {})
        self.assertEqual(code, 0)
        self.assertEqual([task["id"] for task in response["result"]["ready"]], ["fn:child:validate"])

        code, response = self.call("defer", {
            "taskId": "fn:child:validate", "status": "deferred",
            "reason": "need TileMap flag semantics",
            "nextAction": "inspect xrefs", "blockedBy": [],
            "evidenceRefs": ["loco:test:function:0x401000"],
            "retryWhen": "flag semantics observed",
        })
        self.assertEqual(code, 0, response)
        self.assertEqual(response["result"]["task"]["status"], "deferred")
        code, response = self.call("ready", {"includeDeferred": True})
        self.assertEqual(code, 0)
        self.assertEqual([task["id"] for task in response["result"]["ready"]], ["fn:child:validate"])

    def test_write_set_audit_separates_allowed_shared_and_unexpected(self):
        self.task(
            "fn:draw:integrate",
            allowedWrites=["src/decompiled_cpp/game/Building.cpp"],
            sharedWrites=["PROGRESS.md"],
        )
        code, response = self.call("validate-write-set", {
            "taskId": "fn:draw:integrate",
            "before": {
                "src/decompiled_cpp/game/Building.cpp": "old",
                "PROGRESS.md": "old",
                "src/decompiled_cpp/shared/types.h": "old",
            },
            "after": {
                "src/decompiled_cpp/game/Building.cpp": "new",
                "PROGRESS.md": "new",
                "src/decompiled_cpp/shared/types.h": "new",
            },
        })
        self.assertEqual(code, 0, response)
        audit = response["result"]
        self.assertEqual(audit["allowed"], ["src/decompiled_cpp/game/Building.cpp"])
        self.assertEqual(audit["shared"], ["PROGRESS.md"])
        self.assertEqual(audit["unexpected"], ["src/decompiled_cpp/shared/types.h"])

    def test_rejects_unsafe_paths_and_unknown_blockers(self):
        code, response = self.call("upsert-task", {
            "task": {"id": "bad", "phase": "validate", "allowedWrites": ["../outside.cpp"]},
        })
        self.assertEqual(code, 2)
        self.assertEqual(response["error"]["code"], "invalid_path")

        self.task("fn:known:validate")
        code, response = self.call("defer", {
            "taskId": "fn:known:validate", "reason": "missing", "nextAction": "find it",
            "blockedBy": ["does-not-exist"], "evidenceRefs": [],
        })
        self.assertEqual(code, 2)
        self.assertEqual(response["error"]["code"], "unknown_task")


if __name__ == "__main__":
    unittest.main(verbosity=2)
