import sys
import tempfile
import unittest
from pathlib import Path

from fastapi.testclient import TestClient

from tools.re_daemon.app import create_app
from tools.re_daemon.mcp import GhidraConfig


class DaemonWebTests(unittest.TestCase):
    def test_dashboard_api_and_live_websocket_event(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = create_app(f"{temporary}/state.sqlite3", daemon_token="test-capability")
            with TestClient(app) as client:
                self.assertEqual(client.get("/").status_code, 200)
                job = client.post("/api/jobs", json={"title": "Validate Draw", "goal": "Check 0x4343B0"}).json()
                agent = client.post(
                    f"/api/jobs/{job['id']}/agents",
                    json={"role": "validator", "task": "Validate Draw", "write_scope": ["src/decompiled_cpp/game/Building.cpp"]},
                ).json()
                self.assertEqual(agent["write_scope"], ["src/decompiled_cpp/game/Building.cpp"])

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
            app = create_app(root / 'state.sqlite3', daemon_token='test-capability', ghidra_config=GhidraConfig((str(server),), binary, 'test-db'))
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
