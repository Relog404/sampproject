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
        self.assertNotIn('info, "Войти", "Регистрация"', dialog)
        self.assertIn('db_open("railway.db")', SOURCE)


if __name__ == '__main__':
    unittest.main()
