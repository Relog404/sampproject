"""Exercise the actual Pawn SQLite schema without writing a game database."""
import re
import sqlite3
import tempfile
import unittest
from pathlib import Path

SOURCE = Path('server/pawno/include/railway/accounts.inc').read_text(encoding='cp1251')
SCHEMA = re.search(r'RW_DB_Exec\("(CREATE TABLE IF NOT EXISTS `accounts`[^"\r\n]+)"\)', SOURCE).group(1)


class AccountsDatabaseTest(unittest.TestCase):
    def test_local_persistence_and_case_insensitive_unique_nick(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'railway.db'
            with sqlite3.connect(path) as db:
                db.execute(SCHEMA)
                db.execute('INSERT INTO accounts(name,hash,salt) VALUES (?,?,?)',
                           ('Test_Player', 'test-hash-not-a-password', 'test-salt'))
            with sqlite3.connect(path) as db:
                row = db.execute('SELECT name,hash,salt FROM accounts WHERE name=?',
                                 ('test_player',)).fetchone()
                self.assertEqual(row[0], 'Test_Player')
                with self.assertRaises(sqlite3.IntegrityError):
                    db.execute('INSERT INTO accounts(name,hash,salt) VALUES (?,?,?)',
                               ('TEST_PLAYER', 'other-hash', 'other-salt'))
                self.assertEqual(db.execute('SELECT COUNT(*) FROM accounts').fetchone()[0], 1)

    def test_dialog_mode_is_always_derived_from_database(self):
        dialog = SOURCE.split('stock RW_ShowAuthDialog', 1)[1].split('stock RW_ShowLoginDialog', 1)[0]
        self.assertIn('registerMode = !RW_DB_AccountExists(nick)', dialog)
        self.assertNotIn('info, "Login", "Register"', dialog)
        self.assertIn('db_open("railway.db")', SOURCE)


class DatabaseSafetyTest(unittest.TestCase):
    def probe(self, db):
        code = SOURCE.split('stock bool:RW_DB_Probe()', 1)[1].split('stock RW_DB_CreateTable()', 1)[0]
        insert = re.search(r'"(INSERT INTO[^"\r\n]+)"', code).group(1).replace('%s', '__railway_write_probe__123_456')
        db.execute('SAVEPOINT railway_write_probe')
        try:
            db.execute(insert)
            return db.execute('SELECT id FROM accounts WHERE name=?', ('__railway_write_probe__123_456',)).fetchone() is not None
        finally:
            db.execute('ROLLBACK TO railway_write_probe')
            db.execute('RELEASE railway_write_probe')

    def test_scriptfiles_directory_ships_in_checkout(self):
        self.assertTrue(Path('server/scriptfiles/README.txt').is_file())
        setup = Path('setup.ps1').read_text()
        self.assertIn('New-Item -ItemType Directory', setup)
        self.assertIn('server\\scriptfiles', setup)

    def test_write_probe_preserves_real_accounts_and_leaves_no_probe_row(self):
        with sqlite3.connect(':memory:') as db:
            db.execute(SCHEMA)
            db.execute("INSERT INTO accounts(name,hash,salt) VALUES ('__probe__','real-hash','real-salt')")
            self.assertTrue(self.probe(db))
            self.assertEqual(db.execute('SELECT name,hash,salt FROM accounts').fetchall(), [('__probe__','real-hash','real-salt')])

    def test_failed_probe_does_not_replace_legacy_schema(self):
        with sqlite3.connect(':memory:') as db:
            db.execute('CREATE TABLE accounts(id INTEGER PRIMARY KEY, name TEXT, hash TEXT)')
            db.execute("INSERT INTO accounts(name,hash) VALUES ('Old_Player','old-hash')")
            with self.assertRaises(sqlite3.OperationalError):
                self.probe(db)
            self.assertEqual(db.execute('SELECT name,hash FROM accounts').fetchall(), [('Old_Player','old-hash')])
        self.assertNotIn('ALTER TABLE', SOURCE)
        self.assertNotIn('DELETE FROM', SOURCE.split('stock bool:RW_DB_Probe()', 1)[1].split('stock RW_DB_CreateTable()', 1)[0])
        self.assertNotIn('db_open("scriptfiles/railway.db")', SOURCE)

    def test_exec_does_not_report_success_on_failed_query(self):
        code = SOURCE.split('stock bool:RW_DB_Exec', 1)[1].split('stock RW_DB_LogPath', 1)[0]
        self.assertIn('if (res == DBResult:0)', code)
        self.assertIn('return false', code)
        self.assertNotIn('printf("%s", query)', code)

if __name__ == '__main__':
    unittest.main()
