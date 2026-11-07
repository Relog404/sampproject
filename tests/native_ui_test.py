"""Native UI migration contracts plus inspection of the deployable AMX.
No GTA client is emulated here; in-game rendering is a separate manual test.
"""
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / (name+'.inc')).read_text(encoding='cp1251')
def body(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]

class PanelBridgeTest(unittest.TestCase):
    """RW_UI_Panel рисует панель в браузере; без браузера — нативный диалог.
    Тексдрав-панелей больше нет: клиент SA-MP не умеет кириллицу."""

    def setUp(self):
        self.ui = pawn('ui')

    def test_every_menu_mode_knows_its_browser_page(self):
        modes = body(self.ui, 'stock RW_UI_CefMode(mode)', 'stock RW_UI_Panel(')
        for mode in ('UI_VEHICLE', 'UI_PASSENGERS', 'UI_POLICE', 'UI_MDT', 'UI_EMS', 'UI_TRUNK',
                     'UI_LOCKPICK', 'UI_SERVICE', 'UI_SHOP', 'UI_BANK', 'UI_JOB', 'UI_PHONE_APP',
                     'UI_PROPERTY', 'UI_DONATE', 'UI_STORY', 'UI_MARKET', 'UI_CRIME', 'UI_FAMILY',
                     'UI_GOV', 'UI_INVENTORY'):
            self.assertIn('if (mode == %s)' % mode, modes)
        self.assertIn('return CEF_MODE_NONE;', modes)

    def test_the_panel_opens_the_browser_and_prepares_the_dialog(self):
        panel = body(self.ui, 'stock RW_UI_Panel(', 'stock RW_UI_Row(')
        self.assertIn('RW_CefOpen(playerid, cef, title, subtitle)', panel)
        self.assertIn('gUIOnCef[playerid] = true;', panel)
        self.assertIn('return RW_UI_ScheduleNative(playerid);', panel)
        self.assertNotIn('PlayerTextDraw', panel)

    def test_rows_are_drawn_once_the_last_one_is_sent(self):
        row = body(self.ui, 'stock RW_UI_Row(', 'stock RW_UI_Close(')
        self.assertIn('RW_UI_ScheduleCef(playerid)', row)
        flush = body(self.ui, 'public RW_UI_FlushCef(playerid)', 'stock RW_UI_NativeResponse(')
        self.assertIn('return RW_CefEnd(playerid);', flush)

    def test_closing_cancels_both_paths(self):
        close = body(self.ui, 'stock RW_UI_Close(', 'stock RW_UI_OpenAuth(')
        self.assertIn('RW_UI_CancelCef(playerid);', close)
        self.assertIn('RW_UI_CancelNative(playerid);', close)
        self.assertIn('gUIOnCef[playerid] = false;', close)

    def test_no_panel_text_draw_is_left(self):
        """Панель больше не строит и не показывает тексдравы."""
        self.assertNotIn('RW_UI_BuildPanel', self.ui)
        self.assertNotIn('gUILayoutMode', self.ui)
        self.assertNotIn('gUIPanelCreated', self.ui)
        for index in ('22', '23'):
            self.assertNotIn('gUI[playerid][%s]' % index, self.ui.split('stock RW_UI_Panel(')[1])


