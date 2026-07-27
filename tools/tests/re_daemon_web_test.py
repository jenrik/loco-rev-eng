import tempfile
import unittest

from fastapi.testclient import TestClient

from tools.re_daemon.app import create_app


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
