"""City Hall: the lawyer licence, the record sheet and the contract.

Until now only an admin could issue a lawyer licence. The government module
sells it at a real point of the map for money and with a background check,
and it lets a licensed lawyer sign a client with Y — if the client is jailed,
the lawyer is called without lifting a finger. The contract table runs for
real below.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


GOV = pawn('government')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
INTER = pawn('interaction')
POLICE = pawn('police')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class ContractTableTest(unittest.TestCase):
    """One client keeps one lawyer."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(GOV, 'CREATE TABLE IF NOT EXISTS lawyer_contracts') + ';')

    def test_a_contract_keeps_the_lawyer_the_price_and_the_date(self):
        self.db.execute('INSERT INTO lawyer_contracts (client,lawyer,price,stamp) VALUES (9,3,5000,1000)')
        row = self.db.execute('SELECT lawyer,price,stamp FROM lawyer_contracts WHERE client=9').fetchone()
        self.assertEqual(row, (3, 5000, 1000))

    def test_a_client_has_one_lawyer_at_a_time(self):
        self.db.execute('INSERT INTO lawyer_contracts (client,lawyer,price,stamp) VALUES (9,3,5000,1000)')
        self.db.execute('INSERT OR REPLACE INTO lawyer_contracts (client,lawyer,price,stamp) VALUES (9,4,7000,2000)')
        rows = self.db.execute('SELECT lawyer FROM lawyer_contracts WHERE client=9').fetchall()
        self.assertEqual(rows, [(4,)])


class LicenceTest(unittest.TestCase):
    """The licence is bought, not handed out."""

    def test_the_price_and_the_term(self):
        self.assertIn('#define GOV_LICENCE_PRICE       25000', GOV)
        self.assertIn('#define GOV_LICENCE_DAYS        7', GOV)
        self.assertIn('#define GOV_CLEAN_DAYS          3', GOV)

    def test_it_is_sold_only_at_city_hall(self):
        licence = block(GOV, 'stock RW_GovLicence(playerid)')
        self.assertIn('if (!RW_GovNear(playerid)) return RW_Error', licence)
        self.assertIn('GOV_X                   1352.4', GOV)
        self.assertIn('GOV_Y                   -1759.4', GOV)

    def test_a_record_blocks_the_licence(self):
        licence = block(GOV, 'stock RW_GovLicence(playerid)')
        self.assertIn('RW_GovArrests(playerid, GOV_CLEAN_DAYS) > 0', licence)
        self.assertIn('RW_TakeMoney(playerid, GOV_LICENCE_PRICE)', licence)

    def test_an_existing_licence_is_extended(self):
        licence = block(GOV, 'stock RW_GovLicence(playerid)')
        self.assertIn('gLawyerExpire[playerid] + GOV_LICENCE_DAYS * 86400', licence)
        self.assertIn('INSERT OR REPLACE INTO lawyers', licence)
        self.assertIn('gLawyerLicense[playerid] = 1;', licence)

    def test_an_officer_does_not_become_a_lawyer(self):
        licence = block(GOV, 'stock RW_GovLicence(playerid)')
        self.assertIn('RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0', licence)

    def test_the_record_sheet_reads_the_arrests(self):
        self.assertIn('#define GOV_RECORD_PRICE        500', GOV)
        record = block(GOV, 'stock RW_GovRecord(playerid)')
        self.assertIn('RW_GovArrests(playerid, 30)', record)
        self.assertIn('SELECT COUNT(*) AS n FROM arrests WHERE player_id=%d AND stamp>%d', block(GOV, 'stock RW_GovArrests(playerid, days)'))


class ContractTest(unittest.TestCase):
    """Y on a player signs him up, and the jail calls the lawyer."""

    def test_only_a_licensed_lawyer_signs(self):
        offer = block(GOV, 'stock RW_ContractOffer(playerid, target)')
        self.assertIn('if (!RW_IsLawyer(playerid)) return RW_Error', offer)
        self.assertIn('GOV_CONTRACT_MIN', offer)

    def test_the_client_pays_at_once(self):
        offer = block(GOV, 'stock RW_ContractOffer(playerid, target)')
        self.assertIn('RW_TakeMoney(target, price)', offer)
        self.assertIn('RW_AddMoney(playerid, price);', offer)
        self.assertIn('INSERT OR REPLACE INTO lawyer_contracts(client,lawyer,price,stamp)', offer)

    def test_a_contract_expires(self):
        self.assertIn('#define GOV_CONTRACT_HOURS      24', GOV)
        of = block(GOV, 'stock RW_ContractOf(client)')
        self.assertIn('stamp>%d', of)

    def test_the_jail_calls_the_lawyer(self):
        self.assertIn('RW_ContractCall(who);', POLICE)
        call = block(GOV, 'stock RW_ContractCall(client)')
        self.assertIn('RW_ContractOf(client)', call)
        self.assertIn('gAccountId[p] == lawyer', call)

    def test_an_officer_needs_no_lawyer(self):
        offer = block(GOV, 'stock RW_ContractOffer(playerid, target)')
        self.assertIn('RW_PoliceOnDuty(target)', offer)


class GovernmentInterfaceTest(unittest.TestCase):
    """City Hall in the browser, on TextDraw and under Y."""

    def test_both_interfaces_are_wired(self):
        self.assertIn('#define CEF_MODE_GOV                (19)', CORE)
        self.assertIn('#define UI_GOV 29', UI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_GOV) return RW_GovCefClick(playerid, id);', CEFUI)
        self.assertIn('} else if (mode == UI_GOV) {', INTER)
        self.assertIn('return RW_GovClick(playerid, row);', INTER)

    def test_y_opens_city_hall_and_signs_a_client(self):
        self.assertIn('if (RW_GovNear(playerid)) return RW_GovOpen(playerid);', INTER)
        self.assertIn('if (RW_IsLawyer(playerid) && gJailTime[target] <= 0) return RW_ContractOffer(playerid, target);', INTER)

    def test_the_panel_lists_the_services(self):
        rows = block(GOV, 'stock RW_GovRows(playerid)')
        self.assertIn('Лицензия адвоката', rows)
        self.assertIn('Справка о несудимости', rows)
        self.assertIn('Судимость за 3 дня', rows)
        self.assertIn('Договор с адвокатом', rows)

    def test_the_commands_are_registered(self):
        for command in ('cityhall', 'gov', 'licence', 'record', 'contract'):
            self.assertIn('"%s", true' % command, CMDS)

    def test_the_panel_works_without_the_browser_too(self):
        panel = block(GOV, 'stock RW_GovPanel(playerid)')
        self.assertIn('if (!RW_CefOn(playerid)) RW_Info(playerid,', panel)

    def test_the_module_lives_in_the_gamemode(self):
        self.assertIn('#include <railway/government>', MAIN)
        self.assertIn('RW_GovSchema()', MAIN)

    def test_the_licence_is_not_free_of_conditions(self):
        rows = block(GOV, 'stock RW_GovRows(playerid)')
        self.assertIn('25 000, на 7 дней', rows)
        self.assertIn('с ней лицензию не дадут', rows)


if __name__ == '__main__':
    unittest.main()
