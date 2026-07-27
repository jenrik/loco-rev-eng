import re
import sys
import tempfile
import time
import unittest
from pathlib import Path

from fastapi.testclient import TestClient

from tools.re_daemon.app import create_app
from tools.re_daemon.mcp import GhidraConfig


class DaemonWebTests(unittest.TestCase):
    def test_dashboard_api_and_live_websocket_event(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = create_app(f"{temporary}/state.sqlite3", daemon_token="test-capability", pi_binary="/bin/true")
            with TestClient(app) as client:
                dashboard = client.get("/")
                self.assertEqual(dashboard.status_code, 200)
                self.assertIn('<div id="root"></div>', dashboard.text)
                self.assertNotIn("cytoscape", dashboard.text.lower())
                script_path = re.search(r'<script[^>]+src="([^"]+)"', dashboard.text)
                style_path = re.search(r'<link[^>]+href="([^"]+)"', dashboard.text)
                self.assertIsNotNone(script_path)
                self.assertIsNotNone(style_path)
                self.assertEqual(client.get(script_path.group(1)).status_code, 200)
                self.assertEqual(client.get(style_path.group(1)).status_code, 200)
                job = client.post("/api/jobs", json={"title": "Validate Draw", "goal": "Check 0x4343B0"}).json()
                agent = client.post(
                    f"/api/jobs/{job['id']}/agents",
                    json={"role": "validator", "task": "Validate Draw", "write_scope": ["src/decompiled_cpp/game/Building.cpp"]},
                ).json()
                self.assertEqual(agent["write_scope"], ["src/decompiled_cpp/game/Building.cpp"])
                deferred = client.post(
                    f"/api/jobs/{job['id']}/tasks",
                    json={"title": "retry me", "instructions": "recheck xrefs", "role": "investigator", "status": "deferred"},
                ).json()
                retried = client.post(f"/api/tasks/{deferred['id']}/retry", json={"reason": "operator supplied new evidence"})
                self.assertEqual(retried.status_code, 200)
                self.assertEqual(retried.json()["status"], "ready")

                with client.websocket_connect("/ws") as socket:
                    response = client.post(
                        f"/internal/agents/{agent['id']}/events",
                        headers={"x-re-daemon-token": "test-capability"},
                        json={"kind": "assistant_delta", "payload": {"delta": "checking branch"}},
                    )
                    self.assertEqual(response.status_code, 200)
                    event = socket.receive_json()
                    self.assertEqual(event["agent_id"], agent["id"])
                    self.assertEqual(event["payload"]["delta"], "checking branch")

                denied = client.get(f"/internal/agents/{agent['id']}/context")
                self.assertEqual(denied.status_code, 403)

    def test_job_submission_creates_and_schedules_initial_triage(self):
        fake_pi = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        print(json.dumps({'type': 'agent_start'}), flush=True)
        print(json.dumps({'type': 'agent_settled'}), flush=True)
    elif command.get('type') == 'abort':
        print(json.dumps({'type': 'agent_settled'}), flush=True)
        break
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / 'fake-pi'
            executable.write_text(fake_pi.replace('__PYTHON__', sys.executable), encoding='utf-8')
            executable.chmod(0o755)
            app = create_app(root / 'state.sqlite3', daemon_token='test-capability', pi_binary=str(executable))
            with TestClient(app) as client:
                created = client.post('/api/jobs', json={'title': 'Fresh objective', 'goal': 'Collect binary evidence'}).json()
                self.assertEqual(created['initialTask']['title'], 'Initial evidence triage')
                self.assertEqual(created['initialTask']['role'], 'investigator')
                self.assertEqual(len(created['launched']), 1)
                status = client.get('/api/status').json()
                initial = next(task for task in status['tasks'] if task['id'] == created['initialTask']['id'])
                self.assertEqual(initial['status'], 'in_progress')
                self.assertEqual(initial['assigned_agent_id'], created['launched'][0]['agent']['id'])
                draft = app.state.store.create_job('Existing draft', 'Start it now')
                started = client.post(f"/api/jobs/{draft['id']}/bootstrap")
                self.assertEqual(started.status_code, 200)
                self.assertEqual(started.json()['initialTask']['title'], 'Initial evidence triage')
                self.assertEqual(len(started.json()['launched']), 1)
                self.assertEqual(client.post(f"/api/jobs/{draft['id']}/bootstrap").status_code, 409)

    def test_operator_recovery_fails_stuck_in_progress_task(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = create_app(f"{temporary}/state.sqlite3", daemon_token="test-capability", pi_binary="/bin/true")
            with TestClient(app) as client:
                store = app.state.store
                job = store.create_job("recover", "stuck task")
                task = store.create_task(job["id"], "stuck", "recover it", "investigator")
                agent = store.create_agent(job["id"], "investigator", "stuck", f"{temporary}/session")
                store.transition_task(task["id"], "in_progress", "launched", agent["id"])
                response = client.post(f"/api/tasks/{task['id']}/recover", json={"reason": "no tool completion"})
                self.assertEqual(response.status_code, 200)
                self.assertEqual(response.json()["status"], "failed")
                self.assertIn("operator recovery", response.json()["transition_reason"])
                self.assertEqual(store.get_agent(agent["id"])["status"], "failed")


    def test_internal_ghidra_query_records_evidence(self):
        fake_mcp = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    request = json.loads(line)
    if 'id' not in request:
        continue
    if request['method'] == 'initialize':
        result = {'protocolVersion': '2024-11-05', 'capabilities': {}, 'serverInfo': {}}
    elif request['method'] == 'tools/call':
        result = {'content': [{'type': 'text', 'text': 'decompiled'}]}
    else:
        result = {}
    print(json.dumps({'jsonrpc': '2.0', 'id': request['id'], 'result': result}), flush=True)
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / 'loco.exe'
            binary.write_bytes(b'MZ')
            server = root / 'fake-mcp'
            server.write_text(fake_mcp.replace('__PYTHON__', sys.executable), encoding='utf-8')
            server.chmod(0o755)
            app = create_app(root / 'state.sqlite3', daemon_token='test-capability', pi_binary='/bin/true', ghidra_config=GhidraConfig((str(server),), binary, 'test-db'))
            with TestClient(app) as client:
                job = client.post('/api/jobs', json={'title': 'Ghidra', 'goal': 'decompile'}).json()
                agent = client.post(f"/api/jobs/{job['id']}/agents", json={'role': 'investigator', 'task': 'query'}).json()
                response = client.post(
                    f"/internal/agents/{agent['id']}/ghidra",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'operation': 'decompile_function', 'arguments': {'address': '0x401000'}},
                )
                self.assertEqual(response.status_code, 200)
                payload = response.json()
                self.assertEqual(payload['evidence']['source'], 'ghidra')
                self.assertEqual(payload['response']['content'][0]['text'], 'decompiled')
                cached = client.post(
                    f"/internal/agents/{agent['id']}/ghidra",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'operation': 'decompile_function', 'arguments': {'address': '0x401000'}},
                ).json()
                self.assertTrue(cached['cacheHit'])
                hypothesis = client.post(
                    f"/internal/agents/{agent['id']}/hypotheses",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'subject': 'test function', 'statement': 'returns decompiled value', 'evidence_ids': [payload['evidence']['id']]},
                )
                self.assertEqual(hypothesis.status_code, 200)
                self.assertEqual(client.get(f"/api/jobs/{job['id']}/hypotheses").json()['hypotheses'][0]['revision'], 1)
                scope = client.post(
                    f"/internal/agents/{agent['id']}/write-scope-requests",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'path': 'game/Building.h', 'reason': 'need named field'},
                ).json()
                approved = client.post(
                    f"/api/write-scope-requests/{scope['id']}/resolve", json={'decision': 'approved', 'reason': 'approved by test'}
                )
                self.assertEqual(approved.status_code, 200)
                context = client.get(f"/internal/agents/{agent['id']}/context", headers={'x-re-daemon-token': 'test-capability'}).json()
                self.assertIn('game/Building.h', context['agent']['write_scope'])

    def test_startup_resumes_a_queued_graph(self):
        fake_pi = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        print(json.dumps({'type': 'agent_start'}), flush=True)
        print(json.dumps({'type': 'agent_settled'}), flush=True)
    elif command.get('type') == 'abort':
        break
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / 'fake-pi'
            executable.write_text(fake_pi.replace('__PYTHON__', sys.executable), encoding='utf-8')
            executable.chmod(0o755)
            app = create_app(root / 'state.sqlite3', project_root=Path.cwd(), daemon_token='test-capability', pi_binary=str(executable))
            job = app.state.store.create_job('resume graph', 'continue queued work')
            task = app.state.store.create_task(job['id'], 'queued validation', 'validate one function', 'validator')
            with TestClient(app) as client:
                current = app.state.store.get_task(task['id'])
                self.assertEqual(current['status'], 'in_progress')
                self.assertIsNotNone(current['assigned_agent_id'])
                completed = client.post(
                    f"/internal/agents/{current['assigned_agent_id']}/task/transition",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'status': 'completed', 'reason': 'startup resume verified'},
                )
                self.assertEqual(completed.status_code, 200)

    def test_agent_expands_graph_and_completion_launches_successor(self):
        fake_pi = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        print(json.dumps({'type': 'agent_start'}), flush=True)
        print(json.dumps({'type': 'agent_settled'}), flush=True)
    elif command.get('type') == 'abort':
        print(json.dumps({'type': 'agent_settled'}), flush=True)
        break
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / 'fake-pi'
            executable.write_text(fake_pi.replace('__PYTHON__', sys.executable), encoding='utf-8')
            executable.chmod(0o755)
            app = create_app(root / 'state.sqlite3', project_root=Path.cwd(), daemon_token='test-capability', pi_binary=str(executable))
            with TestClient(app) as client:
                created = client.post('/api/jobs', json={'title': 'autonomous', 'goal': 'validate 0x4343B0'}).json()
                source_agent = created['launched'][0]['agent']['id']
                headers = {'x-re-daemon-token': 'test-capability'}
                refused = client.post(
                    f"/internal/agents/{source_agent}/task/transition", headers=headers,
                    json={'status': 'completed', 'reason': 'prose plan only'},
                )
                self.assertEqual(refused.status_code, 409)
                expansion = client.post(
                    f"/internal/agents/{source_agent}/task/expand", headers=headers,
                    json={
                        'rationale': 'Triage evidence identifies one bounded validation pass.',
                        'tasks': [{
                            'key': 'validate-4343b0', 'title': 'Validate Draw 0x4343B0',
                            'instructions': 'Compare implementation with disassembly at 0x4343B0.',
                            'role': 'validator', 'write_scope': [],
                        }],
                        'dependencies': [],
                    },
                )
                self.assertEqual(expansion.status_code, 200)
                successor_id = expansion.json()['tasks'][0]['id']
                completed = client.post(
                    f"/internal/agents/{source_agent}/task/transition", headers=headers,
                    json={'status': 'completed', 'reason': 'evidence and graph persisted'},
                )
                self.assertEqual(completed.status_code, 200)
                status = client.get('/api/status').json()
                successor = next(task for task in status['tasks'] if task['id'] == successor_id)
                self.assertEqual(successor['status'], 'in_progress')
                self.assertIsNotNone(successor['assigned_agent_id'])
                self.assertTrue(any(edge['task_id'] == successor_id and edge['dependency_task_id'] == created['initialTask']['id'] for edge in status['taskEdges']))
                successor_done = client.post(
                    f"/internal/agents/{successor['assigned_agent_id']}/task/transition", headers=headers,
                    json={'status': 'completed', 'reason': 'validation complete'},
                )
                self.assertEqual(successor_done.status_code, 200)

    def test_terminal_task_transition_aborts_live_pi_attempt(self):
        fake_pi = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    command = json.loads(line)
    if command.get('type') == 'prompt':
        print(json.dumps({'type': 'agent_start'}), flush=True)
        print(json.dumps({'type': 'agent_settled'}), flush=True)
    elif command.get('type') == 'abort':
        print(json.dumps({'type': 'agent_settled'}), flush=True)
        break
