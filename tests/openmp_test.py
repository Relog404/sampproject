"""open.mp compatibility: the gamemode must run there without a rebuild.

The same Pawn scripts run on SA-MP 0.3.7 and on open.mp, but the two hosts
disagree on one thing the gamemode stores: timer ids. SA-MP numbers them
from one, open.mp from zero, and `0` is a live timer there, not "nothing".
Everything here keeps that difference out of the game logic.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'server/pawno/include/railway'


def pawn(name):
    return (INC / f'{name}.inc').read_text(encoding='cp1251')


CORE = pawn('core')
UI = pawn('ui')
ACCOUNTS = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
SOURCES = {'core.inc': CORE, 'ui.inc': UI, 'accounts.inc': ACCOUNTS, 'railway.pwn': MAIN}


class TimerIdTest(unittest.TestCase):
    """A stored timer id is shifted by one, so 0 always means "no timer"."""

    def test_the_shift_lives_in_one_place(self):
        self.assertIn('stock RW_TimerArm(&slot, id)', CORE)
        self.assertIn('slot = id + 1;', CORE)
        self.assertIn('stock RW_TimerStop(&slot)', CORE)
        self.assertIn('KillTimer(slot - 1);', CORE)
        self.assertIn('slot = 0;', CORE)

    def test_every_stored_timer_goes_through_the_helpers(self):
        self.assertIn('RW_TimerArm(gUINativeTimer[playerid], SetTimerEx("RW_UI_FlushNative"', UI)
        self.assertIn('RW_TimerStop(gUINativeTimer[playerid]);', UI)
        self.assertIn('RW_TimerArm(gUICefTimer[playerid], SetTimerEx("RW_UI_FlushCef"', UI)
        self.assertIn('RW_TimerStop(gUICefTimer[playerid]);', UI)
        self.assertIn('RW_TimerArm(gAuthTimer[playerid], SetTimerEx("RW_AuthCamera"', ACCOUNTS)
        self.assertTrue(ACCOUNTS.count('RW_TimerStop(gAuthTimer[playerid]);') >= 2)

    def test_no_stored_timer_is_killed_by_hand(self):
        """KillTimer(id) with a raw stored id is the bug this prevents."""
        for name, src in SOURCES.items():
            self.assertNotIn('KillTimer(gAuthTimer', src, name)
            self.assertNotIn('KillTimer(gUINativeTimer', src, name)
            self.assertNotIn('KillTimer(gUICefTimer', src, name)

    def test_the_flush_helpers_still_guard_on_zero(self):
        self.assertIn('if (gUINativeTimer[playerid] == 0)', UI)
        self.assertIn('if (gUICefTimer[playerid] == 0)', UI)


class HostAgnosticTest(unittest.TestCase):
    """Nothing in the gamemode may depend on the host being SA-MP."""

    def test_the_database_is_opened_by_a_relative_name(self):
        """Both hosts resolve a relative name inside scriptfiles/."""
        self.assertIn('db_open("railway.db")', ACCOUNTS)
        self.assertNotIn('gamemodes/', ACCOUNTS)

    def test_the_browser_state_lives_outside_the_cef_module(self):
        """Plugins come and go; the state the rest of the gamemode reads
        must compile in both builds (RAILWAY_CEF 0 and 1)."""
        self.assertIn('new bool:gCefEnabled;', CORE)
        self.assertIn('new bool:gCefReady[MAX_PLAYERS];', CORE)
        self.assertIn('new bool:gCefPong[MAX_PLAYERS];', CORE)


if __name__ == '__main__':
    unittest.main()
