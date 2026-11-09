"""The handshake verdict must be readable from the server log.

When the browser stays silent, the only way to tell "wrong client" from
"client never answered" from "key exchange failed" is the reason code
omp-cef reports in OnCefInitialize. Both outcomes get logged, because a
log line that appears only on failure cannot distinguish "not reached"
from "worked".
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CEFUI = (ROOT / 'server/pawno/include/railway' / 'cefui.inc').read_bytes().decode('cp1251')
CEF_INC = (ROOT / 'server/pawno/include' / 'cef.inc').read_bytes().decode('cp1251')
BRIDGE = (ROOT / 'server/scriptfiles/cef/railway' / 'js' / 'bridge.js').read_text(encoding='utf-8')
CORE = (ROOT / 'server/pawno/include/railway' / 'core.inc').read_bytes().decode('cp1251')


class HandshakeLogTest(unittest.TestCase):
    def test_success_is_logged_too(self):
        self.assertIn('[cef] player %d: init ok (%s)', CEFUI)

    def test_failure_still_reports_the_message(self):
        self.assertIn('[cef] player %d: init failed (%s): %s', CEFUI)

    def test_every_reason_code_gets_a_name(self):
        for code in ('CEF_INIT_OK', 'CEF_INIT_TIMEOUT', 'CEF_INIT_VERSION_MISMATCH',
                     'CEF_INIT_HANDSHAKE_FAILED', 'CEF_INIT_IP_MISMATCH'):
            self.assertIn(code, CEF_INC, code)
            self.assertIn(f'case {code}:', CEFUI, code)

    def test_unknown_reasons_do_not_print_an_empty_bracket(self):
        self.assertIn('default:', CEFUI)
        self.assertIn('format(cause, sizeof cause, "unknown")', CEFUI)


if __name__ == '__main__':
    unittest.main()


class PageScriptProbeTest(unittest.TestCase):
    """Separates "the page never ran" from "events do not come back".

    The ping waits for a pong, so a blank page and a broken event path look
    identical from the outside. A page that says hello on load distinguishes
    them: no hello means the script never executed at all.
    """

    def test_the_page_greets_the_server(self):
        self.assertIn("RW.emit('ui:hello')", BRIDGE)

    def test_the_hello_fires_once_the_bridge_exists(self):
        self.assertIn('if (helloed || !RW.ready()) return;', BRIDGE)
        self.assertIn('hello();', BRIDGE)

    def test_the_server_listens_for_it(self):
        self.assertIn('CEF_RegisterEvent("ui:hello", "RW_CefHello")', CEFUI)
        self.assertIn('[cef] player %d: page script is running (ui:hello)', CEFUI)

    def test_it_is_logged_once_per_player(self):
        self.assertIn('if (gCefHello[playerid]) return 1;', CEFUI)
        self.assertIn('gCefHello[playerid] = true;', CEFUI)

    def test_a_reconnect_clears_the_flag(self):
        self.assertIn('gCefHello[playerid] = false;', CEFUI)

    def test_the_flag_is_declared(self):
        self.assertIn('new bool:gCefHello[MAX_PLAYERS];', CORE)

    def test_the_client_side_is_announced_too(self):
        """Otherwise "no pong" and "no client at all" look the same."""
        self.assertIn('[cef] player %d: client CEF detected; browsers created', CEFUI)
