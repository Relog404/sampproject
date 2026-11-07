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
MEDICAL = pawn('medical')


class ClickabilityTest(unittest.TestCase):
    def test_every_browser_mode_answers_a_click(self):
        """Страница без обработчика — кнопка, которая ничего не делает."""
        modes = re.findall(r'#define (CEF_MODE_[A-Z_]+)\s+\(\d+\)', CORE)
        self.assertTrue(modes)
        dispatch = CEF.split('public RW_CefClick(playerid, browserid, id)', 1)[1].split('\n}', 1)[0]
        for name in modes:
            if name == 'CEF_MODE_NONE':
                continue
            # обработчик может быть один на несколько режимов (настройки,
            # бинды и карта разбираются общим RW_UI_Click), главное — чтобы
            # режим вообще встречался в разборе клика
            self.assertIn(name, dispatch)

    def test_every_panel_mode_is_known_to_the_bridge(self):
        """UI-режим панели знает, какую страницу открыть, — или честно
        остаётся на нативном диалоге."""
        bridge = block(UI, 'stock RW_UI_CefMode(mode)')
        for mode in ('UI_GARAGE', 'UI_KEYMENU', 'UI_DOWNED', 'UI_INVENTORY',
                     'UI_SETTINGS', 'UI_BIND', 'UI_MAP', 'UI_BANK', 'UI_SHOP', 'UI_JOB'):
            self.assertIn('if (mode == %s)' % mode, bridge)
        self.assertIn('return CEF_MODE_NONE;', bridge)

    def test_browser_pages_end_with_a_render(self):
        """Строки копятся на странице и рисуются один раз по команде «конец»:
        у гаража это RW_CefEnd, у инвентаря — RW_CefInvEnd."""
        self.assertIn('return RW_CefEnd(playerid);', block(GARAGE, 'stock RW_GarageCef(playerid)'))
        self.assertIn('return RW_CefEnd(playerid);', block(GARAGE, 'stock RW_KeyMenuCef(playerid)'))
        self.assertIn('return RW_CefInvEnd(playerid);', block(pawn('inventory'), 'stock RW_InvCef(playerid)'))


class KeyMenuScreenTest(unittest.TestCase):
    def test_the_key_menu_opens_a_page(self):
        open_ = block(GARAGE, 'stock RW_KeyMenuOpen(playerid, target)')
        self.assertIn('if (RW_CefOn(playerid)) return RW_KeyMenuCef(playerid);', open_)
        screen = block(GARAGE, 'stock RW_KeyMenuCef(playerid)')
        self.assertIn('RW_CefOpen(playerid, CEF_MODE_KEYS, "Выдать ключ", subtitle)', screen)
        self.assertIn('return RW_CefEnd(playerid);', screen)

    def test_the_rows_are_shared_with_the_text_draw_menu(self):
        screen = block(GARAGE, 'stock RW_KeyMenuCef(playerid)')
        self.assertIn('RW_KeyMenuRow(playerid, row, label, sizeof label)', screen)
        row = block(GARAGE, 'stock bool:RW_KeyMenuRow(')
        self.assertIn('RW_VehicleName(', row)

    def test_the_buttons_do_not_fall_on_the_rows(self):
        """Строки — 0..5, кнопки — 6..8: они же помещаются в нативный диалог."""
        self.assertIn('#define CEF_KEYS_PREV               6', CORE)
        self.assertIn('#define CEF_KEYS_NEXT               7', CORE)
        self.assertIn('#define CEF_KEYS_CLOSE              8', CORE)

    def test_a_click_grants_the_key_or_turns_the_page(self):
        click = block(GARAGE, 'stock RW_KeyMenuCefClick(playerid, id)')
        self.assertIn('if (id == CEF_KEYS_CLOSE) return RW_UI_Close(playerid);', click)
        self.assertIn('RW_KeyMenuBuild(playerid);', click)
        self.assertIn('if (id >= 0 && id < 6) return RW_KeyMenuGrant(playerid, id);', click)
        grant = block(GARAGE, 'stock RW_KeyMenuGrant(playerid, row)')
        self.assertIn('RW_InvGrant(target, ITEM_CAR_KEY, data)', grant)
        self.assertIn('Target player is no longer available', grant)

    def test_without_a_browser_the_menu_is_a_native_dialog(self):
        """Без браузера тот же список показывает нативный диалог."""
        native = block(GARAGE, 'stock RW_KeyMenuNative(playerid)')
        self.assertIn('RW_UI_Panel(playerid, UI_KEYMENU, "Выдать ключ", subtitle);', native)
        self.assertIn('RW_KeyMenuRow(playerid, row, label, sizeof label)', native)
        self.assertIn('RW_UI_Row(playerid, CEF_KEYS_CLOSE, "Закрыть");', native)
        for marker in ('gKeyTD', 'gKeyCreated', 'PlayerTextDraw', 'RW_KeyMenuClick', 'RW_KeyMenuRender'):
            self.assertNotIn(marker, GARAGE)


