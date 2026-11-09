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
