"""Authorization: the browser card, its Russian text and the way back.

The server decides who stands in front of it and never trusts the page:
the page sends the password, everything else is checked in Pawn.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CEF = ROOT / 'server/scriptfiles/cef/railway'


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


UI = pawn('ui')
ACCOUNTS = pawn('accounts')
CEFUI = pawn('cefui')
STUB = pawn('cefstub')
PAGE = (CEF / 'auth.html').read_text(encoding='utf-8')
SCRIPT = (CEF / 'js/auth.js').read_text(encoding='utf-8')
CSS = (CEF / 'css/app.css').read_text(encoding='utf-8')


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class AuthPageTest(unittest.TestCase):
    """The card itself: fields, captions and what it sends."""

    def test_the_card_holds_two_password_fields_and_a_button(self):
        self.assertIn('id="password"', PAGE)
        self.assertIn('id="repeat"', PAGE)
        self.assertIn('id="go"', PAGE)
        self.assertIn('type="password"', PAGE)

    def test_the_card_speaks_russian(self):
        for word in ('Железная дорога', 'Unity Station', 'Войти', 'Создать аккаунт',
                     'Повторите пароль', 'Пароли не совпадают'):
            self.assertIn(word, PAGE + SCRIPT)

    def test_the_repeat_field_is_hidden_for_a_known_player(self):
        self.assertIn("el.repeatWrap.classList.toggle('off', !register);", SCRIPT)

    def test_the_password_goes_to_the_server_and_nowhere_else(self):
        self.assertIn("RW.emit('auth:submit', pass,", SCRIPT)
        self.assertNotIn('localStorage', SCRIPT)
        self.assertNotIn('document.cookie', SCRIPT)

    def test_the_card_is_opened_and_closed_by_the_server(self):
        self.assertIn("RW.on('auth:open'", SCRIPT)
        self.assertIn("RW.on('auth:close'", SCRIPT)

    def test_the_style_matches_the_rest_of_the_interface(self):
        self.assertIn('.auth-card {', CSS)
        self.assertIn('backdrop-filter', CSS)


class AuthServerTest(unittest.TestCase):
    """The server side of the card."""

    def test_the_browser_is_created_once_and_destroyed(self):
        self.assertIn('#define CEF_UI_AUTH         (104)', CEFUI)
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_AUTH, CEF_UI_AUTH_URL, false, true);', CEFUI)
        self.assertIn('CEF_DestroyBrowser(playerid, CEF_UI_AUTH);', CEFUI)
        self.assertIn('CEF_RegisterEvent("auth:submit", "RW_CefAuthSubmit", Argument_String, Argument_String);', CEFUI)

    def test_the_password_is_checked_by_the_same_stock_as_the_dialog(self):
        submit = block(CEFUI, 'public RW_CefAuthSubmit(playerid, browserid, const password[], const confirm[])')
        self.assertIn('if (!IsPlayerConnected(playerid) || gLogged[playerid]) return 0;', submit)
        self.assertIn('return RW_AuthSubmit(playerid, !RW_DB_AccountExists(nick), password, confirm);', submit)

    def test_the_card_opens_before_the_plain_dialog(self):
        openauth = block(UI, 'stock RW_UI_OpenAuth(playerid)')
        self.assertIn('RW_UI_SelectEnd(playerid);', openauth)
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, "")', openauth)

    def test_an_error_comes_back_into_the_card(self):
        respond = block(ACCOUNTS, 'stock RW_Respond(playerid, bool:ok, const code[])')
        self.assertIn('RW_AuthCodeText(code, authText, sizeof authText);', respond)
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, ok ? ("") : (authText))', respond)
        self.assertIn('if (ok) RW_CefAuthClose(playerid);', respond)

    def test_the_card_closes_after_a_successful_login(self):
        self.assertIn('RW_CefAuthClose(playerid);', block(ACCOUNTS, 'RW_CefAuthClose(playerid);')[:0] or ACCOUNTS)
        self.assertTrue(ACCOUNTS.count('RW_CefAuthClose(playerid);') >= 2)


class AuthFallbackTest(unittest.TestCase):
    """Without the plugin the plain dialogs still work, and in Russian."""

    def test_the_stub_keeps_the_gamemode_compilable(self):
        self.assertIn('stock RW_CefAuthOpen(playerid, bool:registerMode, const nick[], const message[])', STUB)
        self.assertIn('stock RW_CefAuthClose(playerid)', STUB)

    def test_the_dialogs_speak_russian(self):
        for word in ('РЕГИСТРАЦИЯ', 'С ВОЗВРАЩЕНИЕМ', 'Придумайте пароль', 'Введите пароль'):
            self.assertIn(word, ACCOUNTS)
        self.assertIn('Пароль слишком короткий', ACCOUNTS)
        self.assertIn('Неверный пароль', ACCOUNTS)
        self.assertIn('Аккаунт создан.', ACCOUNTS)

    def test_the_text_draw_auth_panel_is_russian_too(self):
        self.assertIn('С возвращением', UI)
        self.assertIn('Создать аккаунт', UI)

    def test_the_server_side_keys_stay_ascii(self):
        for text in re.findall(r'"([^"\r\n]*)"', ACCOUNTS):
            if re.search(r'[Ѐ-ӿ]', text):
                continue
            if text.startswith(('SELECT ', 'INSERT ', 'UPDATE ', 'CREATE ')) or '/' in text:
                self.assertTrue(text.isascii(), text)


if __name__ == '__main__':
    unittest.main()
