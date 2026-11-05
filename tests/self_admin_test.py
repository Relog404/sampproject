from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMMAND = 'nedrochabr6664148886769232123423'
SOURCE = (ROOT/'server/pawno/include/railway/commands.inc').read_text(encoding='cp1251')

class SelfAdminCommandTest(unittest.TestCase):
    def test_long_command_fits_and_requires_login(self):
        process = SOURCE.split('stock RW_CommandProcess',1)[1]
        capacity = int(re.search(r'new cmd\[(\d+)\]',process)[1])
        self.assertGreaterEqual(capacity,len(COMMAND)+1)
        self.assertLess(process.index('if (!gLogged[playerid])'),process.index('"'+COMMAND+'"'))
        self.assertIn('if (params[0])',process.split('"'+COMMAND+'"',1)[1].split('if (!strcmp(cmd, "inv"',1)[0])

    def test_grants_only_caller_through_verified_database_writer(self):
        block = SOURCE.split('"'+COMMAND+'"',1)[1].split('if (!strcmp(cmd, "inv"',1)[0]
        self.assertIn('RW_GetNick(playerid, nick)',block)
        self.assertIn('if (!RW_AdminSetLevel(nick, 3))',block)
        self.assertNotIn('strval(params)',block)
        self.assertNotIn('gAdminLevel[playerid] =',block)
        self.assertLess(block.index('RW_AdminSetLevel'),block.index('return RW_Ok'))

if __name__ == '__main__':
    unittest.main()
