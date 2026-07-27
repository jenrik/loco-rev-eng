import tempfile
from pathlib import Path
import unittest

from tools.re_daemon.store import DaemonStore


class TaskGraphTests(unittest.TestCase):
    def test_requires_edges_gate_ready_tasks_and_transitions_preserve_reason(self):
        with tempfile.TemporaryDirectory() as temporary:
            store = DaemonStore(Path(temporary) / 'state.sqlite3')
            store.initialize()
            job = store.create_job('Building', 'validate draw')
            prerequisite = store.create_task(job['id'], 'disassemble', 'Inspect instructions', 'investigator')
            dependent = store.create_task(job['id'], 'transcribe', 'Implement instructions', 'transcriber', ['src/decompiled_cpp/game/Building.cpp'])
            store.add_task_dependency(dependent['id'], prerequisite['id'])
            self.assertEqual([task['id'] for task in store.ready_tasks(job['id'])], [prerequisite['id']])
            store.transition_task(prerequisite['id'], 'completed', 'disassembly captured')
            self.assertEqual([task['id'] for task in store.ready_tasks(job['id'])], [dependent['id']])
            deferred = store.transition_task(dependent['id'], 'deferred', 'need caller xrefs')
            self.assertEqual(deferred['transition_reason'], 'need caller xrefs')
            self.assertEqual(store.ready_tasks(job['id']), [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
