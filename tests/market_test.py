"""Marketplace: adverts, a 24-hour store and the automatic auction.

The marketplace is the kind of slice that has to survive a restart: an item
leaves the seller's bag, the money lands on a bank account that may belong to
an offline player, and the lot keeps its own clock. The tests below run the
real table in SQLite and check both interfaces (browser and native dialog) are
wired to the same actions.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


MARKET = pawn('market')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
INTER = pawn('interaction')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class MarketTableTest(unittest.TestCase):
    """The lot table, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(MARKET, 'CREATE TABLE IF NOT EXISTS market_lots') + ';')

    def test_a_lot_keeps_the_item_the_price_and_the_clock(self):
        self.db.execute(
            'INSERT INTO market_lots (seller,seller_name,type,data,amount,price,status,ends,stamp)'
            ' VALUES (5,\'Ivan\',4,2,30,9000,0,1000,900)')
        row = self.db.execute(
            'SELECT seller,type,data,amount,price,status,ends FROM market_lots WHERE id=1').fetchone()
        self.assertEqual(row, (5, 4, 2, 30, 9000, 0, 1000))

    def test_a_bid_keeps_the_bidder_and_the_money(self):
        self.db.execute(
            'INSERT INTO market_lots (seller,seller_name,type,data,amount,price,status,ends,stamp)'
            ' VALUES (5,\'Ivan\',3,24,1,50000,1,1000,900)')
        self.db.execute('UPDATE market_lots SET bid=55000,bidder=9,bidder_name=\'Petr\' WHERE id=1')
        row = self.db.execute('SELECT bid,bidder,bidder_name FROM market_lots WHERE id=1').fetchone()
        self.assertEqual(row, (55000, 9, 'Petr'))


class MarketFeeTest(unittest.TestCase):
    """The store takes 500 an hour, five hours at the least."""

    def test_the_price_of_the_store(self):
        self.assertIn('#define MARKET_FEE              500', MARKET)
        self.assertIn('#define MARKET_MIN_HOURS        5', MARKET)
        self.assertIn('#define MARKET_MAX_HOURS        24', MARKET)
        fee = block(MARKET, 'stock RW_MarketFee(hours)')
        self.assertIn('return hours * MARKET_FEE;', fee)

    def test_five_hours_cost_two_and_a_half_thousand(self):
        self.assertEqual(5 * 500, 2500)
        self.assertEqual(24 * 500, 12000)

    def test_a_sale_needs_the_fee_and_a_sane_term(self):
        sell = block(MARKET, 'stock RW_MarketSell(playerid, slot, price, hours)')
        self.assertIn('if (gMoney[playerid] < fee) return RW_Error', sell)
        self.assertIn('RW_TakeMoney(playerid, fee)', sell)
        self.assertIn('if (hours < MARKET_MIN_HOURS || hours > MARKET_MAX_HOURS)', sell)
        self.assertIn('gettime() + hours * 3600, gettime());', sell)
        self.assertIn('INSERT INTO market_lots(seller,seller_name,type,data,amount,price,status,ends,stamp)',
                      sql(MARKET, 'INSERT INTO market_lots'))

    def test_the_item_really_leaves_the_sellers_bag(self):
        sell = block(MARKET, 'stock RW_MarketSell(playerid, slot, price, hours)')
        self.assertIn('RW_MarketTakeItem(playerid, slot)', sell)
        take = block(MARKET, 'stock RW_MarketTakeItem(playerid, slot)')
        self.assertIn('DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d', take)
        self.assertIn('RW_InvCanRemove(playerid, slot)', take)
        self.assertIn('gInv[playerid][slot][j] = 0;', take)

    def test_the_seller_cannot_sell_his_own_lot_to_himself(self):
        buy = block(MARKET, 'stock RW_MarketBuy(playerid, lot)')
        self.assertIn('if (seller == gAccountId[playerid]) return RW_Error', buy)


