"""SQLite transition tests using SQL extracted from Pawn, plus source contracts.
These tests do not emulate GTA rendering or execute the Pawn VM.
"""
from pathlib import Path
import re
import sqlite3
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / (name + '.inc')).read_text(encoding='cp1251')
INV = pawn('inventory')

def sql(prefix):
    return next(s for s in re.findall(r'"([^"\r\n]+)"', INV) if s.startswith(prefix))

def section(start, end):
    return INV.split(start, 1)[1].split(end, 1)[0]

class InventoryStorageTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql('CREATE TABLE'))
        self.db.execute('CREATE TABLE permanent_keys(uid INTEGER PRIMARY KEY, car INTEGER)')
        self.db.execute('CREATE TABLE owned_cars(id INTEGER PRIMARY KEY, owner INTEGER, model INTEGER, token INTEGER UNIQUE)')

    def tearDown(self):
        self.db.close()

    def item(self, owner=1, slot=0, kind=1, data=0):
        self.db.execute(sql('INSERT INTO') % (owner, slot, kind, data))
        return self.db.execute('SELECT last_insert_rowid()').fetchone()[0]

    def drop(self, uid, owner=1, slot=0):
        return self.db.execute(sql('UPDATE inventory_items SET owner=NULL') % (1.0, 2.0, 3.0, uid, owner, slot)).rowcount

    def take(self, uid, owner=2, slot=0):
        return self.db.execute(sql('UPDATE inventory_items SET owner=%d') % (owner, slot, uid)).rowcount

    def test_unique_slots_and_domain_constraints(self):
        self.item()
        for args in ((1, 0, 2, 10), (1, 80, 1, 0), (1, -1, 1, 0), (0, 1, 1, 0), (2, 1, 9, 0)):
            with self.assertRaises(sqlite3.IntegrityError):
                self.item(*args)
        self.item(2, 0)  # Same slot on a different account is independent.

    def test_two_players_cannot_pick_up_same_uid(self):
        uid = self.item()
        self.assertEqual(self.drop(uid), 1)
        self.assertEqual(self.take(uid, 2), 1)
        self.assertEqual(self.take(uid, 3), 0)
        self.assertEqual(self.db.execute('SELECT owner,slot FROM inventory_items WHERE uid=?', (uid,)).fetchone(), (2, 0))

    def test_replayed_drop_and_stale_slot_cannot_mutate_item(self):
        uid = self.item()
        move = sql('UPDATE inventory_items SET slot=%d')
        self.assertEqual(self.db.execute(move % (50, uid, 1, 0)).rowcount, 1)
        self.assertEqual(self.drop(uid, slot=0), 0)
        self.assertEqual(self.drop(uid, owner=2, slot=50), 0)
        self.assertEqual(self.drop(uid, slot=50), 1)
        self.assertEqual(self.drop(uid, slot=50), 0)

    def test_full_destination_preserves_ground_item(self):
        uid = self.item()
        self.drop(uid)
        self.item(2, 0)
        with self.assertRaises(sqlite3.IntegrityError):
            self.take(uid, 2)
        self.assertIsNone(self.db.execute('SELECT owner FROM inventory_items WHERE uid=?', (uid,)).fetchone()[0])

    def test_multiple_ground_rows_do_not_conflict(self):
        for i in range(50):
            uid = self.item(1, i)
            self.assertEqual(self.drop(uid, slot=i), 1)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_items WHERE owner IS NULL').fetchone()[0], 50)

    def test_mutation_can_roll_back_before_memory_publication(self):
        uid = self.item()
        self.db.execute('SAVEPOINT inventory_mutation')
        self.assertEqual(self.drop(uid), 1)
        self.db.execute('ROLLBACK TO inventory_mutation')
        self.db.execute('RELEASE inventory_mutation')
        self.assertEqual(self.db.execute('SELECT owner,slot FROM inventory_items').fetchone(), (1, 0))

    def test_restart_keeps_backpacks_and_drops_but_invalidates_session_keys(self):
        self.db.execute('CREATE TABLE accounts(id INTEGER, hash TEXT)')
        self.db.execute("INSERT INTO accounts VALUES(1,'unchanged-hash')")
        bag = self.item(1, 0)
        self.drop(bag)
        self.item(1, 1, 1)
        self.item(1, 2, 2, 500)
        with tempfile.TemporaryDirectory() as d:
            file = Path(d)/'inventory.db'
            dest = sqlite3.connect(file)
            self.db.backup(dest)
            dest.close()
            reopened = sqlite3.connect(file)
            reopened.execute(sql('DELETE FROM inventory_items WHERE type=2'))
            self.assertEqual(reopened.execute('SELECT type,owner FROM inventory_items ORDER BY uid').fetchall(), [(1, None), (1, 1)])
            self.assertEqual(reopened.execute('SELECT hash FROM accounts').fetchone()[0], 'unchanged-hash')
            reopened.close()

    def test_vehicle_tokens_are_not_reused_after_delete(self):
        old_uid = self.item(1, 0, 2)
        self.db.execute('DELETE FROM inventory_items')
        new_uid = self.item(1, 0, 2)
        self.assertGreater(new_uid, old_uid)