class NativeInterfaceTest(unittest.TestCase):
    def setUp(self):
        self.main = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.ui = pawn('ui')
        self.control = pawn('interaction')

    def test_browser_layer_is_optional_and_lives_in_one_file(self):
        """CEF is an add-on: only railway/cefui.inc talks to the plugin, and
        every call is guarded so a player without the plugin keeps this UI."""
        files = {p.name: p.read_text(encoding='cp1251') for p in (ROOT/'server/pawno/include/railway').glob('*.inc')}
        code = self.main + ''.join(files.values())
        self.assertIn('#include <cef>', files['cefui.inc'])
        for name, text in files.items():
            if name == 'cefui.inc':
                continue
            body = '\n'.join(l for l in text.splitlines() if not l.lstrip().startswith('#define'))
            self.assertNotRegex(body, r'\bCEF_[A-Z_]+\s*\(', name)   # no plugin call outside the bridge
        # each CEF entry point checks the player state first
        bridge = files['cefui.inc']
        for stock in ('RW_CefToast', 'RW_CefOpen', 'RW_CefRow', 'RW_CefAction', 'RW_CefEnd', 'RW_CefClose'):
            block = bridge.split('stock %s(' % stock)[1].split('\nstock ')[0]
            self.assertIn('RW_CefOn(playerid)', block, stock)

    def test_deployment_still_works_without_the_plugin(self):
        config = (ROOT/'server/server.cfg').read_text(encoding='cp1251')
        self.assertRegex(config, r'(?m)^plugins cef$')
        setup = (ROOT/'setup.ps1').read_text()
        self.assertNotIn('Invoke-WebRequest', setup)
        self.assertNotIn('DownloadFile', setup)
        # missing plugin -> probe aborts, CEF stays off, native UI keeps working
        bridge = (ROOT/'server/pawno/include/railway/cefui.inc').read_text(encoding='cp1251')
        self.assertIn('gCefEnabled = RW_CefProbe();', bridge)
        self.assertIn('plugin unavailable; native interface kept.', bridge)

    def test_connect_opens_native_auth_immediately(self):
        connect = body(self.main, 'public OnPlayerConnect', 'public OnPlayerDisconnect')
        self.assertIn('RW_UI_Create(playerid)', connect)
        self.assertIn('RW_UI_OpenAuth(playerid)', connect)
        self.assertNotIn('SetTimer', connect)
        auth = body(self.ui, 'stock RW_UI_OpenAuth', 'stock RW_UI_PasswordBegin')
        self.assertIn('RW_DB_AccountExists', auth)
        self.assertIn('UI_AUTH', auth)

    def test_masked_password_dialog_and_database_flow_are_preserved(self):
        accounts = pawn('accounts')
        self.assertIn('DIALOG_STYLE_PASSWORD', accounts)
        self.assertIn('RW_UI_PasswordBegin(playerid)', accounts)
        respond = body(accounts, 'stock RW_Respond', 'stock RW_AuthSubmit')
        self.assertIn('RW_AuthDialogRespond', respond)
        self.assertIn('SHA256_PassHash', accounts)

    def test_native_clicks_and_safe_cursor_cancellation(self):
        """Клик по тексдрав-панели удалён: панель рисует браузер, а без
        него игрок отвечает в нативном диалоге."""
        self.assertNotIn('public OnPlayerClickPlayerTextDraw', self.main)
        self.assertNotIn('public OnPlayerClickTextDraw', self.main)
        self.assertNotIn('gUI[playerid]', self.main)
        self.assertNotIn('gUI[playerid]', self.ui)
        # курсор SelectTextDraw нужен был только чтобы кликать по тексдравам
        for marker in ('RW_UI_SelectBegin', 'RW_UI_SelectEnd', 'gUISelecting', 'gUIExpectedCancels'):
            self.assertNotIn(marker, self.ui)
            self.assertNotIn(marker, self.main)
        reset = self.ui.split('stock RW_UI_Reset', 1)[1]
        self.assertNotIn('PlayerTextDrawDestroy', reset)
        self.assertNotIn('gUICreated', reset)
        self.assertIn('RW_UI_CancelNative(playerid)', reset)
        self.assertIn('RW_UI_Reset(playerid)', body(self.main, 'public OnPlayerDisconnect', 'public OnPlayerRequestClass'))

    def test_native_binding_defaults_and_swap(self):
        for i, key in enumerate(('KEY_CTRL_BACK', 'KEY_NO', 'KEY_YES')):
            self.assertIn(f'gControlKeys[playerid][{i}] = {key}', self.control)
        self.assertIn('gControlKeys[playerid][i] = old', self.control)
        self.assertIn('!(newkeys & key) || (oldkeys & key)', self.main)
        self.assertIn('gUIMode[playerid] != 0', self.main)
        self.assertNotIn('221', self.control)

    def test_vehicle_actions_remain_server_authoritative(self):
        execute = body(self.control, 'stock RW_ControlExecute', 'stock RW_EngineCommand')
        for required in ('!gLogged[playerid]', 'IsPlayerInRangeOfPoint', 'GetVehicleVirtualWorld',
                         '(action == 0 || action == 2) && !RW_InvHasKey', '(action == 3 || action == 4) && !RW_IsVehicleOwner', '!gVeh[slot][vsKey]',
                         'GetPlayerVehicleSeat(playerid) != 0', 'GetPlayerVehicleSeat(target) <= 0'):
            self.assertIn(required, execute)
        self.assertIn('RW_ControlExecute(playerid, gControlVehicle[playerid], 4, gUITarget', self.control)
        self.assertIn('gUIHasNext[playerid]', self.control)

    def test_the_speedometer_lives_on_the_hud_page(self):
        """Спидометр рисует HUD-страница: сервер шлёт цифры, тексдравов нет."""
        show = body(self.ui, 'stock RW_UI_SpeedoShow', 'stock RW_UI_SpeedoHide')
        self.assertIn('RW_CefHudSpeedo(playerid, true, name, 0, 0, 0, 0);', show)
        self.assertIn('if (!RW_SettingsOn(playerid, SET_SPEEDO)) return 0;', show)
        hide = body(self.ui, 'stock RW_UI_SpeedoHide', '// Notices use')
        self.assertIn('RW_CefHudSpeedo(playerid, false, "", 0, 0, 0, 0);', hide)
        self.assertIn('gSpeedoShown[playerid] = false', hide)
        update = body(self.ui, 'stock RW_UI_SpeedoUpdate', '// Notices use')
        self.assertIn('if (gUIMode[playerid] != 0) return 1', update)
        self.assertIn('RW_CefHudSpeedo(playerid, true, "", speed, gear, rpm, flags);', update)
        for marker in ('gUI[playerid][12]', 'gUI[playerid][14]', 'gUI[playerid][18]'):
            self.assertNotIn(marker, self.ui)
        # страница умеет показывать и скрывать спидометр
        page = (ROOT / 'server/scriptfiles/cef/railway/js/hud.js').read_text(encoding='utf-8')
        self.assertIn("RW.on('hud:speedo'", page)
        self.assertIn("speedo.className = 'speedo on'", page)
        self.assertIn("speedo.className = 'speedo'", page)

    def test_the_ground_hint_lives_on_the_hud_page(self):
        """Метку предмета на земле рисует HUD-страница: сервер шлёт только текст."""
        ground = pawn('ground')
        show = body(ground, 'stock RW_GroundShow', 'stock RW_GroundTake')
        self.assertIn('RW_CefHudGround(playerid, true, label);', show)
        self.assertIn('gGroundAim[playerid] = gGround[index][igUid];', show)
        self.assertNotIn('PlayerTextDraw', show)
        reset = body(ground, 'stock RW_GroundHide', 'stock RW_GroundTarget')
        self.assertIn('RW_CefHudGround(playerid, false, "");', reset)
        self.assertIn('gGroundAim[playerid]=0;', reset)
        for marker in ('gGroundHint', 'gGroundHintCreated', 'PlayerTextDraw'):
            self.assertNotIn(marker, ground)
        # подсказку показывает и скрывает та же страница, что и спидометр
        markup = (ROOT / 'server/scriptfiles/cef/railway/hud.html').read_text(encoding='utf-8')
        self.assertIn('class="ground" id="ground"', markup)
        page = (ROOT / 'server/scriptfiles/cef/railway/js/hud.js').read_text(encoding='utf-8')
        self.assertIn("RW.on('hud:ground'", page)
        self.assertIn("ground.className = 'ground on'", page)
        self.assertIn("ground.className = 'ground'", page)
        css = (ROOT / 'server/scriptfiles/cef/railway/css/app.css').read_text(encoding='utf-8')
        self.assertIn('.ground { position: absolute;', css)

    def test_notifications_never_touch_a_textdraw(self):
        """Уведомления рисует браузер; тексдравов-всплывашек больше нет.
        RW_UITick оставлен пустым на случай старых сборок."""
        tick = body(self.ui, 'public RW_UITick', 'stock RW_UI_Reset')
        self.assertNotIn('PlayerTextDrawShow', tick)
        self.assertNotIn('PlayerTextDrawColor', tick)
        notify = body(self.ui, 'stock RW_UI_Notify', 'public RW_UITick')
        self.assertIn('RW_CefToast', notify)
        self.assertNotIn('RW_UI_Create', notify)

    def test_map_teleport_is_confirmed_and_rechecked(self):
        click = body(self.main, 'public OnPlayerClickMap', 'public OnPlayerKeyStateChange')
        self.assertIn('RW_MapPrompt(playerid)', click)
        self.assertIn('RW_UI_Panel(playerid, UI_MAP', self.control)
        self.assertNotIn('ShowPlayerDialog', click)
        confirm = self.control.split('mode == UI_MAP', 1)[1]
        self.assertIn('gMapPending[playerid] && gMapHasPoint[playerid] && row == 0', confirm)
        self.assertIn('PLAYER_STATE_ONFOOT', confirm)
        self.assertIn('GetPlayerVirtualWorld(playerid) != 0', confirm)
        self.assertIn('SetPlayerPosFindZ', confirm)

    def test_amx_imports_and_callbacks_are_native_only(self):
        data = (ROOT/'server/gamemodes/railway.amx').read_bytes()
        self.assertEqual(struct.unpack_from('<H', data, 4)[0], 0xF1E0)
        self.assertEqual(data[6], 8)
        self.assertGreaterEqual(struct.unpack_from('<i', data, 28)[0], 0)
        stride = struct.unpack_from('<H', data, 10)[0]
        publics, natives, libraries = struct.unpack_from('<III', data, 32)
        def names(start, end):
            result = []
            for offset in range(start, end, stride):
                name_offset = struct.unpack_from('<I', data, offset+4)[0]
                result.append(data[name_offset:data.index(b'\0', name_offset)].decode('ascii'))
            return result
        imported = names(natives, libraries)
        exported = names(publics, natives)
        # the shipped build talks to omp-cef, and only through these natives
        self.assertEqual(sorted(n for n in imported if n.startswith('CEF_')),
                         ['CEF_AddResource', 'CEF_CreateBrowser', 'CEF_DestroyBrowser',
                          'CEF_EmitEvent', 'CEF_EnableKey', 'CEF_FocusBrowser',
                          'CEF_IsChatInputOpen', 'CEF_PlayerHasPlugin',
                          'CEF_RegisterEvent', 'CEF_SetBrowserLayer',
                          'CEF_SetBrowserVisible', 'CEF_SetKeyCapture'])
        self.assertIn('plugins cef', (ROOT/'server/server.cfg').read_text(encoding='cp1251'))
        for name in ('ShowPlayerDialog', 'db_open', 'SHA256_PassHash'):
            self.assertIn(name, imported)
        # панели рисуются в браузере: тексдравов в сборке не осталось
        for name in ('CreatePlayerTextDraw', 'PlayerTextDrawSetString', 'PlayerTextDrawShow',
                     'PlayerTextDrawSetPreviewModel', 'PlayerTextDrawDestroy',
                     'SelectTextDraw', 'CancelSelectTextDraw'):
            self.assertNotIn(name, imported)
        for name in ('OnPlayerClickPlayerTextDraw', 'OnPlayerClickTextDraw'):
            self.assertNotIn(name, exported)
        for name in ('OnPlayerKeyStateChange', 'OnPlayerRequestClass'):
            self.assertIn(name, exported)

if __name__ == '__main__':
    unittest.main()
