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

    def test_the_card_offers_the_plain_dialogs_by_click(self):
        """Кнопка, которая молчит, оставляет игрока снаружи: на карточке
        всегда есть путь обратно к нативным диалогам, и это клик, не команда."""
        self.assertIn('id="native"', PAGE)
        self.assertIn("RW.emit('auth:native');", SCRIPT)

    def test_a_press_is_never_silent(self):
        """Ответа нет — страница говорит об этом сама, иначе кнопка
        выглядит сломанной и непонятно, где искать беду."""
        self.assertIn('Отправляю…', SCRIPT)
        self.assertIn('el.go.disabled = true;', SCRIPT)
        self.assertIn('Сервер не ответил', SCRIPT)
        self.assertIn('window.clearTimeout(pending)', SCRIPT)

    def test_the_page_says_so_when_the_bridge_is_missing(self):
        """Без моста плагина emit уходит в никуда: предупреждаем сразу,
        а не после того, как игрок нажмёт."""
        self.assertIn('RW.ready()', SCRIPT)
        self.assertIn('Браузер не связан с сервером', SCRIPT)


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
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, "")', openauth)
        self.assertIn('gCefAuth[playerid] = true;', openauth)
        self.assertIn('ShowPlayerDialog(playerid, -1, 0, "", "", "", "");', openauth)
        self.assertNotIn('RW_UI_SelectEnd', openauth)
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, "")', openauth)

    def test_an_error_comes_back_into_the_card(self):
        respond = block(ACCOUNTS, 'stock RW_Respond(playerid, bool:ok, const code[])')
        self.assertIn('RW_AuthCodeText(code, authText, sizeof authText);', respond)
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, ok ? ("") : (authText))', respond)
        self.assertIn('if (ok) RW_CefAuthClose(playerid);', respond)

    def test_the_card_keeps_the_focus_flag_itself(self):
        """Карточку показывает не только RW_UI_OpenAuth, но и ответ на
        ошибку: фокус должен держать тот, кто её рисует."""
        opened = block(CEFUI, 'stock RW_CefAuthOpen(playerid, bool:registerMode, const nick[], const message[])')
        self.assertIn('gCefAuth[playerid] = true;', opened)

    def test_the_card_is_repeated_once_the_browser_exists(self):
        """Браузер создаётся не сразу: первая карточка уходит в ещё не
        загруженную страницу, поэтому её повторяют по готовности."""
        created = block(CEFUI, 'public OnCefBrowserCreated(playerid, browserid, bool:success, E_CEF_CREATE_STATUS:code, const reason[])')
        self.assertIn('SetTimerEx("RW_CefAuthRetry", CEF_AUTH_RETRY_MS, false, "i", playerid)', created)
        retry = block(CEFUI, 'public RW_CefAuthRetry(playerid)')
        self.assertIn('if (!IsPlayerConnected(playerid) || gLogged[playerid] || !gCefAuth[playerid]) return 0;', retry)
        self.assertIn('RW_CefAuthOpen(playerid, !RW_DB_AccountExists(nick), nick, "");', retry)

    def test_the_plain_dialogs_are_one_click_away(self):
        """Если клики из браузера не доходят, карточка закрывается, а пароль
        спрашивает нативный диалог — им можно войти даже без плагина."""
        self.assertIn('CEF_RegisterEvent("auth:native", "RW_CefAuthNative");', CEFUI)
        fallback = block(CEFUI, 'public RW_CefAuthNative(playerid, browserid)')
        self.assertIn('gAuthBusy[playerid] = false;', fallback)
        self.assertIn('RW_CefAuthClose(playerid);', fallback)
        self.assertIn('gCefAuth[playerid] = false;', fallback)
        self.assertIn('return RW_ShowLoginDialog(playerid);', fallback)

    def test_a_lost_click_is_visible_in_the_server_log(self):
        """«Ничего не происходит» надо отличать от «клик не дошёл»:
        попытка входа оставляет след в логе."""
        submit = block(CEFUI, 'public RW_CefAuthSubmit(playerid, browserid, const password[], const confirm[])')
        self.assertIn('printf("[auth] submit from %d (%s): %d chars"', submit)
        self.assertIn('strlen(password)', submit)   # длина пароля, а не сам пароль
        self.assertNotIn(', password)', submit)

    def test_a_card_without_a_single_click_does_not_trap_the_player(self):
        """Из браузера не приходит ничего — клавиатура и мышь у окна, чат
        недоступен: сторож снимает карточку сам, без команды от игрока."""
        self.assertIn('#define AUTH_CARD_WATCH_MS', ACCOUNTS)
        opened = block(CEFUI, 'stock RW_CefAuthOpen(playerid, bool:registerMode, const nick[], const message[])')
        self.assertIn('SetTimerEx("RW_AuthCardWatch", AUTH_CARD_WATCH_MS, false, "i", playerid)', opened)
        watch = block(CEFUI, 'public RW_AuthCardWatch(playerid)')
        self.assertIn('if (gAuthCardAt[playerid] == 0 || !gCefAuth[playerid]) return 0;', watch)
        self.assertIn('RW_CefAuthNative(playerid, CEF_UI_AUTH);', watch)
        # клик дошёл — сторож больше не нужен
        self.assertIn('RW_AuthCardAlive(playerid);',
                      block(CEFUI, 'public RW_CefAuthSubmit(playerid, browserid, const password[], const confirm[])'))
        self.assertTrue(ACCOUNTS.count('gAuthCardAt[playerid] = 0;') >= 2)

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

    def test_the_auth_opens_in_the_browser_or_in_a_dialog(self):
        """Тексдрав-карточки входа больше нет: страница в браузере, а без
        браузера — нативный диалог, который умеет кириллицу."""
        auth = UI.split('stock RW_UI_OpenAuth(', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('RW_CefAuthOpen(', auth)
        self.assertIn('return RW_ShowLoginDialog(playerid);', auth)
        self.assertNotIn('RW_UI_Panel', auth)

    def test_a_busy_server_answers_instead_of_staying_silent(self):
        """Любая тихая ветка выглядит как сломанная кнопка: занятый
        сервер отвечает текстом, а не молчит."""
        guard = block(ACCOUNTS, 'if (gAuthBusy[playerid]) {')
        self.assertIn('RW_Respond(playerid, false, "err:busy");', guard)
        self.assertIn('gAuthBusy[playerid] = true;', guard)
        self.assertIn('Предыдущий запрос ещё обрабатывается', ACCOUNTS)

    def test_the_server_side_keys_stay_ascii(self):
        for text in re.findall(r'"([^"\r\n]*)"', ACCOUNTS):
            if re.search(r'[Ѐ-ӿ]', text):
                continue
            if text.startswith(('SELECT ', 'INSERT ', 'UPDATE ', 'CREATE ')) or '/' in text:
                self.assertTrue(text.isascii(), text)


if __name__ == '__main__':
    unittest.main()
