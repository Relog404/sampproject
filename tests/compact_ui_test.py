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

    def test_the_panel_never_builds_text_draws(self):
        """Панель рисуется в браузере, поэтому тексдрав-раскладка (бывший
        RW_UI_BuildPanel) удалена: геометрия больше не на сервере."""
        self.assertNotIn('RW_UI_BuildPanel', self.ui)
        self.assertNotIn('gUILayoutMode', self.ui)
        self.assertIn('stock RW_UI_CefMode(mode)', self.ui)
        panel = self.ui.split('stock RW_UI_Panel(', 1)[1].split('\nstock ', 1)[0]
        self.assertNotIn('PlayerTextDraw', panel)

    def test_a_row_goes_to_the_browser_or_to_the_dialog(self):
        """Строка панели уходит в браузер, а без браузера — в нативный диалог:
        отдельных хитбоксов у строк больше нет."""
        row = self.ui.split('stock RW_UI_Row(', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('if (gUIOnCef[playerid]) { RW_CefRow(playerid, row, text, "", "bag"); return RW_UI_ScheduleCef(playerid); }', row)
        # страница рисуется один раз, когда все строки уже отправлены
        self.assertIn('SetTimerEx("RW_UI_FlushCef", 50, false, "i", playerid)', self.ui)
        self.assertIn('format(gUINativeRows[playerid][row], 128, "%s", text);', row)
        self.assertNotIn('PlayerTextDraw', row)

    def test_all_text_goes_to_the_browser_or_a_native_dialog(self):
        """Транслитерация под тексдравы удалена: кириллицу читает браузер
        и нативный диалог."""
        for marker in ('RW_TD_Encode', 'RW_TD_Label', 'RW_TD_Box', 'RW_UI_Text',
                       'CreatePlayerTextDraw'):
            self.assertNotIn(marker, self.ui)
        self.assertNotIn('gUILatin', self.ui)
        self.assertIn('gUIRussianDialogs[playerid] = true', self.main)
        commands = source('commands')
        self.assertIn('"tdlatin"', commands)
        self.assertIn('"tdrus"', commands)
        for text in ('Lock doors', 'Insert key', 'Passengers', 'Settings', 'Choose key'):
            self.assertIn(text, self.control)

    def test_auth_opens_in_the_browser_and_asks_the_password_in_a_dialog(self):
        """Вход: сначала страница в браузере, без браузера — нативный диалог в
        два шага (логин, потом пароль)."""
        auth = self.ui.split('stock RW_UI_OpenAuth(', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('RW_CefAuthOpen(', auth)
        self.assertIn('return RW_ShowLoginDialog(playerid);', auth)
        self.assertNotIn('RW_UI_Panel', auth)
        begin = self.ui.split('stock RW_UI_PasswordBegin', 1)[1].split('\n}', 1)[0]
        self.assertIn('gUIInput[playerid] = true', begin)
        account = source('accounts')
        self.assertIn('Регистрация 1/2', account)
        self.assertIn('Регистрация 2/2', account)
        self.assertIn('DIALOG_REGISTER_CONFIRM, DIALOG_STYLE_PASSWORD', account)

    def test_marker_is_saved_and_reusable_without_forced_teleport(self):
        click = self.main.split('public OnPlayerClickMap', 1)[1].split('public OnPlayerClickTextDraw', 1)[0]
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
