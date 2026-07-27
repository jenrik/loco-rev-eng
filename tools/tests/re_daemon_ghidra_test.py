import asyncio
from pathlib import Path
import sys
import tempfile
import unittest

from tools.re_daemon.mcp import GhidraAdapter, GhidraConfig, McpError
from tools.re_daemon.store import DaemonStore


FAKE_MCP = """#!__PYTHON__
import json
import sys
for line in sys.stdin:
    request = json.loads(line)
    if 'id' not in request:
        continue
    if request['method'] == 'initialize':
        result = {'protocolVersion': '2024-11-05', 'capabilities': {}, 'serverInfo': {'name': 'fake-ghidra', 'version': '1'}}
    elif request['method'] == 'tools/call':
        call = request['params']
        if call['name'] == 'ghidra_decompile_function':
            result = {'content': [{'type': 'text', 'text': 'int FUN_00401000(void) { return 7; }'}], 'arguments': call['arguments']}
        else:
            result = {'content': [], 'arguments': call['arguments']}
    else:
        result = {}
    print(json.dumps({'jsonrpc': '2.0', 'id': request['id'], 'result': result}), flush=True)
"""


class GhidraAdapterTests(unittest.TestCase):
    def test_allowlisted_query_opens_raw_binary_and_persists_evidence(self):
        async def scenario():
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                server = root / 'fake-mcp'
                server.write_text(FAKE_MCP.replace('__PYTHON__', sys.executable), encoding='utf-8')
                server.chmod(0o755)
                binary = root / 'loco.exe'
                binary.write_bytes(b'MZ')
                adapter = GhidraAdapter(GhidraConfig((str(server),), binary, 'test-db'))
                result = await adapter.query('decompile_function', {'address': '0x401000'})
                self.assertEqual(result['arguments']['database'], 'test-db')
                self.assertTrue(adapter.status()['opened'])
                with self.assertRaises(McpError):
                    await adapter.query('rename_function', {'address': '0x401000'})
                await adapter.close()

                store = DaemonStore(root / 'state.sqlite3')
                store.initialize()
                job = store.create_job('Ghidra evidence', 'test')
                task = store.create_task(job['id'], 'decompile', 'decompile function', 'investigator')
                first = store.record_evidence(job['id'], task['id'], None, 'ghidra', 'decompile_function', {'address': '0x401000'}, result)
                duplicate = store.record_evidence(job['id'], task['id'], None, 'ghidra', 'decompile_function', {'address': '0x401000'}, result)
                self.assertEqual(first['id'], duplicate['id'])
                loaded = store.load_evidence(first['id'])
                self.assertEqual(loaded['response'], result)

        asyncio.run(scenario())


if __name__ == '__main__':
    unittest.main(verbosity=2)
