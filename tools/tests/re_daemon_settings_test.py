import json
from pathlib import Path
import tempfile
import unittest

from tools.re_daemon.settings import SettingsError, load_ghidra_config


class GhidraSettingsTests(unittest.TestCase):
    def test_project_local_non_secret_config_and_cli_precedence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / 'loco.exe'
            binary.write_bytes(b'MZ')
            config_path = root / '.pi' / 're-daemon-ghidra.json'
            config_path.parent.mkdir()
            config_path.write_text(json.dumps({
                'command': ['/opt/re-mcp-ghidra', 'proxy'],
                'binary': 'loco.exe',
                'databaseIdPrefix': 'loco-daemon',
            }), encoding='utf-8')
            discovered = load_ghidra_config(root, None, None, None, None, 99)
            self.assertEqual(discovered.command, ('/opt/re-mcp-ghidra', 'proxy'))
            self.assertEqual(discovered.database_id, 'loco-daemon-99')
            explicit = load_ghidra_config(root, None, '/other/mcp proxy', binary, 'manual-db', 99)
            self.assertEqual(explicit.command, ('/other/mcp', 'proxy'))
            self.assertEqual(explicit.database_id, 'manual-db')
            with self.assertRaises(SettingsError):
                load_ghidra_config(root, None, 'only-command', None, None, 99)


if __name__ == '__main__':
    unittest.main(verbosity=2)
