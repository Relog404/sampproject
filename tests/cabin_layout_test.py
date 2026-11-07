"""Plane cabin (up to 20), the compact inventory grid and the /giveitem item words.

Source-level contracts only: no Pawn VM and no GTA client is available here.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'server/pawno/include/railway'
def pawn(n): return (INC / f'{n}.inc').read_text(encoding='cp1251')

CORE = pawn('core')
CABIN = pawn('cabin')
INV = pawn('inventory')
UI = pawn('ui')
GARAGE = pawn('garage')
WEP = pawn('weapons')
VEH = pawn('vehicles')
INTER = pawn('interaction')
PAGE = (ROOT / 'server/scriptfiles/cef/railway/js/inventory.js').read_text(encoding='utf-8')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
CEFUI = pawn('cefui')
CEFSTUB = pawn('cefstub')
UIPAGE = (ROOT / 'server/scriptfiles/cef/railway/js/ui.js').read_text(encoding='utf-8')
INDEX = (ROOT / 'server/scriptfiles/cef/railway/index.html').read_text(encoding='utf-8')


class CabinSourceTest(unittest.TestCase):
    def test_capacity_is_twenty_and_shared(self):
        self.assertIn('#define CABIN_MAX                   (20)', CORE)
        # defined in core.inc because interaction.inc is included before cabin.inc
        self.assertNotIn('#define CABIN_MAX', CABIN)

    def test_plane_models(self):
        self.assertIn('stock bool:RW_CabinIsPlane(model)', CORE)
        for model in ('460', '476', '511', '513', '519', '553', '577', '592'):
            self.assertIn(model, CORE.split('RW_CabinIsPlane')[1][:200])
        listing = CORE.split('RW_CabinIsPlane')[1][:200]
        self.assertNotIn('400', listing)   # cars are not planes

    def test_real_seats_are_used_first(self):
        self.assertIn('PutPlayerInVehicle(playerid, vehicleid, seat)', CABIN)
        self.assertIn('if (GetPlayerVehicleID(playerid) == vehicleid && GetPlayerVehicleSeat(playerid) == seat)', CABIN)
        # the pilot's seat is never taken when boarding
        self.assertIn('for (new s = 1; s < 10; s++)', CABIN)

    def test_overflow_goes_to_the_virtual_cabin(self):
        self.assertIn('TogglePlayerControllable(playerid, false)', CABIN)
        self.assertIn('RW_CabinPlace(playerid);', CABIN)
        self.assertIn('if (RW_CabinCount(vehicleid) >= CABIN_MAX)', CABIN)
        self.assertIn('The plane cabin is full (20).', CABIN)

    def test_boarding_guards(self):
        for text in ('Only planes have a passenger cabin.', 'Stand within 25 metres of the plane.',
                     'You cannot board now.', 'You are already on board.'):
            self.assertIn(text, CABIN)
        self.assertIn('if (gDowned[playerid])', CABIN)

    def test_leaving_unfreezes_and_places_the_player(self):
        leave = CABIN.split('stock RW_CabinLeave')[1].split('\nstock ')[0]
        self.assertIn('TogglePlayerControllable(playerid, true)', leave)
        self.assertIn('RemovePlayerFromVehicle(playerid)', leave)
        self.assertIn('SetPlayerPos(playerid, x, y, z + 1.0)', leave)

    def test_pulse_follows_the_plane(self):
        self.assertIn('public RW_CabinPulse()', CABIN)
        pulse = CABIN.split('public RW_CabinPulse()')[1]
        self.assertIn('RW_CabinPlace(p)', pulse)
        self.assertIn('if (!GetVehicleModel(v)) { RW_CabinLeave(p); continue; }', pulse)
        self.assertIn('if (GetPlayerVehicleID(p) == v) { gCabinVehicle[p] = 0; TogglePlayerControllable(p, true); continue; }', pulse)
        self.assertIn('SetTimer("RW_CabinPulse", 100, true);', MAIN)

    def test_main_wiring(self):
        self.assertIn('#include <railway/cabin>', MAIN)
        self.assertIn('RW_CabinReset(playerid);', MAIN.split('public OnPlayerDisconnect')[1][:400])

    def test_commands(self):
        self.assertIn('if (!strcmp(cmd, "board", true)) return RW_CabinCommand(playerid, params);', CMDS)
        self.assertIn('if (!strcmp(cmd, "unboard", true)) return RW_CabinLeave(playerid);', CMDS)
        self.assertIn('/board', CMDS)
        self.assertIn('/unboard', CMDS)

    def test_destroyed_plane_drops_its_passengers(self):
        self.assertIn('RW_CabinClearVehicle(gVeh[slot][vsVehicleId]);', VEH)
        self.assertIn('The plane is gone. You are on your own.', CABIN)

    def test_vehicle_card_row(self):
        self.assertIn('if (RW_CabinIsPlane(GetVehicleModel(v)))', INTER)
        self.assertIn('RW_CabinInside(playerid) ? ("Leave cabin") : ("Board cabin")', INTER)
        self.assertIn('RW_CabinCount(v), CABIN_MAX', INTER)
        self.assertIn('if (RW_CabinInside(playerid) || GetPlayerVehicleID(playerid) == cv) RW_CabinLeave(playerid);', INTER)
        self.assertIn('else RW_CabinBoard(playerid, cv);', INTER)


class CompactGridTest(unittest.TestCase):
    """Сетка инвентаря рисуется страницей: геометрия живёт в CSS."""

    def test_the_grid_is_ten_columns_of_square_cells(self):
        self.assertIn('grid-template-columns:repeat(10,1fr)', PAGE)
        self.assertIn('aspect-ratio:1/1', PAGE)

    def test_the_page_fits_the_screen(self):
        self.assertIn('max-width:96vw', PAGE)
        self.assertIn('max-height:88vh', PAGE)
        self.assertIn('overflow-y:auto', PAGE)

    def test_the_card_sits_next_to_the_grid(self):
        self.assertIn('.inv-card{width:250px', PAGE)
        self.assertIn('inv-card on', PAGE)

    def test_the_server_sends_the_whole_grid(self):
        """80 ячеек карманов и рюкзака плюс то, что лежит рядом."""
        grid = INV.split('stock RW_InvCef(playerid, bool:opening = true)', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('for (new i; i < INV_SLOTS; i++)', grid)
        self.assertIn('RW_InvCefCell(playerid, 80 + i,', grid)
        self.assertNotIn('gInvTD', INV)

    def test_car_preview_helpers(self):
        """Превью машины шлёт браузер: в ui.inc не осталось ни одного тексдрава."""
        self.assertIn('stock RW_CefPreview(playerid, model, const name[])', CEFUI)
        self.assertIn('"ui:preview"', CEFUI)
        self.assertIn('stock RW_CefPreview(playerid, model, const name[])', CEFSTUB)
        for marker in ('RW_UI_PreviewShow', 'RW_UI_PreviewHide', 'gUIPreview', 'PlayerTextDrawSetPreviewModel'):
            self.assertNotIn(marker, UI)
            self.assertNotIn(marker, GARAGE)
            self.assertNotIn(marker, INTER)
        # страница умеет показать и скрыть превью
        self.assertIn('class="preview" id="preview"', INDEX)
        self.assertIn("RW.on('ui:preview', function (model, name)", UIPAGE)
        self.assertIn("elPreview.className = 'preview on'", UIPAGE)
        self.assertIn("elPreview.className = 'preview'", UIPAGE)


class CardPreviewTest(unittest.TestCase):
    def test_vehicle_card_shows_the_car(self):
        self.assertIn('RW_CefPreview(playerid, GetVehicleModel(v), name);', INTER)
        # карточка машины открывается в браузере: UI_VEHICLE знает свой режим
        self.assertIn('if (mode == UI_VEHICLE) return CEF_MODE_VEHICLE;', UI)

    def test_the_cabin_card_goes_to_the_browser(self):
        # раскладка строк больше не считается на сервере: порядок строк
        # определяет страница, а сервер только шлёт их
        self.assertIn('if (mode == UI_VEHICLE) return CEF_MODE_VEHICLE;', UI)
        self.assertIn('if (mode == UI_PASSENGERS) return CEF_MODE_PASSENGERS;', UI)
        self.assertNotIn('19.0*slot', UI)

    def test_garage_card_shows_the_car(self):
        self.assertIn('RW_CefPreview(playerid, previewModel, previewName);', GARAGE)
        self.assertIn('if (row == 0) { previewModel = model; format(previewName, sizeof previewName, "%s", name); }', GARAGE)
        self.assertNotIn('RW_UI_PreviewShow', GARAGE)
        self.assertNotIn('RW_UI_PreviewHide', GARAGE)


class GiveItemTest(unittest.TestCase):
    def body(self): return INV.split('stock RW_InvGiveCommand')[1].split('\nstock ')[0]

    def test_item_words(self):
        for word in ('gun', 'weapon', 'ammo', 'bullets', 'rounds', 'bandage', 'bandages', 'syringe', 'adrenaline'):
            self.assertIn('"%s"' % word, INV.split('stock RW_InvItemWord')[1].split('\n}')[0])

    def test_calibre_words(self):
        words = INV.split('stock RW_InvCaliberWord')[1].split('\n}')[0]
        for word in ('9mm', '.44', '12ga', '5.56', '7.62', '.308'):
            self.assertIn('"%s"' % word, words)

    def test_ammo_count_is_clamped_to_the_calibre(self):
        body = self.body()
        self.assertIn('amount = RW_CaliberMax(caliber);', body)
        self.assertIn('if (amount > RW_CaliberMax(caliber)) amount = RW_CaliberMax(caliber);', body)

    def test_stackables_are_granted_one_by_one(self):
        self.assertIn('else for (new given; given < amount; given++) { uid = RW_InvGrant(target, type, data, 1); if (!uid) break; }', self.body())

    def test_no_argument_left_overs(self):
        body = self.body()
        self.assertIn('A key takes one argument:', body)
        self.assertIn('A gun takes no third argument.', body)
        self.assertIn('Bandages and syringes take one count, 1-20.', body)
        self.assertIn('A backpack takes no arguments.', body)
        # the old guard rejected every fourth token, which killed the round count
        self.assertNotIn('!a[0] || !b[0] || extra[0]', body)

    def test_result_messages(self):
        body = self.body()
        self.assertIn('ammunition: %d rounds (item %d).', body)
        self.assertIn('%d bandage(s) granted.', body)
        self.assertIn('%d adrenaline syringe(s) granted.', body)
        self.assertIn('%s granted (item %d). Equip it from the inventory.', body)


class WeaponTextFixTest(unittest.TestCase):
    def test_no_integer_fed_to_a_string_format(self):
        equip = WEP.split('stock RW_WpEquip')[1].split('\nstock ')[0]
        self.assertIn('RW_CaliberName(RW_WpCaliber(weapon), cbuf, sizeof cbuf);', equip)
        self.assertNotIn('Load %s ammunition.", name, RW_WpCaliber(weapon));', equip)

    def test_calibre_is_captured_before_the_stack_is_cleared(self):
        reload_body = WEP.split('stock RW_WpReloadFrom')[1].split('\nstock ')[0]
        self.assertIn('new cal = gInv[playerid][slot][iiData];', reload_body)
        self.assertIn('RW_CaliberName(cal, cname, sizeof cname);', reload_body)
        self.assertNotIn('RW_CaliberName(gInv[playerid][slot][iiData], cname, sizeof cname);', reload_body)


if __name__ == '__main__':
    unittest.main()
