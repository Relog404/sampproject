"""Cursor state/event regression model and matching Pawn source contracts.
Does not emulate SA-MP networking or prove client rendering.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT/'server/pawno/include/railway'/f'{name}.inc').read_text(encoding='cp1251')

class CursorModel:
    def __init__(self):
        self.selecting = False
        self.mode = None
        self.expected = 0
        self.echoes = []

    def close(self, keep=False):
        self.mode = None
        if not keep and self.selecting:
            self.selecting = False
            self.expected += 1
            self.echoes.append('cancel')

    def open(self, mode):
        self.close(keep=True)
        self.mode = mode
        self.selecting = True

    def invalid_click(self):
        if self.expected:
            self.expected -= 1
            return
        if not self.selecting:
            return
        self.selecting = False
        self.close()

class CursorLifecycleTest(unittest.TestCase):
    def test_opening_inactive_inventory_does_not_emit_cancel(self):
        state = CursorModel(); state.open('inventory')
        self.assertEqual(state.echoes, [])
        self.assertTrue(state.selecting)
        self.assertEqual(state.mode, 'inventory')

    def test_switching_inventory_to_garage_keeps_cursor_without_cancel(self):
        state = CursorModel(); state.open('inventory'); state.open('garage')
        self.assertEqual(state.echoes, [])
        self.assertTrue(state.selecting)
        self.assertEqual(state.mode, 'garage')

    def test_real_escape_closes_once_without_another_cancel(self):
        state = CursorModel(); state.open('inventory'); state.invalid_click()
        self.assertFalse(state.selecting)
        self.assertIsNone(state.mode)
        self.assertEqual(state.echoes, [])

    def test_late_programmatic_cancel_does_not_close_reopened_inventory(self):
        state = CursorModel(); state.open('inventory'); state.close(); state.open('inventory')
        state.invalid_click()  # Delayed echo of the explicit Close button.
        self.assertEqual(state.mode, 'inventory')
        self.assertTrue(state.selecting)
        state.invalid_click()  # Actual Esc from the current screen.
        self.assertIsNone(state.mode)
        self.assertFalse(state.selecting)

    def test_source_matches_selection_and_transition_guards(self):
        ui = source('ui')
        end = ui.split('stock RW_UI_SelectEnd',1)[1].split('stock RW_UI_ScheduleNative',1)[0]
        self.assertLess(end.index('if (!gUISelecting[playerid]) return 1'), end.index('CancelSelectTextDraw'))
        self.assertLess(end.index('gUISelecting[playerid] = false'), end.index('CancelSelectTextDraw'))
        self.assertLess(end.index('gUIExpectedCancels[playerid]++'), end.index('CancelSelectTextDraw'))
        close = ui.split('stock RW_UI_Close(',1)[1].split('stock RW_UI_OpenAuth',1)[0]
        self.assertIn('if (!keepSelection) RW_UI_SelectEnd(playerid)', close)
        for module, marker in [('garage','stock RW_GarageOpen')]:
            opening = source(module).split(marker,1)[1].split('\nstock ',1)[0]
            self.assertIn('RW_UI_Close(playerid, true)', opening)
            self.assertIn('RW_UI_SelectBegin(playerid)', opening)
        callback = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251').split('public OnPlayerClickTextDraw',1)[1].split('public OnPlayerKeyStateChange',1)[0]
        self.assertLess(callback.index('gUIExpectedCancels[playerid] > 0'), callback.index('gUISelecting[playerid] = false'))
        self.assertLess(callback.index('gUISelecting[playerid] = false'), callback.index('return RW_UI_Close'))
        reset = ui.split('stock RW_UI_Reset',1)[1]
        self.assertIn('gUISelecting[playerid] = false', reset)
        self.assertIn('gUIExpectedCancels[playerid] = 0', reset)

if __name__ == '__main__':
    unittest.main()
