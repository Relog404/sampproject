"""Client-side clothing textures: the files stay in the player's arm/ folder,
the server only names the TXD and the texture inside it.
Names can be changed in game (/skin), so they live in the database too.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(n):
    return (ROOT / 'server/pawno/include/railway' / f'{n}.inc').read_text(encoding='cp1251')

SKINS = pawn('skins')
CORE = pawn('core')
ARM = pawn('armour')
INV = pawn('inventory')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def body(start, end='\nstock '):
    return SKINS.split(start, 1)[1].split(end, 1)[0]


class SkinRegistryTest(unittest.TestCase):
    def test_every_entry_is_described_in_one_place(self):
        for fn in ('RW_SkinModel', 'RW_SkinTxdCfg', 'RW_SkinTextureCfg', 'RW_SkinLabel'):
            text = body('stock %s(id' % fn)
            self.assertEqual(text.count('case '), 3, fn)
        self.assertIn('stock RW_SkinCount() { return 3; }', SKINS)

    def test_txd_names_carry_no_extension(self):
        txd = body('stock RW_SkinTxdCfg(id')
        for name in re.findall(r'format\(dest, size, "([^"]*)"\)', txd):
            self.assertNotIn('.', name, 'TXD names go without the extension')

    def test_compiled_default_for_the_armour(self):
        """arm/war1.txd is the file the player keeps in the game folder."""
        self.assertIn('format(dest, size, "war1");', body('stock RW_SkinTxdCfg(id'))
        self.assertIn('format(dest, size, "armour");', body('stock RW_SkinTextureCfg(id'))

    def test_effective_names_come_from_the_cache(self):
        self.assertIn('return format(dest, size, "%s", gSkinTxd[id]);', body('stock RW_SkinTxd(id'))
        self.assertIn('return format(dest, size, "%s", gSkinTexture[id]);', body('stock RW_SkinTexture(id'))
        self.assertIn('return gSkinTxd[id][0] && gSkinTexture[id][0];', body('stock bool:RW_SkinTextured(id)'))

    def test_apply_uses_the_shape_of_the_same_entry(self):
        apply_body = body('stock RW_SkinApply(objectid')
        self.assertIn('if (objectid <= 0 || !RW_SkinTextured(id)) return 0;', apply_body)
        self.assertIn('SetObjectMaterial(objectid, 0, RW_SkinModel(id), gSkinTxd[id], gSkinTexture[id], 0);', apply_body)

    def test_bounds(self):
        self.assertIn('stock bool:RW_SkinValid(id) { return id >= 0 && id < RW_SkinCount() && id < SKIN_MAX; }', SKINS)
        self.assertIn('#define SKIN_MAX (16)', CORE)   # needed by the globals in core.inc
        self.assertIn('new gSkinTxd[SKIN_MAX][32];', CORE)
        self.assertIn('new gSkinTexture[SKIN_MAX][32];', CORE)

    def test_items_point_at_a_skin(self):
        item = body('stock RW_ItemSkin(type')
        self.assertIn('if (type == ITEM_ARMOUR) return data;', item)
        self.assertIn('return -1;', item)

    def test_names_are_sanitised_before_sql(self):
        check = body('stock bool:RW_SkinName(const value[])')
        self.assertIn("if (!value[0] || strlen(value) > 31) return false;", check)
        for allowed in ("c >= 'a' && c <= 'z'", "c >= 'A' && c <= 'Z'", "c >= '0' && c <= '9'", "c == '_'"):
            self.assertIn(allowed, check)
        command = body('stock RW_SkinCommand(playerid')
        self.assertIn('if (!RW_SkinName(b) || !RW_SkinName(c))', command)

    def test_skin_command(self):
        command = body('stock RW_SkinCommand(playerid')
        self.assertIn('REPLACE INTO skins(id,txd,texture)', command)
        self.assertIn('RW_SkinRefresh(id);', command)
        self.assertIn('gAdminLevel[playerid] < 1', command)
        self.assertIn('if (!strcmp(cmd, "skin", true) || !strcmp(cmd, "skins", true)) return RW_SkinCommand(playerid, params);', CMDS)
        self.assertIn('/skin [id txd texture]', CMDS)

    def test_changing_a_name_retextures_what_is_already_out(self):
        refresh = body('stock RW_SkinRefresh(id)')
        self.assertIn('if (IsPlayerConnected(p) && gInvWorn[p] > 0 && gArmourData[p] == id) RW_ArmourAttach(p, true);', refresh)
        self.assertIn('if (gGround[i][igUid] > 0 && RW_ItemSkin(gGround[i][igType], gGround[i][igData]) == id) RW_SkinApply(gGround[i][igObject], id);', refresh)

    def test_liquid_or_personal(self):
        """The same item type: one switch decides whether it can be traded."""
        bound = body('stock bool:RW_SkinBound(id)')
        self.assertEqual(bound.count('case '), 3)
        self.assertIn('stock bool:RW_SkinBoundItem(type, data)', SKINS)
        self.assertIn('new id = RW_ItemSkin(type, data);', body('stock bool:RW_SkinBoundItem(type'))
        # nothing is bound out of the box: every item stays liquid
        for value in re.findall(r'return (true|false);', bound):
            self.assertEqual(value, 'false')

    def test_bound_items_never_leave_the_account(self):
        drop = INV.split('stock RW_InvDrop')[1].split('\nstock ')[0]
        self.assertIn('if (RW_SkinBoundItem(gInv[playerid][source][iiType], gInv[playerid][source][iiData])) return RW_Error(playerid, "This item is bound to your account and cannot be dropped.");', drop)
        self.assertIn('if(!RW_SkinBoundItem(type,data)) PlayerTextDrawShow(playerid,gInvTD[playerid][159]);', INV)
        self.assertIn('bound to this account', INV)

    def test_server_never_sends_files(self):
        """The art stays on the client: no URL, no download, no file IO."""
        for banned in ('http://', 'DownloadFile', 'fopen', 'fread', 'FileOpen'):
            self.assertNotIn(banned, SKINS)

    def test_folder_scheme_is_documented(self):
        head = SKINS.split('*/', 1)[0]
        self.assertIn('arm/', head)
        self.assertIn('mod loader', head.lower())
        self.assertIn('never sends', head)

    def test_wiring(self):
        self.assertIn('#include <railway/skins>', MAIN)
        self.assertLess(MAIN.index('#include <railway/skins>'), MAIN.index('#include <railway/armour>'))
        self.assertIn('if (!RW_SkinSchema()) return 0;', INV)
        self.assertIn('if (!RW_SkinLoadAll()) return 0;', INV)
        self.assertIn('RW_SkinApply(object, gArmourData[playerid]);', ARM)
        self.assertIn('RW_SkinApply(gGround[i][igObject], RW_ItemSkin(gGround[i][igType], gGround[i][igData]));', INV)
        self.assertIn('RW_SkinApply(object, RW_ItemSkin(gInv[playerid][source][iiType], gInv[playerid][source][iiData]));', INV)


class SkinTableTest(unittest.TestCase):
    """The /skin override table, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        create = next(s for s in re.findall(r'"([^"\r\n]+)"', SKINS) if s.startswith('CREATE TABLE IF NOT EXISTS skins'))
        self.db.execute(create)

    def tearDown(self):
        self.db.close()

    def replace(self, id_, txd, texture):
        sql = next(s for s in re.findall(r'"([^"\r\n]+)"', SKINS) if s.startswith('REPLACE INTO skins'))
        return self.db.execute(sql % (id_, txd, texture))

    def test_one_row_per_id(self):
        self.replace(0, 'war1', 'armour')
        self.replace(0, 'war1', 'vest')
        self.assertEqual(self.db.execute('SELECT txd,texture FROM skins WHERE id=0').fetchone(), ('war1', 'vest'))

    def test_load_query_reads_them_all(self):
        self.replace(0, 'war1', 'armour')
        self.replace(2, 'mytxd', 'mytex')
        load = next(s for s in re.findall(r'"([^"\r\n]+)"', SKINS) if s.startswith('SELECT id,txd,texture FROM skins'))
        rows = self.db.execute(load).fetchall()
        self.assertEqual(sorted(rows), [(0, 'war1', 'armour'), (2, 'mytxd', 'mytex')])


if __name__ == '__main__':
    unittest.main()
