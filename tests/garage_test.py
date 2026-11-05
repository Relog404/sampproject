"""Execute extracted SQL with SQLite; source contracts for native UI/motion.
Not a GTA or Pawn VM simulation.
"""
from pathlib import Path
import re
import sqlite3
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(n):
    return (ROOT/'server/pawno/include/railway'/f'{n}.inc').read_text(encoding='cp1251')
GARAGE, INV, MOTION = source('garage'), source('inventory'), source('motion')
def query(code, prefix):
    return next(s for s in re.findall(r'"([^"\r\n]+)"', code) if s.startswith(prefix))

class PermanentGarageStorageTest(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(query(INV, 'CREATE TABLE'))
        self.db.execute(query(INV, 'CREATE TABLE IF NOT EXISTS inventory_equipment'))
        self.db.execute(query(GARAGE, 'CREATE TABLE IF NOT EXISTS owned_cars'))
        self.db.execute(query(GARAGE, 'CREATE TABLE IF NOT EXISTS permanent_keys'))

    def tearDown(self):
        self.db.close()

    def grant(self, owner=1, slot=0, model=411):
        self.db.execute(query(GARAGE, 'INSERT INTO inventory_items') % (owner, slot))
        uid = self.db.execute('SELECT last_insert_rowid()').fetchone()[0]
        self.db.execute(query(GARAGE, 'UPDATE inventory_items SET data') % (uid, uid))
        self.db.execute(query(GARAGE, 'INSERT INTO owned_cars') % (owner, model, uid))
        car = self.db.execute('SELECT last_insert_rowid()').fetchone()[0]
        self.db.execute(query(GARAGE, 'INSERT INTO permanent_keys(uid,car) VALUES') % (uid, car))
        return uid, car

    def test_permanent_keys_survive_restart_while_temp_keys_expire(self):
        uid, car = self.grant()
        self.db.execute(query(INV,'INSERT INTO inventory_items') % (1, 1, 2, 9999))
        # Additional permanent key in another account, as /giveitem creates it.
        self.db.execute(query(INV,'INSERT INTO inventory_items') % (2, 0, 2, uid))
        copy = self.db.execute('SELECT last_insert_rowid()').fetchone()[0]
        self.db.execute(query(GARAGE, 'INSERT INTO permanent_keys(uid,car) SELECT') % (copy, uid))
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'railway.db'
            target = sqlite3.connect(path)
            self.db.backup(target); target.close()
            reopened = sqlite3.connect(path)
            reopened.execute(query(INV, 'DELETE FROM inventory_items WHERE type=2'))
            self.assertEqual(reopened.execute('SELECT uid FROM inventory_items ORDER BY uid').fetchall(), [(uid,), (copy,)])
            self.assertEqual(reopened.execute('SELECT owner,model,token FROM owned_cars').fetchone(), (1, 411, uid))
            reopened.close()

    def test_ground_permanent_key_survives_and_is_not_listed_until_picked_up(self):
        uid, _ = self.grant()
        self.db.execute(query(INV,'UPDATE inventory_items SET owner=NULL') % (1.,2.,3.,uid,1,0))
        listing = query(GARAGE,'SELECT DISTINCT c.id,c.model')
        self.assertEqual(self.db.execute(listing % 1).fetchall(), [])
        self.db.execute(query(INV,'DELETE FROM inventory_items WHERE type=2'))
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_items').fetchone()[0], 1)
        self.db.execute(query(INV,'UPDATE inventory_items SET owner=%d') % (2,0,uid))
        self.assertEqual(self.db.execute(listing % 2).fetchall(), [(1,411)])
        self.assertEqual(self.db.execute(listing % 1).fetchall(), [])

    def test_duplicate_keys_do_not_duplicate_car_menu_entries(self):
        uid, _ = self.grant()
        self.db.execute(query(INV, 'INSERT INTO inventory_items') % (1, 1, 2, uid))
        self.assertEqual(self.db.execute(query(GARAGE,'SELECT DISTINCT c.id,c.model') % 1).fetchall(), [(1,411)])

    def test_failed_grant_rolls_back_both_car_and_key(self):
        self.db.execute('SAVEPOINT garage_give')
        self.grant()
        with self.assertRaises(sqlite3.IntegrityError):
            self.grant()  # Same owner and slot.
        self.db.execute('ROLLBACK TO garage_give'); self.db.execute('RELEASE garage_give')
        for table in ('inventory_items','owned_cars','permanent_keys'):
            self.assertEqual(self.db.execute(f'SELECT COUNT(*) FROM {table}').fetchone()[0], 0)

    def test_offline_accounts_key_lives_in_inventory_before_login(self):
        uid, _ = self.grant(owner=876, model=560)
        self.assertEqual(self.db.execute('SELECT slot,type,data FROM inventory_items WHERE owner=876').fetchone(), (0,2,uid))
        self.assertEqual(self.db.execute(query(GARAGE,'SELECT DISTINCT c.id,c.model') % 876).fetchall(), [(1,560)])

class GarageSourceContractsTest(unittest.TestCase):
    def test_durable_grant_is_admin_only_and_publishes_after_commit(self):
        grant = GARAGE.split('stock RW_GarageGive',1)[1].split('stock RW_GarageExit',1)[0]
        self.assertIn('gAdminLevel[playerid] < 1', grant)
        self.assertIn('RW_ValidNick(targetText)', grant)
        self.assertIn('RW_SafeSqlText(targetText)', grant)
        self.assertIn('weight+100 > INV_MAIN_GRAMS', grant)
        self.assertLess(grant.index('RELEASE garage_give'), grant.index('RW_InvLoad(target)'))
        self.assertIn('ROLLBACK TO garage_give', grant)
        self.assertIn('target = strval(targetText)', grant)

    def test_recall_revalidates_key_proximity_and_spawn_pad(self):
        spawn = GARAGE.split('stock RW_GarageSpawn',1)[1].split('stock RW_GarageClick',1)[0]
        for required in ('RW_GarageNear(playerid)', 'RW_InvHasToken(playerid, token)', 'IsPlayerInAnyVehicle(p)', 'Spawn pad blocked', 'RW_Veh_GetFreeSlot', 'gVehSpawnStamp[slot] = ++gVehStampCounter'):
            self.assertIn(required, spawn)
        self.assertIn('GetPlayerInterior(playerid) == 0', GARAGE)
        self.assertIn('GetPlayerVirtualWorld(playerid) == 0', GARAGE)

    def test_permanent_entity_deletion_does_not_revoke_key(self):
        vehicles = source('vehicles')
        self.assertIn('if (!gVehGarageId[slot]) RW_InvRevokeKey', vehicles)
        self.assertIn('gVehSpawnStamp[slot] == token', vehicles)
        # Marked permanent copies and temporary grants share the existing item schema.
        self.assertIn('RW_GarageMarkKey(uid, data)', INV)
        self.assertIn('data NOT IN (SELECT token FROM owned_cars)', INV)

    def test_pickup_and_proximity_use_latch_and_textdraw_view(self):
        self.assertIn('CreateObject(3881', GARAGE)
        self.assertIn('CreatePickup(1239, 1', GARAGE)
        # The booth and pickup stand next to the spawn point, not far away.
        self.assertIn('#define GARAGE_X (STATION_X + 2.5)', GARAGE)
        self.assertIn('#define GARAGE_Y (STATION_Y + 2.4)', GARAGE)
        self.assertIn('CreateObject(3881, GARAGE_BOOTH_X, GARAGE_BOOTH_Y, GARAGE_BOOTH_Z', GARAGE)
        self.assertIn('Create3DTextLabel("STATION GARAGE', GARAGE)
        self.assertIn('Delete3DTextLabel(gGarageLabel)', GARAGE)
        self.assertIn('IsPlayerInRangeOfPoint(playerid, GARAGE_RADIUS', GARAGE)
        self.assertIn('gGarageLatch[playerid]', GARAGE)
        self.assertIn('IsPlayerInRangeOfPoint(p, 2.0', GARAGE)
        self.assertIn('UI_GARAGE', source('ui'))
        self.assertIn('RW_UI_SelectBegin', GARAGE)
        self.assertNotIn('ShowPlayerDialog', GARAGE)

    def test_context_menu_has_conditional_actions_and_stays_in_budget(self):
        td_count = int(re.search(r'#define INV_TDS\s+(\d+)', INV)[1])
        self.assertLessEqual(td_count+24+13+2, 256)
        for label in ('"TO BAG"','"TO MAIN"','"DROP"','"WEAR"','"USE KEY"'):
            self.assertIn(label, INV)
        context = INV.split('stock RW_InvContextRender',1)[1].split('stock RW_InvRender',1)[0]
        self.assertIn('RW_InvPack(playerid)>=0 && type!=ITEM_BACKPACK', context)
        self.assertIn('gInvFromGround[playerid] || slot>=INV_MAIN', context)
        self.assertIn('RW_GarageKeyModel', context)
        self.assertIn('gInvContext[playerid] = true', INV)

    def test_system_display_literals_are_english_ascii(self):
        paths = [ROOT/'server/gamemodes/railway.pwn', *list((ROOT/'server/pawno/include/railway').glob('*.inc'))]
        for path in paths:
            for text in re.findall(r'"([^"\r\n]*)"', path.read_text(encoding='cp1251')):
                self.assertTrue(text.isascii(), (path.name,text))

    def test_flight_and_tuning_authorization_lifecycle_and_units(self):
        for term in ('gAdminLevel[playerid] < 1','PLAYER_STATE_DRIVER','RW_Veh_CanDrive(playerid, v)','value > 500','value > 100'):
            self.assertIn(term, MOTION)
        for term in ('GetVehicleRotationQuat', 'SetVehicleAngularVelocity', 'KEY_ANALOG_UP','KEY_ANALOG_DOWN','KEY_LOOK_LEFT','KEY_LOOK_RIGHT','gMotionAccel[v]*dt', '/180.0', 'elapsed > 200'):
            self.assertIn(term, MOTION)
        self.assertIn('gAdminLevel[p] < 1', MOTION)
        self.assertIn('gMotionDriver[v] != p', MOTION)
        self.assertIn('gMotionFly[v] = false', MOTION)
        self.assertIn('RW_MotionReset(vehicleid)', source('vehicles'))

if __name__ == '__main__':
    unittest.main()
