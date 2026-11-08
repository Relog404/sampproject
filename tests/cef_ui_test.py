"""omp-cef browser layer: plugin wiring, Pawn bridge and the shipped pages.

Server and client speak a tiny protocol (ui:open / ui:row / ui:action /
ui:end / ui:close / ui:toast one way, ui:click / ui:closed back). These tests
keep both sides in sync. No browser and no game here - contracts only.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'server/pawno/include'
CEF = INC / 'cef.inc'
CEFUI = INC / 'railway' / 'cefui.inc'
CORE = INC / 'railway' / 'core.inc'
UI = INC / 'railway' / 'ui.inc'
INTER = INC / 'railway' / 'interaction.inc'
MAIN = ROOT / 'server/gamemodes/railway.pwn'
PAGES = ROOT / 'server/scriptfiles/cef/railway'

def pawn(path):
    return path.read_text(encoding='cp1251')

CEFUI_SRC = pawn(CEFUI)
UI_SRC = pawn(UI)


def split_args(text):
    """Аргументы через запятую: запятая внутри кавычек не делит."""
    args, cur, quote = [], '', False
    for c in text:
        if c == "'":
            quote = not quote
        elif c == ',' and not quote:
            args.append(cur.strip())
            cur = ''
            continue
        cur += c
    args.append(cur.strip())
    return args


def block(src, signature):
    start = src.index(signature)
    depth = 0
    for i in range(start + len(signature) - 1, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError('не закрыт блок: %s' % signature)


def strings(code):
    return re.findall(r'"([^"\r\n]+)"', code)


class IconTest(unittest.TestCase):
    """Иконка строки: сервер называет то, что страница умеет рисовать.

    Неизвестное имя страница молча заменяет на «шеврон» — строка выглядит
    пустой и непонятно, что нажать. Сверяем все имена из Pawn с ICONS в ui.js.
    """

    def split_args(self, text):
        args, cur, depth, i = [], '', 0, 0
        while i < len(text):
            c = text[i]
            if c == '"':
                j = i + 1
                while j < len(text) and text[j] != '"':
                    j += 2 if text[j] == '\\' else 1
                cur += text[i:j + 1]
                i = j + 1
                continue
            if c in '([':
                depth += 1
            elif c in ')]':
                depth -= 1
                if depth < 0:
                    break
            if c == ',' and depth == 0:
                args.append(cur.strip())
                cur = ''
                i += 1
                continue
            cur += c
            i += 1
        args.append(cur.strip())
        return args

    def server_icons(self):
        icons = set()
        for name in ('core', 'ui', 'interaction', 'economy', 'ems', 'police', 'transport',
                     'garage', 'market', 'property', 'crime', 'family', 'government',
                     'city', 'phone', 'phoneapps', 'medical', 'donate', 'story'):
            src = pawn(INC / 'railway' / ('%s.inc' % name))
            for match in re.finditer(r'RW_CefRow\(', src):
                end = src.find(');', match.end())
                if end < 0:
                    continue
                args = self.split_args(src[match.end():end])
                if len(args) >= 5 and args[4].startswith('"'):
                    icons.add(args[4].strip('"'))
        return icons

    def page_icons(self):
        js = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        return set(re.findall(r'^\s{4}(\w+):', js, re.M))

    def test_every_icon_the_server_names_is_drawn(self):
        known, used = self.page_icons(), self.server_icons()
        self.assertTrue(used, 'иконок не нашлось: проверь тест')
        self.assertEqual(used - known, set(), 'нет в ICONS: %s' % sorted(used - known))

    def test_the_icons_everyone_needs_exist(self):
        for name in ('wallet', 'cash', 'info', 'phone', 'settings', 'map', 'key',
                     'band', 'clock', 'door', 'engine', 'car', 'close', 'check', 'refresh'):
            self.assertIn(name, self.page_icons(), name)

    def wrappers(self):
        """Обёртки строк: у них иконка — последний аргумент, а вызывают их
        везде, поэтому RW_CefRow напрямую в тех местах не найти."""
        found = {}
        for path in sorted(INC.glob('railway/*.inc')):
            src = pawn(path)
            for match in re.finditer(r'stock [^\n(]*\b(\w+)\(([^)]*)\)\s*\{', src):
                params = match.group(2)
                if 'icon' not in params:
                    continue
                if 'RW_CefRow(' in block(src, match.group(0)):
                    found[match.group(1)] = params
        return found

    def test_every_icon_the_row_wrappers_name_is_drawn(self):
        known, used = self.page_icons(), set()
        for path in sorted(INC.glob('railway/*.inc')):
            src = pawn(path)
            for name, params in self.wrappers().items():
                arity = len(self.split_args(params))
                for match in re.finditer(r'\b%s\(' % name, src):
                    end = src.find(');', match.end())
                    if end < 0:
                        continue
                    args = self.split_args(src[match.end():end])
                    if len(args) == arity and args[-1].startswith('"'):
                        used.add(args[-1].strip('"'))
        self.assertTrue(used, 'иконок не нашлось: проверь тест')
        self.assertEqual(used - known, set(), 'нет в ICONS: %s' % sorted(used - known))

    def test_every_phone_app_icon_is_drawn(self):
        """Иконки приложений сервер называет строкой (RW_PhoneAppIcon): имя,
        которого нет в ICONS, страница нарисует шевроном."""
        apps = block(pawn(INC / 'railway' / 'phone.inc'), 'stock RW_PhoneAppIcon(id, dest[], size = sizeof dest)')
        known, used = self.page_icons(), set(re.findall(r'format\(dest, size, "(\w+)"\)', apps))
        self.assertTrue(used, 'иконок не нашлось: проверь тест')
        self.assertEqual(used - known, set(), 'нет в ICONS: %s' % sorted(used - known))

    def test_a_row_can_name_its_icon(self):
        row = block(UI_SRC, 'stock RW_UI_Row(playerid, row, const text[], const icon[] = "bag")')
        self.assertIn('RW_CefRow(playerid, row, text, "", icon);', row)
        inter = pawn(INTER)
        self.assertIn('RW_UI_Row(playerid, 0, "Yes, teleport", "map");', inter)
        self.assertIn('RW_UI_Row(playerid, 1, "No, stay here", "close");', inter)
        self.assertIn('RW_UI_Row(playerid, 0, "Вызвать скорую", "band");', pawn(INC / 'railway' / 'medical.inc'))
        self.assertIn('RW_UI_Row(playerid, 0, row, "wallet");', pawn(INC / 'railway' / 'phone.inc'))


class DemoTest(unittest.TestCase):
    """Предпросмотр без игры: адрес на каждую панель, иконки из тех, что есть.

    `index.html?fake=impound` должен показать штрафстоянку, а не первую
    попавшуюся карточку, иначе смотреть новую панель негде.
    """

    def demos(self):
        js = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        start = js.index('var DEMOS = {')
        depth = 0
        for i in range(start, len(js)):
            if js[i] == '{':
                depth += 1
            elif js[i] == '}':
                depth -= 1
                if depth == 0:
                    body = js[start:i + 1]
                    break
        else:
            raise AssertionError('DEMOS не закрыт')
        names = re.findall(r'^\s{4}(\w+): function', body, re.M)
        return js, body, names

    def test_every_panel_has_an_address(self):
        js, _, names = self.demos()
        self.assertEqual(set(names), {'car', 'impound', 'market', 'bank', 'property'}, names)
        self.assertIn("if (which === '1' || which === 'car') demo('car');", js)
        self.assertIn('else if (DEMOS[which]) demo(which);', js)

    def test_the_demo_rows_use_icons_the_page_can_draw(self):
        js, body, _ = self.demos()
        known = set(re.findall(r'^\s{4}(\w+):', js, re.M))
        used = set()
        for match in re.finditer(r"RW\.fake\('ui:row', \[([^\]]*)\]", body):
            args = split_args(match.group(1))
            if len(args) >= 4:
                used.add(args[3].strip().strip("'"))
        self.assertTrue(used, 'в демо нет ни одной строки: проверь тест')
        self.assertEqual(used - known, set(), 'нет в ICONS: %s' % sorted(used - known))

    def test_the_impound_demo_shows_the_lot(self):
        _, body, _ = self.demos()
        lot = body[body.index('impound: function'):]
        self.assertIn("'Impound lot'", lot)
        self.assertIn("'Fee 500 for a car, the lot at 1635 / -1700'", lot)
        self.assertIn("'car'", lot)


class ClosedCardTest(unittest.TestCase):
    """Esc и клик мимо листа закрывают панель и на сервере."""

    def block(self, signature):
        return block(CEFUI_SRC, signature)

    def test_the_state_is_released_with_the_page(self):
        closed = self.block('public RW_CefClosed(playerid, browserid)')
        self.assertIn('if (RW_InputBusy(playerid)) return RW_InputCancel(playerid);', closed)
        self.assertIn('if (gUIOnCef[playerid] && gUIMode[playerid] != UI_DOWNED) {', closed)
        self.assertIn('gUIOnCef[playerid] = false;', closed)
        self.assertIn('return RW_UI_Close(playerid);', closed)

    def test_the_bleeding_screen_comes_back(self):
        """Экран раненого исключение: Esc его не снимает, кровотечение идёт."""
        closed = self.block('public RW_CefClosed(playerid, browserid)')
        self.assertIn('gUIMode[playerid] != UI_DOWNED', closed)

    def test_the_login_card_is_not_a_panel(self):
        closed = self.block('public RW_CefClosed(playerid, browserid)')
        self.assertIn('if (gCefAuth[playerid]) { RW_CefClose(playerid); RW_CefFocusRelease(playerid); return 1; }', closed)

    def test_the_escape_key_is_registered(self):
        self.assertIn('CEF_RegisterEvent("ui:closed", "RW_CefClosed");', CEFUI_SRC)
        js = (ROOT / 'server/scriptfiles/cef/railway/js/ui.js').read_text(encoding='utf-8')
        self.assertIn("var CLOSE_ID = 8;", js)
        self.assertIn("if (event.key === 'Escape' && open) RW.emit('ui:closed');", js)
        self.assertIn("elScrim.addEventListener('click', function () {", js)
        self.assertIn("if (open) RW.emit('ui:closed');", js)


class PluginWiringTest(unittest.TestCase):
    def test_plugin_shipped_and_loaded(self):
        self.assertTrue((ROOT / 'server/plugins/cef.dll').exists())
        self.assertTrue((ROOT / 'server/plugins/cef.so').exists())
        config = pawn(ROOT / 'server/server.cfg')
        self.assertIn('plugins cef', config)
        self.assertTrue(CEF.exists(), 'cef.inc from the plugin must be in the includes')

    def test_natives_we_use_are_declared(self):
        cef = pawn(CEF)
        for native in ('CEF_AddResource', 'CEF_CreateBrowser', 'CEF_SetBrowserLayer',
                       'CEF_SetBrowserVisible', 'CEF_FocusBrowser', 'CEF_DestroyBrowser',
                       'CEF_RegisterEvent', 'CEF_EmitEvent', 'CEF_PlayerHasPlugin',
                       'CEF_SetKeyCapture', 'CEF_EnableKey', 'CEF_IsChatInputOpen'):
            self.assertRegex(cef, r'\bnative\s+(?:bool:)?%s\(' % native)

    def test_pluginless_build_is_possible(self):
        """RAILWAY_CEF 0 builds a gamemode with no plugin import at all."""
        main = pawn(MAIN)
        self.assertIn('#define RAILWAY_CEF             (1)', main)
        self.assertIn('#if RAILWAY_CEF', main)
        self.assertIn('#include <railway/cefui>', main)
        self.assertIn('#include <railway/cefstub>', main)
        stub = pawn(INC / 'railway' / 'cefstub.inc')
        for name in ('RW_CefOn', 'RW_CefInit', 'RW_CefStart', 'RW_CefReset', 'RW_CefToast', 'RW_CefClose'):
            self.assertIn('stock%s%s(' % (' ' if not name.startswith('RW_CefOn') else ' bool:', name), stub, name)
        # the two menus only reach the browser behind the switch
        inter = pawn(INTER)
        self.assertIn('#if RAILWAY_CEF\n        if (RW_CefOn(playerid)) return RW_CefVehicle(playerid);', inter)

    def test_init_and_lifecycle_in_the_gamemode(self):
        main = pawn(MAIN)
        self.assertIn('#include <railway/cefui>', main)
        self.assertIn('RW_CefInit();', main)
        self.assertIn('RW_CefReset(playerid);', main.split('public OnPlayerDisconnect')[1][:400])
        self.assertIn('RW_CefReset(playerid);', main.split('public OnPlayerConnect')[1][:400])

    def test_readiness_is_polled_not_assumed(self):
        """SA-MP builds may never fire OnCefReady, so we poll the plugin."""
        self.assertIn('SetTimer("RW_CefPulse", 1000, true);', CEFUI_SRC)
        self.assertIn('if (!CEF_PlayerHasPlugin(p)) continue;', CEFUI_SRC)
        self.assertIn('RW_CefStart(p);', CEFUI_SRC)
        self.assertIn('public OnCefReady(playerid)', CEFUI_SRC)

    def test_the_browser_starts_before_the_login(self):
        """Раньше CEF поднимался только после входа, поэтому окно
        авторизации всегда оставалось нативным диалогом: auth.html из
        игры видно не было."""
        pulse = block(CEFUI_SRC, 'public RW_CefPulse()')
        self.assertIn('if (gCefReady[p] || !IsPlayerConnected(p)) continue;', pulse)
        self.assertIn('if (!gLogged[p]) RW_UI_OpenAuth(p);', pulse)
        self.assertNotIn('!gLogged[p]) continue;', pulse)

    def test_a_window_of_its_own_keeps_the_focus(self):
        """Вход, телефон и поле ввода держат фокус сами: страховка от
        застрявшего курсора не должна его вырывать."""
        pulse = block(CEFUI_SRC, 'public RW_CefPulse()')
        self.assertIn('if (gCefAuth[p] || gPhoneShown[p] || RW_InputBusy(p)) continue;', pulse)
        release = block(CEFUI_SRC, 'stock RW_CefFocusRelease(playerid)')
        self.assertIn('if (!gPhoneShown[playerid]) CEF_FocusBrowser(playerid, CEF_UI_PHONE, false);', release)
        self.assertIn('if (!gHudMouse[playerid]) CEF_FocusBrowser(playerid, CEF_UI_HUD, false);', release)
        self.assertIn('if (!gCefAuth[playerid]) CEF_FocusBrowser(playerid, CEF_UI_AUTH, false);', release)

    def test_missing_plugin_leaves_the_native_interface(self):
        """The probe runs first; if it aborts, CEF stays off and nothing calls it."""
        init = CEFUI_SRC.split('stock RW_CefInit()')[1].split('\nstock ')[0]
        self.assertIn('gCefEnabled = RW_CefProbe();', init)
        self.assertIn('return 0;', init)
        self.assertIn('stock bool:RW_CefAlive(playerid) { return gCefEnabled && gCefReady[playerid]; }', CEFUI_SRC)
        self.assertIn('stock bool:RW_CefOn(playerid) { return RW_CefAlive(playerid) && !gCefDead[playerid]; }', CEFUI_SRC)

    def test_browsers_and_layers(self):
        start = CEFUI_SRC.split('stock RW_CefStart(playerid)')[1].split('\nstock ')[0]
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_MENU, CEF_UI_MENU_URL, false, true);', start)
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_NOTIFY, CEF_UI_TOAST_URL, false, true);', start)
        self.assertIn('CEF_SetBrowserLayer(playerid, CEF_UI_NOTIFY, 200);', start)   # toasts above menus
        self.assertIn('CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);', start)


class ProtocolTest(unittest.TestCase):
    """Every event the server emits is handled by the pages, and back."""

    def server_events(self):
        emitted = set()
        for block in CEFUI_SRC.split('CEF_EmitEvent(')[1:]:
            emitted |= set(re.findall(r'"(ui:[^"]+)"', block.split(');')[0]))
        return emitted

    def page_events(self):
        names = set()
        for page in PAGES.rglob('*.js'):
            names |= set(re.findall(r"RW\.on\('([^']+)'", page.read_text(encoding='utf-8')))
        return names

    def test_pages_exist(self):
        for name in ('index.html', 'notify.html', 'css/app.css', 'js/bridge.js', 'js/ui.js', 'js/notify.js'):
            self.assertTrue((PAGES / name).exists(), name)

    def test_events_match(self):
        server = self.server_events()
        pages = self.page_events()
        self.assertEqual(server, {'ui:open', 'ui:row', 'ui:action', 'ui:end', 'ui:close', 'ui:toast', 'ui:preview', 'ui:ask'})
        self.assertEqual(server - pages, set(), 'server emits something no page handles')

    def test_pages_answer_with_registered_events(self):
        registered = set(re.findall(r'CEF_RegisterEvent\("([^"]+)"', CEFUI_SRC))
        emitted = set()
        for page in PAGES.rglob('*.js'):
            emitted |= set(re.findall(r"RW\.emit\('([^']+)'", page.read_text(encoding='utf-8')))
        self.assertTrue(emitted, 'the pages must answer the server')
        self.assertTrue(emitted <= registered, 'unregistered: %s' % (emitted - registered))

    def test_row_shape_is_the_same_on_both_sides(self):
        emit = CEFUI_SRC.split('stock RW_CefRow(')[1].split('\nstock ')[0]
        args = re.findall(r'CEF_(STR|INT)\(([^)]+)\)', emit)
        self.assertEqual([a[0] for a in args], ['INT', 'STR', 'STR', 'STR', 'STR', 'STR'])
        handler = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        self.assertIn("RW.on('ui:row', function (id, label, value, icon, note, valueClass)", handler)

    def test_resource_urls_point_at_the_shipped_files(self):
        self.assertIn('#define CEF_RES             "railway"', CEFUI_SRC)
        self.assertIn('#define CEF_UI_MENU_URL     "http://cef/railway/index.html"', CEFUI_SRC)
        self.assertIn('#define CEF_UI_TOAST_URL    "http://cef/railway/notify.html"', CEFUI_SRC)
        for url in strings(CEFUI_SRC):
            if url.startswith('http://cef/railway/'):
                self.assertTrue((PAGES / url.split('/')[-1]).exists(), url)

    def test_pages_use_the_plugin_bridge(self):
        bridge = (PAGES / 'js/bridge.js').read_text(encoding='utf-8')
        self.assertIn('window.cef.on', bridge)
        self.assertIn('window.cef.emit', bridge)


class CefHandshakeTest(unittest.TestCase):
    """Плагинов, которые не отдают события обратно, хватает: сервер
    проверяет страницу сам, а не надеется, что клики когда-нибудь дойдут."""

    def test_the_server_asks_the_page_before_trusting_it(self):
        self.assertIn('CEF_RegisterEvent("cef:pong", "RW_CefPong");', CEFUI_SRC)
        ping = block(CEFUI_SRC, 'public RW_CefPing(playerid)')
        self.assertIn('RW_CefPingAsk(playerid);', ping)
        self.assertIn('SetTimerEx("RW_CefPing", CEF_PING_MS, false, "i", playerid)', ping)
        self.assertIn('return RW_CefDead(playerid);', ping)
        ask = block(CEFUI_SRC, 'stock RW_CefPingAsk(playerid)')
        for browser in ('CEF_UI_HUD', 'CEF_UI_AUTH', 'CEF_UI_MENU', 'CEF_UI_NOTIFY', 'CEF_UI_PHONE'):
            self.assertIn('CEF_EmitEvent(playerid, %s, "cef:ping"' % browser, ask)

    def test_the_handshake_starts_with_the_browser(self):
        start = block(CEFUI_SRC, 'stock RW_CefStart(playerid)')
        self.assertIn('gCefPong[playerid] = false;', start)
        self.assertIn('RW_CefPingAsk(playerid);', start)
        self.assertIn('SetTimerEx("RW_CefPing", CEF_PING_MS, false, "i", playerid)', start)

    def test_the_card_waits_for_the_handshake(self):
        """Карточку показываем только странице, которая ответила: иначе
        игрок минуту жмёт кнопки окна, которое их не отдаёт."""
        openauth = UI_SRC.split('stock RW_UI_OpenAuth(playerid)')[1].split('\nstock ')[0]
        self.assertIn('gCefPong[playerid] && RW_CefAuthOpen(', openauth)

    def test_a_mute_browser_does_not_reopen_the_dialog(self):
        """Игрок уже печатает пароль в диалоге: перерисовка его бы стёрла."""
        dead = block(CEFUI_SRC, 'stock RW_CefDead(playerid)')
        self.assertIn('gUIMode[playerid] != UI_AUTH', dead)

    def test_a_page_that_never_answers_is_no_interface(self):
        dead = block(CEFUI_SRC, 'stock RW_CefDead(playerid)')
        self.assertIn('the page never answered', dead)
        self.assertIn('RW_CefAuthNative(playerid, CEF_UI_AUTH);', dead)
        # сначала закрываем браузер, потом выключаем: иначе RW_CefOn уже не сработает
        self.assertLess(dead.find('RW_CefAuthNative('), dead.find('gCefDead[playerid] = true;'))

    def test_a_late_answer_turns_the_browser_back_on(self):
        pong = block(CEFUI_SRC, 'public RW_CefPong(playerid, browserid)')
        self.assertIn('gCefPong[playerid] = true;', pong)
        self.assertIn('gCefDead[playerid] = false;', pong)
        self.assertIn('RW_UI_OpenAuth(playerid);', pong)

    def test_a_mute_window_stops_covering_the_game(self):
        """Окно рисуется поверх игры: пока оно висит, нативный диалог
        оказывается под ним, и игрок не видит ни диалога, ни кнопок."""
        dead = block(CEFUI_SRC, 'stock RW_CefDead(playerid)')
        self.assertIn('RW_CefHideAll(playerid);', dead)
        self.assertLess(dead.find('RW_CefHideAll('), dead.find('gCefDead[playerid] = true;'))
        hide = block(CEFUI_SRC, 'stock RW_CefHideAll(playerid)')
        for browser in ('CEF_UI_AUTH', 'CEF_UI_MENU', 'CEF_UI_PHONE', 'CEF_UI_NOTIFY', 'CEF_UI_HUD'):
            self.assertIn('CEF_SetBrowserVisible(playerid, %s, false);' % browser, hide)
        self.assertIn('SendClientMessage(playerid, COLOR_ERROR,', dead)

    def test_the_log_names_the_cause_once(self):
        self.assertIn('before 1.4.0 deliver no browser events', CEFUI_SRC)
        self.assertIn('Update server/plugins/cef.dll', CEFUI_SRC)

    def test_a_mute_browser_still_releases_the_focus(self):
        """Немое окно держит курсор и клавиатуру: иначе нативный диалог
        не получит ни одного нажатия."""
        release = block(CEFUI_SRC, 'stock RW_CefFocusRelease(playerid)')
        self.assertIn('if (!RW_CefAlive(playerid)) return 0;', release)

    def test_every_page_answers_the_ping(self):
        bridge = (PAGES / 'js/bridge.js').read_text(encoding='utf-8')
        self.assertIn("RW.on('cef:ping'", bridge)
        self.assertIn("RW.emit('cef:pong')", bridge)


class CursorFocusTest(unittest.TestCase):
    """Курсор мыши появляется вместе с фокусом браузера и убирается вместе с ним."""

    def test_focus_is_released_by_one_helper(self):
        self.assertIn('stock RW_CefFocusRelease(playerid)', CEFUI_SRC)
        release = CEFUI_SRC.split('stock RW_CefFocusRelease(playerid)')[1].split('\n}')[0]
        for browser in ('CEF_UI_MENU', 'CEF_UI_AUTH'):
            self.assertIn('CEF_FocusBrowser(playerid, %s, false);' % browser, release)

    def test_every_close_path_releases_the_focus(self):
        close = CEFUI_SRC.split('stock RW_CefClose(playerid)')[1].split('\n}')[0]
        self.assertIn('RW_CefFocusRelease(playerid);', close)
        self.assertIn('CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);', close)
        closed = CEFUI_SRC.split('public RW_CefClosed(playerid, browserid)')[1].split('\n}')[0]
        self.assertIn('RW_CefFocusRelease(playerid);', closed)

    def test_a_stuck_cursor_is_dropped_by_the_pulse(self):
        pulse = CEFUI_SRC.split('public RW_CefPulse()')[1].split('\n}')[0]
        self.assertIn('gCefMode[p] == CEF_MODE_NONE && gUIMode[p] == 0', pulse)
        self.assertIn('RW_CefFocusRelease(p);', pulse)

    def test_the_close_button_closes_the_sheet_everywhere(self):
        """Кнопка «Закрыть» — id 8 во всех панелях: лист прячется сам и
        серверу уходит ui:closed, даже если панель не разобрала клик."""
        page = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        self.assertIn('var CLOSE_ID = 8;', page)
        self.assertIn('if (id === CLOSE_ID)', page)
        self.assertIn("RW.emit('ui:closed');", page)

    def test_the_inventory_closes_without_waiting_for_the_server(self):
        page = (PAGES / 'js/inventory.js').read_text(encoding='utf-8')
        self.assertIn("RW.emit('ui:click', 3000);", page)
        self.assertIn("RW.emit('ui:closed');", page)

    def test_the_bridge_keeps_every_listener(self):
        """ui.js и inventory.js слушают один и тот же ui:close, поэтому мост
        хранит список обработчиков, а не один, и биндится, даже если
        плагин внедрил window.cef позже скриптов."""
        bridge = (PAGES / 'js/bridge.js').read_text(encoding='utf-8')
        self.assertIn('handlers[name].push(callback)', bridge)
        self.assertIn('window.cef.on(name,', bridge)
        self.assertIn('window.setInterval(flush, 50)', bridge)
        self.assertNotIn('handlers[name] = callback;', bridge)

    def test_the_demo_never_covers_the_live_interface(self):
        """Демо включается только адресом ?fake=...: в игре лист остаётся пустым
        и сервер рисует его сам, когда доедет первый пакет."""
        page = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        self.assertIn("var which = (window.location.search.match(/fake=([a-z0-9]+)/i) || [])[1];", page)
        self.assertIn("if (which === '1' || which === 'car') demo('car');", page)
        self.assertIn('else if (DEMOS[which]) demo(which);', page)
        self.assertNotIn('if (!RW.ready()) demo();', page)
        self.assertNotIn('\n  demo();', page)


class CapturedKeyTest(unittest.TestCase):
    """Телефон и инвентарь открываются клавишами, которых SA-MP не отдаёт.

    `[` и `U` сервер через OnPlayerKeyStateChange не видит, поэтому их
    перехватывает плагин на уровне окна игры: OnCefPressKey приходит и без
    фокуса браузера, то есть курсор для этого не нужен.
    """

    def setUp(self):
        src = CEFUI_SRC
        i = src.index('public OnCefPressKey(')
        self.handler = src[i:src.index('\n}\n', i)]

    def test_the_capture_is_switched_on_with_the_browsers(self):
        self.assertIn('CEF_SetKeyCapture(playerid, true);', CEFUI_SRC)
        self.assertIn('CEF_EnableKey(playerid, CEF_VK_BRACKET, true);', CEFUI_SRC)
        self.assertIn('CEF_EnableKey(playerid, CEF_VK_U, true);', CEFUI_SRC)
        self.assertIn('RW_CefKeys(playerid);', CEFUI_SRC)
        reset = CEFUI_SRC.split('stock RW_CefReset(playerid)')[1].split('\n}')[0]
        self.assertIn('CEF_SetKeyCapture(playerid, false);', reset)

    def test_the_scan_codes_are_layout_proof(self):
        """Скан-код физической клавиши одинаков на любой раскладке."""
        self.assertIn('#define CEF_SCAN_BRACKET    (26)', CEFUI_SRC)
        self.assertIn('#define CEF_SCAN_U          (22)', CEFUI_SRC)
        self.assertIn('#define CEF_MOD_ALT         (8)', CEFUI_SRC)
        self.assertIn('if (scancode == CEF_SCAN_BRACKET)', self.handler)
        self.assertIn('if (scancode == CEF_SCAN_U)', self.handler)
        self.assertNotIn('if (key ==', self.handler)

    def test_alt_opens_the_phone_and_the_inventory(self):
        self.assertIn('if (!(modifiers & CEF_MOD_ALT)) return 1;', self.handler)
        self.assertIn('if (gPhoneShown[playerid]) return RW_PhoneClose(playerid);', self.handler)
        self.assertIn('return RW_PhoneOpen(playerid);', self.handler)
        self.assertIn('return RW_InvOpen(playerid);', self.handler)

    def test_only_a_fresh_press_counts(self):
        """Отпускание и автоповтор не должны дёргать панели."""
        self.assertIn('if (!down || repeat) return 1;', self.handler)
        self.assertIn('if (!gLogged[playerid] || !RW_CefOn(playerid)) return 1;', self.handler)
        self.assertIn('if (CEF_IsChatInputOpen(playerid)) return 1;', self.handler)

    def test_the_hints_name_both_keys(self):
        hud = pawn(INC / 'railway' / 'hud.inc')
        self.assertIn('"AltBracket:phone;AltU:inventory;AltY:mouse;"', hud)
        script = (PAGES / 'js/hud.js').read_text(encoding='utf-8')
        self.assertIn("AltBracket: 'Alt + ['", script)
        self.assertIn("AltU: 'Alt + U'", script)


class FallbackTest(unittest.TestCase):
    """Players without the plugin keep the native dialog interface."""

    def test_notify_is_the_browser_only_and_never_a_textdraw(self):
        """Текст уведомления уже ушёл в чат, всплывашка рисуется только
        в браузере: тексдрав больше не всплывает над экраном — особенно
        на входе, когда браузер ещё не готов."""
        notify = pawn(UI).split('stock RW_UI_Notify')[1].split('\nstock ')[0]
        self.assertIn('return RW_CefToast(playerid, text, kind);', notify)
        self.assertNotIn('RW_UI_Text', notify)
        self.assertNotIn('gToastActive', notify)
        self.assertNotIn('SetTimer("RW_UITick"', pawn(UI))

    def test_toast_kind_follows_the_message_type(self):
        core = pawn(CORE)
        self.assertIn('RW_UI_Notify(playerid, text, 1);', core)   # RW_Ok
        self.assertIn('RW_UI_Notify(playerid, text, 2);', core)   # RW_Error

    def test_menus_switch_on_the_player_state(self):
        inter = pawn(INTER)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefVehicle(playerid);', inter)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefPassengers(playerid);', inter)
        self.assertIn('RW_CefClose(playerid);', pawn(UI).split('stock RW_UI_Close')[1][:300])

    def test_car_card_has_the_same_actions_in_both_worlds(self):
        inter = pawn(INTER)
        card = inter.split('stock RW_CefVehicle(playerid)')[1].split('\nstock ')[0]
        for row in ('RW_CefRow(playerid, 0,', 'RW_CefRow(playerid, 1,', 'RW_CefRow(playerid, 2,',
                    'RW_CefRow(playerid, 3,', 'RW_CefRow(playerid, 4,'):
            self.assertIn(row, card)
        clicks = inter.split('stock RW_CefVehicleClick(playerid, id)')[1].split('\nstock ')[0]
        self.assertIn('RW_ControlExecute(playerid, v, id);', clicks)
        self.assertIn('RW_CabinBoard(playerid, v);', clicks)


class PageStyleTest(unittest.TestCase):
    """iOS-ish: rounded, soft, animated - and no square corners anywhere."""

    def test_design_tokens(self):
        css = (PAGES / 'css/app.css').read_text(encoding='utf-8')
        for token in ('--radius: 22px', '--ease: cubic-bezier', 'backdrop-filter', 'border-radius'):
            self.assertIn(token, css)
        self.assertNotIn('border: 1px solid', css)          # no hard frames
        self.assertNotIn('border-radius: 0', css)

    def test_animations(self):
        css = (PAGES / 'css/app.css').read_text(encoding='utf-8')
        self.assertIn('@keyframes row-in', css)
        self.assertIn('@keyframes toast-in', css)
        self.assertIn('animation-delay: calc(var(--i)', css)   # staggered rows

    def test_pages_load_the_styles(self):
        for page in ('index.html', 'notify.html'):
            html = (PAGES / page).read_text(encoding='utf-8')
            self.assertIn('css/app.css', html)
            self.assertIn('js/bridge.js', html)


if __name__ == '__main__':
    unittest.main()
