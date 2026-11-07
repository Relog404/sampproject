"""Settings: the interface switches and the key bindings.

Every setting here really changes something on the server: a component is
switched off, a key is rebound, a name tag is hidden. Nothing is drawn just
to look like a setting. There is no full-screen menu any more: /set, /key
and the Alt + H panel do the same job and work without a mouse.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CEF = ROOT / 'server/scriptfiles/cef/railway'


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


MENU = pawn('menu')
HUD = pawn('hud')
CITY = pawn('city')
UI = pawn('ui')
INTERACTION = pawn('interaction')
CORE = pawn('core')
CEFUI = pawn('cefui')
STUB = pawn('cefstub')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
CSS = (CEF / 'css/app.css').read_text(encoding='utf-8')
HUDPAGE = (CEF / 'hud.html').read_text(encoding='utf-8')
HUDJS = (CEF / 'js/hud.js').read_text(encoding='utf-8')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class SettingsTableTest(unittest.TestCase):
    """Settings survive a restart."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(MENU, 'CREATE TABLE IF NOT EXISTS settings') + ';')

    def test_flags_are_stored_for_a_player(self):
        self.db.execute('INSERT INTO settings (player_id,flags) VALUES (4,119)')
        self.assertEqual(self.db.execute('SELECT flags FROM settings WHERE player_id=4').fetchone(), (119,))

    def test_the_save_query_rewrites_the_row(self):
        save = sql(MENU, 'INSERT OR REPLACE INTO settings')
        self.db.execute(save.replace('%d,%d', '4,127'))
        self.db.execute(save.replace('%d,%d', '4,63'))
        self.assertEqual(self.db.execute('SELECT flags FROM settings WHERE player_id=4').fetchall(), [(63,)])


class SettingsServerTest(unittest.TestCase):
    """The server applies the settings and rebinds the keys."""

    def test_the_keys_really_open_things(self):
        self.assertIn('if ((newkeys & KEY_CROUCH) && !(oldkeys & KEY_CROUCH))', MAIN)
        self.assertIn('return RW_DonateCommand(playerid);', MAIN)
        self.assertIn('if ((newkeys & KEY_CTRL_BACK) && (newkeys & KEY_WALK) && !(oldkeys & KEY_CTRL_BACK))', MAIN)
        self.assertIn('return RW_SettingsCommand(playerid);', MAIN)
        self.assertIn('if (!strcmp(cmdtext, "/", true)) return RW_PhoneOpen(playerid);', CMDS)

    def test_every_setting_is_reachable_without_a_mouse(self):
        for command in ('set', 'key', 'settings'):
            self.assertIn('"%s", true' % command, CMDS)
        set_block = block(MENU, 'stock RW_SetCommand(playerid, const params[])')
        for name in ('hud', 'hints', 'speedo', 'names', 'markers', 'welcome'):
            self.assertIn('"%s", true' % name, set_block)
        self.assertIn('RW_MenuKeyFlip(playerid, slot)', block(MENU, 'stock RW_KeyCommand(playerid, const params[])'))

    def test_settings_really_change_the_game(self):
        apply_block = block(MENU, 'stock RW_SettingsApply(playerid)')
        self.assertIn('RW_HudShow(playerid, RW_SettingsOn(playerid, SET_HUD)', apply_block)
        self.assertIn('ShowPlayerNameTagForPlayer(playerid, i, RW_SettingsOn(playerid, SET_NAMES));', apply_block)
        self.assertIn('SetPlayerMarkerForPlayer(playerid, i, 0x00000000);', apply_block)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_HINTS))', HUD)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_SPEEDO)) return 0;', UI)
        self.assertIn('if (RW_SettingsOn(playerid, SET_WELCOME)) RW_CityWelcome(playerid);', CITY)

    def test_the_flags_are_defined_before_the_modules_that_read_them(self):
        for flag in ('SET_HUD', 'SET_HINTS', 'SET_SPEEDO', 'SET_NAMES', 'SET_MARKERS', 'SET_WELCOME', 'SET_DEFAULT'):
            self.assertIn('#define %s' % flag, CORE)
            self.assertNotIn('#define %s ' % flag, MENU)

    def test_rebinding_takes_a_free_key_only(self):
        flip = block(MENU, 'stock RW_MenuKeyFlip(playerid, slot)')
        self.assertIn('static const keys[] = { KEY_YES, KEY_NO, KEY_CTRL_BACK, KEY_WALK };', flip)
        self.assertIn('Эта клавиша уже занята другим действием.', flip)
        self.assertIn('gControlKeys[playerid][slot] = next;', flip)

    def test_rebinding_does_not_reopen_a_menu(self):
        flip = block(MENU, 'stock RW_MenuKeyFlip(playerid, slot)')
        self.assertIn('return RW_Ok(playerid, message);', flip)
        self.assertNotIn('RW_MenuOpen', flip)

    def test_the_settings_panel_is_a_small_panel(self):
        self.assertIn('return RW_UI_Settings(playerid);', block(MENU, 'stock RW_SettingsCommand(playerid)'))
        self.assertIn('stock RW_UI_Settings(playerid)', INTERACTION)


