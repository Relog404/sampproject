"""Кликабельность CEF-меню и страница гаража.

Каждое меню, у которого есть страница в браузере, обязано уметь отвечать
на клик: иначе кнопка есть, а нажатие теряется. Здесь проверяется, что у
каждого CEF_MODE_* есть обработчик, что номера кнопок не пересекаются с
номерами строк и что страница заканчивается отрисовкой (RW_CefEnd).
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


CORE = pawn('core')
CEF = pawn('cefui')
GARAGE = pawn('garage')
UI = pawn('ui')


class ClickabilityTest(unittest.TestCase):
    def test_every_browser_mode_answers_a_click(self):
        """Страница без обработчика — кнопка, которая ничего не делает."""
        modes = re.findall(r'#define (CEF_MODE_[A-Z_]+)\s+\((\d+)\)', CORE)
        self.assertTrue(modes)
        for name, number in modes:
            if name == 'CEF_MODE_NONE':
                continue
            self.assertIn('if (gCefMode[playerid] == %s) return RW_' % name, CEF)

    def test_every_panel_mode_is_known_to_the_bridge(self):
        """UI-режим панели знает, какую страницу открыть, — или честно
        остаётся на нативном диалоге."""
        bridge = block(UI, 'stock RW_UI_CefMode(mode)')
        for mode in ('UI_GARAGE', 'UI_INVENTORY', 'UI_BANK', 'UI_SHOP', 'UI_JOB'):
            self.assertIn('if (mode == %s)' % mode, bridge)
        self.assertIn('return CEF_MODE_NONE;', bridge)

    def test_browser_pages_end_with_a_render(self):
        """Строки копятся на странице и рисуются один раз по команде «конец»:
        у гаража это RW_CefEnd, у инвентаря — RW_CefInvEnd."""
        self.assertIn('return RW_CefEnd(playerid);', block(GARAGE, 'stock RW_GarageCef(playerid)'))
        self.assertIn('return RW_CefInvEnd(playerid);', block(pawn('inventory'), 'stock RW_InvCef(playerid)'))


class GarageScreenTest(unittest.TestCase):
    def test_the_garage_opens_a_page(self):
        open_ = block(GARAGE, 'stock RW_GarageOpen(playerid, bool:resetPage = true)')
        self.assertIn('if (RW_CefOn(playerid)) return RW_GarageCef(playerid);', open_)
        screen = block(GARAGE, 'stock RW_GarageCef(playerid)')
        self.assertIn('RW_CefOpen(playerid, CEF_MODE_GARAGE, "Гараж", subtitle)', screen)

    def test_six_cars_per_page_with_the_ownership_note(self):
        screen = block(GARAGE, 'stock RW_GarageCef(playerid)')
        self.assertIn('for (new row; row < 6; row++)', screen)
        self.assertIn('gGarageList[playerid][idx] > 0 ? ("в собственности") : ("временный ключ")', screen)
        self.assertIn('RW_CefRow(playerid, row, label,', screen)

    def test_the_buttons_do_not_fall_on_the_rows(self):
        """Строки — 0..5, кнопки — с 20: иначе клик по машине читался бы
        как «Назад»."""
        self.assertIn('#define CEF_GARAGE_PREV             20', CORE)
        self.assertIn('#define CEF_GARAGE_NEXT             21', CORE)
        self.assertIn('#define CEF_GARAGE_CLOSE            22', CORE)
        screen = block(GARAGE, 'stock RW_GarageCef(playerid)')
        self.assertIn('RW_CefAction(playerid, CEF_GARAGE_PREV, "Назад", "");', screen)
        self.assertIn('RW_CefAction(playerid, CEF_GARAGE_NEXT, "Вперёд", "");', screen)
        self.assertIn('RW_CefAction(playerid, CEF_GARAGE_CLOSE, "Закрыть", "danger");', screen)

    def test_a_click_spawns_a_car_or_turns_the_page(self):
        click = block(GARAGE, 'stock RW_GarageCefClick(playerid, id)')
        self.assertIn('if (id == CEF_GARAGE_CLOSE) return RW_UI_Close(playerid);', click)
        self.assertIn('if (id == CEF_GARAGE_PREV)', click)
        self.assertIn('if (id == CEF_GARAGE_NEXT)', click)
        self.assertIn('return RW_GarageSpawn(playerid, gGarageList[playerid][index]);', click)
        self.assertIn('if (id >= 0 && id < 6)', click)

    def test_the_paging_buttons_follow_the_page(self):
        screen = block(GARAGE, 'stock RW_GarageCef(playerid)')
        self.assertIn('if (gGaragePage[playerid] > 0) RW_CefAction(playerid, CEF_GARAGE_PREV', screen)
        self.assertIn('if ((gGaragePage[playerid] + 1) * 6 < gGarageCount[playerid]) RW_CefAction(playerid, CEF_GARAGE_NEXT', screen)

    def test_turning_the_page_redraws_the_same_way(self):
        """После пролистывания страница рисуется той же функцией, что и
        при открытии: иначе браузер и тексдрав-меню расходятся."""
        click = block(GARAGE, 'stock RW_GarageCefClick(playerid, id)')
        self.assertIn('RW_GarageRefresh(playerid)', click)
        refresh = block(GARAGE, 'stock RW_GarageRefresh(playerid)')
        self.assertIn('if (RW_CefOn(playerid)) return RW_GarageCef(playerid);', refresh)
        self.assertIn('return RW_GarageOpen(playerid, false);', refresh)


if __name__ == '__main__':
    unittest.main()
