"""Interface: HUD, donate menu, city people and the newcomer's welcome.

The server keeps everything: the browser pages only draw what they are given.
Pawn strings stay short English keys and the page prints them in Russian, so
the same server works with the browser and with the text interface.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CEF = ROOT / 'server/scriptfiles/cef/railway'


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


HUD = pawn('hud')
DONATE = pawn('donate')
CITY = pawn('city')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
CMDS = pawn('commands')
ECON = pawn('economy')
ACCOUNTS = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
PAGE = (CEF / 'hud.html').read_text(encoding='utf-8')
SCRIPT = (CEF / 'js/hud.js').read_text(encoding='utf-8')
CSS = (CEF / 'css/app.css').read_text(encoding='utf-8')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class HudRadarTest(unittest.TestCase):
    """The radar is gone: the game has its own, and the HUD stays quiet."""

    def test_no_radar_is_drawn_or_sent(self):
        self.assertNotIn('gHudPoi', HUD)
        self.assertNotIn('gHudZone', HUD)
        self.assertNotIn('hud:radar', HUD)
        self.assertNotIn('id="radar"', PAGE)
        self.assertNotIn('id="bonus"', PAGE)
        self.assertNotIn('.radar {', CSS)
        self.assertNotIn('.bonus {', CSS)
        self.assertNotIn('hud:radar', SCRIPT)

    def test_the_pulse_updates_the_hints(self):
        self.assertIn('SetTimer("RW_HudPulse", 500, true);', MAIN)
        self.assertIn('forward RW_HudPulse();', CORE)
        pulse = block(HUD, 'public RW_HudPulse()')
        self.assertIn('RW_HudHints(playerid);', pulse)
        self.assertNotIn('RW_HudBonusCard', pulse)


class HudKeysTest(unittest.TestCase):
    """Key hints and the HUD page itself."""

    def test_hints_change_with_the_situation(self):
        hints = block(HUD, 'stock RW_HudHints(playerid)')
        self.assertIn('"N:engine;H:doors;"', hints)
        self.assertIn('"Y:interaction;"', hints)
        self.assertIn('"F2:donate;"', hints)

    def test_the_page_shows_the_hints_in_russian(self):
        self.assertIn("interaction: 'Взаимодействие'", SCRIPT)
        self.assertIn("donate: 'Донат'", SCRIPT)
        self.assertIn("engine: 'Двигатель'", SCRIPT)

    def test_hints_are_on_the_page(self):
        self.assertIn('id="hints"', PAGE)
        self.assertNotIn('id="radar"', PAGE)
        self.assertNotIn('id="bonus"', PAGE)

    def test_the_browser_menu_of_the_page_is_disabled(self):
        self.assertIn("document.addEventListener('contextmenu'", SCRIPT)
        self.assertIn('e.preventDefault()', SCRIPT)

    def test_the_mouse_is_toggled_by_a_key(self):
        self.assertIn('if ((newkeys & KEY_YES) && (newkeys & KEY_WALK) && !(oldkeys & KEY_YES))', MAIN)
        self.assertIn('return RW_HudMouseToggle(playerid);', MAIN)
        self.assertIn('forward RW_HudMouseToggle(playerid);', CORE)
        self.assertIn('stock RW_HudMouseToggle(playerid)', HUD)
        self.assertIn('stock RW_HudMouseCommand(playerid) { return RW_HudMouseToggle(playerid); }', HUD)
        self.assertIn('"AltY:mouse;"', HUD)
        self.assertIn("mouse: 'Мышь'", SCRIPT)
        self.assertIn('Alt + Y - курсор мыши', CMDS)

    def test_the_mouse_is_hidden_until_the_player_asks_for_it(self):
        self.assertIn("RW.on('hud:mouse'", SCRIPT)
        self.assertIn('.hud.mouse {', CSS)
        self.assertIn('stock RW_CefHudMouse(playerid, on)', CEFUI)
        self.assertIn('CEF_FocusBrowser(playerid, CEF_UI_HUD, on ? true : false);', CEFUI)
        self.assertIn('stock RW_HudMouseCommand(playerid)', HUD)
        self.assertIn('"mouse", true', CMDS)
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_HUD, CEF_UI_HUD_URL, false, false);', CEFUI)

    def test_the_style_is_transparent_and_minimal(self):
        self.assertIn('.hint {', CSS)
        self.assertIn('.hints {', CSS)
        self.assertIn('backdrop-filter', CSS)
        for colour in ('rgba(255,255,255,.05)', 'rgba(14,18,26,.45)'):
            self.assertIn(colour, CSS)

    def test_the_browser_is_created_and_destroyed(self):
        self.assertIn('#define CEF_UI_HUD          (103)', CEFUI)
        self.assertIn('CEF_CreateBrowser(playerid, CEF_UI_HUD, CEF_UI_HUD_URL, false, false);', CEFUI)
        self.assertIn('CEF_DestroyBrowser(playerid, CEF_UI_HUD);', CEFUI)


class DonateTest(unittest.TestCase):
    """The donate menu on F2: packages, points and the VIP income bonus."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(DONATE, 'CREATE TABLE IF NOT EXISTS donates') + ';')

    def test_points_are_kept_for_a_player(self):
        self.db.execute('INSERT INTO donates (player_id,points,vip) VALUES (3,150,1)')
        self.assertEqual(self.db.execute('SELECT points,vip FROM donates WHERE player_id=3').fetchone(), (150, 1))

    def test_eight_packages_including_the_money_exchange(self):
        names = block(DONATE, 'static const gDonateName[DONATE_MAX][] = {', '};')
        prices = block(DONATE, 'static const gDonatePrice[DONATE_MAX] = {', '};')
        kinds = block(DONATE, 'static const gDonateKind[DONATE_MAX] = {', '};')
        args = block(DONATE, 'static const gDonateArg[DONATE_MAX] = {', '};')
        self.assertEqual(len(re.findall(r'"[^"]+"', names)), 8)
        self.assertEqual(len(re.findall(r'\d+', prices)), 8)
        self.assertEqual(kinds.count('DONATE_KIND_'), 8)
        self.assertEqual(len(re.findall(r'\d+', args)), 8)
        self.assertEqual(kinds.count('DONATE_KIND_EXCHANGE'), 2)
        self.assertIn('Обмен', names)

    def test_the_exchange_works_without_any_top_up(self):
        buy = block(DONATE, 'stock RW_DonateBuy(playerid, row)')
        self.assertIn('if (gDonateKind[row] == DONATE_KIND_EXCHANGE) {', buy)
        self.assertIn('if (gBank[playerid] < gDonatePrice[row]) return RW_Error(playerid, "На счёте мало денег для обмена.");', buy)
        self.assertIn('RW_BankTake(playerid, gDonatePrice[row]);', buy)
        self.assertIn('gDonate[playerid] = gDonate[playerid] + gDonateArg[row];', buy)
        self.assertTrue(buy.index('DONATE_KIND_EXCHANGE') < buy.index('DONATE_KIND_VIP'))

    def test_the_menu_is_redrawn_after_a_purchase(self):
        self.assertIn('stock RW_DonateRefresh(playerid)', DONATE)
        refresh = block(DONATE, 'stock RW_DonateRefresh(playerid)')
        self.assertIn('if (gCefMode[playerid] != CEF_MODE_DONATE) return 1;', refresh)
        self.assertIn('return RW_DonateOpen(playerid);', refresh)
        buy = block(DONATE, 'stock RW_DonateBuy(playerid, row)')
        self.assertEqual(buy.count('RW_DonateRefresh(playerid);'), 3)

    def test_the_menu_speaks_russian(self):
        self.assertIn('Баланс: %d очков', DONATE)
        self.assertIn('VIP: %d (прибавка к доходу', DONATE)
        self.assertIn('Очки можно обменять на рубли', DONATE)

    def test_vip_raises_the_income_on_the_server(self):
        bonus = block(DONATE, 'stock RW_DonateBonus(playerid, amount)')
        self.assertIn('new const percent[] = DONATE_VIP_BONUS;', bonus)
        self.assertIn('return amount + amount * percent[bonus] / 100;', bonus)
        self.assertIn('salary = RW_DonateBonus(playerid, RW_JobSalary', ECON)

    def test_a_purchase_needs_points_and_never_gives_money_to_the_client(self):
        buy = block(DONATE, 'stock RW_DonateBuy(playerid, row)')
        self.assertIn('if (gDonate[playerid] < gDonatePrice[row])', buy)
        self.assertIn('gDonate[playerid] = gDonate[playerid] - gDonatePrice[row];', buy)
        self.assertIn('RW_BankAdd(playerid, gDonateArg[row]);', buy)

    def test_points_are_given_by_an_administrator(self):
        give = block(DONATE, 'stock RW_GiveDonateCommand(playerid, const params[])')
        self.assertIn('if (gAdminLevel[playerid] < 5) return RW_Error(playerid, "Admins only.");', give)

    def test_the_menu_opens_from_the_hud_and_from_a_command(self):
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_DONATE) return RW_DonateCefClick(playerid, id);', CEFUI)
        self.assertIn('else if (mode == UI_DONATE) {', pawn('interaction'))
        self.assertIn('return RW_DonateClick(playerid, row);', pawn('interaction'))
        self.assertIn('#define UI_DONATE 24', UI)
        self.assertIn('#define CEF_MODE_DONATE             (14)', CORE)
        self.assertIn('return RW_DonateOpen(playerid);', block(HUD, 'public RW_HudDonateEvent(playerid, browserid, arg)'))
        self.assertIn("RW.emit('hud:donate', 1)", SCRIPT)
        self.assertIn("e.key === 'F2'", SCRIPT)


