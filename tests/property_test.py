"""Property: houses, businesses, doors, entering and taxes.

Eight real addresses in Los Santos. Everything is calculated on the server:
the browser menu and the native dialog call the same stocks, the state lives
in SQLite and survives a restart. SQL from the Pawn sources is executed here
against a real SQLite database.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


PROP = pawn('property')
ECON = pawn('economy')
CORE = pawn('core')
UI = pawn('ui')
INTER = pawn('interaction')
CEFUI = pawn('cefui')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class PropertyTableTest(unittest.TestCase):
    """The properties table, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(PROP, 'CREATE TABLE IF NOT EXISTS properties') + ';')
        # В исходнике вставка одна и стоит в цикле по PROP_MAX: повторяем её восемь раз.
        statement = sql(PROP, 'INSERT OR IGNORE INTO properties')
        for i in range(8):
            self.db.execute(statement.replace('%d', str(i)))
        self.db.commit()

    def test_eight_addresses_are_seeded(self):
        rows = self.db.execute('SELECT id,owner_id,doors,tax_due,missed,cash FROM properties ORDER BY id').fetchall()
        self.assertEqual(len(rows), 8)
        for row in rows:
            self.assertEqual(row[1:], (0, 0, 0, 0, 0))    # город, двери закрыты, долга нет

    def test_the_row_is_updated_by_the_save_query(self):
        query = sql(PROP, 'UPDATE properties SET')
        self.assertIn('owner_id=%d', query)
        self.assertIn('WHERE id=%d', query)


class PropertyMapTest(unittest.TestCase):
    """Addresses: real districts of Los Santos, one door each."""

    def test_there_are_four_houses_and_four_businesses(self):
        info = PROP.split('static const gPropInfo[][4] = {', 1)[1].split('};', 1)[0]
        kinds = re.findall(r'\{ PROP_KIND_(\w+),', info)
        self.assertEqual(kinds.count('HOUSE'), 4)
        self.assertEqual(kinds.count('BIZ'), 4)

    def test_every_address_is_named_after_a_place(self):
        names = PROP.split('static const gPropNames[][] = {', 1)[1].split('};', 1)[0]
        for place in ('Ganton', 'Jefferson', 'Willowfield', 'El Corona', 'Downtown', 'Las Colinas', 'Unity'):
            self.assertIn(place, names)

    def test_the_doors_are_distinct_points_in_los_santos(self):
        doors = PROP.split('static const Float:gPropDoor[][3] = {', 1)[1].split('};', 1)[0]
        points = re.findall(r'\{\s*(-?\d+\.\d+),\s*(-?\d+\.\d+),\s*(\d+\.\d+)\s*\}', doors)
        self.assertEqual(len(points), 8)
        self.assertEqual(len(set(points)), 8)            # двери не совпадают
        for x, y, z in points:
            self.assertTrue(1200.0 < float(x) < 2600.0 and -2100.0 < float(y) < -1300.0)
            self.assertTrue(0.0 < float(z) < 50.0)

    def test_the_interior_points_are_indoors(self):
        inside = PROP.split('static const Float:gPropIn[][3] = {', 1)[1].split('};', 1)[0]
        points = re.findall(r'\{\s*(-?\d+\.\d+),\s*(-?\d+\.\d+),\s*(\d+\.\d+)\s*\}', inside)
        self.assertEqual(len(points), 8)
        for x, y, z in points:
            self.assertGreater(float(z), 900.0)          # интерьеры выше улицы

    def test_every_property_has_an_interior_id(self):
        info = PROP.split('static const gPropInfo[][4] = {', 1)[1].split('};', 1)[0]
        interiors = re.findall(r',\s*(\d+)\s*\}', info)
        self.assertEqual(len(interiors), 8)
        for value in interiors:
            self.assertIn(int(value), (1, 2, 17))


