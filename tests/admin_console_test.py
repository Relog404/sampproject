"""Console routing source/AMX contracts and extracted SQL persistence tests."""
from pathlib import Path
import re
import sqlite3
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMMANDS = (ROOT/'server/pawno/include/railway/commands.inc').read_text(encoding='cp1251')
SET = COMMANDS.split('stock RW_AdminSetLevel',1)[1].split('stock RW_AdminConsoleSet',1)[0]
PARSER = COMMANDS.split('stock RW_AdminConsoleSet',1)[1].split('stock RW_CommandKick',1)[0]

def sql(prefix):
    return next(s for s in re.findall(r'"([^"\r\n]+)"', SET) if s.startswith(prefix))

class AdminStorageTest(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        # A legacy schema without NOCASE: lookup must explicitly ignore case.
        self.db.execute('CREATE TABLE accounts(id INTEGER PRIMARY KEY, name TEXT UNIQUE, admin INTEGER, hash TEXT, salt TEXT)')
        self.db.executemany('INSERT INTO accounts VALUES(?,?,?,?,?)', [(1,'Player_One',0,'hash1','salt1'),(2,'Other_Player',2,'hash2','salt2')])

    def tearDown(self):
        self.db.close()

    def test_case_insensitive_lookup_and_update_by_exact_account_id(self):
        account = self.db.execute(sql('SELECT id') % 'pLaYeR_oNe').fetchone()[0]
        self.db.execute('SAVEPOINT admin_level')
        self.db.execute(sql('UPDATE accounts') % (3,account))
        self.assertEqual(self.db.execute(sql('SELECT admin') % account).fetchone()[0],3)
        self.db.execute('RELEASE admin_level')
        self.assertEqual(self.db.execute('SELECT id,admin,hash,salt FROM accounts ORDER BY id').fetchall(),[(1,3,'hash1','salt1'),(2,2,'hash2','salt2')])

    def test_unknown_account_is_not_created(self):
        self.assertIsNone(self.db.execute(sql('SELECT id') % 'Missing_Name').fetchone())
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM accounts').fetchone()[0],2)

    def test_failed_verification_can_rollback_and_zero_removes_rights(self):
        self.db.execute('SAVEPOINT admin_level')
        self.db.execute(sql('UPDATE accounts') % (3,1))
        self.db.execute('ROLLBACK TO admin_level'); self.db.execute('RELEASE admin_level')
        self.assertEqual(self.db.execute(sql('SELECT admin') % 1).fetchone()[0],0)
        self.db.execute(sql('UPDATE accounts') % (0,2))
        self.assertEqual(self.db.execute(sql('SELECT admin') % 2).fetchone()[0],0)

class AdminConsoleContractsTest(unittest.TestCase):
    def test_relay_loaded_and_routes_to_unique_gamemode_public(self):
        config = (ROOT/'server/server.cfg').read_text(encoding='cp1251')
        self.assertRegex(config,r'(?m)^filterscripts\s+admin_console\s*$')
        relay = (ROOT/'server/filterscripts/admin_console.pwn').read_text()
        self.assertIn('public OnRconCommand',relay)
        self.assertIn('CallRemoteFunction("RW_ConsoleCommand", "s", cmd)',relay)
        self.assertNotIn('OnPlayerCommandText',relay)
        main = (ROOT/'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertIn('public RW_ConsoleCommand(cmd[])',main)
        self.assertIn('RW_AdminConsoleSet(params)',main)
        self.assertIn('"dbcheck"',main.split('public RW_ConsoleCommand',1)[1])

    def test_only_console_can_assign_and_strict_levels_and_targets(self):
        player_commands = COMMANDS.split('stock RW_CommandProcess',1)[1]
        self.assertNotIn('RW_AdminConsoleSet(',player_commands)
        self.assertIn('RW_AdminSetLevel(nick, 3)',player_commands)
        # Ordinary /setadmin is still console-only; the explicit recovery alias
        # is an owner-requested self-grant and is covered by self_admin_test.
        self.assertIn('Admin assignment is console-only:', player_commands)
        for check in ('strlen(levelText) != 1', "levelText[0] < '0'", "levelText[0] > '3'", 'extra[0]', 'target >= MAX_PLAYERS', '!IsPlayerConnected(target)', '!gLogged[target]', 'RW_ValidNick(targetText)', 'RW_SafeSqlText(targetText)'):
            self.assertIn(check,PARSER)
        self.assertIn('level < 0 || level > 3',SET)
        self.assertIn('gAccountId[i] != account',SET)
        self.assertLess(SET.index('RELEASE admin_level'),SET.index('gAdminLevel[i] = level'))
        self.assertIn('SetPlayerColor',SET)
        self.assertIn('gDB == DB:0 || !gDBWritable',SET)

    def test_relay_binary_is_v8_and_exports_rcon_callback(self):
        data = (ROOT/'server/filterscripts/admin_console.amx').read_bytes()
        self.assertEqual(struct.unpack_from('<H',data,4)[0],0xF1E0)
        self.assertEqual(data[6],8)
        stride = struct.unpack_from('<H',data,10)[0]
        publics,natives,libraries = struct.unpack_from('<III',data,32)
        def names(start,end):
            result=[]
            for offset in range(start,end,stride):
                index=struct.unpack_from('<I',data,offset+4)[0]
                result.append(data[index:data.index(b'\0',index)].decode('ascii'))
            return result
        self.assertIn('OnRconCommand',names(publics,natives))
        self.assertIn('CallRemoteFunction',names(natives,libraries))
        self.assertIn('admin_console', (ROOT/'setup.ps1').read_text())
        self.assertIn('admin_console', (ROOT/'server/tools/build-gamemode.ps1').read_text())

if __name__ == '__main__':
    unittest.main()
