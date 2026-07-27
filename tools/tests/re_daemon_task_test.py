import tempfile
from pathlib import Path
import unittest

from tools.re_daemon.store import DaemonStore


class TaskGraphTests(unittest.TestCase):
    def test_agent_expansion_persists_idempotent_gated_dag(self):
        with tempfile.TemporaryDirectory() as temporary:
            store = DaemonStore(Path(temporary) / 'state.sqlite3')
            store.initialize()
            job = store.create_job('Draw pipeline', 'integrate 0x4343B0')
            source = store.create_task(job['id'], 'Initial evidence triage', 'collect evidence', 'investigator')
            agent = store.create_agent(job['id'], 'investigator', 'collect evidence', str(Path(temporary) / 'session'))
            store.transition_task(source['id'], 'in_progress', 'launched', agent['id'])
            tasks = [
                {'key': 'inspect-4343b0', 'title': 'Inspect Draw', 'instructions': 'Disassemble 0x4343B0.', 'role': 'investigator'},
                {'key': 'transcribe-4343b0', 'title': 'Transcribe Draw', 'instructions': 'Implement only observed behavior.', 'role': 'transcriber', 'write_scope': ['src/decompiled_cpp/game/Building.cpp']},
                {'key': 'validate-4343b0', 'title': 'Validate Draw', 'instructions': 'Compare every instruction.', 'role': 'validator'},
            ]
            edges = [
                {'task_key': 'transcribe-4343b0', 'dependency_key': 'inspect-4343b0', 'relation': 'requires'},
                {'task_key': 'validate-4343b0', 'dependency_key': 'transcribe-4343b0', 'relation': 'requires'},
            ]
            expansion = store.expand_task_graph(agent['id'], 'The address needs three assembly-first passes.', tasks, edges)
            replay = store.expand_task_graph(agent['id'], 'The address needs three assembly-first passes.', tasks, edges)
            self.assertTrue(replay['idempotentReplay'])
            self.assertEqual(replay['id'], expansion['id'])
            self.assertEqual(len(store.list_tasks(job['id'])), 4)
            self.assertEqual(store.ready_tasks(job['id']), [])
            inspect_id = next(task['id'] for task in expansion['tasks'] if task['key'] == 'inspect-4343b0')
            store.transition_task(source['id'], 'completed', 'triage persisted')
            self.assertEqual([task['id'] for task in store.ready_tasks(job['id'])], [inspect_id])
            self.assertEqual(len(store.snapshot()['taskEdges']), 3)

    def test_agent_expansion_rejects_requires_cycle_without_partial_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            store = DaemonStore(Path(temporary) / 'state.sqlite3')
            store.initialize()
            job = store.create_job('Cycle', 'reject invalid graph')
            source = store.create_task(job['id'], 'source', 'plan', 'investigator')
            agent = store.create_agent(job['id'], 'investigator', 'plan', str(Path(temporary) / 'session'))
            store.transition_task(source['id'], 'in_progress', 'launched', agent['id'])
            tasks = [
                {'key': 'a', 'title': 'A', 'instructions': 'A', 'role': 'investigator'},
                {'key': 'b', 'title': 'B', 'instructions': 'B', 'role': 'validator'},
            ]
            with self.assertRaisesRegex(ValueError, 'acyclic'):
                store.expand_task_graph(agent['id'], 'invalid cycle', tasks, [
                    {'task_key': 'a', 'dependency_key': 'b', 'relation': 'requires'},
                    {'task_key': 'b', 'dependency_key': 'a', 'relation': 'requires'},
                ])
            self.assertEqual([task['id'] for task in store.list_tasks(job['id'])], [source['id']])

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