class DownedScreenTest(unittest.TestCase):
    """Экран раненого: две кнопки, которые должны работать."""

    def test_the_screen_opens_a_page(self):
        open_ = block(MEDICAL, 'stock RW_MedOpen(playerid)')
        self.assertIn('return RW_MedCef(playerid);', open_)
        self.assertNotIn('PlayerTextDraw', open_)
        screen = block(MEDICAL, 'stock RW_MedCef(playerid)')
        self.assertIn('RW_UI_Panel(playerid, UI_DOWNED, "Вы ранены", subtitle);', screen)
        self.assertIn('RW_UI_Row(playerid, 0, "Вызвать скорую");', screen)
        self.assertIn('RW_UI_Row(playerid, 1, "Подождать ещё 7 минут");', screen)

    def test_the_timer_is_shared(self):
        render = block(MEDICAL, 'stock RW_MedRender(playerid)')
        self.assertIn('return RW_MedCef(playerid);', render)
        time = block(MEDICAL, 'stock RW_MedTimeLeft(')
        self.assertIn('gDownEnd[playerid] - GetTickCount()', time)

    def test_both_buttons_are_clickable(self):
        click = block(MEDICAL, 'stock RW_MedCefClick(playerid, id)')
        self.assertIn('if (id == 0) return RW_MedCall(playerid);', click)
        self.assertIn('if (id == 1) return RW_MedWait(playerid);', click)
        self.assertIn('GetTickCount() - gMedTick[playerid] < 400', click)

    def test_the_downed_screen_has_no_text_draw(self):
        """Экран раненого один: общая панель, тексдравов больше нет."""
        for marker in ('gMedTD', 'gMedCreated', 'PlayerTextDraw', 'RW_MedClick', 'RW_MedLabel', 'RW_MedIcon'):
            self.assertNotIn(marker, MEDICAL)
        main = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
        self.assertNotIn('RW_MedClick', main)


class SettingsScreenTest(unittest.TestCase):
    """Настройки, бинды и карта телепорта: общая панель, общий клик."""

    def setUp(self):
        self.interaction = pawn('interaction')

    def test_the_modes_are_mapped(self):
        bridge = block(UI, 'stock RW_UI_CefMode(mode)')
        self.assertIn('if (mode == UI_SETTINGS) return CEF_MODE_SETTINGS;', bridge)
        self.assertIn('if (mode == UI_BIND) return CEF_MODE_BIND;', bridge)
        self.assertIn('if (mode == UI_MAP) return CEF_MODE_MAP;', bridge)

    def test_clicks_go_to_the_same_handler(self):
        """Клик с тексдрава и клик в браузере попадают в RW_UI_Click,
        который разбирает строку по gUIMode: расхождений быть не должно."""
        dispatch = CEF.split('public RW_CefClick(playerid, browserid, id)', 1)[1].split('\n}', 1)[0]
        self.assertIn('return RW_UI_Click(playerid, id);', dispatch)
        click = block(self.interaction, 'stock RW_UI_Click(playerid, row)')
        self.assertIn('if (mode == UI_SETTINGS)', click)
        self.assertIn('} else if (mode == UI_BIND) {', click)
        self.assertIn('} else if (mode == UI_MAP) {', click)

    def test_binding_lists_the_keys(self):
        settings = block(self.interaction, 'stock RW_UI_Settings(playerid)')
        self.assertIn('RW_UI_Row(playerid, i, label);', settings)
        self.assertIn('RW_UI_Row(playerid, 6, "Reset: H / N / Y");', settings)


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
        """Строки — 0..5, кнопки — 6..8: иначе клик по машине читался бы
        как «Назад». Те же номера идут и в нативный диалог."""
        self.assertIn('#define CEF_GARAGE_PREV             6', CORE)
        self.assertIn('#define CEF_GARAGE_NEXT             7', CORE)
        self.assertIn('#define CEF_GARAGE_CLOSE            8', CORE)
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
        """После пролистывания список рисуется той же функцией, что и
        при открытии: браузер и нативный диалог не расходятся."""
        click = block(GARAGE, 'stock RW_GarageCefClick(playerid, id)')
        self.assertIn('RW_GarageRefresh(playerid)', click)
        refresh = block(GARAGE, 'stock RW_GarageRefresh(playerid)')
        self.assertIn('return RW_GarageCef(playerid);', refresh)
        self.assertIn('return RW_GarageNative(playerid);', refresh)
        native = block(GARAGE, 'stock RW_GarageNative(playerid)')
        self.assertIn('RW_UI_Panel(playerid, UI_GARAGE, "Гараж", subtitle);', native)
        self.assertIn('RW_UI_Row(playerid, CEF_GARAGE_CLOSE, "Закрыть");', native)
        for marker in ('gGarageTD', 'gGarageCreated', 'PlayerTextDraw', 'RW_GarageClick'):
            self.assertNotIn(marker, GARAGE)
        refresh = block(GARAGE, 'stock RW_GarageRefresh(playerid)')
        self.assertIn('if (RW_CefOn(playerid)) return RW_GarageCef(playerid);', refresh)
        self.assertIn('return RW_GarageNative(playerid);', refresh)


if __name__ == '__main__':
    unittest.main()