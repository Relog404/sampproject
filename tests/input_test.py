"""Поле ввода в браузере: цена, ставка и сумма — кликом, а не командой.

Команды в чате остались, но там, где игроку надо ввести число, сервер
теперь открывает карточку с полем: страница отдаёт текст, сервер его
разбирает и проверяет сам (числа — только цифры, срок — от 5 до 24).
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGES = ROOT / 'server/scriptfiles/cef/railway'


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


def block(src, signature):
    start = src.index(signature)
    depth = 0
    for i in range(start + len(signature) - 1, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError('не закрыт блок: %s' % signature)


INPUT = pawn('input')
CEFUI = pawn('cefui')
STUB = pawn('cefstub')
CORE = pawn('core')
UI = pawn('ui')
INV = pawn('inventory')
MARKET = pawn('market')
PHONEAPPS = pawn('phoneapps')
PROP = pawn('property')
ECON = pawn('economy')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
SCRIPT = (PAGES / 'js/input.js').read_text(encoding='utf-8')
HTML = (PAGES / 'index.html').read_text(encoding='utf-8')


class InputProtocolTest(unittest.TestCase):
    """Сервер просит — страница отвечает."""

    def test_the_module_is_included_after_everyone_it_calls(self):
        self.assertIn('#include <railway/input>', MAIN)
        self.assertTrue(MAIN.index('#include <railway/commands>') < MAIN.index('#include <railway/input>'))

    def test_the_server_asks_and_the_page_answers(self):
        self.assertIn('CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:ask"', CEFUI)
        self.assertIn('CEF_RegisterEvent("ui:answer", "RW_CefInputSubmit", Argument_String);', CEFUI)
        self.assertIn('public RW_CefInputSubmit(playerid, browserid, const value[])', INPUT)
        self.assertIn("RW.on('ui:ask'", SCRIPT)
        self.assertIn("RW.emit('ui:answer'", SCRIPT)
        self.assertIn('<script src="js/input.js"></script>', HTML)

    def test_the_input_has_its_own_browser_mode(self):
        self.assertIn('#define CEF_MODE_INPUT', CORE)
        self.assertIn('#define UI_INPUT 30', UI)
        self.assertIn('if (mode == UI_INPUT) return CEF_MODE_INPUT;', UI)
        self.assertIn('gCefMode[playerid] = CEF_MODE_INPUT;', CEFUI)

    def test_the_pluginless_build_keeps_the_same_functions(self):
        for name in ('RW_CefInput', 'RW_CefInputClose'):
            self.assertIn('stock %s(' % name, CEFUI)
            self.assertIn('stock %s(' % name, STUB)

    def test_the_card_closes_by_every_road(self):
        """Esc, клик мимо и «Отмена» не оставляют игру с занятыми клавишами."""
        self.assertIn("if (event.key === 'Escape') { event.preventDefault(); abort(); }", SCRIPT)
        self.assertIn("RW.emit('ui:closed')", SCRIPT)
        closed = block(CEFUI, 'public RW_CefClosed(playerid, browserid)')
        self.assertIn('if (RW_InputBusy(playerid)) return RW_InputCancel(playerid);', closed)
        self.assertIn('gUIInput[playerid] = false;', block(INPUT, 'stock RW_InputReset(playerid)'))
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_INPUT) { RW_InputCancel(playerid); return 1; }', CEFUI)


class InputNumbersTest(unittest.TestCase):
    """Страница отдаёт текст; числа разбирает сервер."""

    def test_only_digits_become_a_number(self):
        number = block(INPUT, 'stock RW_InputNumber(const text[])')
        self.assertIn("if (text[i] < '0' || text[i] > '9') return -1;", number)
        self.assertIn('if (digits > 9) return -1;', number)
        self.assertIn('return digits ? value : -1;', number)

    def test_an_empty_answer_changes_nothing(self):
        submit = block(INPUT, 'public RW_CefInputSubmit(playerid, browserid, const value[])')
        self.assertIn('if (!RW_InputTrim(text)) return RW_Error', submit)
        self.assertIn('if (!gInputOn[playerid]) return 0;', submit)


class SellByClickTest(unittest.TestCase):
    """Продажа: цена и срок спрашиваются по очереди, а не одной командой."""

    def test_the_card_got_a_sell_button(self):
        self.assertIn('#define INV_CEF_ACT_SELL        6', INV)
        context = block(INV, 'stock RW_InvCefContext(playerid, slot)')
        self.assertIn('RW_MarketSellable(type)', context)
        self.assertIn('RW_CefInvAction(playerid, INV_CEF_ACT_SELL', context)
        click = block(INV, 'stock RW_InvCefClick(playerid, id)')
        self.assertIn('if (action == INV_CEF_ACT_SELL) return RW_InputSellPrice(playerid, gInvSelected[playerid]);', click)

    def test_not_everything_can_be_sold(self):
        sellable = block(MARKET, 'stock bool:RW_MarketSellable(type)')
        self.assertIn('ITEM_BACKPACK', sellable)
        self.assertIn('ITEM_CAR_KEY', sellable)

    def test_price_then_term_then_the_lot(self):
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('if (kind == INPUT_SELL_PRICE)', apply_block)
        self.assertIn('gInputArg2[playerid] = price;', apply_block)
        self.assertIn('RW_InputOpen(playerid, INPUT_SELL_HOURS, arg', apply_block)
        self.assertIn('RW_MarketSell(playerid, arg, arg2, hours);', apply_block)

    def test_the_slot_is_found_by_the_item_not_by_luck(self):
        open_block = block(INPUT, 'stock RW_InputSellPrice(playerid, uid)')
        self.assertIn('new slot = RW_InvFind(playerid, uid);', open_block)
        self.assertIn('RW_InvCanRemove(playerid, slot)', open_block)
        self.assertIn('RW_InputOpen(playerid, INPUT_SELL_PRICE, slot', open_block)


class BidByClickTest(unittest.TestCase):
    """Ставка на маркете и на аукционе недвижимости — кликом."""

    def test_the_marketplace_got_a_bid_row(self):
        rows = block(MARKET, 'stock RW_MarketRows(playerid)')
        self.assertIn('RW_MarketRowOut(playerid, 7,', rows)
        self.assertIn('RW_CefOn(playerid) && gCefMode[playerid] == CEF_MODE_MARKET', rows)
        self.assertIn('"Поднять ставку: лот №%d"', rows)

    def test_the_bid_row_asks_for_the_amount(self):
        action = block(MARKET, 'stock RW_MarketAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 7 && cef)', action)
        self.assertIn('RW_InputOpen(playerid, INPUT_BID, pick', action)
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('RW_MarketBid(playerid, arg, amount);', apply_block)

    def test_the_property_auction_bids_the_same_way(self):
        action = block(PROP, 'stock RW_PropAction(playerid, row, bool:cef)')
        self.assertIn('RW_InputOpen(playerid, INPUT_PROP_BID, id', action)
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('RW_PropBid(playerid, arg, amount);', apply_block)

    def test_the_commands_still_work_and_are_one_liners_now(self):
        self.assertIn('return RW_MarketBid(playerid, strval(id), strval(sum));', MARKET)
        self.assertIn('return RW_MarketSell(playerid, strval(slot)', MARKET)
        self.assertIn('<script src="js/input.js"></script>', HTML)


class TillByClickTest(unittest.TestCase):
    """Касса бизнеса: положить и взять по клику, налог и ограбление тоже."""

    def test_the_till_rows_are_clickable(self):
        rows = block(PROP, 'stock RW_PropRows(playerid)')
        self.assertIn('Put money in', rows)
        self.assertIn('Take money out of the till', rows)
        action = block(PROP, 'stock RW_PropAction(playerid, row, bool:cef)')
        self.assertIn('INPUT_TILL_PUT, id', action)
        self.assertIn('INPUT_TILL_TAKE, id', action)
        self.assertIn('return RW_PropPayTax(playerid, id);', action)
        self.assertIn('if (row == 6 && gPropInfo[id][0] == PROP_KIND_BIZ) return RW_PropRob(playerid, id);', action)

    def test_the_amount_reaches_the_shared_function(self):
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('RW_PropTillAmount(playerid, arg, amount, kind == INPUT_TILL_PUT);', apply_block)

    def test_the_property_text_stays_latin(self):
        """Панель недвижимости живёт и в нативном диалоге: кириллица там
        распадётся, поэтому строки в property.inc — только латиница."""
        stripped = re.sub(r'/\*.*?\*/', '', PROP, flags=re.S)
        for text in re.findall(r'"([^"\r\n]*)"', stripped):
            if text.startswith(('INSERT', 'UPDATE', 'SELECT', 'CREATE')):
                continue
            for char in text:
                self.assertLess(ord(char), 128, text)


class MarketByClickTest(unittest.TestCase):
    """Покупка, снятие и выдача — кнопками витрины, а не командами."""

    def test_the_row_under_the_list_tells_what_the_lot_can_do(self):
        rows = block(MARKET, 'stock RW_MarketRows(playerid)')
        self.assertIn('"Снять лот №%d с витрины"', rows)
        self.assertIn('"Купить лот №%d"', rows)
        self.assertIn('"Поднять ставку: лот №%d"', rows)
        self.assertIn('"Выберите лот: нажмите на строку выше"', rows)
        self.assertIn('"Лот закрыт: выберите другой"', rows)

    def test_the_action_follows_the_lot_not_the_habit(self):
        action = block(MARKET, 'stock RW_MarketAction(playerid, row, bool:cef)')
        self.assertIn('return RW_MarketUnsell(playerid, pick);', action)
        self.assertIn('return RW_MarketBuy(playerid, pick);', action)
        self.assertIn('if (row == 9 && cef) return RW_MarketClaim(playerid);', action)

    def test_the_browser_needs_no_second_click(self):
        action = block(MARKET, 'stock RW_MarketAction(playerid, row, bool:cef)')
        self.assertIn('if (cef) { RW_MarketRefresh(playerid); return RW_Info', action)
        self.assertIn('return RW_Info(playerid, "Нажмите на эту строку ещё раз, чтобы купить.");', action)

    def test_the_commands_are_one_liners_over_the_same_functions(self):
        self.assertIn('return RW_MarketUnsell(playerid, strval(params));', MARKET)
        self.assertIn('stock RW_TakeCommand(playerid) { return RW_MarketClaim(playerid); }', MARKET)
        self.assertIn('stock RW_CefBank(playerid)', ECON)


class BankByClickTest(unittest.TestCase):
    """Банкомат: суммы и перевод — по клику, а не по id из головы."""

    def test_the_atm_asks_for_the_sum(self):
        action = block(ECON, 'stock RW_BankAction(playerid, row, bool:cef)')
        self.assertIn('INPUT_BANK_PUT, 0, "Bank"', action)
        self.assertIn('INPUT_BANK_TAKE, 0, "Bank"', action)
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('RW_BankDeposit(playerid, amount);', apply_block)
        self.assertIn('RW_BankWithdraw(playerid, amount);', apply_block)
        self.assertIn('RW_BankTransfer(playerid, arg, amount);', apply_block)

    def test_the_transfer_picks_a_player_standing_nearby(self):
        near = block(ECON, 'stock RW_BankNearRows(playerid)')
        self.assertIn('BANK_TRANSFER_RANGE', near)
        self.assertIn('if (p == playerid || !IsPlayerConnected(p) || !gLogged[p]) continue;', near)
        action = block(ECON, 'stock RW_BankAction(playerid, row, bool:cef)')
        self.assertIn('"That player has walked away: open the ATM again."', action)

    def test_the_panel_comes_back_with_the_new_numbers(self):
        apply_block = block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')
        self.assertIn('return RW_BankRender(playerid);', apply_block)
        self.assertIn('return RW_MarketOpen(playerid);', apply_block)
        self.assertIn('return RW_PropRender(playerid);', apply_block)


class PhoneByClickTest(unittest.TestCase):
    """Телефон: строка приложения спрашивает, а не отсылает в чат."""

    def apply_block(self):
        return block(INPUT, 'stock RW_InputApply(playerid, kind, arg, arg2, const text[])')

    def test_the_transfer_asks_for_the_player_first(self):
        apply_block = self.apply_block()
        self.assertIn('if (kind == INPUT_TRANSFER_ID) {', apply_block)
        self.assertIn('if (target == playerid || !IsPlayerConnected(target) || !gLogged[target])', apply_block)
        self.assertIn('gInputArg2[playerid] = target;', apply_block)
        self.assertIn('RW_InputOpen(playerid, INPUT_TRANSFER_SUM, arg, title,', apply_block)
        self.assertIn('format(title, sizeof title, "Перевод: %s", nick);', apply_block)
        self.assertIn('"Transfer"', PHONEAPPS)

    def test_the_transfer_sum_goes_to_the_same_function_as_the_atm(self):
        apply_block = self.apply_block()
        self.assertIn('return RW_BankTransfer(playerid, arg2, amount);', apply_block)
        self.assertIn('if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");', apply_block)

    def test_the_group_invite_and_the_group_chat_are_fields(self):
        apply_block = self.apply_block()
        self.assertIn('return RW_GroupInvite(playerid, target);', apply_block)
        self.assertIn('return RW_GroupSay(playerid, line);', apply_block)
        self.assertIn('if (!RW_InputText(copy, line, sizeof line))', apply_block)

    def test_the_advert_is_written_in_a_field(self):
        apply_block = self.apply_block()
        self.assertIn('if (kind == INPUT_NEWS_TEXT) {', apply_block)
        self.assertIn('return RW_NewsPost(playerid, line);', apply_block)

    def test_the_new_kinds_have_their_numbers(self):
        for name, value in (('INPUT_LAW_PRICE', 12), ('INPUT_TRANSFER_ID', 13),
                            ('INPUT_TRANSFER_SUM', 14), ('INPUT_GROUP_ID', 15),
                            ('INPUT_GROUP_TEXT', 16), ('INPUT_NEWS_TEXT', 17)):
            self.assertRegex(CORE, r'#define %s\s+\(%d\)' % (name, value))


class PropertyByClickTest(unittest.TestCase):
    """Налог по клику и честная строка налога у чужого объекта."""

    def test_the_tax_row_answers(self):
        action = block(PROP, 'stock RW_PropAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 3) {', action)
        self.assertIn('return RW_PropPayTax(playerid, id);', action)

    def test_a_strangers_tax_row_shows_the_sum_not_the_format(self):
        rows = block(PROP, 'stock RW_PropRows(playerid)')
        self.assertNotIn('RW_AppRowProp(playerid, 3, "Tax per payday: %s"', rows)
        self.assertIn('"Tax per payday: %s (the owner pays)"', rows)


if __name__ == '__main__':
    unittest.main()
