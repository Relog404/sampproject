from pathlib import Path
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT/'server/pawno/include/railway/commands.inc').read_text(encoding='cp1251')
BODY = SOURCE.split('stock RW_CommandSetSkin', 1)[1].split('stock RW_CommandProcess', 1)[0]

class SetSkinTest(unittest.TestCase):
    def test_authorization_and_validated_arguments(self):
        for guard in ('!gLogged[playerid] || gAdminLevel[playerid] < 1', 'RW_IsNumeric(targetText)', 'RW_IsNumeric(skinText)', 'extra[0]', 'target < 0 || target >= MAX_PLAYERS', '!IsPlayerConnected(target)', '!gLogged[target]', 'skin < 0 || skin > 311 || skin == 74', 'PLAYER_STATE_ONFOOT', 'PLAYER_STATE_DRIVER', 'PLAYER_STATE_PASSENGER'):
            self.assertIn(guard, BODY)
        self.assertIn('if (!strcmp(cmd, "setskin", true)) return RW_CommandSetSkin', SOURCE)

    def test_failed_save_cannot_publish_skin_and_preview_refreshes(self):
        save = BODY.index('!gDBWritable || !RW_DB_Exec(query)')
        self.assertLess(save, BODY.index('gSkinId[target] = skin'))
        self.assertLess(save, BODY.index('SetPlayerSkin(target, skin)'))
        self.assertIn('PlayerTextDrawSetPreviewModel(target, gInvTD[target][130], skin)', BODY)
        self.assertIn('RW_InvAttach(target,true)', BODY)
        self.assertIn('gAccountId[playerid], skin, gAccountId[target]', BODY)

    def test_sql_changes_only_target_skin(self):
        db = sqlite3.connect(':memory:')
        db.execute('CREATE TABLE accounts(id INTEGER PRIMARY KEY, skin INTEGER, hash TEXT)')
        db.executemany('INSERT INTO accounts VALUES(?,?,?)', [(1,0,'hash1'), (2,10,'hash2')])
        import re
        query = re.search(r'"(UPDATE accounts SET skin=%d WHERE id=%d)"', BODY)[1]
        db.execute(query % (104,2))
        self.assertEqual(db.execute('SELECT * FROM accounts ORDER BY id').fetchall(), [(1,0,'hash1'), (2,104,'hash2')])
        db.close()

if __name__ == '__main__':
    unittest.main()
