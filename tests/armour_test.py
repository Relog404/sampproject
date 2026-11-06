"""Body armour: a separate inventory item, worn on the player.

SQL transitions are executed for real; the rest are source contracts.
No Pawn VM and no GTA client is available here.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(n):
    return (ROOT / 'server/pawno/include/railway' / f'{n}.inc').read_text(encoding='cp1251')

ARM = pawn('armour')
INV = pawn('inventory')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def strings(code):
    return re.findall(r'"([^"\r\n]+)"', code)


def first(code, prefix):
    return next(s for s in strings(code) if s.startswith(prefix))


def section(code, start, end):
    return code.split(start, 1)[1].split(end, 1)[0]


class ArmourSqlTest(unittest.TestCase):
    """The wearable vest: one per account, never a ground item."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(first(INV, 'CREATE TABLE IF NOT EXISTS inventory_items'))
        self.db.execute(first(INV, 'CREATE TABLE IF NOT EXISTS inventory_equipment'))
        self.db.execute("CREATE TABLE inventory_active (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")
        self.db.execute(first(ARM, 'CREATE TABLE IF NOT EXISTS inventory_wear'))
        self.db.execute(first(ARM, 'CREATE TABLE IF NOT EXISTS armour_fit'))

    def tearDown(self):
        self.db.close()

    def item(self, owner=1, slot=0, kind=7, data=2):
        self.db.execute("INSERT INTO inventory_items(owner,slot,type,data) VALUES(?,?,?,?)", (owner, slot, kind, data))
        return self.db.execute('SELECT last_insert_rowid()').fetchone()[0]

    def wear(self, uid, owner=1, slot=0, hp=100):
        self.db.execute("SAVEPOINT wear_armour")
        self.db.execute(first(ARM, 'INSERT INTO inventory_wear') % (owner, uid, hp))
        self.db.execute(first(ARM, 'UPDATE inventory_items SET owner=NULL,slot=-1') % (uid, owner, slot, 7))
        self.db.execute("RELEASE wear_armour")

    def ground_query(self):
        return next(s for s in strings(INV) if s.startswith('SELECT uid,type,data,amount,x,y,z FROM inventory_items'))

    def test_schema_keeps_one_vest_per_account(self):
        tables = [row[0] for row in self.db.execute("SELECT name FROM sqlite_master WHERE type='table'")]
        self.assertIn('inventory_wear', tables)
        self.assertIn('armour_fit', tables)
        self.db.execute(first(ARM, 'INSERT INTO inventory_wear') % (1, 10, 100))
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(first(ARM, 'INSERT INTO inventory_wear') % (1, 11, 50))   # one vest per owner
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(first(ARM, 'INSERT INTO inventory_wear') % (2, 10, 50))   # one owner per vest

    def test_worn_armour_is_not_a_ground_item(self):
        uid = self.item()
        self.wear(uid)
        self.assertEqual(self.db.execute(self.ground_query()).fetchall(), [])

    def test_ground_row_stays_when_it_is_not_the_worn_one(self):
        uid = self.item()
        self.wear(uid)
        other = self.item(owner=None, slot=-1)
        self.assertEqual([row[0] for row in self.db.execute(self.ground_query()).fetchall()], [other])

    def test_unwear_only_touches_the_row_the_player_wears(self):
        uid = self.item()
        self.wear(uid)
        sql = first(ARM, 'UPDATE inventory_items SET owner=%d,slot=%d WHERE uid=%d AND owner IS NULL AND slot=-1')
        self.assertEqual(self.db.execute(sql % (1, 4, uid, 1, uid)).rowcount, 1)
        self.assertEqual(self.db.execute(sql % (2, 4, uid, 1, uid)).rowcount, 0)   # wrong owner

    def test_taking_off_removes_the_wear_row(self):
        uid = self.item()
        self.wear(uid)
        self.assertEqual(self.db.execute(first(ARM, 'DELETE FROM inventory_wear WHERE owner') % (1, uid)).rowcount, 1)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_wear').fetchone()[0], 0)

    def test_broken_armour_leaves_nothing_behind(self):
        uid = self.item()
        self.wear(uid)
        self.db.execute(first(ARM, 'DELETE FROM inventory_wear WHERE owner') % (1, uid))
        self.db.execute(first(ARM, 'DELETE FROM inventory_items WHERE uid=%d AND owner IS NULL AND slot=-1') % (uid, 7))
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_wear').fetchone()[0], 0)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM inventory_items').fetchone()[0], 0)

    def test_hit_points_are_persisted(self):
        uid = self.item()
        self.wear(uid)
        self.db.execute(first(ARM, 'UPDATE inventory_wear SET hp') % (37, 1, uid))
        self.assertEqual(self.db.execute('SELECT hp FROM inventory_wear WHERE owner=1').fetchone()[0], 37)

    def test_fit_is_stored_per_skin(self):
        self.db.execute(first(ARM, 'REPLACE INTO armour_fit') % (1, 101, 0.1, 0.2, 0.3, 1.0, 2.0, 3.0))
        self.db.execute(first(ARM, 'REPLACE INTO armour_fit') % (1, 102, 0.4, 0.5, 0.6, 4.0, 5.0, 6.0))
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM armour_fit WHERE owner=1').fetchone()[0], 2)
        self.assertEqual(self.db.execute('SELECT z FROM armour_fit WHERE owner=1 AND skin=102').fetchone()[0], 0.6)


