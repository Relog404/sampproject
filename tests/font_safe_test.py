from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
def src(name):
    return (ROOT/'server/pawno/include/railway'/f'{name}.inc').read_text(encoding='cp1251')

class FontSafeInterfaceTest(unittest.TestCase):
    def setUp(self):
        self.ui = src('ui')
        self.main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')

    def test_raw_cyrillic_is_never_sent_to_gta_font(self):
        """Тексдравов больше нет: кириллица идёт в браузер и в нативные
        диалоги напрямую, транслитерация RW_TD_Encode удалена."""
        for marker in ('RW_TD_Encode', 'RW_TD_Label', 'RW_TD_Box', 'RW_UI_Text',
                       'CreatePlayerTextDraw', 'PlayerTextDrawSetString'):
            self.assertNotIn(marker, self.ui)
            self.assertNotIn(marker, self.main)
        self.assertNotIn('gUILatin', self.ui + self.main)

    def test_russian_default_uses_native_dialog_font_not_textdraw_font(self):
        self.assertIn('gUIRussianDialogs[playerid] = true', self.main)
        self.assertIn('DIALOG_STYLE_TABLIST_HEADERS', self.ui)
        self.assertIn('DIALOG_NATIVE_MENU, DIALOG_STYLE_MSGBOX', self.ui)
        self.assertIn('return RW_ShowLoginDialog(playerid)', self.ui.split('stock RW_UI_OpenAuth',1)[1])
        response = self.main.split('public OnDialogResponse',1)[1]
        self.assertLess(response.index('DIALOG_NATIVE_MENU'), response.index('if (gLogged[playerid])'))

    def test_native_render_is_batched_and_cancelled(self):
        self.assertIn('gUINativeTimer[playerid] == 0', self.ui)
        flush = self.ui.split('public RW_UI_FlushNative',1)[1].split('stock RW_UI_NativeResponse',1)[0]
        self.assertIn('!IsPlayerConnected(playerid)', flush)
        self.assertIn('gUIInput[playerid]', flush)
        for marker in ('stock RW_UI_Close', 'stock RW_UI_Reset'):
            body = self.ui.split(marker,1)[1].split('\n}',1)[0]
            self.assertIn('RW_UI_CancelNative(playerid)', body)

    def test_native_sparse_rows_are_mapped_and_map_cancel_does_not_teleport(self):
        self.assertIn('gUINativeMap[playerid][gUINativeCount[playerid]++] = i', self.ui)
        response = self.ui.split('stock RW_UI_NativeResponse',1)[1].split('stock RW_UI_Init',1)[0]
        self.assertIn('listitem < 0 || listitem >= gUINativeCount[playerid]', response)
        self.assertIn('RW_UI_Click(playerid, response ? 0 : 1)', response)
        self.assertIn('RW_UI_Click(playerid, gUINativeMap[playerid][listitem])', response)
        # Both views share existing server-side vehicle/teleport checks.
        control = src('interaction')
        self.assertIn('gMapPending[playerid] && gMapHasPoint[playerid] && row == 0', control)

    def test_old_commands_do_not_reenable_raw_font_output(self):
        """Команд переключения латиницы больше нет: /uitd и /tdlatin удалены,
        а старые алиасы просто напоминают про браузер."""
        commands = src('commands')
        for command in ('"tdrus"','"uiru"','"uinative"'):
            self.assertIn(command, commands)
        for command in ('"tdlatin"','"uitd"'):
            self.assertNotIn(command, commands)
        language = src('interaction').split('stock RW_UI_Language',1)[1]
        self.assertIn('gUIRussianDialogs[playerid] = true', language)
        self.assertNotIn('gUILatin', language)
        self.assertNotIn('latin ?', language)

if __name__ == '__main__':
    unittest.main()
