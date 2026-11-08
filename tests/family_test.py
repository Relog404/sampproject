"""Families: the shared garage, the seniors' stash and the territories.

A family owns things: a stash of items where the shared car keys live, a cash
box fed by the territories, and a roster with three ranks. Everything is
server-side and kept in SQLite, so a family outlives a restart and works with
its leader offline. The tables run for real below.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


FAMILY = pawn('family')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
INTER = pawn('interaction')
CMDS = pawn('commands')
ACCOUNTS = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class FamilyTableTest(unittest.TestCase):
    """Four tables, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        for prefix in ('CREATE TABLE IF NOT EXISTS families',
                       'CREATE TABLE IF NOT EXISTS family_members',
                       'CREATE TABLE IF NOT EXISTS family_items',
                       'CREATE TABLE IF NOT EXISTS family_zones'):
            self.db.executescript(sql(FAMILY, prefix) + ';')

    def test_a_family_keeps_its_name_leader_and_cash(self):
        self.db.execute('INSERT INTO families (name,leader,cash,stamp) VALUES (\'Volki\',7,12000,1000)')
        row = self.db.execute('SELECT name,leader,cash FROM families WHERE id=1').fetchone()
        self.assertEqual(row, ('Volki', 7, 12000))

    def test_the_name_is_unique(self):
        self.db.execute('INSERT INTO families (name,leader,cash,stamp) VALUES (\'Volki\',7,0,1)')
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute('INSERT INTO families (name,leader,cash,stamp) VALUES (\'Volki\',8,0,2)')

    def test_a_member_has_exactly_one_seat_in_a_family(self):
        self.db.execute('INSERT INTO family_members (family,player_id,rank) VALUES (1,7,2)')
        self.db.execute('INSERT OR REPLACE INTO family_members (family,player_id,rank) VALUES (1,7,1)')
        rows = self.db.execute('SELECT rank FROM family_members WHERE family=1 AND player_id=7').fetchall()
        self.assertEqual(rows, [(1,)])

    def test_the_stash_holds_items_and_stacks(self):
        self.db.execute('INSERT INTO family_items (family,type,data,amount) VALUES (1,2,55,1)')
        self.db.execute('INSERT INTO family_items (family,type,data,amount) VALUES (1,19,0,3)')
        rows = self.db.execute('SELECT type,data,amount FROM family_items WHERE family=1 ORDER BY id').fetchall()
        self.assertEqual(rows, [(2, 55, 1), (19, 0, 3)])

    def test_a_zone_belongs_to_one_family(self):
        self.db.execute('INSERT INTO family_zones (zone,family,stamp) VALUES (2,1,1000)')
        self.db.execute('INSERT OR REPLACE INTO family_zones (zone,family,stamp) VALUES (2,3,2000)')
        row = self.db.execute('SELECT family FROM family_zones WHERE zone=2').fetchone()
        self.assertEqual(row, (3,))


class FamilyCreateTest(unittest.TestCase):
    """Registration costs money and a name."""

    def test_the_price_and_the_limits(self):
        self.assertIn('#define FAMILY_CREATE_COST  50000', FAMILY)
        self.assertIn('#define FAMILY_SLOTS        40', FAMILY)
        create = block(FAMILY, 'stock RW_FamilyCreateNamed(playerid, const name[])')
        self.assertIn('if (gMoney[playerid] < FAMILY_CREATE_COST) return RW_Error', create)
        self.assertIn('RW_TakeMoney(playerid, FAMILY_CREATE_COST)', create)
        self.assertIn('if (strlen(name) < 3)', create)
        self.assertIn('RW_SafeSqlText(name)', create)

    def test_the_founder_becomes_the_leader(self):
        create = block(FAMILY, 'stock RW_FamilyCreateNamed(playerid, const name[])')
        self.assertIn('FAMILY_RANK_LEADER', create)
        self.assertIn('gFamilyRank[playerid] = FAMILY_RANK_LEADER;', create)
        self.assertIn('INSERT INTO families(name,leader,cash,stamp)', create)

    def test_you_cannot_found_a_second_family(self):
        create = block(FAMILY, 'stock RW_FamilyCreateNamed(playerid, const name[])')
        self.assertIn('if (gFamily[playerid] != FAMILY_NONE) {', create)