class NoFullScreenMenuTest(unittest.TestCase):
    """Ни F3-меню, ни F2-доната: браузер больше не ловит F-клавиши."""

    def test_the_second_browser_is_gone(self):
        self.assertNotIn('CEF_UI_BIGMENU', CEFUI)
        self.assertNotIn('CEF_UI_BIGMENU', STUB)
        self.assertNotIn('CEF_UI_BIGMENU', CORE)
        self.assertNotIn('CEF_UI_MENU_URL2', CEFUI)
        for name in ('RW_CefMenuOpen', 'RW_CefMenuRow', 'RW_CefMenuEnd',
                     'RW_CefMenuFocus', 'RW_CefMenuClose'):
            self.assertNotIn(name, CEFUI)
            self.assertNotIn(name, STUB)

    def test_the_page_and_its_script_are_gone(self):
        self.assertNotIn('menu:action', CEFUI)
        self.assertNotIn('menu:action', MENU)
        self.assertFalse((CEF / 'menu.html').exists())
        self.assertFalse((CEF / 'js' / 'menu.js').exists())

    def test_the_hud_page_forgets_f2_and_f3(self):
        self.assertNotIn("e.key === 'F2'", HUDJS)
        self.assertNotIn("e.key === 'F3'", HUDJS)
        self.assertNotIn('hud:menu', HUDJS)
        self.assertNotIn('hud:donate', HUDJS)
        self.assertNotIn('RW_HudMenuEvent', HUD)
        self.assertNotIn('RW_HudDonateEvent', HUD)

    def test_no_key_or_command_opens_a_menu(self):
        for marker in ('RW_MenuToggle', 'RW_MenuOpen', 'RW_MenuClose',
                       'RW_MenuAction', 'RW_MenuCommand', 'gMenuShown',
                       'gMenuSection', 'MENU_MAIN', 'MENU_SETTINGS', 'MENU_DONATE'):
            self.assertNotIn(marker, MENU)
            self.assertNotIn(marker, MAIN)
            self.assertNotIn(marker, CMDS)

    def test_the_hints_stopped_mentioning_f2(self):
        self.assertNotIn('F2:donate;', HUD)
        self.assertNotIn("donate: 'Донат'", HUDJS)
        self.assertNotIn('F2', HUDPAGE)

    def test_donate_still_has_its_key_and_command(self):
        self.assertIn('return RW_DonateCommand(playerid);', MAIN)
        self.assertIn('"donate", true', CMDS)


class HudMouseTest(unittest.TestCase):
    """Мышь включается клавишей и с самой страницы: курсор нужен для клика."""

    def test_the_player_freezes_while_the_cursor_is_on(self):
        self.assertIn('TogglePlayerControllable(playerid, !gHudMouse[playerid]);', HUD)

    def test_the_mouse_toggles_from_the_page_too(self):
        self.assertIn("RW.emit('hud:mousetoggle'", HUDJS)
        self.assertIn('CEF_RegisterEvent("hud:mousetoggle", "RW_HudMouseEvent", Argument_Integer);', CEFUI)
        self.assertIn('public RW_HudMouseEvent(playerid, browserid, on)', HUD)

    def test_the_bonus_card_is_gone(self):
        """Карточки больше нет: ни события, ни картинки, ни стилей."""
        self.assertNotIn('hud:bonusclaim', HUDJS)
        self.assertNotIn('hud:bonusclaim', CEFUI)
        self.assertNotIn('RW_HudNextSeconds', HUD)
        self.assertNotIn('id="bonusArt"', HUDPAGE)
        self.assertNotIn('.bonus-art {', CSS)
        self.assertNotIn('.bonus-go.done', CSS)


if __name__ == '__main__':
    unittest.main()
