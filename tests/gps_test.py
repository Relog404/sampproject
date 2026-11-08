"""Навигатор: метка на карте, расстояние в HUD, список мест по клику.

/gps не телепортирует: метка стоит на карте и на радаре, а HUD показывает,
сколько метров осталось. Приехал — метка снимается сама.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGES = ROOT / 'server/scriptfiles/cef/railway'


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


def block(src, signature, end=None):
    start = src.index(signature)
    if end is not None:
        return src[start:src.index(end, start) + len(end)]
    depth = 0
    for i in range(start + len(signature) - 1, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError('не закрыт блок: %s' % signature)


CITY = pawn('city')
CORE = pawn('core')
UI = pawn('ui')
CMDS = pawn('commands')
HUD = pawn('hud')
CEFUI = pawn('cefui')
STUB = pawn('cefstub')
HUD_JS = (PAGES / 'js/hud.js').read_text(encoding='utf-8')
HUD_HTML = (PAGES / 'hud.html').read_text(encoding='utf-8')
CSS = (PAGES / 'css/app.css').read_text(encoding='utf-8')


class GpsWiringTest(unittest.TestCase):
    """Команда, панель и режим браузера на своём месте."""

    def test_the_command_is_registered(self):
        self.assertIn('"gps", true', CMDS)
        self.assertIn('return RW_GpsCommand(playerid, params);', CMDS)

    def test_the_panel_has_its_own_mode(self):
        self.assertIn('#define CEF_MODE_GPS', CORE)
        self.assertIn('#define UI_GPS 31', UI)
        self.assertIn('if (mode == UI_GPS) return CEF_MODE_GPS;', UI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_GPS) return RW_GpsAction(playerid, id, true);', CEFUI)
        self.assertIn('} else if (mode == UI_GPS) {', pawn('interaction'))

    def test_the_marker_has_its_own_icon_slot(self):
        self.assertIn('#define GPS_ICON', CORE)
        set_block = block(CITY, 'stock RW_GpsSet(playerid, index)')
        self.assertIn('SetPlayerMapIcon(playerid, GPS_ICON,', set_block)
        clear = block(CITY, 'stock RW_GpsClear(playerid)')
        self.assertIn('RemovePlayerMapIcon(playerid, GPS_ICON);', clear)

    def test_the_pulse_runs_whether_or_not_the_hud_is_on(self):
        """Метка ведёт и с выключенным HUD: пульс зовётся до проверки экрана."""
        pulse = block(HUD, 'public RW_HudPulse()')
        self.assertTrue(pulse.index('RW_GpsPulse(playerid);') < pulse.index('if (!RW_CefOn(playerid)'))
        self.assertIn('forward RW_GpsPulse(playerid);', CORE)


class GpsRouteTest(unittest.TestCase):
    """Ехать надо самому: метка ведёт и снимается на месте."""

    def test_the_distance_is_measured_and_shown(self):
        pulse = block(CITY, 'public RW_GpsPulse(playerid)')
        self.assertIn('GetPlayerPos(playerid, x, y, z);', pulse)
        self.assertIn('format(text, sizeof text, "%s: %d м", gGpsLabel[playerid], left);', pulse)
        self.assertIn('return RW_CefHudGps(playerid, text);', pulse)

    def test_arriving_clears_the_marker(self):
        pulse = block(CITY, 'public RW_GpsPulse(playerid)')
        self.assertIn('if (left <= GPS_ARRIVE)', pulse)
        self.assertIn('RW_GpsClear(playerid);', pulse)
        self.assertIn('"Вы на месте."', pulse)
        self.assertIn('#define GPS_ARRIVE', CITY)

    def test_the_same_distance_is_not_sent_twice(self):
        pulse = block(CITY, 'public RW_GpsPulse(playerid)')
        self.assertIn('if (left == gGpsLeft[playerid]) return 1;', pulse)

    def test_off_takes_the_marker_away(self):
        command = block(CITY, 'stock RW_GpsCommand(playerid, const params[])')
        self.assertIn('"off"', command)
        self.assertIn('RW_GpsClear(playerid);', command)
        self.assertIn('"Метка снята."', command)


class GpsPlacesTest(unittest.TestCase):
    """Места настоящие: те же координаты, что и у остального сервера."""

    def test_the_places_match_the_city(self):
        places = block(CITY, 'static const Float:gGpsPlace[GPS_MAX_PLACES][3] = {', '};')
        names = block(CITY, 'static const gGpsPlaceName[GPS_MAX_PLACES][] = {', '};')
        self.assertEqual(len(re.findall(r'\{[^}]+\}', places)), len(re.findall(r'"[^"]+"', names)))
        self.assertIn('#define GPS_MAX_PLACES', CITY)

    def test_the_names_are_russian_and_unique(self):
        names = re.findall(r'"([^"]+)"', block(CITY, 'static const gGpsPlaceName[GPS_MAX_PLACES][] = {', '};'))
        self.assertTrue(len(names) >= 8)
        self.assertEqual(len(names), len(set(names)))
        for name in names:
            self.assertTrue(any('а' <= c <= 'я' or 'А' <= c <= 'Я' for c in name), name)

    def test_the_station_is_the_station(self):
        """Точка вокзала в навигаторе и точка появления игрока — одна."""
        places = block(CITY, 'static const Float:gGpsPlace[GPS_MAX_PLACES][3] = {', '};')
        station = re.search(r'STATION_X\s+\((\d+\.\d+)\)', CORE)
        self.assertTrue(station)
        self.assertIn(station.group(1), places)

    def test_the_search_finds_by_the_beginning(self):
        find = block(CITY, 'stock RW_GpsFind(const text[])')
        self.assertIn('strfind(gGpsPlaceName[i], text, true) == 0', find)
        self.assertIn('return -1;', find)


class GpsHudTest(unittest.TestCase):
    """Расстояние видно на экране, а не в чате."""

    def test_the_hud_has_a_place_for_it(self):
        self.assertIn('id="gps"', HUD_HTML)
        self.assertIn("RW.on('hud:gps'", HUD_JS)
        self.assertIn("gps.className = 'gps on';", HUD_JS)
        self.assertIn('.gps.on', CSS)

    def test_the_event_is_registered_and_stubbed(self):
        self.assertIn('CEF_RegisterEvent("hud:gps", "RW_GpsEvent", Argument_String);', CEFUI)
        self.assertIn('CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:gps"', CEFUI)
        self.assertIn('stock RW_CefHudGps(playerid, const text[])', CEFUI)
        self.assertIn('stock RW_CefHudGps(playerid, const text[])', STUB)


class ScrollbarTest(unittest.TestCase):
    """Полоса прокрутки от Chrome в игре не нужна."""

    def test_the_bar_is_gone_but_scrolling_stays(self):
        self.assertIn('*::-webkit-scrollbar { width: 0; height: 0; display: none; }', CSS)
        self.assertIn('scrollbar-width: none;', CSS)
        self.assertIn('overscroll-behavior: none;', CSS)

    def test_a_long_panel_scrolls_inside_the_card(self):
        """Десять строк GPS или восемь лотов рынка не должны уезжать за экран."""
        sheet = block(CSS, '.sheet {', '}')
        self.assertIn('max-height: 88vh;', sheet)
        self.assertIn('display: flex;', sheet)
        self.assertIn('flex-direction: column;', sheet)
        rows = block(CSS, '.rows {', '}')
        self.assertIn('overflow-y: auto;', rows)
        self.assertIn('min-height: 0;', rows)
        self.assertIn('overscroll-behavior: contain;', rows)
        card = block(CSS, '.card {', '}')
        self.assertIn('min-height: 0;', card)
        self.assertIn('flex: 1 1 auto;', card)
        self.assertIn('.sheet-head { padding: 0 6px 14px; text-align: center; flex: 0 0 auto; }', CSS)


class ClickThroughTest(unittest.TestCase):
    """Окно выше меню не должно съедать нажатия."""

    def test_the_phone_catches_clicks_only_with_the_device(self):
        phone = (PAGES / 'css/phone.css').read_text(encoding='utf-8')
        stage = block(phone, '.stage {')
        self.assertIn('pointer-events: none;', stage)
        self.assertIn('.device { pointer-events: auto; }', phone)
        self.assertIn('.device:not(.on) { pointer-events: none; }', phone)

    def test_the_toasts_are_read_only(self):
        self.assertIn('pointer-events: none;', block(CSS, '.toasts {'))


if __name__ == '__main__':
    unittest.main()