class FamilyRosterTest(unittest.TestCase):
    """Three ranks, invitations and the leader's seat."""

    def test_three_ranks(self):
        self.assertIn('#define FAMILY_RANK_MEMBER  0', FAMILY)
        self.assertIn('#define FAMILY_RANK_SENIOR  1', FAMILY)
        self.assertIn('#define FAMILY_RANK_LEADER  2', FAMILY)
        names = block(FAMILY, 'static const gFamilyRankName[3][] = {', '};')
        self.assertEqual(len(re.findall(r'"[^"]+"', names)), 3)

    def test_seniors_and_the_leader_invite(self):
        invite = block(FAMILY, 'stock RW_FamilyInvite(playerid, const params[])')
        self.assertIn('if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error', invite)
        # команда только разбирает слово: приглашает общая функция, её же
        # зовёт строка «Пригласить бойца» в панели.
        self.assertIn('return RW_FamilyInviteId(playerid, strval(params));', invite)
        shared = block(FAMILY, 'stock RW_FamilyInviteId(playerid, target)')
        self.assertIn('gFamilyInvite[target] = gFamily[playerid];', shared)

    def test_joining_needs_an_invitation(self):
        join = block(FAMILY, 'stock RW_FamilyJoin(playerid)')
        self.assertIn('if (gFamilyInvite[playerid] == FAMILY_NONE) {', join)
        self.assertIn('FAMILY_RANK_MEMBER', join)
        self.assertIn('gFamilyInvite[playerid] = FAMILY_NONE;', join)

    def test_the_leader_does_not_leave_a_full_family_first(self):
        leave = block(FAMILY, 'stock RW_FamilyLeave(playerid)')
        self.assertIn('RW_FamilyMembers(family) > 1', leave)
        self.assertIn('DELETE FROM family_members WHERE family=%d AND player_id=%d', leave)

    def test_a_senior_cannot_kick_his_betters(self):
        kick = block(FAMILY, 'stock RW_FamilyKick(playerid, const params[])')
        self.assertIn('if (gFamilyRank[target] >= gFamilyRank[playerid]) return RW_Error', kick)
        self.assertIn('if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error', kick)

    def test_handing_over_the_lead_demotes_the_old_leader(self):
        rank = block(FAMILY, 'stock RW_FamilyRank(playerid, const params[])')
        self.assertIn('if (gFamilyRank[playerid] != FAMILY_RANK_LEADER) return RW_Error', rank)
        self.assertIn('UPDATE family_members SET rank=%d WHERE family=%d AND rank=%d', rank)
        self.assertIn('gFamilyRank[playerid] = FAMILY_RANK_SENIOR;', rank)


class FamilyCarTest(unittest.TestCase):
    """A car is handed over with its key."""

    def test_the_key_goes_to_the_stash_and_leaves_the_inventory(self):
        give = block(FAMILY, 'stock RW_FamilyGiveCar(playerid)')
        self.assertIn('INSERT INTO family_items(family,type,data,amount)', give)
        self.assertIn('DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d', give)
        self.assertIn('gInv[playerid][slot][j] = 0;', give)
        self.assertIn('ITEM_CAR_KEY', block(FAMILY, 'stock RW_FamilyKeySlot(playerid)'))

    def test_the_stash_is_limited(self):
        for caller in ('stock RW_FamilyGiveCar(playerid)', 'stock RW_FamilyStoreSlot(playerid, slot)'):
            body = block(FAMILY, caller)
            self.assertIn('FAMILY_SLOTS', body)

    def test_taking_an_item_back_grants_it_to_the_bag(self):
        take = block(FAMILY, 'stock RW_FamilyTakeId(playerid, id)')
        self.assertIn('RW_FamilyGrant(playerid, type, data, amount)', take)
        self.assertIn('DELETE FROM family_items WHERE id=%d AND family=%d', take)
        grant = block(FAMILY, 'stock RW_FamilyGrant(playerid, type, data, amount)')
        self.assertIn('UPDATE inventory_items SET amount=%d WHERE uid=%d', grant)

    def test_only_seniors_fill_the_stash(self):
        store = block(FAMILY, 'stock RW_FamilyStoreSlot(playerid, slot)')
        self.assertIn('if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error', store)
        self.assertIn('RW_InvCanRemove(playerid, slot)', store)