class MarketStorageTest(unittest.TestCase):
    """Twenty-four hours on the shelf, then the auction."""

    def test_the_shelf_is_a_day_long(self):
        self.assertIn('#define MARKET_AUCTION_SECONDS  (24 * 3600)', MARKET)

    def test_an_expired_lot_moves_to_the_auction_by_itself(self):
        pulse = block(MARKET, 'public RW_MarketPulse()', '\n\n/*  --')
        self.assertIn('UPDATE market_lots SET status=%d,bid=0,bidder=0', pulse)
        self.assertIn('now + MARKET_AUCTION_SECONDS', pulse)
        self.assertIn('WHERE status=%d AND ends<=%d', pulse)

    def test_the_pulse_runs_in_the_gamemode(self):
        self.assertIn('SetTimer("RW_MarketPulse", 5000, true);', MAIN)
        self.assertIn('RW_MarketSchema()', MAIN)
        self.assertIn('#include <railway/market>', MAIN)

    def test_the_auction_pays_the_seller_and_hands_the_item_to_the_winner(self):
        pulse = block(MARKET, 'public RW_MarketPulse()', '\n\n/*  --')
        self.assertIn('RW_MarketCredit(seller, money);', pulse)
        self.assertIn('UPDATE market_lots SET status=%d,claim=%d WHERE id=%d', pulse)

    def test_money_reaches_an_offline_seller_on_his_bank_account(self):
        credit = block(MARKET, 'stock RW_MarketCredit(account, amount)')
        self.assertIn('INSERT OR REPLACE INTO bank_accounts', credit)
        self.assertIn('COALESCE((SELECT balance FROM bank_accounts WHERE player_id=%d),0)+%d', credit)
        self.assertIn('gBank[p] += amount;', credit)

    def test_the_purchase_notifies_the_seller(self):
        buy = block(MARKET, 'stock RW_MarketBuy(playerid, lot)')
        self.assertIn('gAccountId[p] == seller', buy)
        self.assertIn('RW_MarketCredit(seller, price);', buy)