class ArmourSourceTest(unittest.TestCase):
    def test_item_type_is_registered(self):
        self.assertIn('#define ITEM_ARMOUR  7', INV)
        self.assertIn('#define INV_TYPE_MAX 7', INV)
        self.assertIn('if (type == ITEM_ARMOUR) return RW_ArmourModel(data);', INV)
        self.assertIn('if (type == ITEM_ARMOUR) return RW_ArmourGrams(data);', INV)
        self.assertIn('if (gInvWorn[playerid]>0) weight += RW_InvGrams(ITEM_ARMOUR, gArmourData[playerid]);', INV)

    def test_variants(self):
        for fn, values in (('RW_ArmourHpOf', ('50', '100', '150')),
                           ('RW_ArmourAbsorb', ('50', '65', '80')),
                           ('RW_ArmourGrams', ('3000', '5000', '7000'))):
            body = section(ARM, 'stock %s(variant)' % fn, '\nstock ')
            for value in values:
                self.assertIn('return %s;' % value, body)

    def test_art_comes_from_the_skin_registry(self):
        """The shape and the texture live in skins.inc, not here."""
        self.assertIn('stock RW_ArmourModel(variant) { return RW_SkinModel(variant); }', ARM)
        self.assertIn('stock RW_ArmourName(variant, dest[], size = sizeof dest) { return RW_SkinLabel(variant, dest, size); }', ARM)
        self.assertIn('stock bool:RW_ArmourVariant(variant) { return RW_SkinValid(variant); }', ARM)
        self.assertNotIn('RW_ArmourTxd', ARM)
        self.assertNotIn('RW_ArmourTexture', ARM)

    def test_named_texture_uses_a_server_object(self):
        attach = section(ARM, 'stock RW_ArmourAttach(playerid', '\nstock ')
        self.assertIn('if (RW_SkinTextured(gArmourData[playerid]) && !gArmourFitting[playerid])', attach)
        self.assertIn('RW_SkinApply(object, gArmourData[playerid]);', attach)
        self.assertIn('AttachObjectToPlayer(object, playerid, x, y, z, rx, ry, rz);', attach)
        self.assertIn('CreateObject(RW_ArmourModel(gArmourData[playerid]), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 300.0)', attach)
        # no material without a real object
        self.assertNotIn('SetObjectMaterial', ARM)
        self.assertIn('SetPlayerAttachedObject(playerid, ARMOUR_ATTACH', attach)

    def test_damage_is_absorbed_and_healed_back(self):
        damage = section(ARM, 'stock RW_ArmourDamage', '\nstock ')
        self.assertIn('floatround(amount * float(RW_ArmourAbsorb(gArmourData[playerid])) / 100.0)', damage)
        self.assertIn('GetPlayerHealth(playerid, hp);', damage)
        self.assertIn('SetPlayerHealth(playerid, hp);', damage)
        self.assertIn('SetPlayerArmour(playerid, float(gArmourHp[playerid]));', damage)
        self.assertIn('if (gArmourHp[playerid] <= 0) return RW_ArmourBreak(playerid);', damage)
        self.assertIn('public OnPlayerTakeDamage(playerid, issuerid, Float:amount, weaponid, bodypart)', MAIN)
        self.assertIn('RW_ArmourDamage(playerid, amount);', MAIN)

    def test_downed_player_gets_no_protection(self):
        self.assertIn('if (gDowned[playerid] || amount <= 0.0) return 0;', section(ARM, 'stock RW_ArmourDamage', '\nstock '))

    def test_load_validates_the_row(self):
        load = section(ARM, 'stock RW_ArmourLoad', '\nstock ')
        self.assertIn('type != ITEM_ARMOUR', load)
        self.assertIn('!RW_ArmourVariant(data)', load)
        self.assertIn('hp > maximum ? maximum : hp', load)

    def test_inventory_card(self):
        self.assertIn('RW_InvCell(playerid,133,gInvWorn[playerid]>0 ? ITEM_ARMOUR : 0', INV)
        self.assertIn('PlayerTextDrawSetSelectable(playerid,gInvTD[playerid][133],1);', INV)
        self.assertIn('Put body armour here: select it in MAIN and press WEAR.', INV)
        self.assertIn('if(slot>=0 && gInv[playerid][slot][iiType]==ITEM_ARMOUR) return RW_ArmourWear(playerid, slot);', INV)
        self.assertIn('if(gInvSelected[playerid]==gInvWorn[playerid]) return RW_ArmourRemove(playerid);', INV)
        self.assertIn('gInvSelected[playerid] == gInvWorn[playerid] ? ("TAKE OFF") : ("WEAR")', INV)

    def test_admin_commands(self):
        self.assertIn('if (!strcmp(cmd, "armfit", true)) return RW_ArmourFitCommand(playerid);', CMDS)
        self.assertIn('/armfit', CMDS)
        self.assertIn('armour [0-2]', INV)
        self.assertIn('Armour skins: 0 standard, 1 light, 2 heavy.', INV)
        self.assertIn('"7", true) || !strcmp(word, "armour", true)', INV)

    def test_fit_editor_routes_slot_eight(self):
        self.assertIn('if (index == 8) return RW_ArmourFitSave(', MAIN)
        self.assertIn('EditAttachedObject(playerid, ARMOUR_ATTACH);', ARM)
        self.assertIn('REPLACE INTO armour_fit(owner,skin,x,y,z,rx,ry,rz)', ARM)

    def test_dropped_items_keep_the_texture(self):
        self.assertIn('RW_SkinApply(gGround[i][igObject], RW_ItemSkin(gGround[i][igType], gGround[i][igData]));', INV)
        self.assertIn('RW_SkinApply(object, RW_ItemSkin(gInv[playerid][source][iiType], gInv[playerid][source][iiData]));', INV)


if __name__ == '__main__':
    unittest.main()
