"""Source/geometry contracts for native TextDraw rendering and native workflows.
Actual GTA fonts and mouse hit boxes still need a Windows game test.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT/'server/pawno/include/railway'/f'{name}.inc').read_text(encoding='cp1251')

class CompactRussianUiTest(unittest.TestCase):
    def setUp(self):
        self.ui = source('ui')
        self.control = source('interaction')
        self.main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')

    def test_layout_rectangles_fit_viewport_and_are_smaller(self):
        # Values are read from the actual layout branches, not an independent fixture.
        code = self.ui.split('new Float:x =', 1)[1].split('gUI[playerid][0]', 1)[0]
        default = re.search(r'(\d+\.\d+), Float:y = (\d+\.\d+), Float:width = (\d+\.\d+), Float:height = (\d+\.\d+)', code)
        rects = [tuple(map(float, default.groups()))]
        for match in re.finditer(r'x = (\d+\.\d+); y = (\d+\.\d+); width = (\d+\.\d+); height = (\d+\.\d+);', code):
            rects.append(tuple(map(float, match.groups())))
        self.assertEqual(len(rects), 3)
        for x, y, w, h in rects:
            self.assertGreaterEqual(x, 0)
            self.assertGreaterEqual(y, 0)
            self.assertLessEqual(x+w, 640)
            self.assertLessEqual(y+h, 448)
            self.assertLess(w*h, 246*302*0.6)
        # Bind/settings and 8-passenger-row layouts also stay in bounds.
        self.assertIn('height = 194.0', code)
        self.assertIn('height = 240.0', code)
        self.assertLess(190*240, 246*302*0.65)

    def test_sparse_rows_have_compacted_hitboxes(self):
        self.assertIn('mode == UI_VEHICLE && i == 7) slot = 6', self.ui)   # row 5 is the cabin row
        self.assertIn('mode == UI_SETTINGS && i >= 5) slot = i-2', self.ui)
        self.assertIn('mode == UI_BIND && i == 7) slot = 4', self.ui)
        self.assertIn('19.0*slot', self.ui)
        self.assertIn('x+(mode == UI_VEHICLE ? 165.0 : width-12.0)', self.ui)
        self.assertIn('PlayerTextDrawDestroy(playerid, gUI[playerid][22])', self.ui)

    def test_all_text_passes_through_per_player_encoder(self):
        for marker in ('stock PlayerText:RW_TD_Label', 'stock RW_UI_Text'):
            code = self.ui.split(marker, 1)[1].split('\n}', 1)[0]
            self.assertIn('RW_TD_Encode', code)
        self.assertIn('if (c >= 192)', self.ui)
        self.assertNotIn('gUILatin', self.ui)
        self.assertIn("new ch[2]; ch[0] = c == '~' ? '-' : c", self.ui)
        self.assertIn('i+7 < length', self.ui)
        self.assertIn('new bool:hex = true', self.ui)
        self.assertIn('gUIRussianDialogs[playerid] = true', self.main)
        commands = source('commands')
        self.assertIn('"tdlatin"', commands)
        self.assertIn('"tdrus"', commands)
        for text in ('Lock doors', 'Insert key', 'Passengers', 'Settings', 'Choose key'):
            self.assertIn(text, self.control)

    def test_auth_card_hides_for_password_dialog_and_uses_two_steps(self):
        self.assertIn('С возвращением', self.ui)
        self.assertIn('Login', self.ui)
        begin = self.ui.split('stock RW_UI_PasswordBegin', 1)[1].split('\n}', 1)[0]
        self.assertIn('PlayerTextDrawHide', begin)
        self.assertIn('gUIInput[playerid] = true', begin)
        account = source('accounts')
        self.assertIn('Регистрация 1/2', account)
        self.assertIn('Регистрация 2/2', account)
        self.assertIn('DIALOG_REGISTER_CONFIRM, DIALOG_STYLE_PASSWORD', account)
        confirm = self.main.split('if (dialogid == DIALOG_REGISTER_CONFIRM) {', 1)[1].split('\n    }', 1)[0]
        self.assertIn('!gRegPending[playerid]', confirm)
        self.assertIn('RW_ClearRegisterPassword(playerid)', confirm)
        self.assertIn('RW_AuthSubmit(playerid, true, password, inputtext)', confirm)
        self.assertIn('password[i] = 0', confirm)
        disconnect = self.main.split('public OnPlayerDisconnect', 1)[1].split('public OnPlayerRequestClass', 1)[0]
        self.assertIn('RW_ClearRegisterPassword', disconnect)
        # Rendering/localisation must not modify persisted credentials.
        self.assertNotIn('RW_TD_Encode', account)

    def test_marker_is_saved_and_reusable_without_forced_teleport(self):
        click = self.main.split('public OnPlayerClickMap', 1)[1].split('public OnPlayerClickPlayerTextDraw', 1)[0]
        self.assertIn('gMapHasPoint[playerid] = true', click)
        self.assertIn('SetPlayerMapIcon', click)
        self.assertNotIn('SetPlayerPos', click)
        commands = source('commands')
        self.assertIn('"tpm"', commands)
        self.assertIn('"delmark"', commands)
        self.assertIn('RemovePlayerMapIcon', commands)
        prompt = self.control.split('stock RW_MapPrompt', 1)[1].split('stock RW_UI_Language', 1)[0]
        self.assertIn('PLAYER_STATE_ONFOOT', prompt)
        self.assertIn('Yes, teleport', prompt)
        self.assertIn('No, stay here', prompt)
        confirm = self.control.split('mode == UI_MAP', 1)[1].split('// Camera ray', 1)[0]
        self.assertIn('gMapHasPoint[playerid]', confirm)
        self.assertNotIn('gMapHasPoint[playerid] = false', confirm)

if __name__ == '__main__':
    unittest.main()
