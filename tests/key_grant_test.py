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

    def test_help_texts_use_new_syntax(self):
        self.assertIn('/giveitem <id> <bag|key> [vehicle id, p<plate> or c<garage car id>]',COMMANDS)
        self.assertIn('/cars - managed cars',COMMANDS)

if __name__=='__main__':
    unittest.main()