class PropertyDealTest(unittest.TestCase):
    """Buying, selling, doors, entering - all checked on the server."""

    def test_the_price_comes_from_the_account(self):
        buy = block(PROP, 'stock RW_PropBuy(playerid, id)')
        self.assertIn('if (gBank[playerid] < price) return RW_Error(playerid, "Not enough money on the account.");', buy)
        self.assertIn('RW_BankTake(playerid, price);', buy)
        self.assertIn('gPropOwner[id] = gAccountId[playerid];', buy)
        self.assertIn('RW_PropSave(id);', buy)

    def test_one_owner_cannot_hold_two_properties(self):
        buy = block(PROP, 'stock RW_PropBuy(playerid, id)')
        self.assertIn('if (gPropOwner[id] > 0) return RW_Error(playerid, "It is already owned.");', buy)
        self.assertIn('if (RW_PropOwns(playerid) >= 0) return RW_Error(playerid, "You already own property. Sell it first.");', buy)

    def test_selling_returns_seventy_five_percent_and_the_till(self):
        self.assertIn('#define PROP_SELL_PERCENT       75', PROP)
        sell = block(PROP, 'stock RW_PropSell(playerid, id)')
        self.assertIn('gPropInfo[id][1] * PROP_SELL_PERCENT / 100', sell)
        self.assertIn('back = back + gPropCash[id];', sell)
        self.assertIn('RW_BankAdd(playerid, back);', sell)
        self.assertIn('gPropOwner[id] = 0;', sell)

    def test_only_the_owner_opens_the_door(self):
        doors = block(PROP, 'stock RW_PropToggleDoors(playerid, id)')
        self.assertIn('if (!RW_PropMine(playerid, id)) return RW_Error(playerid, "Only the owner opens these doors.");', doors)
        enter = block(PROP, 'stock RW_PropEnter(playerid, id)')
        self.assertIn('if (!RW_PropMine(playerid, id) && !gPropDoorOpen[id])', enter)
        self.assertIn('The door is locked. Only the owner can open it.', enter)

    def test_entering_moves_the_player_into_the_interior(self):
        enter = block(PROP, 'stock RW_PropEnter(playerid, id)')
        self.assertIn('SetPlayerInterior(playerid, gPropInfo[id][3]);', enter)
        self.assertIn('SetPlayerPos(playerid, gPropIn[id][0], gPropIn[id][1], gPropIn[id][2]);', enter)
        self.assertIn('gPropAt[playerid] = id;', enter)
        exit_ = block(PROP, 'stock RW_PropExit(playerid)')
        self.assertIn('SetPlayerInterior(playerid, 0);', exit_)
        self.assertIn('SetPlayerPos(playerid, gPropDoor[id][0]', exit_)
        self.assertIn('gPropAt[playerid] = -1;', exit_)


class PropertyTaxTest(unittest.TestCase):
    """Taxes: taken from the account, otherwise they pile up as debt."""

    def test_the_tax_is_taken_on_payday(self):
        self.assertIn('RW_PropTaxTick(playerid);', block(ECON, 'stock RW_EconomyPayday(playerid)'))
        self.assertIn('forward RW_PropTaxTick(playerid);', CORE)

    def test_unpaid_tax_becomes_debt_and_seals_the_doors(self):
        tick = block(PROP, 'public RW_PropTaxTick(playerid)')
        self.assertIn('if (gBank[playerid] >= tax) {', tick)
        self.assertIn('RW_BankTake(playerid, tax);', tick)
        self.assertIn('gPropTaxDue[i] = gPropTaxDue[i] + tax;', tick)
        self.assertIn('gPropMissed[i]++;', tick)
        self.assertIn('if (gPropMissed[i] >= PROP_TAX_LIMIT) {', tick)
        self.assertIn('gPropDoorOpen[i] = 0;', tick)
        self.assertIn('if (gPropAucEnd[i] <= 0) RW_PropAucStart(i);', tick)
        self.assertIn('#define PROP_TAX_LIMIT          3', PROP)

    def test_the_debt_is_paid_from_the_account(self):
        pay = block(PROP, 'stock RW_PropPayTax(playerid, id)')
        self.assertIn('if (gBank[playerid] < gPropTaxDue[id]) return RW_Error(playerid, "Not enough money on the account.");', pay)
        self.assertIn('gPropTaxDue[id] = 0;', pay)
        self.assertIn('gPropMissed[id] = 0;', pay)

    def test_sealed_doors_do_not_open_until_the_tax_is_paid(self):
        doors = block(PROP, 'stock RW_PropToggleDoors(playerid, id)')
        self.assertIn('gPropMissed[id] >= PROP_TAX_LIMIT', doors)
        self.assertIn('The city sealed the doors: pay the tax with /paytax.', doors)


