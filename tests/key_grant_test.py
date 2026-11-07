"""Key issuance input handling; source contracts only, no Pawn VM or GTA run."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def pawn(n): return (ROOT/'server/pawno/include/railway'/f'{n}.inc').read_text(encoding='cp1251')
INV=pawn('inventory')
COMMANDS=pawn('commands')

class KeyGrantInputTest(unittest.TestCase):
    def test_item_words_are_accepted_not_only_codes(self):
        words=INV.split('stock RW_InvItemWord',1)[1].split('stock RW_InvGiveVehicle',1)[0]
        for term in ('"bag"','"backpack"','"key"','"carkey"','"1"','"2"'):
            self.assertIn(term,words)
        self.assertIn('return -1',words)
    def test_vehicle_argument_forms(self):
        resolve=INV.split('stock RW_InvGiveVehicle',1)[1].split('stock RW_InvExplainGrant',1)[0]
        self.assertIn('arg[0] == \'p\'',resolve)              # plate RW-NNN
        self.assertIn('arg[0] == \'c\'',resolve)              # stored permanent car
        self.assertIn('return VEHICLE_SLOTS',resolve)         # token-only grant
        self.assertIn('/cars lists them',resolve)
        self.assertIn('has no key managed by the server',resolve)
        self.assertIn('No vehicle with id %d',resolve)
    def test_giveitem_reports_specific_failure_and_logs_uid(self):
        give=INV.split('stock RW_InvGiveCommand',1)[1].split('// Equipment owns',1)[0]
        self.assertIn('Player must be connected and logged in',give)
        self.assertIn('inventory is not loaded yet',give)
        self.assertIn('/givecar <name> <model> works offline',give)
        self.assertIn('printf("[inventory] admin account=%d gives type=%d data=%d uid=%d to account=%d"',give)
        self.assertIn('Key granted (item %d, car token %d)',give)
        self.assertNotIn('No space, weight limit exceeded or database error',INV)
    def test_cars_command_is_admin_only_and_lists_both_car_kinds(self):
        listing=pawn('vehicles').split('stock RW_Veh_ListCommand',1)[1].split('stock RW_Veh_CommandMyCar',1)[0]
        self.assertIn('gAdminLevel[playerid] < 1',listing)
        self.assertIn('gVehGarageId[slot] ? ("permanent") : ("temporary")',listing)
        self.assertIn('SELECT id,owner,model,token FROM owned_cars',listing)
        self.assertIn('stored in the garage',listing)
        self.assertIn('/giveitem <playerid> key',listing)
        router=COMMANDS.split('stock RW_CommandProcess',1)[1]
        self.assertIn('"cars"',router)
        self.assertIn('RW_Veh_ListCommand',router)
    def test_givecar_reports_the_real_reason(self):
        garage=pawn('garage').split('stock RW_GarageGive',1)[1]
        self.assertIn('No free MAIN slot in that account inventory',garage)
        self.assertIn('would carry %.1f kg of %.1f kg in MAIN',garage)
        self.assertIn('printf("[garage] givecar failed for account=%d',garage)
        self.assertNotIn('inventory full, overweight or database error',garage)

    def test_givekey_menu_selects_car_by_samp_id(self):
        garage = pawn('garage')
        open_menu = garage.split('stock RW_KeyMenuOpen',1)[1].split('stock RW_KeyMenuClick',1)[0]
        click = garage.split('stock RW_KeyMenuClick',1)[1].split('stock RW_KeyMenuCommand',1)[0]
        build = garage.split('stock RW_KeyMenuBuild',1)[1].split('stock RW_KeyMenuRender',1)[0]
        self.assertIn('gAdminLevel[playerid] < 1', open_menu)
        self.assertIn('!gInvLoaded[target]', open_menu)
        self.assertIn('gUIMode[playerid] = UI_KEYMENU', open_menu)
        self.assertIn('RW_UI_SelectBegin(playerid)', open_menu)
        # Spawned cars are listed by their SA-MP vehicle id, stored ones by c<id>.
        self.assertIn('gKeyList[playerid][gKeyCount[playerid]++] = gVeh[s][vsVehicleId]', build)
        self.assertIn('-db_get_field_assoc_int(r, "id")', build)
        # подпись строки одна для тексдрав-меню и для страницы в браузере
        row = garage.split('stock bool:RW_KeyMenuRow(', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('"%s / RW-%03d / %d"', row)
        self.assertIn('"%s / c%d / на стоянке"', row)
        self.assertIn('return RW_KeyMenuGrant(playerid, row);', click)
        # выдача одна и та же для тексдрав-меню и для страницы в браузере
        grant = garage.split('stock RW_KeyMenuGrant(', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('RW_InvGrant(target, ITEM_CAR_KEY, data)', grant)
        self.assertIn('RW_InvExplainGrant(playerid, target, ITEM_CAR_KEY, data)', grant)
        self.assertIn('RW_UI_Close(playerid)', grant)
        self.assertIn('Target player is no longer available', grant)
    def test_key_menu_is_a_real_ui_mode(self):
        ui = pawn('ui')
        self.assertIn('#define UI_KEYMENU 9', ui)
        self.assertIn('RW_KeyMenuHide(playerid)', ui.split('stock RW_UI_Close',1)[1].split('stock RW_UI_OpenAuth',1)[0])
        self.assertIn('RW_KeyMenuReset(playerid)', ui.split('stock RW_UI_Reset',1)[1])
        main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertIn('UI_KEYMENU) return RW_KeyMenuClick', main)
        self.assertIn('gUIMode[playerid] == UI_KEYMENU) return RW_UI_Close(playerid)', main)
        self.assertIn('gKeyTarget[i] = -1', main)
        commands = pawn('commands')
        self.assertIn('"givekey"', commands)
        self.assertIn('RW_KeyMenuCommand', commands)
        self.assertIn('/givekey <id> - menu', commands)

    def test_help_texts_use_new_syntax(self):
        self.assertIn('/giveitem <id> <bag|key> [vehicle id, p<plate> or c<garage car id>]',COMMANDS)
        self.assertIn('/cars - managed cars',COMMANDS)

if __name__=='__main__':
    unittest.main()
