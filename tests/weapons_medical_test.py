"""Weapons, ammunition and medical SQL/source contracts. No Pawn VM or GTA run."""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT=Path(__file__).resolve().parents[1]
def pawn(n): return (ROOT/'server/pawno/include/railway'/f'{n}.inc').read_text(encoding='cp1251')
INV=pawn('inventory')
WEP=pawn('weapons')
MED=pawn('medical')
ARM=pawn('armour')
MAIN=(ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
def strings(code):
    return re.findall(r'"([^"\r\n]+)"', code)
def first(code, prefix):
    return next(s for s in strings(code) if s.startswith(prefix))

class ItemUpgradeTest(unittest.TestCase):
    """The old table blocks extra types with CHECK(type IN (1,2))."""
    OLD = ("CREATE TABLE inventory_items (uid INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER, slot INTEGER NOT NULL, "
           "type INTEGER NOT NULL CHECK(type IN (1,2)), data INTEGER NOT NULL DEFAULT 0, x REAL NOT NULL DEFAULT 0, "
           "y REAL NOT NULL DEFAULT 0, z REAL NOT NULL DEFAULT 0, UNIQUE(owner,slot), "
           "CHECK((owner IS NULL AND slot=-1) OR (owner>0 AND slot>=0 AND slot<80)))")
    def setUp(self):
        self.db=sqlite3.connect(':memory:',isolation_level=None)
        self.db.execute(self.OLD)
        self.db.execute("CREATE TABLE inventory_equipment (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute("CREATE TABLE inventory_active (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute(first(ARM,'CREATE TABLE IF NOT EXISTS inventory_wear'))
        for owner,slot,type_,data in ((1,0,1,0),(1,1,2,55),(2,5,2,77),(None,-1,1,0)):
            self.db.execute("INSERT INTO inventory_items(owner,slot,type,data) VALUES(?,?,?,?)",(owner,slot,type_,data))
    def tearDown(self):
        self.db.close()
    def upgrade(self):
        rename = next(s for s in strings(WEP) if s.startswith('ALTER TABLE inventory_items'))
        create = next(s for s in strings(WEP) if s.startswith('CREATE TABLE inventory_items ('))
        copy = next(s for s in strings(WEP) if s.startswith('INSERT INTO inventory_items(uid,owner,slot,type,data,amount,x,y,z)'))
        verify = next(s for s in strings(WEP) if s.startswith('SELECT (SELECT COUNT(*) FROM inventory_items_prev)'))
        self.db.execute(rename)
        self.db.execute(create)
        self.db.execute(copy)
        row=self.db.execute(verify).fetchone()
        return row  # old_count, new_count, lost
    def test_upgrade_preserves_every_row_and_adds_amount(self):
        old_count,new_count,lost = self.upgrade()
        self.assertEqual((old_count,new_count,lost),(4,4,0))
        self.assertEqual(self.db.execute("SELECT uid,owner,slot,type,data,amount FROM inventory_items ORDER BY uid").fetchall(),
                         [(1,1,0,1,0,1),(2,1,1,2,55,1),(3,2,5,2,77,1),(4,None,-1,1,0,1)])
        self.db.execute("DROP TABLE inventory_items_prev")
        # New types are now storable, which the old CHECK rejected.
        self.db.execute("INSERT INTO inventory_items(owner,slot,type,data,amount) VALUES(9,3,3,31,1)")
        self.db.execute("INSERT INTO inventory_items(owner,slot,type,data,amount) VALUES(9,4,4,4,60)")
    def test_old_schema_is_detected_by_its_definition(self):
        self.assertEqual(first(WEP,'SELECT sql FROM sqlite_master'),
                         "SELECT sql FROM sqlite_master WHERE type='table' AND name='inventory_items'")
        self.assertIn('strfind(def, "amount", true)', WEP)
        self.assertIn('strfind(def, "CHECK(type IN", true)', WEP)
        self.assertIn('DROP TABLE inventory_items_prev', WEP)
        self.assertIn('ROLLBACK TO inventory_upgrade', WEP)

class WeaponSqlTest(unittest.TestCase):
    def setUp(self):
        self.db=sqlite3.connect(':memory:',isolation_level=None)
        self.db.execute(first(INV,'CREATE TABLE IF NOT EXISTS inventory_items'))
        self.db.execute("CREATE TABLE inventory_equipment (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute("CREATE TABLE inventory_active (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute(first(ARM,'CREATE TABLE IF NOT EXISTS inventory_wear'))
    def tearDown(self):
        self.db.close()
    def item(self,owner=1,slot=0,type_=3,data=31,amount=7):
        self.db.execute("INSERT INTO inventory_items(owner,slot,type,data,amount) VALUES(?,?,?,?,?)",(owner,slot,type_,data,amount))
        return self.db.execute("SELECT last_insert_rowid()").fetchone()[0]
    def equip(self):
        return (first(WEP,'UPDATE inventory_items SET owner=NULL,slot=-1 WHERE uid=%d AND owner=%d AND slot=%d'),
                first(WEP,'INSERT INTO inventory_active(owner,uid) VALUES'))
    def test_equipped_weapon_is_not_a_ground_item_and_cannot_be_stolen(self):
        uid=self.item()
        update,insert=self.equip()
        self.db.execute("SAVEPOINT weapon_equip")
        self.assertEqual(self.db.execute(update % (uid,1,0)).rowcount,1)
        self.db.execute(insert % (1,uid))
        self.db.execute("RELEASE weapon_equip")
        self.assertEqual(self.db.execute("SELECT COUNT(*) FROM inventory_items WHERE owner IS NULL AND uid NOT IN(SELECT uid FROM inventory_active)").fetchone()[0],0)
        take=first(INV,'UPDATE inventory_items SET owner=%d,slot=%d,x=0,y=0,z=0 WHERE uid=%d AND owner IS NULL')
        self.assertEqual(self.db.execute(take % (2,0,uid)).rowcount,0)
    def test_unequip_returns_the_same_uid_to_the_grid(self):
        uid=self.item()
        update,insert=self.equip()
        self.db.execute("SAVEPOINT weapon_equip"); self.db.execute(update % (uid,1,0)); self.db.execute(insert % (1,uid)); self.db.execute("RELEASE weapon_equip")
        back=first(WEP,'UPDATE inventory_items SET owner=%d,slot=%d,amount=%d WHERE uid=%d AND owner IS NULL AND slot=-1')
        self.assertEqual(self.db.execute(back % (1,3,7,uid)).rowcount,1)
        self.assertEqual(self.db.execute("DELETE FROM inventory_active WHERE owner=%d AND uid=%d" % (1,uid)).rowcount,1)
        self.assertEqual(self.db.execute("SELECT uid,owner,slot,amount FROM inventory_items").fetchone(),(uid,1,3,7))
    def test_one_active_weapon_per_account(self):
        uid=self.item(); other=self.item(slot=1)
        update,insert=self.equip()
        self.db.execute("SAVEPOINT weapon_equip"); self.db.execute(update % (uid,1,0)); self.db.execute(insert % (1,uid)); self.db.execute("RELEASE weapon_equip")
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(insert % (1,other))
    def test_ammo_consumption_updates_guard_against_overspend(self):
        uid=self.item(type_=4,data=4,amount=60)
        spend=first(WEP,'UPDATE inventory_items SET amount=amount-%d')
        self.assertEqual(self.db.execute(spend % (30,uid,1,0,4,30)).rowcount,1)
        self.assertEqual(self.db.execute("SELECT amount FROM inventory_items WHERE uid=?",(uid,)).fetchone()[0],30)
        self.assertEqual(self.db.execute(spend % (40,uid,1,0,4,40)).rowcount,0)   # not enough rounds

class WeaponSourceTest(unittest.TestCase):
    def test_calibers_and_magazines_are_defined_per_weapon(self):
        for term in ('case 22, 23, 28, 29, 32: return CAL_9MM','case 25, 26, 27: return CAL_12GA','case 31: return CAL_556','case 30, 33: return CAL_762','case 34: return CAL_308'):
            self.assertIn(term,WEP)
        self.assertIn('case 22, 23: return 17',WEP)
        self.assertIn('case 31: return 50',WEP)
        self.assertIn('stock RW_WeaponGrams(weaponid)',WEP)
        self.assertIn('stock RW_CaliberGrams(cal)',WEP)
    def test_weapon_only_leaves_the_inventory_through_the_active_slot(self):
        self.assertIn('ResetPlayerWeapons(playerid)',WEP)
        self.assertIn('GivePlayerWeapon(playerid, gWpWeapon[playerid], gWpAmmo[playerid])',WEP)
        self.assertIn('Unequip the current weapon first',WEP)
        self.assertIn('gWpActive[playerid] > 0 && gInvSelected[playerid] == gWpActive[playerid]',INV)
        self.assertIn('Unequip the weapon first',INV)
        self.assertIn('RW_WpShot(playerid, weaponid)',MAIN)
        self.assertIn('SetPlayerAmmo(playerid, weaponid, gWpAmmo[playerid])',WEP)
        self.assertIn('RW_WpFlush(p)',INV)          # rounds are persisted without a write per bullet
    def test_weight_and_previews_cover_the_new_types(self):
        self.assertIn('stock RW_InvGrams(type, data = 0, amount = 1)',INV)
        self.assertIn('if (type == ITEM_AMMO) return amount * RW_CaliberGrams(data);',INV)
        self.assertIn('if (type == ITEM_WEAPON) return RW_WeaponModel(data);',INV)
        self.assertIn('RW_WpCarriedGrams(playerid)',INV)

class MedicalSourceTest(unittest.TestCase):
    def test_bandage_rules(self):
        self.assertIn('#define BANDAGE_HP     (15)',MED)
        self.assertIn('#define MED_USE_MS     (3000)',MED)
        self.assertIn('#define MED_COOLDOWN   (1000)',MED)
        self.assertIn('ApplyAnimation(playerid, "BOMBER", "BOM_Plant"',MED)
        self.assertIn('SetTimerEx("RW_MedBandageDone", MED_USE_MS',MED)
        self.assertIn('Wait a second between bandages',MED)
        self.assertIn('healed > 100.0',MED)
    def test_downed_state_timers_and_the_ambulance_call(self):
        self.assertIn('#define DOWN_MS        (180000)',MED)
        self.assertIn('#define DOWN_EXTRA_MS  (420000)',MED)
        self.assertIn('RW_UI_Row(playerid, 0, "Вызвать скорую", "band");',MED)
        # кнопка больше не заглушка: она создаёт вызов 911 для скорой
        self.assertIn('RW_EmsCall(playerid, 3, gDownX[playerid], gDownY[playerid], gDownZ[playerid])',MED)
        self.assertIn('The ambulance is on its way',MED)
        self.assertIn('You hold on for 7 more minutes',MED)
        self.assertIn('SetTimerEx("RW_MedBleedOut", DOWN_MS',MED)
        self.assertIn('if (RW_MedSpawn(playerid)) return 1;',MAIN)
        self.assertIn('RW_MedDown(playerid, dx, dy, dz)',MAIN)
        self.assertIn('RW_MedPulse();',INV)
        self.assertIn('UI_DOWNED',pawn('ui'))
        self.assertIn('if (gDowned[playerid]) return RW_Error(playerid, "You are downed and cannot reach your backpack.");',INV)
    def test_the_downed_panel_shows_the_call_instead_of_hiding_it(self):
        """/calls печатал номер вызова в чат: теперь он виден на панели."""
        self.assertIn('"Вызов №%d: скорая в пути"', MED)
        self.assertIn('gDownCall[playerid] = id;', MED)
        self.assertIn('RW_MedRender(playerid);', MED)
        self.assertIn('your call is on the panel', MED)
        self.assertIn('gDownCall[playerid] = 0;', MED)
        self.assertIn('new gDownCall[MAX_PLAYERS];', pawn('core'))

    def test_syringe_revives_a_downed_player_nearby(self):
        syringe=MED.split('stock RW_UseSyringe',1)[1].split('public RW_MedSyringeDone',1)[0]
        self.assertIn('No downed player within 3 metres',syringe)
        self.assertIn('SetTimerEx("RW_MedSyringeDone", MED_USE_MS',syringe)
        self.assertIn('RW_MedRevive(target)',MED)
        self.assertIn('RW_EmsShakeStart(target)',MED)   # адреналин: 10 секунд тряски
        self.assertIn('#define REVIVE_HP      (30.0)',MED)
        self.assertIn('You are downed. Someone else must inject you',syringe)

if __name__=='__main__':
    unittest.main()
