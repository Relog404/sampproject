"""Инвентарь в браузере: сетка вещей и карточка по правой кнопке.

Раньше инвентарь был только тексдрав-сеткой. Теперь `/inv` и клавиша
открывают страницу в браузере: ячейки приходят событиями `inv:cell`,
по правой кнопке сервер присылает карточку (`inv:context`) с описанием
вещи и кнопки действий (`inv:action`). Ответ страницы — одно число
в `ui:click`, поэтому всё, что видно игроку, собрано здесь же.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


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


INV = pawn('inventory')
CEF = pawn('cefui')
STUB = pawn('cefstub')
CORE = pawn('core')
PAGE = (ROOT / 'server/scriptfiles/cef/railway/js/inventory.js').read_text(encoding='utf-8')
HTML = (ROOT / 'server/scriptfiles/cef/railway/index.html').read_text(encoding='utf-8')


class InventoryModeTest(unittest.TestCase):
    def test_inventory_has_its_own_browser_mode(self):
        self.assertIn('#define CEF_MODE_INVENTORY          (20)', CORE)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_INVENTORY) return RW_InvCefClick(playerid, id);', CEF)

    def test_the_page_is_loaded_by_the_menu(self):
        self.assertIn('<script src="js/inventory.js"></script>', HTML)

    def test_the_events_are_emitted_to_the_menu_browser(self):
        for event in ('inv:open', 'inv:cell', 'inv:context', 'inv:action', 'inv:end'):
            self.assertIn('"%s"' % event, CEF)

    def test_the_stub_build_keeps_the_same_entry_points(self):
        stub = pawn('cefstub')
        for name in ('RW_CefInvOpen', 'RW_CefInvCell', 'RW_CefInvInfo', 'RW_CefInvAction', 'RW_CefInvEnd'):
            self.assertIn('stock %s(' % name, stub)


class InventoryGridTest(unittest.TestCase):
    def test_opening_prefers_the_browser(self):
        open_ = block(INV, 'stock RW_InvOpen(playerid)')
        self.assertIn('if (RW_CefOn(playerid)) { gUIMode[playerid] = UI_INVENTORY; return RW_InvCef(playerid, true); }', open_)
        # без браузера игроку честно говорят, что инвентарь — страница
        self.assertIn('Инвентарь открывается в браузере', open_)

    def test_the_grid_shows_equipment_pockets_bag_and_the_ground(self):
        grid = block(INV, 'stock RW_InvCef(playerid, bool:opening = true)')
        self.assertIn('RW_InvCefCell(playerid, 131,', grid)     # рюкзак
        self.assertIn('RW_InvCefCell(playerid, 132,', grid)     # оружие
        self.assertIn('RW_InvCefCell(playerid, 133,', grid)     # броня
        self.assertIn('for (new i; i < INV_SLOTS; i++)', grid)
        self.assertIn('RW_InvCefCell(playerid, 80 + i,', grid)  # рядом на земле
        self.assertIn('return RW_CefInvEnd(playerid);', grid)

    def test_a_locked_bag_is_shown_but_not_filled(self):
        grid = block(INV, 'stock RW_InvCef(playerid, bool:opening = true)')
        self.assertIn('if (i >= INV_MAIN && !hasBag) { RW_CefInvCell(playerid, i, "", "", 0, 0, "locked"); continue; }', grid)

    def test_the_page_draws_cells_and_asks_for_the_card_on_the_right_button(self):
        self.assertIn("RW.on('inv:cell'", PAGE)
        self.assertIn("RW.emit('ui:click', 1000 + id)", PAGE)
        self.assertIn("RW.emit('ui:click', id)", PAGE)
        self.assertIn('contextmenu', PAGE)


class InventoryCardTest(unittest.TestCase):
    def test_the_card_carries_the_name_the_note_and_the_weight(self):
        card = block(INV, 'stock RW_InvCefContext(playerid, slot)')
        self.assertIn('RW_InvTitle(type, data, amount, title, sizeof title);', card)
        self.assertIn('RW_CefInvInfo(playerid, slot, title, desc, stats);', card)
        self.assertIn('RW_InvGrams(type, data, amount)', card)

    def test_a_thing_on_the_ground_is_offered_to_pick_up(self):
        card = block(INV, 'stock RW_InvCefContext(playerid, slot)')
        self.assertIn('RW_CefInvAction(playerid, INV_CEF_ACT_TAKE, "Поднять", "ok");', card)

    def test_a_thing_in_the_pockets_is_offered_to_drop_and_to_move(self):
        card = block(INV, 'stock RW_InvCefContext(playerid, slot)')
        self.assertIn('RW_CefInvAction(playerid, INV_CEF_ACT_DROP, "Выбросить", "danger");', card)
        self.assertIn('slot < INV_MAIN ? ("В рюкзак") : ("В карманы")', card)

    def test_the_card_buttons_are_rendered_by_the_page(self):
        self.assertIn("RW.on('inv:context'", PAGE)
        self.assertIn("RW.on('inv:action'", PAGE)
        self.assertIn("RW.emit('ui:click', 2000 + id)", PAGE)


class InventoryClickTest(unittest.TestCase):
    def test_the_click_numbers_do_not_overlap(self):
        self.assertIn('#define INV_CEF_CONTEXT         1000', INV)
        self.assertIn('#define INV_CEF_ACTION          2000', INV)
        self.assertIn('#define INV_CEF_CLOSE           3000', INV)

    def test_actions_run_the_same_functions_as_the_commands(self):
        click = block(INV, 'stock RW_InvCefClick(playerid, id)')
        self.assertIn('if (action == INV_CEF_ACT_DROP) { RW_InvDrop(playerid); return RW_InvCef(playerid, false); }', click)
        self.assertIn('if (action == INV_CEF_ACT_TAKE) { RW_GroundTake(playerid); return RW_InvCef(playerid, false); }', click)
        self.assertIn('RW_InvTransfer(playerid,', click)
        self.assertIn('if (id == INV_CEF_CLOSE) return RW_UI_Close(playerid);', click)

    def test_every_action_redraws_the_grid(self):
        click = block(INV, 'stock RW_InvCefClick(playerid, id)')
        # четыре действия перерисовывают сетку, пятое (левый клик) тоже:
        # «Взять в руки» и «Использовать» закрывают инвентарь перед действием.
        self.assertEqual(click.count('return RW_InvCef(playerid, false);'), 5)
        self.assertIn('if (action == INV_CEF_ACT_WEAR) { RW_InvWearSelected(playerid); return RW_InvCef(playerid, false); }', click)
        self.assertIn('if (action == INV_CEF_ACT_USE) { RW_UI_Close(playerid); return RW_InvUseSelected(playerid); }', click)

    def test_the_grid_is_refreshed_without_grabbing_the_focus(self):
        """Перерисовка раз в секунду не трогает фокус: иначе браузер снова
        перехватывает ввод каждую секунду и клик по кнопке пропадает."""
        self.assertIn('if (opening) {', block(INV, 'stock RW_InvCef(playerid, bool:opening = true)'))
        self.assertIn('} else RW_CefInvRefresh(playerid);', block(INV, 'stock RW_InvCef(playerid, bool:opening = true)'))
        render = block(INV, 'stock RW_InvRender(playerid)')
        self.assertIn('return RW_InvCef(playerid, false);', render)
        self.assertNotIn('RW_CefInvOpen', render)
        self.assertIn('CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:redraw");', CEF)
        self.assertIn('stock RW_CefInvRefresh(playerid)', CEF)
        self.assertIn('stock RW_CefInvRefresh(playerid)', STUB)
        self.assertIn("RW.on('inv:redraw'", PAGE)
        self.assertNotIn('show(true)', block(PAGE, "RW.on('inv:redraw'"))

    def test_the_card_of_a_vanished_item_is_closed_by_the_server(self):
        self.assertIn('if (gInvSelected[playerid] > 0 && !RW_InvKept(playerid, gInvSelected[playerid]))',
                      block(INV, 'stock RW_InvCef(playerid, bool:opening = true)'))
        self.assertIn('RW_CefInvInfo(playerid, -1, "", "", "");', INV)
        self.assertIn('stock bool:RW_InvKept(playerid, uid)', INV)
        self.assertIn('if (id < 0) { if (card) card.className = '  + "'inv-card'" + '; return; }', PAGE)

    def test_russian_item_names_are_built_once(self):
        title = block(INV, 'stock RW_InvTitle(')
        self.assertIn('Рюкзак', title)
        self.assertIn('Дефибриллятор', title)
        self.assertIn('Отмычка', title)
        self.assertIn('Прекурсор', title)
        self.assertIn('Шишки', title)


class InventoryTextTest(unittest.TestCase):
    def test_the_new_strings_the_player_reads_are_russian(self):
        for text in ('"Инвентарь", subtitle', '"Поднять"', '"Выбросить"', '"В рюкзак"', '"В карманы"',
                     '"Вещь лежит в инвентаре."', '"Лежит на земле: поднимите."', '"Пусто"'):
            self.assertIn(text, INV)
        self.assertIn('Свободная ячейка.', INV)


if __name__ == '__main__':
    unittest.main()