class CityTest(unittest.TestCase):
    """City people, the spawn intro and the newcomer's welcome."""

    def test_six_people_with_names_lines_and_skins(self):
        points = block(CITY, 'static const Float:gCityActorPos[CITY_ACTORS][4] = {', '};')
        names = block(CITY, 'static const gCityActorName[CITY_ACTORS][] = {', '};')
        lines = block(CITY, 'static const gCityActorLine[CITY_ACTORS][] = {', '};')
        skins = block(CITY, 'static const gCityActorSkin[CITY_ACTORS] = {', '};')
        self.assertEqual(len(re.findall(r'\{[^}]+\}', points)), 6)
        self.assertEqual(len(re.findall(r'"[^"]+"', names)), 6)
        self.assertEqual(len(re.findall(r'"[^"]+"', lines)), 6)
        self.assertEqual(len(re.findall(r'\d+', skins)), 6)

    def test_people_stand_in_los_santos_and_are_safe(self):
        points = block(CITY, 'static const Float:gCityActorPos[CITY_ACTORS][4] = {', '};')
        for row in re.findall(r'\{([^}]+)\}', points):
            x, y, z, angle = [float(part) for part in row.split(',')]
            self.assertTrue(300.0 < x < 3000.0, row)
            self.assertTrue(-2700.0 < y < -300.0, row)
            self.assertTrue(0.0 < z < 40.0, row)
            self.assertTrue(-360.0 <= angle <= 360.0, row)
        self.assertIn('SetActorInvulnerable(gCityActor[i], true);', CITY)
        self.assertIn('Create3DTextLabel(text, 0xF5B04CFF', CITY)

    def test_talking_reaches_only_the_nearest_person(self):
        near = block(CITY, 'stock RW_CityNear(playerid)')
        self.assertIn('CITY_TALK_RANGE * CITY_TALK_RANGE', near)
        self.assertIn('return RW_Error(playerid, "There is nobody to talk to here.', CITY)

    def test_a_newcomer_gets_a_camera_intro_and_a_start_kit(self):
        self.assertIn('RW_CitySpawn(playerid);', MAIN)
        spawn = block(CITY, 'stock RW_CitySpawn(playerid)')
        self.assertIn('if (gCityWelcomed[playerid]) return 0;', spawn)
        self.assertIn('RW_CityIntro(playerid);', spawn)
        self.assertIn('TogglePlayerControllable(playerid, false);', CITY)
        self.assertIn('SetTimerEx("RW_CityIntroEnd", CITY_INTRO_MS, false, "i", playerid);', CITY)
        self.assertIn('TogglePlayerControllable(playerid, true);', CITY)
        welcome = block(CITY, 'stock RW_CityWelcome(playerid)')
        self.assertIn('RW_AddMoney(playerid, CITY_START_CASH);', welcome)
        self.assertIn('RW_BankAdd(playerid, CITY_START_BANK);', welcome)
        self.assertIn('if (gMoney[playerid] <= 0 && gBank[playerid] <= 0)', welcome)

    def test_the_tables_and_people_are_created_on_start(self):
        self.assertIn('RW_CityInit();', MAIN)
        self.assertIn('RW_DonateSchema()', MAIN)
        self.assertIn('RW_HudLoad(playerid);', ACCOUNTS)
        self.assertIn('RW_DonateLoad(playerid);', ACCOUNTS)
        self.assertIn('RW_DonateSave(playerid);', MAIN)
        self.assertNotIn('RW_HudSchema', HUD + MAIN)
        self.assertNotIn('RW_HudSave', HUD + MAIN)

    def test_every_command_reaches_the_server(self):
        for command in ('hud', 'donate', 'points', 'givedonate', 'talk'):
            self.assertIn('"%s", true' % command, CMDS)


