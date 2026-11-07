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
def strings(code):
    return re.findall(r'"([^"\r\n]+)"', code)


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
                       'CEF_RegisterEvent', 'CEF_EmitEvent', 'CEF_PlayerHasPlugin'):
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
        self.assertIn('if (CEF_PlayerHasPlugin(p)) RW_CefStart(p);', CEFUI_SRC)
        self.assertIn('public OnCefReady(playerid)', CEFUI_SRC)

    def test_missing_plugin_leaves_the_native_interface(self):
        """The probe runs first; if it aborts, CEF stays off and nothing calls it."""
        init = CEFUI_SRC.split('stock RW_CefInit()')[1].split('\nstock ')[0]
        self.assertIn('gCefEnabled = RW_CefProbe();', init)
        self.assertIn('return 0;', init)
        self.assertIn('stock bool:RW_CefOn(playerid) { return gCefEnabled && gCefReady[playerid]; }', CEFUI_SRC)

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
        self.assertEqual(server, {'ui:open', 'ui:row', 'ui:action', 'ui:end', 'ui:close', 'ui:toast', 'ui:preview'})
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


class CursorFocusTest(unittest.TestCase):
    """Курсор мыши появляется вместе с фокусом браузера и убирается вместе с ним."""

    def test_focus_is_released_by_one_helper(self):
        self.assertIn('stock RW_CefFocusRelease(playerid)', CEFUI_SRC)
        release = CEFUI_SRC.split('stock RW_CefFocusRelease(playerid)')[1].split('\n}')[0]
        for browser in ('CEF_UI_MENU', 'CEF_UI_BIGMENU', 'CEF_UI_AUTH'):
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
        page = (PAGES / 'js/ui.js').read_text(encoding='utf-8')
        self.assertIn('if (RW.demo()) demo();', page)
        self.assertNotIn('if (!RW.ready()) demo();', page)


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
