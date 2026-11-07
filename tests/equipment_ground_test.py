"""Extracted SQLite transitions and source contracts; not a Pawn/GTA emulator."""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT=Path(__file__).resolve().parents[1]
def source(n):
    return (ROOT/'server/pawno/include/railway'/f'{n}.inc').read_text(encoding='cp1251')
INV=source('inventory')
GROUND=source('ground')
ARMOUR=source('armour')
def section(start,end):
    return INV.split(start,1)[1].split(end,1)[0]
def query(code,prefix):
    return next(s for s in re.findall(r'"([^"\r\n]+)"',code) if s.startswith(prefix))
EQUIP=section('stock RW_InvEquip(', 'stock RW_InvUnequip(')
UNEQUIP=section('stock RW_InvUnequip(', 'stock RW_InvFitCommand(')
MIGRATION=section('stock RW_InvEquipmentSchema(', 'stock RW_InvEquipmentLoad(')

class EquipmentSqlTest(unittest.TestCase):
    def setUp(self):
        self.db=sqlite3.connect(':memory:',isolation_level=None)
        self.db.execute(query(INV,'CREATE TABLE IF NOT EXISTS inventory_items'))
        self.db.execute(query(INV,'CREATE TABLE IF NOT EXISTS inventory_equipment'))
        self.db.execute("CREATE TABLE IF NOT EXISTS inventory_active (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute(query(ARMOUR,'CREATE TABLE IF NOT EXISTS inventory_wear'))
    def tearDown(self):
        self.db.close()
    def item(self,owner=1,slot=0,kind=1,data=0):
        self.db.execute(query(INV,'INSERT INTO inventory_items') % (owner,slot,kind,data))
        return self.db.execute('SELECT last_insert_rowid()').fetchone()[0]
    def equip(self,uid,owner=1,slot=0):
        self.db.execute('SAVEPOINT equip_backpack')
        self.db.execute(query(EQUIP,'INSERT INTO inventory_equipment') % (owner,uid))
        self.assertEqual(self.db.execute(query(EQUIP,'UPDATE inventory_items') % (uid,owner,slot)).rowcount,1)
        self.db.execute('RELEASE equip_backpack')
    def test_equip_moves_same_uid_and_frees_main_slot(self):
        uid=self.item(); self.equip(uid)
        self.assertEqual(self.db.execute('SELECT uid,owner,slot FROM inventory_items').fetchone(),(uid,None,-1))
        self.assertEqual(self.db.execute('SELECT owner,uid FROM inventory_equipment').fetchone(),(1,uid))
        self.item(slot=0,kind=2,data=123)  # Former MAIN slot really is free.
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_items').fetchone()[0],2)
    def test_equipment_is_not_ground_and_cannot_be_stolen_by_pickup_sql(self):
        uid=self.item(); self.equip(uid)
        self.assertEqual(self.db.execute(query(INV,'SELECT uid,type,data,amount,x,y,z')).fetchall(),[])
        take=query(INV,'UPDATE inventory_items SET owner=%d')
        self.assertEqual(self.db.execute(take % (2,0,uid)).rowcount,0)
    def test_unequip_moves_uid_back_without_copying(self):
        uid=self.item(); self.equip(uid)
        self.db.execute('SAVEPOINT unequip_backpack')
        self.assertEqual(self.db.execute(query(UNEQUIP,'UPDATE inventory_items') % (1,2,uid,1,uid)).rowcount,1)
        self.db.execute(query(UNEQUIP,'DELETE FROM inventory_equipment') % (1,uid))
        self.db.execute('RELEASE unequip_backpack')
        self.assertEqual(self.db.execute('SELECT uid,owner,slot FROM inventory_items').fetchall(),[(uid,1,2)])
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_equipment').fetchone()[0],0)
    def test_failed_equip_rolls_back_reference_and_does_not_move_another_owners_bag(self):
        uid=self.item(owner=2)
        self.db.execute('SAVEPOINT equip_backpack')
        self.db.execute(query(EQUIP,'INSERT INTO inventory_equipment') % (1,uid))
        self.assertEqual(self.db.execute(query(EQUIP,'UPDATE inventory_items') % (uid,1,0)).rowcount,0)
        self.db.execute('ROLLBACK TO equip_backpack'); self.db.execute('RELEASE equip_backpack')
        self.assertEqual(self.db.execute('SELECT owner,slot FROM inventory_items').fetchone(),(2,0))
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_equipment').fetchone()[0],0)
    def test_legacy_occupied_bag_migration_preserves_items_and_does_not_auto_equip_new_bags(self):
        legacy=self.item(owner=1); key=self.item(owner=1,slot=50,kind=2,data=800)
        fresh=self.item(owner=2)
        for _ in range(2):  # Idempotent on later restarts.
            self.db.execute(query(MIGRATION,'INSERT INTO inventory_equipment'))
            self.db.execute(query(MIGRATION,'UPDATE inventory_items'))
        self.assertEqual(self.db.execute('SELECT owner,uid FROM inventory_equipment').fetchall(),[(1,legacy)])
        self.assertEqual(self.db.execute('SELECT uid FROM inventory_items ORDER BY uid').fetchall(),[(legacy,),(key,),(fresh,)])
        self.assertEqual(self.db.execute('SELECT owner,slot FROM inventory_items WHERE uid=?',(fresh,)).fetchone(),(2,0))
        self.assertEqual(self.db.execute('SELECT owner,slot FROM inventory_items WHERE uid=?',(key,)).fetchone(),(1,50))
    def test_one_backpack_per_owner_and_one_owner_per_uid(self):
        first=self.item(); second=self.item(slot=1); self.equip(first)
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(query(EQUIP,'INSERT INTO inventory_equipment') % (1,second))
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(query(EQUIP,'INSERT INTO inventory_equipment') % (2,first))

class EquipmentSourceTest(unittest.TestCase):
    def test_capacity_requires_equipped_bag_and_unequip_is_guarded(self):
        pack=section('stock RW_InvPack(', 'stock RW_InvWeight(')
        self.assertIn('gInvEquipped[playerid]>0',pack)
        self.assertNotIn('for (',pack) # Merely carrying a backpack is not enough.
        self.assertIn('RW_InvWeight(playerid,1)>0',UNEQUIP)
        self.assertIn('dest<0',UNEQUIP)
        self.assertLess(EQUIP.index('RELEASE equip_backpack'),EQUIP.index('gInvEquipped[playerid]=uid'))
        self.assertLess(UNEQUIP.index('RELEASE unequip_backpack'),UNEQUIP.index('gInvEquipped[playerid]=0'))
        self.assertIn('gInvEquipped[playerid]>0) weight +=',INV)
    def test_equipment_slot_is_selectable_and_uses_real_equip_action(self):
        # слот рюкзака — ячейка 131 сетки; клик по ней выбирает вещь
        grid = section('stock RW_InvCef(playerid)', '\nstock ')
        self.assertIn('RW_InvCefCell(playerid, 131,', grid)
        click = section('stock RW_InvCefClick(', '\nstock ')
        self.assertIn('gInvSelected[playerid] = gInvEquipped[playerid]', click)
        context = section('stock RW_InvCefContext(playerid, slot)', '\nstock ')
        self.assertIn('uid = gInvEquipped[playerid]', context)
        self.assertIn('return RW_InvEquip(playerid)', section('stock RW_InvWearSelected(playerid)', '\nstock '))
        self.assertIn('return RW_WpUnequip(playerid)', section('stock RW_InvUseSelected(playerid)', '\nstock '))
    def test_backpack_fit_not_sideways_and_saved_per_skin_with_validation(self):
        attach=section('stock RW_InvAttach(', 'stock bool:RW_InvWrite(')
        self.assertIn('Float:rx,Float:ry,Float:rz',attach)
        self.assertNotIn('90.0',attach)
        self.assertIn('3026, 1, x,y,z,rx,ry,rz,1.0,1.0,1.0',attach)
        self.assertIn('AND skin=%d',attach)
        fit=INV.split('stock RW_InvFitSave',1)[1]
        for term in ('EDIT_RESPONSE_UPDATE','gAdminLevel[playerid]>=1','gInvFitUid[playerid]','gInvFitSkin[playerid]','x>=-0.6','PRIMARY KEY(owner,skin)'):
            self.assertIn(term,fit if term!='PRIMARY KEY(owner,skin)' else INV)
    def test_ground_hint_uses_camera_ray_and_shared_validated_pickup(self):
        for term in ('GetPlayerCameraPos','GetPlayerCameraFrontVector','gUIMode[playerid]!=0','PLAYER_STATE_ONFOOT','GetPlayerInterior(playerid)!=0','GetPlayerVirtualWorld(playerid)!=0','along<0.0','side>radius*radius'):
            self.assertIn(term,GROUND)
        self.assertIn('RW_InvTransfer(playerid,section)',GROUND)
        self.assertIn('RW_GroundTarget(playerid)',GROUND.split('stock RW_GroundTake',1)[1])
        main=(ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertIn('KEY_SECONDARY_ATTACK',main)
        self.assertIn('gGroundAim[playerid]>0) return RW_GroundTake',main)
        self.assertIn('RW_GroundReset(playerid)',source('ui'))
        self.assertIn('"take"',source('commands'))
        self.assertNotIn('[E]',GROUND) # No false claim of arbitrary physical-key support.
    def test_key_receipts_use_identity_instead_of_connection_last_rowid(self):
        grant=section('stock RW_InvGrant(', 'stock bool:RW_InvHasKey(')
        self.assertNotIn('last_insert_rowid',grant)
        self.assertIn('WHERE owner=%d AND slot=%d AND type=%d',grant)
        self.assertIn('RW_InvKeyTokenKnown(data)',grant)
        give=source('garage').split('stock RW_GarageGive(',1)[1].split('stock RW_GarageExit',1)[0]
        self.assertNotIn('last_insert_rowid',give)
        self.assertIn('WHERE token=%d AND owner=%d',give)
        self.assertIn('weight+=equipped*RW_InvGrams(ITEM_BACKPACK)',give)
        self.assertIn('if(changed && gUIMode[p]==UI_INVENTORY) RW_InvRender(p)',INV)

if __name__=='__main__':
    unittest.main()