class InventorySourceTests(unittest.TestCase):
    def test_capacity_and_independent_integer_gram_limits(self):
        for name, value in [('INV_MAIN', 50), ('INV_BAG', 30), ('INV_SLOTS', 80), ('INV_NEAR', 50), ('INV_MAIN_GRAMS', 50000), ('INV_BAG_GRAMS', 40000)]:
            self.assertRegex(INV, rf'#define {name}\s+{value}\b')
        free = section('stock RW_InvFree', 'stock RW_InvFind')
        self.assertIn('RW_InvWeight(playerid, section) + RW_InvGrams(type)', free)
        self.assertIn('section ? INV_BAG_GRAMS : INV_MAIN_GRAMS', free)
        self.assertIn('RW_InvPack(playerid) < 0 || type == ITEM_BACKPACK', free)
        remove = section('stock bool:RW_InvCanRemove', 'stock RW_InvTransfer')
        self.assertIn('!other && RW_InvWeight(playerid, 1) > 0', remove)
        self.assertIn('gVeh[v][vsKey]', remove)

    def test_writes_are_transactional_and_precede_memory_updates(self):
        write = section('stock bool:RW_InvWrite', 'stock RW_InvInit')
        for term in ('SAVEPOINT inventory_mutation', 'SELECT changes()', 'ROLLBACK TO inventory_mutation', 'RELEASE inventory_mutation', '== 1'):
            self.assertIn(term, write)
        transfer = section('stock RW_InvTransfer', 'stock RW_InvDrop')
        self.assertLess(transfer.index('if (!RW_InvWrite(query))'), transfer.index('gInv[playerid][dest][iiUid] = uid'))
        drop = section('stock RW_InvDrop', 'stock RW_InvLabel')
        self.assertLess(drop.index('if (!RW_InvWrite(query))'), drop.index('gGround[ground][igUid] ='))
        self.assertIn('DestroyObject(object); return RW_Error', drop)

    def test_world_revalidation_and_uid_selection(self):
        near = section('stock bool:RW_InvNearGround', 'stock RW_InvAttach')
        for term in ('PLAYER_STATE_ONFOOT', 'GetPlayerInterior(playerid) == 0', 'GetPlayerVirtualWorld(playerid) == 0', 'IsPlayerInRangeOfPoint(playerid, 3.0'):
            self.assertIn(term, near)
        transfer = section('stock RW_InvTransfer', 'stock RW_InvDrop')
        self.assertIn('RW_InvGroundFind(uid)', transfer)
        self.assertIn('RW_InvNearGround(playerid, ground)', transfer)
        self.assertIn('owner IS NULL', transfer)

    def test_textdraw_budget_preview_and_reserved_equipment(self):
        count = int(re.search(r'#define INV_TDS\s+(\d+)', INV)[1])
        self.assertLessEqual(count + 24, 256)
        create = section('stock RW_InvCreate', 'stock RW_InvCell')
        self.assertLess(create.index('RW_InvSprite(playerid, 139'), create.index('for (new i; i < 130; i++)'))
        self.assertIn('for (new i; i < 8; i++)', create)
        self.assertIn('PlayerTextDrawFont(playerid, gInvTD[playerid][130], 5)', create)
        self.assertIn('GetPlayerSkin(playerid)', INV)
        self.assertIn('SetPlayerAttachedObject(playerid, INV_ATTACH, 3026', INV)

    def test_inventory_callbacks_run_before_russian_dialog_guard(self):
        main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        click = main.split('public OnPlayerClickPlayerTextDraw',1)[1].split('public OnPlayerClickTextDraw',1)[0]
        self.assertLess(click.index('RW_InvClick'), click.index('gUIRussianDialogs'))
        cancel = main.split('public OnPlayerClickTextDraw',1)[1].split('public OnPlayerKeyStateChange',1)[0]
        self.assertLess(cancel.index('UI_INVENTORY'), cancel.index('gUIRussianDialogs'))
        ui = pawn('ui')
        self.assertIn('gUIMode[playerid] >= UI_INVENTORY', ui.split('stock RW_UI_NativeResponse',1)[1].split('stock RW_TD_Encode',1)[0])
        self.assertIn('RW_InvReset(playerid)', ui)

    def test_admin_grants_and_console_only_admin_assignment(self):
        grant = INV.split('stock RW_InvGiveCommand',1)[1]
        self.assertIn('!gLogged[playerid] || gAdminLevel[playerid] < 1', grant)
        self.assertIn('!gLogged[target] || !gInvLoaded[target]', grant)
        self.assertIn('gVehKeyUid[slot] <= 0', grant)
        commands = pawn('commands')
        ingame = commands.split('stock RW_CommandProcess',1)[1]
        self.assertNotIn('RW_AdminSetLevel(', ingame)
        self.assertIn('RW_InvGiveCommand', ingame)
        main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertIn('RW_AdminSetLevel(nick, level)', main.split('public OnRconCommand',1)[1])

    def test_delayed_vehicle_cleanup_cannot_destroy_recycled_vehicle_id(self):
        vehicles = pawn('vehicles')
        callback = vehicles.split('public RW_Veh_RespawnLater', 1)[1].split('stock RW_Veh_OnPlayerDisconnect', 1)[0]
        self.assertIn('gVehSpawnStamp[slot] == token', callback)
        main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertIn('"ii", vehicleid, gVehSpawnStamp[slot]', main)

    def test_spcar_grants_key_before_replacing_previous_vehicle(self):
        spawn = pawn('vehicles').split('stock RW_Veh_Spawn',1)[1].split('stock RW_Veh_DestroySlot',1)[0]
        self.assertLess(spawn.index('RW_InvGrant'), spawn.index('RW_Veh_DestroySlot'))
        self.assertIn('gVehKeyUid[slot] = keyUid', spawn)
        controls = pawn('interaction').split('stock RW_ControlExecute',1)[1]
        self.assertIn('(action == 0 || action == 2) && !RW_InvHasKey', controls)
        # A driver still needs only an already inserted key, not ownership.
        engine = controls.split('case 1:',1)[1].split('case 2:',1)[0]
        self.assertNotIn('RW_IsVehicleOwner', engine)
        self.assertNotIn('RW_InvHasKey', engine)

if __name__ == '__main__':
    unittest.main()
