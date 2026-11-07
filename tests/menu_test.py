"""The full-screen menu on F3, its settings and the fixed bonus card.

Every setting here really changes something on the server: a component is
switched off, a key is rebound, a name tag is hidden. Nothing is drawn just
to look like a setting.
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
CORE = pawn('core')
CEFUI = pawn('cefui')
STUB = pawn('cefstub')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
PAGE = (CEF / 'menu.html').read_text(encoding='utf-8')
SCRIPT = (CEF / 'js/menu.js').read_text(encoding='utf-8')
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


class MenuPageTest(unittest.TestCase):
    """The page: logo, the quit tab and the rows."""

    def test_the_logo_sits_in_the_top_left_corner(self):
        self.assertIn('class="menu-logo"', PAGE)
        self.assertIn('img/logo.png', PAGE)
        self.assertTrue(PAGE.index('menu-logo') < PAGE.index('menu-sheet'))

    def test_the_quit_button_is_the_same_width_as_the_tabs(self):
        self.assertIn('class="menu-tab danger" data-act="quit"', PAGE)
        self.assertIn('Выйти из игры', PAGE)
        self.assertNotIn('menu-quit', PAGE)
        self.assertNotIn('.menu-quit {', CSS)
        self.assertIn('.menu-tab.danger {', CSS)

    def test_the_tabs_and_the_keys_are_there(self):
        for label in ('Вернуться в игру', 'Настройки', 'Донат', 'Выйти из игры'):
            self.assertIn(label, PAGE)
        self.assertIn("e.key === 'Escape'", SCRIPT)
        self.assertIn("RW.emit('menu:action'", SCRIPT)

    def test_the_row_handler_matches_what_the_server_sends(self):
        # Сервер шлёт три аргумента, поэтому и обработчик берёт три.
        self.assertIn("RW.on('menu:row', function (text, action, style)", SCRIPT)
        self.assertIn("CEF_EmitEvent(playerid, CEF_UI_BIGMENU, \"menu:row\", CEF_STR(text), CEF_STR(action), CEF_STR(style));", CEFUI)
        self.assertNotIn("function (id, text, action, style)", SCRIPT)

    def test_the_background_is_transparent(self):
        self.assertIn('html, body { background: transparent; }', CSS)
        self.assertIn('.menu { position: fixed; inset: 0;', CSS)
        self.assertIn('background: transparent; transition: opacity', CSS)
        self.assertNotIn('radial-gradient', CSS)

    def test_the_keys_really_open_things(self):
        self.assertIn('if ((newkeys & KEY_CROUCH) && !(oldkeys & KEY_CROUCH))', MAIN)
        self.assertIn('return RW_DonateCommand(playerid);', MAIN)
        self.assertIn('if ((newkeys & KEY_CTRL_BACK) && (newkeys & KEY_WALK) && !(oldkeys & KEY_CTRL_BACK))', MAIN)
        self.assertIn('return RW_SettingsCommand(playerid);', MAIN)
        self.assertIn('if (!strcmp(cmdtext, "/", true)) return RW_PhoneOpen(playerid);', CMDS)

    def test_clicks_reach_the_page_and_not_the_game(self):
        # Пока меню открыто, персонаж стоит: иначе клик уходит в игру.
        self.assertIn('TogglePlayerControllable(playerid, false);', MENU)
        self.assertIn('if (!gHudMouse[playerid]) TogglePlayerControllable(playerid, true);', MENU)
        # Страница успевает прогрузиться: фокус повторяется.
        self.assertIn('SetTimerEx("RW_MenuFocus", 300, false, "i", playerid);', MENU)
        self.assertIn('public RW_MenuFocus(playerid)', MENU)
        self.assertIn('CEF_FocusBrowser(playerid, CEF_UI_BIGMENU, true);', CEFUI)
        self.assertIn('stock RW_CefMenuFocus(playerid)', STUB)

    def test_every_setting_is_reachable_without_a_mouse(self):
        for command in ('set', 'key', 'menu', 'settings'):
            self.assertIn('"%s", true' % command, CMDS)
        set_block = block(MENU, 'stock RW_SetCommand(playerid, const params[])')
        for name in ('hud', 'hints', 'bonus', 'speedo', 'names', 'markers', 'welcome'):
            self.assertIn('"%s", true' % name, set_block)
        self.assertIn('RW_MenuKeyFlip(playerid, slot)', block(MENU, 'stock RW_KeyCommand(playerid, const params[])'))

    def test_the_rows_come_from_the_server(self):
        self.assertIn("RW.on('menu:open'", SCRIPT)
        self.assertIn("RW.on('menu:row'", SCRIPT)
        self.assertIn("RW.on('menu:close'", SCRIPT)


class MenuServerTest(unittest.TestCase):
    """The server drives the menu and applies the settings."""

    def test_the_browser_is_created_and_destroyed(self):
        self.assertIn('#define CEF_UI_BIGMENU      (105)', CEFUI)
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_BIGMENU, CEF_UI_MENU_URL2, false, true);', CEFUI)
        self.assertIn('CEF_DestroyBrowser(playerid, CEF_UI_BIGMENU);', CEFUI)
        self.assertIn('CEF_RegisterEvent("menu:action", "RW_MenuAction", Argument_String);', CEFUI)
        self.assertIn('stock RW_CefMenuOpen(', STUB)

    def test_the_menu_opens_on_f3_or_alt_n(self):
        self.assertIn("RW.emit('hud:menu', 1)", HUDJS)
        self.assertIn("e.key === 'F3'", HUDJS)
        self.assertIn('if ((newkeys & KEY_NO) && (newkeys & KEY_WALK) && !(oldkeys & KEY_NO))', MAIN)
        self.assertIn('return RW_MenuToggle(playerid);', MAIN)
        self.assertIn('forward RW_MenuToggle(playerid);', CORE)
        self.assertIn('"menu", true', CMDS)
        self.assertIn('"settings", true', CMDS)

    def test_the_actions_are_handled_on_the_server(self):
        action = block(MENU, 'public RW_MenuAction(playerid, browserid, const action[])')
        self.assertIn('"resume"', action)
        self.assertIn('"quit"', action)
        self.assertIn('"settings"', action)
        self.assertIn('"donate"', action)
        self.assertIn('RW_SettingsFlip(playerid, strval(action[4]))', action)
        self.assertIn('RW_MenuKeyFlip(playerid, strval(action[4]))', action)
        self.assertIn('RW_DonateBuy(playerid, strval(action[4]))', action)

    def test_settings_really_change_the_game(self):
        apply_block = block(MENU, 'stock RW_SettingsApply(playerid)')
        self.assertIn('RW_HudShow(playerid, RW_SettingsOn(playerid, SET_HUD)', apply_block)
        self.assertIn('ShowPlayerNameTagForPlayer(playerid, i, RW_SettingsOn(playerid, SET_NAMES));', apply_block)
        self.assertIn('SetPlayerMarkerForPlayer(playerid, i, 0x00000000);', apply_block)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_HINTS))', HUD)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_BONUS))', HUD)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_SPEEDO)) return 0;', UI)
        self.assertIn('if (RW_SettingsOn(playerid, SET_WELCOME)) RW_CityWelcome(playerid);', CITY)

    def test_the_flags_are_defined_before_the_modules_that_read_them(self):
        for flag in ('SET_HUD', 'SET_HINTS', 'SET_BONUS', 'SET_SPEEDO', 'SET_NAMES', 'SET_MARKERS', 'SET_WELCOME', 'SET_DEFAULT'):
            self.assertIn('#define %s' % flag, CORE)
            self.assertNotIn('#define %s ' % flag, MENU)

    def test_rebinding_takes_a_free_key_only(self):
        flip = block(MENU, 'stock RW_MenuKeyFlip(playerid, slot)')
        self.assertIn('static const keys[] = { KEY_YES, KEY_NO, KEY_CTRL_BACK, KEY_WALK };', flip)
        self.assertIn('Эта клавиша уже занята другим действием.', flip)
        self.assertIn('gControlKeys[playerid][slot] = next;', flip)


class BonusCardTest(unittest.TestCase):
    """The card is bigger, collapses and its button really clicks."""

    def test_the_claim_sends_its_own_event(self):
        self.assertIn("RW.emit('hud:bonusclaim', 1)", HUDJS)
        self.assertIn('CEF_RegisterEvent("hud:bonusclaim", "RW_HudBonusClick", Argument_Integer);', CEFUI)
        self.assertNotIn("RW.emit('hud:bonus', 1)", HUDJS)

    def test_after_the_claim_the_button_shows_the_countdown(self):
        self.assertIn('"next"', HUD)
        self.assertIn('RW_HudNextSeconds(playerid)', HUD)
        self.assertIn("next: function ()", HUDJS)
        self.assertIn("bonusGo.textContent = 'Всё'", HUDJS)
        self.assertIn("bonusGo.disabled = true;", HUDJS)
        self.assertIn("startTimer(day, card.text);", HUDJS)
        self.assertIn('.bonus-go.done', CSS)

    def test_the_player_freezes_while_the_cursor_is_on(self):
        self.assertIn('TogglePlayerControllable(playerid, !gHudMouse[playerid]);', HUD)

    def test_the_card_collapses_and_comes_back(self):
        self.assertIn('function setMin(on)', HUDJS)
        self.assertIn("bonus.classList.toggle('min', on)", HUDJS)
        self.assertIn("setMin(false)", HUDJS)
        self.assertNotIn('bonusClose.addEventListener(\'click\', function () {\n    bonus.classList.add(\'off\');', HUDJS)
        self.assertIn('.bonus.min .bonus-body { display: none; }', CSS)

    def test_the_card_grew_and_got_an_image(self):
        self.assertIn('id="bonusArt"', HUDPAGE)
        self.assertIn('img/bonus.png', HUDPAGE)
        self.assertIn('.bonus-art {', CSS)
        self.assertIn('width: 300px', CSS)
        self.assertTrue((CEF / 'img' / 'bonus.png').exists())
        self.assertTrue((CEF / 'img' / 'logo.png').exists())

    def test_the_mouse_toggles_from_the_page_too(self):
        self.assertIn("RW.emit('hud:mousetoggle'", HUDJS)
        self.assertIn('CEF_RegisterEvent("hud:mousetoggle", "RW_HudMouseEvent", Argument_Integer);', CEFUI)
        self.assertIn('public RW_HudMouseEvent(playerid, browserid, on)', HUD)


if __name__ == '__main__':
    unittest.main()