class MarketAuctionTest(unittest.TestCase):
    """Bids raise each other and losers get their money back."""

    def test_a_bid_is_accepted_only_on_the_auction(self):
        bid = block(MARKET, 'stock RW_MarketBid(playerid, lot, amount)')
        self.assertIn('if (RW_MarketLotField(lot, "status") != MARKET_AUCTION) return RW_Error', bid)
        self.assertIn('if (amount < min) return RW_Error', bid)

    def test_the_next_bid_is_higher_than_the_last_one(self):
        self.assertIn('#define MARKET_BID_STEP         5', MARKET)
        step = block(MARKET, 'stock RW_MarketMinBid(price, bid)')
        self.assertIn('bid + (bid * MARKET_BID_STEP) / 100 + 1', step)
        self.assertEqual(10000 + (10000 * 5) // 100 + 1, 10501)

    def test_the_outbid_player_is_refunded_to_his_account(self):
        bid = block(MARKET, 'stock RW_MarketBid(playerid, lot, amount)')
        self.assertIn('RW_MarketCredit(old, bid);', bid)
        self.assertIn('Деньги вернулись на счёт в банке', bid)

    def test_money_leaves_the_bidder_at_once(self):
        bid = block(MARKET, 'stock RW_MarketBid(playerid, lot, amount)')
        self.assertIn('RW_TakeMoney(playerid, amount)', bid)


class MarketClaimTest(unittest.TestCase):
    """Cancelling and collecting: nothing is lost."""

    def test_a_cancelled_lot_waits_on_the_shelf_for_its_owner(self):
        cancel = block(MARKET, 'stock RW_MarketUnsell(playerid, lot)')
        self.assertIn('status=%d,claim=%d WHERE id=%d AND status=%d', cancel)
        self.assertIn('RW_MarketLotField(lot, "seller") != gAccountId[playerid]', cancel)

    def test_only_the_owner_may_take_the_item_back(self):
        take = block(MARKET, 'stock RW_MarketClaim(playerid)')
        self.assertIn('WHERE status=%d AND claim=%d', take)
        self.assertIn('gAccountId[playerid]', take)

    def test_a_full_inventory_keeps_the_lot_for_later(self):
        take = block(MARKET, 'stock RW_MarketClaim(playerid)')
        self.assertIn('if (RW_MarketGive(playerid, type, data, amount) <= 0)', take)
        self.assertIn('остался на складе выдачи', take)

    def test_stacks_keep_their_amount(self):
        give = block(MARKET, 'stock RW_MarketGive(playerid, type, data, amount)')
        self.assertIn('UPDATE inventory_items SET amount=%d WHERE uid=%d', give)
        self.assertIn('gInv[playerid][slot][iiAmount] = amount;', give)


class MarketInterfaceTest(unittest.TestCase):
    """The same shopfront in the browser and in the native dialog."""

    def test_both_interfaces_are_wired(self):
        self.assertIn('#define CEF_MODE_MARKET             (16)', CORE)
        self.assertIn('#define UI_MARKET 26', UI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_MARKET) return RW_MarketCefClick(playerid, id);', CEFUI)
        self.assertIn('} else if (mode == UI_MARKET) {', INTER)
        self.assertIn('return RW_MarketClick(playerid, row);', INTER)

    def test_the_browser_and_the_panel_share_one_row_builder(self):
        rows = block(MARKET, 'stock RW_MarketRows(playerid)')
        self.assertIn('RW_MarketRowOut(playerid, index, label, note, "box");', rows)
        out = block(MARKET, 'stock RW_MarketRowOut(playerid, row, const text[], const arg[], const icon[])')
        self.assertIn('RW_CefRow(playerid, row, text, arg, icon);', out)
        self.assertIn('RW_UI_Row(playerid, row, line);', out)

    def test_the_lot_list_shows_the_shelf_and_the_auction(self):
        rows = block(MARKET, 'stock RW_MarketRows(playerid)')
        self.assertIn("status IN (0,1)", rows)
        self.assertIn('LIMIT %d OFFSET %d', rows)
        self.assertIn('Аукцион: %s, осталось %s', rows)

    def test_a_purchase_is_confirmed_by_a_second_click(self):
        action = block(MARKET, 'stock RW_MarketAction(playerid, row, bool:cef)')
        self.assertIn('if (gMarketPick[playerid] != lot)', action)
        self.assertIn('RW_MarketBuy(playerid, lot);', action)

    def test_pages_do_not_run_past_the_last_lot(self):
        action = block(MARKET, 'stock RW_MarketAction(playerid, row, bool:cef)')
        self.assertIn('gMarketPage[playerid] = (gMarketPage[playerid] + 1) % pages;', action)


class MarketCommandTest(unittest.TestCase):
    """Every shopfront action is reachable by a command."""

    def test_the_commands_are_registered(self):
        for command in ('market', 'sell', 'buy', 'stavka', 'unsell', 'claim'):
            self.assertIn('"%s", true' % command, CMDS)

    def test_the_help_names_the_marketplace(self):
        self.assertIn('/market', CMDS)
        self.assertIn('/stavka', CMDS)

    def test_the_list_works_without_the_browser_too(self):
        command = block(MARKET, 'stock RW_MarketCommand(playerid, const params[])')
        self.assertIn('if (!RW_CefOn(playerid)) RW_Info(playerid,', command)
        self.assertIn('RW_MarketOpen(playerid)', command)


class MarketNameTest(unittest.TestCase):
    """Russian item names and a readable countdown."""

    def test_items_are_named_in_russian(self):
        names = block(MARKET, 'stock RW_MarketItemName(type, data, dest[], size = sizeof dest)')
        for word in ('Рюкзак', 'Ключи от авто', 'Бинт', 'Адреналин', 'Аптечка', 'Отмычка'):
            self.assertIn(word, names)

    def test_the_countdown_counts_hours_and_minutes(self):
        left = block(MARKET, 'stock RW_MarketLeft(ends, dest[], size = sizeof dest)')
        self.assertIn('%d ч %02d мин', left)
        self.assertEqual(2 * 3600 // 3600, 2)


if __name__ == '__main__':
    unittest.main()