class FamilyTerritoryTest(unittest.TestCase):
    """Four districts, two minutes on the point, money every ten minutes."""

    def test_four_real_districts(self):
        zones = block(FAMILY, 'static const Float:gFamilyZone[FAMILY_ZONES][3] = {', '};')
        names = block(FAMILY, 'static const gFamilyZoneName[FAMILY_ZONES][] = {', '};')
        self.assertEqual(len(re.findall(r'\{[^}]+\}', zones)), 4)
        self.assertEqual(len(re.findall(r'"[^"]+"', names)), 4)
        self.assertIn('{  2244.4, -1665.4, 13.5 }', FAMILY)     # Гантон
        self.assertIn('{  1595.5, -2235.9, 13.5 }', FAMILY)     # Оушен-Докс

    def test_the_capture_takes_two_minutes_on_the_point(self):
        self.assertIn('#define FAMILY_CAPTURE      120', FAMILY)
        self.assertIn('#define FAMILY_ZONE_RANGE   10.0', FAMILY)
        tick = block(FAMILY, 'stock RW_FamilyTick(playerid)')
        self.assertIn('IsPlayerInRangeOfPoint(playerid, FAMILY_ZONE_RANGE', tick)
        self.assertIn('gettime() + FAMILY_CAPTURE', tick)
        self.assertIn('INSERT OR REPLACE INTO family_zones(zone,family,stamp)', tick)

    def test_leaving_the_point_breaks_the_capture(self):
        pulse = block(FAMILY, 'public RW_FamilyPulse()', '\n\n/*  ')
        self.assertIn('RW_FamilyTick(p)', pulse)
        self.assertIn('Вы отошли от точки: захват сорван.', pulse)

    def test_the_owners_hear_the_alarm(self):
        capture = block(FAMILY, 'stock RW_FamilyCapture(playerid)')
        self.assertIn('gFamily[p] == owner', capture)
        self.assertIn('забирает «%s»', capture)

    def test_territories_pay_the_cash_box(self):
        self.assertIn('#define FAMILY_INCOME       150', FAMILY)
        self.assertIn('#define FAMILY_PAY_PERIOD   600', FAMILY)
        pulse = block(FAMILY, 'public RW_FamilyPulse()', '\n\n/*  ')
        self.assertIn('SELECT zone,family FROM family_zones WHERE family>0', pulse)
        self.assertIn('RW_FamilyAddMoney', pulse)


class FamilyCashTest(unittest.TestCase):
    """Only the leader spends the crew's money."""

    def test_only_the_leader_takes_from_the_box(self):
        cash = block(FAMILY, 'stock RW_FamilyCashTake(playerid, amount)')
        self.assertIn('if (gFamilyRank[playerid] != FAMILY_RANK_LEADER) return RW_Error', cash)
        self.assertIn('RW_FamilyTakeMoney(gFamily[playerid], amount)', cash)
        self.assertIn('RW_AddMoney(playerid, amount);', cash)

    def test_the_box_cannot_go_negative(self):
        take = block(FAMILY, 'stock bool:RW_FamilyTakeMoney(family, amount)')
        self.assertIn('if (RW_FamilyMoney(family) < amount) return false;', take)
        self.assertIn('UPDATE families SET cash=cash-%d WHERE id=%d', take)


