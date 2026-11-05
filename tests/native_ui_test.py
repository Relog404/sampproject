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

class NativeInterfaceTest(unittest.TestCase):
    def setUp(self):
        self.main = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.ui = pawn('ui')
        self.control = pawn('interaction')

    def test_no_browser_calls_or_include_in_active_gamemode(self):
        code = self.main + ''.join(p.read_text(encoding='cp1251') for p in (ROOT/'server/pawno/include/railway').glob('*.inc'))
        self.assertNotRegex(code, r'\bCEF_\w+\s*\(')
        self.assertNotIn('#include <cef>', code)
        self.assertNotRegex(code, r'public OnCef\w+')
        self.assertFalse((ROOT/'server/scriptfiles/cef').exists())

    def test_deployment_needs_no_plugin_or_client_installer(self):
        config = (ROOT/'server/server.cfg').read_text(encoding='cp1251')
        self.assertRegex(config, r'(?m)^plugins\s*$')
        self.assertNotRegex(config, r'(?m)^cef_')
        setup = (ROOT/'setup.ps1').read_text()
        self.assertNotIn('Invoke-WebRequest', setup)
        self.assertNotIn('DownloadFile', setup)

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
        self.assertIn('public OnPlayerClickPlayerTextDraw', self.main)
        self.assertIn('playertextid == gUI[playerid][3+i]', self.main)
        close = body(self.ui, 'stock RW_UI_Close(playerid,', 'stock RW_UI_OpenAuth')
        self.assertLess(close.index('gUIMode[playerid] = 0'), close.index('RW_UI_SelectEnd'))
        cancel = body(self.main, 'public OnPlayerClickTextDraw', 'public OnPlayerKeyStateChange')
        self.assertIn('gUIInput[playerid]', cancel)
        reset = self.ui.split('stock RW_UI_Reset', 1)[1]
        self.assertIn('PlayerTextDrawDestroy', reset)
        self.assertIn('gUICreated[playerid] = false', reset)
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

    def test_hud_not_rendered_over_open_menu(self):
        panel = body(self.ui, 'stock RW_UI_Panel', 'stock RW_UI_Row')
        self.assertIn('i = 12; i <= 18', panel)
        self.assertIn('PlayerTextDrawHide', panel)
        update = body(self.ui, 'stock RW_UI_SpeedoUpdate', '// Notices use')
        self.assertIn('if (gUIMode[playerid] != 0) return 1', update)
        self.assertIn('gSpeedoShown[playerid] = false', self.ui)

    def test_native_notifications_expire_and_shrink(self):
        tick = body(self.ui, 'public RW_UITick', 'stock RW_UI_Reset')
        self.assertIn('elapsed >= 3000', tick)
        self.assertIn('float(3000-elapsed)/3000.0', tick)
        self.assertIn('PlayerTextDrawColor', tick)
        self.assertIn('PlayerTextDrawHide', tick)

    def test_map_teleport_is_textdraw_confirmed_and_rechecked(self):
        click = body(self.main, 'public OnPlayerClickMap', 'public OnPlayerClickPlayerTextDraw')
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
        self.assertFalse(any(n.startswith('CEF_') for n in imported))
        self.assertFalse(any(n.startswith('OnCef') for n in exported))
        for name in ('CreatePlayerTextDraw', 'PlayerTextDrawSetString', 'SelectTextDraw',
                     'PlayerTextDrawDestroy', 'ShowPlayerDialog', 'db_open', 'SHA256_PassHash'):
            self.assertIn(name, imported)
        for name in ('OnPlayerClickPlayerTextDraw', 'OnPlayerClickTextDraw', 'OnPlayerKeyStateChange'):
            self.assertIn(name, exported)

if __name__ == '__main__':
    unittest.main()