class NoStarterBonusTest(unittest.TestCase):
    """Стартового бонуса больше нет: ни таблицы, ни карточки, ни команды."""

    def test_the_bonus_table_is_gone(self):
        self.assertNotIn('CREATE TABLE IF NOT EXISTS bonuses', HUD)
        self.assertNotIn('gHudDay', HUD)
        self.assertNotIn('gHudDaily', HUD)
        self.assertNotIn('gHudOnline', HUD)
        self.assertNotIn('gHudHidden', HUD)

    def test_no_bonus_money_is_paid(self):
        for marker in ('HUD_ONLINE_BONUS', 'HUD_DAILY_BONUS', 'RW_HudBonusClaim',
                       'RW_BonusCommand', 'RW_CefHudBonus'):
            self.assertNotIn(marker, HUD)
            self.assertNotIn(marker, MAIN)
        self.assertNotIn('"bonus", true', CMDS)

    def test_the_page_forgot_the_card(self):
        self.assertNotIn('hud:bonus', SCRIPT)
        self.assertNotIn('id="bonus"', PAGE)
        self.assertNotIn('.bonus {', CSS)


class InterfaceLanguageTest(unittest.TestCase):
    """Keys stay ASCII, the text a player reads is Russian."""

    def test_hud_keys_stay_ascii_so_the_page_can_translate_them(self):
        for source in (HUD, CITY):
            for text in re.findall(r'"([^"\r\n]*)"', source):
                if re.match(r'^[A-Za-z0-9]+:[a-zA-Z]+(;[A-Za-z0-9]+:[a-zA-Z]+)*;$', text):
                    self.assertTrue(text.isascii(), text)
        self.assertIn('"Y:interaction;"', HUD)

    def test_the_auth_and_the_welcome_speak_russian(self):
        self.assertIn('RW_ShowLoginDialog', UI)
        self.assertIn('добро пожаловать на вокзал', MAIN)
        self.assertIn('Здравствуйте, {F5B04C}%s{FFFFFF}!', ACCOUNTS)

    def test_the_pages_are_russian(self):
        self.assertIn('hud-body', PAGE)
        self.assertIn('Взаимодействие', SCRIPT)
        self.assertIn('Железная дорога', (CEF / 'auth.html').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
