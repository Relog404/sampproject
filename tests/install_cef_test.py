"""One-click installer for the open.mp CEF component.

Anything below 1.4.0 is useless on open.mp: delivering browser events is
a call to ProcessMainThreadTasks() from the component's onTick, and 1.3.0
has neither. The installer must therefore pin the version, match the
server's bitness, install into components/ (not plugins/) and clean up
what the SA-MP plugin setup left behind.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'tools'
PS1 = (TOOLS / 'install-cef-omp.ps1').read_text(encoding='utf-8-sig')
BAT = (TOOLS / 'install-cef-omp.bat').read_text(encoding='cp1251')


class InstallerTest(unittest.TestCase):
    def test_it_installs_the_version_that_can_deliver_events(self):
        self.assertIn("$Version = '1.4.0'", PS1)

    def test_the_bitness_is_read_from_the_server_not_guessed(self):
        self.assertIn('Get-PeMachine $serverExe', PS1)
        self.assertIn('cef-omp-v$Version-$arch.zip', PS1)

    def test_a_bitness_mismatch_is_refused(self):
        self.assertIn('$sourceArch -ne $arch', PS1)

    def test_it_installs_into_components_not_plugins(self):
        self.assertIn("$componentsDir = Join-Path $ServerDir 'components'", PS1)
        self.assertIn("Join-Path $tmp 'components/Cef.dll'", PS1)

    def test_it_drops_cef_from_the_plugins_line(self):
        self.assertIn("$_ -ne 'cef'", PS1)
        self.assertIn("server.cfg", PS1)

    def test_it_removes_the_stale_resource_cache(self):
        self.assertIn('scriptfiles/cef/railway.pak', PS1)

    def test_it_reports_the_installed_file_version(self):
        self.assertIn('FileVersionInfo', PS1)
        self.assertIn('Версия файла', PS1)

    def test_the_bat_runs_it_without_an_execution_policy_hassle(self):
        self.assertIn('-ExecutionPolicy Bypass', BAT)
        self.assertIn('install-cef-omp.ps1', BAT)


if __name__ == '__main__':
    unittest.main()