class PropertyWiringTest(unittest.TestCase):
    """The slice is wired into the panels, the commands and the game mode."""

    def test_the_mode_knows_the_slice(self):
        self.assertIn('#include <railway/property>', MAIN)
        self.assertIn('RW_PropSchema()', MAIN)
        self.assertIn('RW_PropInit();', MAIN)
        self.assertIn('RW_PropReset(playerid);', MAIN)

    def test_both_faces_use_the_same_rows(self):
        self.assertIn('#define UI_PROPERTY 23', UI)
        self.assertIn('#define CEF_MODE_PROPERTY           (13)', CORE)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_PROPERTY) return RW_PropCefClick(playerid, id);', CEFUI)
        self.assertIn('if (mode == UI_PROPERTY) {', INTER)
        self.assertIn('return RW_PropClick(playerid, row);', INTER)
        self.assertIn('RW_PropRows(playerid);', PROP)
        self.assertIn('RW_AppRowProp(playerid,', PROP)

    def test_the_commands_exist(self):
        for command in ('property', 'enter', 'exit', 'doors', 'paytax', 'till', 'bid', 'rob'):
            self.assertIn('"%s", true' % command, CMDS)
        help_text = block(CMDS, 'stock RW_CommandHelp(playerid)')
        self.assertIn('Недвижимость: /property', help_text)
        for command in ('/till', '/bid', '/rob'):
            self.assertIn(command, help_text)

    def test_the_doors_are_marked_in_the_world(self):
        init = block(PROP, 'stock RW_PropInit()')
        self.assertIn('CreatePickup(1272, 1, gPropDoor[i][0]', init)
        self.assertIn('Create3DTextLabel(label,', init)

    def test_player_facing_text_is_english_only(self):
        """Игрок видит только латиницу: кириллица бы распалась в клиенте."""
        stripped = re.sub(r'/\*.*?\*/', '', PROP, flags=re.S)
        literals = re.findall(r'"([^"\r\n]*)"', stripped)
        for text in literals:
            if text.startswith(('INSERT', 'UPDATE', 'SELECT', 'CREATE')):
                continue
            for char in text:
                self.assertLess(ord(char), 128, text)

