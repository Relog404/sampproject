"""Client-side clothing textures: the files stay in the player's arm/ folder,
the server only names the TXD and the texture inside it. Source contracts.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(n):
    return (ROOT / 'server/pawno/include/railway' / f'{n}.inc').read_text(encoding='cp1251')

SKINS = pawn('skins')
ARM = pawn('armour')
INV = pawn('inventory')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def body(start, end='\nstock '):
    return SKINS.split(start, 1)[1].split(end, 1)[0]


class SkinRegistryTest(unittest.TestCase):
    def test_every_entry_is_described_in_one_place(self):
        for fn in ('RW_SkinModel', 'RW_SkinTxd', 'RW_SkinTexture', 'RW_SkinLabel'):
            text = body('stock %s(id' % fn)
            self.assertEqual(text.count('case '), 3, fn)
        self.assertIn('stock RW_SkinCount() { return 3; }', SKINS)

    def test_txd_names_carry_no_extension(self):
        txd = body('stock RW_SkinTxd(id')
        for name in re.findall(r'format\(dest, size, "([^"]*)"\)', txd):
            self.assertNotIn('.', name, 'TXD names go without the extension')

    def test_textured_only_when_both_names_are_given(self):
        apply_body = body('stock RW_SkinApply(objectid', '\n//')
        self.assertIn('if (objectid <= 0 || !RW_SkinTextured(id)) return 0;', apply_body)
        check = body('stock bool:RW_SkinTextured(id)')
        self.assertIn('RW_SkinTxd(id, txd, sizeof txd);', check)
        self.assertIn('RW_SkinTexture(id, texture, sizeof texture);', check)
        self.assertIn('return txd[0] && texture[0];', check)

    def test_apply_uses_the_shape_of_the_same_entry(self):
        apply_body = body('stock RW_SkinApply(objectid')
        self.assertIn('SetObjectMaterial(objectid, 0, RW_SkinModel(id), txd, texture, 0);', apply_body)

    def test_bounds(self):
        self.assertIn('stock bool:RW_SkinValid(id) { return id >= 0 && id < RW_SkinCount() && id < SKIN_MAX; }', SKINS)
        self.assertIn('#define SKIN_MAX (16)', SKINS)

    def test_items_point_at_a_skin(self):
        item = body('stock RW_ItemSkin(type')
        self.assertIn('if (type == ITEM_ARMOUR) return data;', item)
        self.assertIn('return -1;', item)

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
        self.assertIn('RW_SkinApply(object, gArmourData[playerid]);', ARM)
        self.assertIn('RW_SkinApply(gGround[i][igObject], RW_ItemSkin(gGround[i][igType], gGround[i][igData]));', INV)
        self.assertIn('RW_SkinApply(object, RW_ItemSkin(gInv[playerid][source][iiType], gInv[playerid][source][iiData]));', INV)


if __name__ == '__main__':
    unittest.main()
