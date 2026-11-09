"""open.mp compatibility: the gamemode must run there without a rebuild.

The same AMX runs on SA-MP 0.3.7 and on open.mp. These tests pin down
the handful of host assumptions the scripts are allowed to make; the
rest of the gamemode must stay host-agnostic.
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
CEFUI = pawn('cefui')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
SOURCES = {'core.inc': CORE, 'ui.inc': UI, 'accounts.inc': ACCOUNTS, 'railway.pwn': MAIN}


class TimerIdTest(unittest.TestCase):
    """0 means "no timer" on SA-MP and on open.mp alike.

    open.mp numbers timers from one (timers.hpp: `idx = 1`) and returns
    0 only when the timer could not be created, so a stored id can be
    handed straight to KillTimer. Verified by reading the source -- keep
    it that way instead of introducing a host-specific offset.
    """

    def test_no_timer_id_is_ever_shifted(self):
        for name, src in SOURCES.items():
            self.assertNotIn('RW_TimerArm', src, name)
            self.assertNotIn('RW_TimerStop', src, name)

    def test_a_stored_timer_is_killed_as_is(self):
        for name, src in SOURCES.items():
            for token in ('gAuthTimer', 'gUINativeTimer', 'gUICefTimer'):
                self.assertNotIn(f'KillTimer({token}[playerid] - 1)', src, name)
                self.assertNotIn(f'KillTimer({token}[playerid] + 1)', src, name)

    def test_the_zero_guards_are_intact(self):
        self.assertIn('if (gUINativeTimer[playerid] == 0)', UI)
        self.assertIn('if (gUICefTimer[playerid] == 0)', UI)
        self.assertIn('if (gAuthTimer[playerid] != 0)', ACCOUNTS)
        self.assertIn('if (gAuthTimer[playerid] != 0)', MAIN)

    def test_a_cancelled_timer_clears_its_slot(self):
        """Otherwise the next KillTimer would hit a reused id."""
        self.assertIn('KillTimer(gAuthTimer[playerid]);', ACCOUNTS)
        self.assertIn('gAuthTimer[playerid] = 0;', ACCOUNTS)
        self.assertIn('KillTimer(gAuthTimer[playerid]);', MAIN)
        self.assertIn('gAuthTimer[playerid] = 0;', MAIN)
        self.assertIn('KillTimer(gUINativeTimer[playerid]);', UI)
        self.assertIn('gUINativeTimer[playerid] = 0;', UI)
        self.assertIn('KillTimer(gUICefTimer[playerid]);', UI)
        self.assertIn('gUICefTimer[playerid] = 0;', UI)


class HostAgnosticTest(unittest.TestCase):
    """Nothing in the gamemode may depend on the host being SA-MP."""

    def test_the_database_is_opened_by_a_relative_name(self):
        """Both hosts resolve a relative name inside scriptfiles/."""
        self.assertIn('db_open("railway.db")', ACCOUNTS)
        self.assertNotIn('gamemodes/', ACCOUNTS)

    def test_the_browser_state_lives_outside_the_cef_module(self):
        """The plugin comes and goes; the state the rest of the gamemode
        reads must compile in both builds (RAILWAY_CEF 0 and 1)."""
        self.assertIn('new bool:gCefEnabled;', CORE)
        self.assertIn('new bool:gCefReady[MAX_PLAYERS];', CORE)
        self.assertIn('new bool:gCefPong[MAX_PLAYERS];', CORE)

    def test_a_silent_browser_falls_back_to_native_dialogs(self):
        """On a host whose CEF never delivers events, the game must go on."""
        self.assertIn('the page never answered', CEFUI)
        self.assertIn('SA-MP builds before 1.4.0 deliver no browser events', CEFUI)
        self.assertIn('Update server/plugins/cef.dll', CEFUI)


if __name__ == '__main__':
    unittest.main()