class FamilyInterfaceTest(unittest.TestCase):
    """The family panel in the browser and in the native dialog."""

    def test_both_interfaces_are_wired(self):
        self.assertIn('#define CEF_MODE_FAMILY             (18)', CORE)
        self.assertIn('#define UI_FAMILY 28', UI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_FAMILY) return RW_FamilyCefClick(playerid, id);', CEFUI)
        self.assertIn('} else if (mode == UI_FAMILY) {', INTER)
        self.assertIn('return RW_FamilyClick(playerid, row);', INTER)

    def test_the_panel_shows_the_crew_the_box_the_stash_and_the_land(self):
        rows = block(FAMILY, 'stock RW_FamilyRows(playerid)')
        self.assertIn('Семья «%s»', rows)
        self.assertIn('Касса: %d', rows)
        self.assertIn('Склад: %d из %d', rows)
        self.assertIn('Территорий: %d', rows)

    def test_a_lone_player_is_offered_to_found_a_family(self):
        rows = block(FAMILY, 'stock RW_FamilyRows(playerid)')
        self.assertIn('"Создать семью"', rows)
        self.assertIn('"нажмите: название спросит поле ввода"', rows)
        action = block(FAMILY, 'stock RW_FamilyAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 0 && gFamily[playerid] == FAMILY_NONE) return RW_FamilyAskName(playerid);', action)
        self.assertIn('INPUT_FAMILY_NAME, 0, "Новая семья"', block(FAMILY, 'stock RW_FamilyAskName(playerid)'))

    def test_the_stash_opens_as_a_list_not_as_chat(self):
        rows = block(FAMILY, 'stock RW_FamilyStorageRows(playerid)')
        self.assertIn('ORDER BY id LIMIT %d OFFSET %d', rows)
        self.assertIn('gFamilyIds[playerid][index] = id;', rows)
        action = block(FAMILY, 'stock RW_FamilyAction(playerid, row, bool:cef)')
        self.assertIn('if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE)', action)
        self.assertIn('RW_FamilyTakeId(playerid, id);', action)
        self.assertIn('return RW_FamilyOpen(playerid);', action)
        self.assertIn('return RW_FamilyOpenStorage(playerid);', action)

    def test_the_crew_takes_the_cash_and_leaves_by_click(self):
        action = block(FAMILY, 'stock RW_FamilyAction(playerid, row, bool:cef)')
        self.assertIn('INPUT_FAMILY_CASH, 0, "Касса семьи"', action)
        self.assertIn('RW_FamilyLeave(playerid);', action)
        self.assertTrue(block(FAMILY, 'stock bool:RW_FamilyCanStore(playerid)'))
        self.assertIn('RW_FamilyStoreSlot(playerid, slot_stash);',
                      block(pawn('inventory'), 'stock RW_InvCefClick(playerid, id)'))

    def test_the_commands_are_registered(self):
        for command in ('family', 'fcreate', 'finvite', 'fjoin', 'fleave', 'fkick',
                        'frank', 'fgive', 'fstore', 'ftake', 'fcap', 'fzones', 'fcash'):
            self.assertIn('"%s", true' % command, CMDS)

    def test_the_panel_works_without_the_browser_too(self):
        panel = block(FAMILY, 'stock RW_FamilyPanel(playerid)')
        self.assertIn('if (!RW_CefOn(playerid)) RW_Info(playerid,', panel)

    def test_the_family_lives_with_the_account_and_the_gamemode(self):
        self.assertIn('RW_FamilyLoad(playerid);', ACCOUNTS)
        self.assertIn('#include <railway/family>', MAIN)
        self.assertIn('RW_FamilySchema()', MAIN)
        self.assertIn('SetTimer("RW_FamilyPulse", 1000, true);', MAIN)
        self.assertIn('RW_FamilyReset(playerid);', MAIN)


if __name__ == '__main__':
    unittest.main()