class PropertyAuctionTest(unittest.TestCase):
    """Аукцион: три просрочки — торги, владельцу 75 %."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(PROP, 'CREATE TABLE IF NOT EXISTS property_auctions') + ';')
        statement = sql(PROP, 'INSERT OR IGNORE INTO property_auctions')
        for i in range(8):
            self.db.execute(statement.replace('%d', str(i)))
        self.db.commit()

    def test_every_property_has_an_auction_row(self):
        rows = self.db.execute('SELECT id,bid,bidder,escrow,ends FROM property_auctions ORDER BY id').fetchall()
        self.assertEqual(len(rows), 8)
        for row in rows:
            self.assertEqual(row[1:], (0, 0, 0, 0))

    def test_the_auction_starts_when_the_tax_is_forgotten(self):
        start = block(PROP, 'stock RW_PropAucStart(id)')
        self.assertIn('gPropAucEnd[id] = gettime() + PROP_AUCTION_SECONDS;', start)
        self.assertIn('#define PROP_AUCTION_SECONDS    600', PROP)

    def test_a_bid_is_taken_from_the_account_at_once(self):
        bid = block(PROP, 'stock RW_PropBid(playerid, id, amount)')
        self.assertIn('if (amount < gPropBid[id] + PROP_BID_STEP)', bid)
        self.assertIn('if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account for such a bid.");', bid)
        self.assertIn('RW_BankTake(playerid, amount);', bid)
        self.assertIn('gPropEscrow[id] = amount;', bid)

    def test_the_previous_bid_comes_back(self):
        bid = block(PROP, 'stock RW_PropBid(playerid, id, amount)')
        self.assertIn('if (gPropEscrow[id] > 0) RW_PropCredit(gPropBidder[id], gPropEscrow[id]);', bid)
        credit = block(PROP, 'stock RW_PropCredit(account, amount)')
        self.assertIn('UPDATE bank_accounts SET balance=balance+%d WHERE player_id=%d', credit)

    def test_the_owner_cannot_buy_back_his_own_debt(self):
        bid = block(PROP, 'stock RW_PropBid(playerid, id, amount)')
        self.assertIn('if (RW_PropMine(playerid, id)) return RW_Error(playerid, "You cannot buy back your own property: pay the tax.");', bid)

    def test_closing_pays_the_old_owner_seventy_five_percent(self):
        close = block(PROP, 'stock RW_PropAucClose(id)')
        self.assertIn('new back = price * PROP_SELL_PERCENT / 100;', close)
        self.assertIn('if (old > 0) RW_PropCredit(old, back);', close)
        self.assertIn('gPropOwner[id] = gPropBidder[id];', close)
        self.assertIn('gPropMissed[id] = 0;', close)
        self.assertIn('RW_PropAucSave(id);', close)

    def test_the_timer_closes_finished_auctions(self):
        self.assertIn('SetTimer("RW_PropPulse", 1000, true);', MAIN)
        pulse = block(PROP, 'public RW_PropPulse()')
        self.assertIn('if (gPropAucEnd[id] <= 0 || gettime() < gPropAucEnd[id]) continue;', pulse)
        self.assertIn('RW_PropAucClose(id);', pulse)
        self.assertIn('forward RW_PropPulse();', CORE)


class BusinessTillTest(unittest.TestCase):
    """Касса бизнеса: выручка с покупок, внос и изъятие владельцем."""

    def test_a_share_of_every_sale_lands_in_the_till(self):
        self.assertIn('RW_PropSale(playerid, price);', block(ECON, 'stock RW_ShopBuy(playerid, row)'))
        self.assertIn('forward RW_PropSale(playerid, price);', CORE)
        sale = block(PROP, 'public RW_PropSale(playerid, price)')
        self.assertIn('if (id < 0 || gPropOwner[id] <= 0 || gPropInfo[id][0] != PROP_KIND_BIZ) return 0;', sale)
        self.assertIn('gPropCash[id] = gPropCash[id] + price * BIZ_TILL_SHARE / 100;', sale)
        self.assertIn('#define BIZ_TILL_SHARE          10', PROP)

    def test_only_the_owner_touches_the_till(self):
        till = block(PROP, 'stock RW_PropTillCommand(playerid, const params[])')
        self.assertIn('if (id < 0 || !RW_PropMine(playerid, id) || gPropInfo[id][0] != PROP_KIND_BIZ)', till)
        self.assertIn('gPropInfo[id][0] != PROP_KIND_BIZ', till)
        self.assertIn('Stand at the door of your business.', till)

    def test_taking_needs_money_in_the_till(self):
        till = block(PROP, 'stock RW_PropTillCommand(playerid, const params[])')
        self.assertIn('if (amount > gPropCash[id]) return RW_Error(playerid, "The till does not have that much.");', till)
        self.assertIn('RW_BankAdd(playerid, amount);', till)
        self.assertIn('if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account.");', till)


class RobberyTest(unittest.TestCase):
    """Ограбление бизнеса: с 12:00 до 01:00."""

    def test_the_window_is_twelve_to_one(self):
        window = block(PROP, 'stock RW_PropRobWindow()')
        self.assertIn('gettime(hour, minute, second);', window)
        self.assertIn('hour >= ROB_START_HOUR || hour < ROB_END_HOUR', window)
        self.assertIn('#define ROB_START_HOUR          12', PROP)
        self.assertIn('#define ROB_END_HOUR            1', PROP)

    def test_only_a_business_and_only_in_the_window(self):
        rob = block(PROP, 'stock RW_PropRob(playerid, id)')
        self.assertIn('if (gPropInfo[id][0] != PROP_KIND_BIZ) return RW_Error(playerid, "Only a business can be robbed.");', rob)
        self.assertIn('if (!RW_PropRobWindow()) return RW_Error(playerid, "Too early:', rob)
        self.assertIn('gettime() < gPropRobEnd[id]', rob)

    def test_an_officer_on_duty_does_not_rob(self):
        rob = block(PROP, 'stock RW_PropRob(playerid, id)')
        self.assertIn('RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0', rob)
        self.assertIn('An officer on duty does not rob shops.', rob)

    def test_the_alarm_reaches_the_police_and_the_loot_is_a_share(self):
        rob = block(PROP, 'stock RW_PropRob(playerid, id)')
        self.assertIn('Alarm at %s (%d, %d)', rob)
        self.assertIn('RW_PoliceOnDuty(i) && RW_PoliceBadge(i) > 0', rob)
        self.assertIn('new take = gPropCash[id] * ROB_SHARE / 100;', rob)
        self.assertIn('RW_AddMoney(playerid, take);', rob)
        self.assertIn('#define ROB_SHARE               30', PROP)
        self.assertIn('random(100) < ROB_FAIL_CHANCE', rob)



if __name__ == '__main__':
    unittest.main()