"""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            executable = root / 'fake-pi'
            executable.write_text(fake_pi.replace('__PYTHON__', sys.executable), encoding='utf-8')
            executable.chmod(0o755)
            app = create_app(root / 'state.sqlite3', project_root=Path.cwd(), daemon_token='test-capability', pi_binary=str(executable))
            with TestClient(app) as client:
                job = client.post('/api/jobs', json={'title': 'terminal', 'goal': 'stop after completion'}).json()
                task = client.post(
                    f"/api/jobs/{job['id']}/tasks",
                    json={'title': 'one attempt', 'instructions': 'wait for terminal transition', 'role': 'investigator'},
                ).json()
                launched = client.post(f"/api/jobs/{job['id']}/schedule?limit=1").json()['launched'][0]
                agent_id = launched['agent']['id']
                completed = client.post(
                    f"/internal/agents/{agent_id}/task/transition",
                    headers={'x-re-daemon-token': 'test-capability'},
                    json={'status': 'completed', 'reason': 'trial finished'},
                )
                self.assertEqual(completed.status_code, 200)
                for _ in range(100):
                    status = client.get('/api/status').json()
                    agent = next(item for item in status['agents'] if item['id'] == agent_id)
                    if agent['status'] == 'settled':
                        break
                    time.sleep(0.01)
                self.assertEqual(agent['status'], 'settled')
                for _ in range(100):
                    if agent_id not in app.state.agent_manager._agents:
                        break
                    time.sleep(0.02)
                self.assertNotIn(agent_id, app.state.agent_manager._agents)
                self.assertEqual(next(item for item in status['tasks'] if item['id'] == task['id'])['status'], 'completed')
                kinds = [event['kind'] for event in client.get(f"/api/agents/{agent_id}/events?limit=100").json()['events']]
                self.assertIn('terminal_abort_requested', kinds)


if __name__ == "__main__":
    unittest.main(verbosity=2)
