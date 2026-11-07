"""Phone applications: bank, weather, group, lawyers, Uber, news, cars, camera.

The icons and the settings tables already lived in phone.inc; this slice is
what the apps DO. Every rule is on the server, the browser app and the
native dialog call the same stocks, and SQL from the Pawn sources is executed
here against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

APPS = pawn('phoneapps')
PHONE = pawn('phone')
CORE = pawn('core')
UI = pawn('ui')
INV = pawn('inventory')
ECON = pawn('economy')
POLICE = pawn('police')
INTER = pawn('interaction')
CEFUI = pawn('cefui')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class NewsTableTest(unittest.TestCase):
    """The advert board, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql(APPS, 'CREATE TABLE IF NOT EXISTS news_ads '))
        self.db.execute("CREATE TABLE accounts (id INT, name VARCHAR(32))")
        self.db.execute("INSERT INTO accounts VALUES(7,'Ivan')")

    def tearDown(self):
        self.db.close()

    def columns(self, table):
        return [row[1] for row in self.db.execute('PRAGMA table_info(%s)' % table)]

    def test_table_shape(self):
        self.assertEqual(self.columns('news_ads'), ['id', 'player_id', 'body', 'stamp'])

    def test_the_board_shows_the_newest_first(self):
        for body, stamp in (('Old advert', 1000), ('New advert', 2000)):
            self.db.execute(sql(APPS, 'INSERT INTO news_ads(') % (7, body, stamp))
        rows = self.db.execute(sql(APPS, 'SELECT a.name AS who,n.body,n.stamp FROM news_ads n LEFT JOIN accounts a ON a.id=n.player_id ORDER BY n.id DESC LIMIT 10')).fetchall()
        self.assertEqual(rows[0], ('Ivan', 'New advert', 2000))
        self.assertEqual(rows[1], ('Ivan', 'Old advert', 1000))

    def test_an_advert_is_paid_and_short(self):
        post = block(APPS, 'stock RW_NewsPost(playerid, const text[])')
        self.assertIn('if (strlen(text) > NEWS_MAX_LEN) return RW_Error(playerid, "Too long: keep it under 96 characters.");', post)
        self.assertIn('if (gMoney[playerid] < NEWS_PRICE) return RW_Error(playerid, "An advert costs 100 in cash.");', post)
        self.assertIn('RW_TakeMoney(playerid, NEWS_PRICE);', post)
        self.assertIn(sql(APPS, 'INSERT INTO news_ads('), post)

    def test_the_text_is_escaped_before_the_query(self):
        post = block(APPS, 'stock RW_NewsPost(playerid, const text[])')
        self.assertIn('RW_PoliceSqlName(nick, safe, sizeof safe);', post)


class BankAppTest(unittest.TestCase):
    """Fines are paid from the phone, cash only at the ATM."""

    def test_unpaid_fines_are_summed(self):
        self.assertIn(sql(APPS, 'SELECT amount FROM fines WHERE player_id=%d AND paid=0'), APPS)
        self.assertIn(sql(APPS, 'UPDATE fines SET paid=1 WHERE player_id=%d AND paid=0'), APPS)

    def test_the_account_is_used_before_the_cash(self):
        pay = block(APPS, 'stock RW_AppPayFines(playerid)')
        self.assertIn('if (gBank[playerid] >= sum) RW_BankTake(playerid, sum);', pay)
        self.assertIn('else if (gMoney[playerid] >= sum) RW_TakeMoney(playerid, sum);', pay)
        self.assertIn('else return RW_Error(playerid, "Not enough money for the fines.");', pay)
        self.assertIn('if (sum <= 0) return RW_Info(playerid, "No unpaid fines.");', pay)

    def test_cash_still_needs_an_atm(self):
        action = APPS.split('stock RW_PhoneAppAction(playerid, row, bool:cef)', 1)[1]
        self.assertIn('case PHONE_APP_BANK: {', action)
        self.assertIn('if (row == 1) return RW_AppPayFines(playerid);', action)
        self.assertIn('if (row == 2) return RW_BankOpen(playerid);', action)


class WeatherTest(unittest.TestCase):
    """Weather is one for the server and it moves the task pay."""

    def test_the_sky_is_known_and_named(self):
        table = APPS.split('static const gWeatherNames[][] = {', 1)[1].split('};', 1)[0]
        self.assertEqual(len(re.findall(r'"[^"]+"', table)), 6)
        self.assertRegex(CORE, r'#define WEATHER_COUNT\s+6')
        self.assertRegex(CORE, r'#define WEATHER_CHANGE_SECONDS\s+1800')

    def test_clear_sky_pays_couriers_and_truckers(self):
        bonus = block(APPS, 'stock RW_WeatherBonus(job)')
        self.assertIn('case 0: {', bonus)
        self.assertIn('if (job == JOB_TRUCKER || job == JOB_COURIER) return 20;', bonus)
        self.assertIn('if (job == JOB_MUSHROOM) return 20;', bonus)
        self.assertIn('if (job == JOB_QUARRY) return 20;', bonus)
        self.assertIn('if (job == JOB_TRUCKER) return -15;', bonus)

    def test_the_bonus_reaches_the_pay(self):
        done = ECON.split('stock RW_JobTaskDone(playerid)', 1)[1].split('\n}', 1)[0]
        self.assertIn('new weatherBonus = RW_WeatherBonus(gJob[playerid]);', done)
        self.assertIn('new bonus = weatherBonus + timeBonus;', done)
        self.assertIn('if (bonus != 0) pay = pay + floatround(float(pay) * float(bonus) / 100.0);', done)
        self.assertIn('%+d%% за погоду и время', done)

    def test_the_sky_changes_on_its_own(self):
        pulse = APPS.split('public RW_WeatherPulse()', 1)[1].split('\n}', 1)[0]
        self.assertIn('if (gWeatherTick >= WEATHER_CHANGE_SECONDS)', pulse)
        self.assertIn('RW_WeatherSet(next);', pulse)
        setfn = block(APPS, 'stock RW_WeatherSet(index)')
        self.assertIn('SetWeather(gWeatherIds[index]);', setfn)
        self.assertIn('SetPlayerWeather(i, gWeatherIds[index]);', setfn)


class GroupTest(unittest.TestCase):
    """A group is a chat with a map mark, and it lives on the server."""

    def test_the_leader_holds_the_group(self):
        create = block(APPS, 'stock RW_GroupCreate(playerid)')
        self.assertIn('gGroupLeader[playerid] = playerid;', create)
        leave = block(APPS, 'stock RW_GroupLeave(playerid, bool:silent = false)')
        self.assertIn('for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gGroupLeader[i] == playerid) gGroupLeader[i] = -1;', leave)

    def test_an_invitation_is_accepted_once(self):
        accept = block(APPS, 'stock RW_GroupAccept(playerid)')
        self.assertIn('new leader = gGroupInvite[playerid];', accept)
        self.assertIn('if (leader < 0 || !IsPlayerConnected(leader) || !gLogged[leader]) return RW_Error(playerid, "No invitation to accept.");', accept)
        self.assertIn('gGroupInvite[playerid] = -1;', accept)
        self.assertIn('gGroupLeader[playerid] = leader;', accept)

    def test_the_chat_costs_a_message(self):
        say = block(APPS, 'stock RW_GroupSay(playerid, const text[])')
        self.assertIn('if (gPhoneBalance[playerid] < RW_PhoneSmsCost(gPhoneTariff[playerid])) return RW_Error(playerid, "Not enough credit on the SIM.");', say)
        self.assertIn('gPhoneBalance[playerid] -= RW_PhoneSmsCost(gPhoneTariff[playerid]);', say)

    def test_the_leader_is_marked_on_the_map(self):
        pulse = APPS.split('public RW_GroupPulse()', 1)[1].split('\n}', 1)[0]
        self.assertIn('SetPlayerMapIcon(i, APP_ICON_ID, x, y, z, 0, 0x7BD88FFF, MAPICON_GLOBAL);', pulse)
        self.assertIn('RemovePlayerMapIcon(i, APP_ICON_ID);', pulse)


class LawyerAppTest(unittest.TestCase):
    """A lawyer is called from a cell, and the bail is shared."""

    def test_only_a_cell_can_call(self):
        call = block(APPS, 'stock RW_AppLawyerCall(playerid)')
        self.assertIn('if (gJailTime[playerid] <= 0) return RW_Error(playerid, "Lawyers are called from a cell only.");', call)
        self.assertIn('RW_LawyerLicensed(i)', call)
        self.assertIn('No lawyer is on duty right now. Try later or call 911.', call)

    def test_the_licence_expires(self):
        licensed = block(APPS, 'stock bool:RW_LawyerLicensed(playerid)')
        self.assertIn('gLawyerExpire[playerid] > gettime()', licensed)

    def test_the_bail_is_split_by_the_book(self):
        share = block(APPS, 'stock RW_LawyerBailShare(lawyer, officer, bail)')
        self.assertIn('new lawyerPart = bail * 50 / 100, copPart = bail * 10 / 100;', share)
        self.assertIn('gBank[lawyer] += lawyerPart;', share)
        self.assertIn('gBank[officer] += copPart;', share)
        # the same split already drives the real bail command
        bail = POLICE.split('stock RW_PoliceBailRequest(playerid)', 1)[1].split('\nstock ', 1)[0]
        self.assertIn('new lawyerCut = bail / 2, officerCut = bail / 10;', bail)


class UberTest(unittest.TestCase):
    """Orders, a taxi and a fare counted on drop-off."""

    def test_an_order_needs_a_free_taxi(self):
        order = block(APPS, 'stock RW_UberOrder(playerid)')
        self.assertIn('if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Step out of the car first.");', order)
        self.assertIn('RW_UberTaxi(i)', order)
        self.assertIn('gUberWait[playerid] = 1;', order)

    def test_only_a_taxi_takes_the_order(self):
        accept = block(APPS, 'stock RW_UberAccept(driver, passenger)')
        self.assertIn('if (!RW_UberTaxi(driver) || GetPlayerState(driver) != PLAYER_STATE_DRIVER) return RW_Error(driver, "Take a taxi first.");', accept)
        self.assertIn('if (passenger < 0 || passenger >= MAX_PLAYERS || !IsPlayerConnected(passenger) || gUberWait[passenger] != 1)', accept)
        self.assertIn('return RW_Error(driver, "That order is gone.");', accept)
        self.assertIn('SetPlayerMapIcon(driver, UBER_ICON,', accept)
        self.assertIn('static const gTaxiModels[] = { 420, 438 };', APPS)

    def test_the_fare_is_base_plus_distance(self):
        finish = APPS.split('stock RW_UberFinish(driver)', 1)[1].split('\n}', 1)[0]
        self.assertIn('new fare = UBER_BASE_FARE + metres * UBER_PER_METRE / 10;', finish)
        self.assertIn('RW_TakeMoney(passenger, fare);', finish)
        self.assertIn('RW_AddMoney(driver, fare);', finish)
        self.assertIn('if (gMoney[passenger] < fare)', finish)

    def test_the_ride_ends_when_the_passenger_steps_out(self):
        pulse = APPS.split('public RW_UberPulse()', 1)[1].split('\n}', 1)[0]
        self.assertIn('GetPlayerVehicleID(passenger) != vehicleid', pulse)
        self.assertIn('RW_UberFinish(d);', pulse)
        self.assertIn('No taxi took the order. It is cancelled.', pulse)


class AutoAppTest(unittest.TestCase):
    """Auto Plus lists your cars and brings them over."""

    def test_only_cars_with_your_key_are_listed(self):
        cars = block(APPS, 'stock RW_AutoCars(playerid, dest[][2], limit)')
        self.assertIn(sql(APPS, 'SELECT DISTINCT c.id,c.model FROM owned_cars c'), cars)

    def test_delivery_costs_and_moves_the_car(self):
        deliver = block(APPS, 'stock RW_AutoDeliver(playerid, carid)')
        self.assertIn('if (gMoney[playerid] < AUTO_DELIVER_FEE) return RW_Error(playerid, "Delivery costs 200 in cash.");', deliver)
        self.assertIn('Somebody sits in it: delivery refused.', deliver)
        self.assertIn('if (VectorSize(vx - x, vy - y, vz - z) < AUTO_DELIVER_DISTANCE) return RW_Info(playerid, "The car is already near you.");', deliver)
        self.assertIn('SetVehiclePos(vehicleid, x + 5.0 * floatsin(-a, degrees), y + 5.0 * floatcos(-a, degrees), z + 0.5);', deliver)
        self.assertIn('RW_TakeMoney(playerid, AUTO_DELIVER_FEE);', deliver)

    def test_a_car_in_the_garage_is_not_delivered(self):
        deliver = block(APPS, 'stock RW_AutoDeliver(playerid, carid)')
        self.assertIn('That car is in the garage: take it from the garage lot.', deliver)


class CameraTest(unittest.TestCase):
    """A shot is an item: it keeps the place and the time."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)

    def tearDown(self):
        self.db.close()

    def test_the_photo_is_an_item(self):
        self.assertRegex(CORE, r'#define ITEM_PHOTO\s+18')
        self.assertRegex(INV, r'#define INV_TYPE_MAX (18|19|20)')
        self.assertIn('if (type == ITEM_PHOTO) return 2273;', INV)
        self.assertIn('if (type == ITEM_PHOTO) return 50;', INV)
        self.assertIn('!strcmp(word, "photo", true)', INV)
        self.assertIn('if (type==ITEM_PHOTO && (data<0 || data>=PHOTO_PLACES || amount<0 || amount>1439)) return 0;', INV)
        self.assertIn('"Показать"', block(INV, 'stock RW_InvUseLabel('))

    def test_the_place_is_the_nearest_landmark(self):
        spots = APPS.split('static const Float:gPhotoSpots[][3] = {', 1)[1].split('};', 1)[0]
        self.assertEqual(len(re.findall(r'\{[^}]+\}', spots)), 6)
        place = block(APPS, 'stock RW_PhotoPlaceAt(Float:x, Float:y, Float:z)')
        self.assertIn('return (dist <= 250.0) ? (best) : (PHOTO_PLACES - 1);', place)

    def test_a_shot_costs_credit_and_room(self):
        take = block(APPS, 'stock RW_PhotoTake(playerid)')
        self.assertIn('if (gPhoneBalance[playerid] < RW_PhoneSmsCost(gPhoneTariff[playerid])) return RW_Error(playerid, "Not enough credit on the SIM for a shot.");', take)
        self.assertIn('if (RW_InvFree(playerid, 0, ITEM_PHOTO, place, 1) < 0) return RW_Error(playerid, "No room for the photo in your pockets.");', take)
        self.assertIn('new uid = RW_InvGrant(playerid, ITEM_PHOTO, place, minute);', take)
        self.assertIn('gPhoneBalance[playerid] -= RW_PhoneSmsCost(gPhoneTariff[playerid]);', take)

    def test_a_photo_is_shown_to_everybody_nearby(self):
        show = block(APPS, 'stock RW_PhotoShow(playerid)')
        self.assertIn('if (!IsPlayerInRangeOfPoint(i, 8.0, x, y, z)) continue;', show)
        self.assertIn('%s shows a photo: %s at %02d:%02d.', show)


class PhoneAppWiringTest(unittest.TestCase):
    """The apps are reachable from both faces and from the chat."""

    def test_the_mode_knows_the_slice(self):
        self.assertIn('#include <railway/phoneapps>', MAIN)
        self.assertIn('RW_PhoneAppsSchema()', MAIN)
        self.assertIn('RW_WeatherSet(1);', MAIN)
        self.assertIn('SetTimer("RW_WeatherPulse", 1000, true);', MAIN)
        self.assertIn('SetTimer("RW_GroupPulse", 1000, true);', MAIN)
        self.assertIn('SetTimer("RW_UberPulse", 1000, true);', MAIN)
        self.assertIn('RW_PhoneAppsReset(playerid);', MAIN)

    def test_modes_are_registered(self):
        self.assertRegex(UI, r'#define UI_PHONE_APP 2[0-9]')
        self.assertRegex(CORE, r'#define CEF_MODE_APP\s+\(12\)')
        for name, value in (('PHONE_APP_BANK', 2), ('PHONE_APP_WEATHER', 3), ('PHONE_APP_GROUP', 4),
                            ('PHONE_APP_LAWYER', 5), ('PHONE_APP_UBER', 6), ('PHONE_APP_NEWS', 7),
                            ('PHONE_APP_AUTO', 8), ('PHONE_APP_CAMERA', 9)):
            self.assertRegex(CORE, r'#define %s\s+%d' % (name, value))

    def test_the_browser_opens_the_app(self):
        handler = CEFUI.split('public RW_CefPhoneApp(playerid, browserid, app)', 1)[1].split('\n}', 1)[0]
        self.assertIn('PHONE_APP_BANK', handler)
        self.assertIn('RW_PhoneAppOpen(playerid, app);', handler)
        self.assertIn('if (app == 5) {', handler)          # lawyers keep their own list
        self.assertIn('RW_CefPhoneLawyers(playerid);', handler)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_APP) return RW_PhoneAppCefClick(playerid, id);', CEFUI)

    def test_the_text_draw_phone_has_an_applications_row(self):
        menu = PHONE.split('stock RW_PhoneMenu(playerid)', 1)[1].split('\n}', 1)[0]
        self.assertIn('RW_UI_Row(playerid, 6, "Applications");', menu)
        click = PHONE.split('stock RW_PhoneClick(playerid, row, mode)', 1)[1]
        self.assertIn('if (row == 6) return RW_PhoneAppList(playerid);', click)
        self.assertIn('else if (mode == UI_PHONE_APP) {', INTER)
        self.assertIn('return RW_PhoneAppClick(playerid, row);', INTER)

    def test_both_faces_share_one_handler(self):
        self.assertIn('stock RW_PhoneAppCefClick(playerid, row) { return RW_PhoneAppAction(playerid, row, true); }', APPS)
        self.assertIn('stock RW_PhoneAppClick(playerid, row) { return RW_PhoneAppAction(playerid, row, false); }', APPS)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefAppMenu(playerid);', APPS)
        self.assertIn('RW_UI_Panel(playerid, UI_PHONE_APP,', APPS)
        self.assertIn('RW_CefOpen(playerid, CEF_MODE_APP,', APPS)

    def test_an_uninstalled_app_does_not_open(self):
        action = APPS.split('stock RW_PhoneAppAction(playerid, row, bool:cef)', 1)[1]
        self.assertIn('if (!gPhoneApp[id]) return RW_Error(playerid, "This app is not installed.");', action)

    def test_commands_exist_in_both_faces(self):
        for cmd, stock in (('apps', 'RW_PhoneAppList'), ('weather', 'RW_WeatherCommand'), ('group', 'RW_GroupInfo'),
                           ('gaccept', 'RW_GroupAccept'), ('gleave', 'RW_GroupLeave'), ('taxi', 'RW_UberOrder'),
                           ('news', 'RW_NewsList'), ('photo', 'RW_PhotoCommand'), ('snap', 'RW_PhotoTake'),
                           ('lawcall', 'RW_AppLawyerCall'), ('payfines', 'RW_AppPayFines')):
            self.assertIn('if (!strcmp(cmd, "%s", true)) return %s(playerid)' % (cmd, stock), CMDS)
        for cmd, stock in (('ginvite', 'RW_GroupInviteCommand'), ('g', 'RW_GroupSayCommand'),
                           ('ad', 'RW_NewsPostCommand'), ('deliver', 'RW_AutoDeliverCommand')):
            self.assertIn('if (!strcmp(cmd, "%s", true)) return %s(playerid, params);' % (cmd, stock), CMDS)
        self.assertIn('if (!strcmp(cmd, "uber", true)) return RW_PhoneAppOpen(playerid, PHONE_APP_UBER);', CMDS)
        self.assertIn('if (!strcmp(cmd, "auto", true)) return RW_PhoneAppOpen(playerid, PHONE_APP_AUTO);', CMDS)
        self.assertIn('Приложения: /apps, /weather, /group, /g <текст>, /ginvite, /taxi, /uber, /news, /ad, /auto, /deliver, /snap, /photo, /lawcall, /radio.', CMDS)

    def test_no_cyrillic_reaches_the_player(self):
        """Comments are Russian, every string the player can read is English."""
        code = re.sub(r'/\*[\s\S]*?\*/', '', APPS)
        for line in code.splitlines():
            line = line.split('//', 1)[0]
            self.assertNotRegex(line, r'[А-Яа-я]')

class RadioTest(unittest.TestCase):
    """Радио: станции лежат в базе, поток включает сервер."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(APPS, 'CREATE TABLE IF NOT EXISTS radio_stations') + ';')
        for statement in re.findall(r'"(INSERT OR IGNORE INTO radio_stations[^"]+)"', APPS):
            self.db.execute(statement)
        self.db.commit()

    def test_the_stations_are_in_the_database(self):
        rows = self.db.execute('SELECT id,name,genre,url FROM radio_stations ORDER BY id').fetchall()
        self.assertEqual(len(rows), 5)
        for name, genre in (('SomaFM Groove Salad', 'Ambient'), ('SomaFM Drone Zone', 'Drone'),
                            ('SomaFM Underground 80s', 'Synthpop'), ('Radio Paradise', 'Rock'),
                            ('Local LS FM', 'Talk')):
            self.assertIn((name, genre), [(r[1], r[2]) for r in rows])

    def test_a_station_without_a_stream_says_so(self):
        # Пятая станция — заглушка: потока нет, включать нечего.
        url = self.db.execute('SELECT url FROM radio_stations WHERE id=5').fetchone()[0]
        self.assertEqual(url, '')
        tune = block(APPS, 'stock RW_RadioTune(playerid, row)')
        self.assertIn('if (!url[0]) return RW_Error(playerid, "This station is silent:', tune)

    def test_the_stream_is_started_by_the_server(self):
        tune = block(APPS, 'stock RW_RadioTune(playerid, row)')
        self.assertIn('PlayAudioStreamForPlayer(playerid, url);', tune)
        self.assertIn('gRadio[playerid] = index;', tune)
        off = block(APPS, 'stock RW_RadioOff(playerid)')
        self.assertIn('StopAudioStreamForPlayer(playerid);', off)
        self.assertIn('gRadio[playerid] = -1;', off)

    def test_the_app_shows_the_current_station(self):
        rows = block(APPS, 'stock RW_RadioRows(playerid)')
        self.assertIn('Playing: %s - press to turn off', rows)
        self.assertIn('"Radio is off"', rows)
        self.assertIn('SELECT name,genre,url FROM radio_stations ORDER BY id LIMIT %d', rows)
        self.assertIn('url[0] ? ("play") : ("silent")', rows)
        self.assertIn('case PHONE_APP_RADIO: RW_RadioRows(playerid);', APPS)
        self.assertIn('case PHONE_APP_RADIO: return RW_RadioTune(playerid, row);', APPS)
        self.assertIn('case PHONE_APP_RADIO: format(subtitle, ssize, "%s", gRadio[playerid] >= 0', APPS)

    def test_leaving_the_game_stops_the_stream(self):
        reset = block(APPS, 'stock RW_PhoneAppsReset(playerid)')
        self.assertIn('RW_RadioStop(playerid);', reset)
        stop = block(APPS, 'stock RW_RadioStop(playerid)')
        self.assertIn('if (gRadio[playerid] >= 0) StopAudioStreamForPlayer(playerid);', stop)

    def test_the_command_opens_the_app(self):
        self.assertIn('if (!strcmp(cmd, "radio", true)) return RW_RadioCommand(playerid);', CMDS)
        self.assertIn('stock RW_RadioCommand(playerid) { return RW_PhoneAppOpen(playerid, PHONE_APP_RADIO); }', APPS)
        self.assertIn('/radio', block(CMDS, 'stock RW_CommandHelp(playerid)'))

    def test_thirteen_apps_fit_on_three_pages(self):
        action = block(APPS, 'stock RW_PhoneAppAction(playerid, row, bool:cef)')
        self.assertIn('gPhoneAppPage[playerid] = (gPhoneAppPage[playerid] + 1) % ((PHONE_APPS + 5) / 6);', action)
        self.assertIn('"Page %d of %d", gPhoneAppPage[playerid] + 1, (PHONE_APPS + 5) / 6', APPS)


class PhoneYActionTest(unittest.TestCase):
    """Y-действия телефона: такси на стоянке и адвокат для задержанного."""

    def test_y_at_a_taxi_rank_orders_a_car(self):
        rank = block(APPS, 'public RW_TaxiRankCall(playerid)')
        self.assertIn('IsPlayerInRangeOfPoint(playerid, TAXI_RANK_RANGE', rank)
        self.assertIn('RW_UberOrder(playerid);', rank)
        self.assertIn('static const Float:gTaxiRanks[][3]', APPS)
        table = APPS.split('static const Float:gTaxiRanks', 1)[1].split('};', 1)[0]
        for place in ('Unity Station', 'LS International', 'Pershing Square'):
            self.assertIn(place, table)
        # Действие висит на Y на себя, после сдачи задания.
        self_menu = block(INTER, 'stock RW_SelfMenu(playerid)')
        self.assertIn('if (RW_TaxiRankCall(playerid)) return 1;', self_menu)
        self.assertIn('forward RW_TaxiRankCall(playerid);', CORE)

    def test_y_on_a_detained_player_calls_a_lawyer(self):
        call = block(APPS, 'public RW_LawyerCallFor(playerid, target)')
        self.assertIn('if (target < 0 || !IsPlayerConnected(target) || gJailTime[target] <= 0)', call)
        self.assertIn('RW_AppLawyerCall(target);', call)
        self.assertIn('called a lawyer for you.', call)
        menu = block(INTER, 'stock RW_PlayerMenu(playerid, target)')
        self.assertIn('if (gJailTime[target] > 0) return RW_LawyerCallFor(playerid, target);', menu)
        self.assertIn('forward RW_LawyerCallFor(playerid, target);', CORE)
        self.assertIn('forward RW_UberOrder(playerid);', CORE)

    def test_the_lawyer_action_comes_after_the_duty_actions(self):
        # Полицейский и адвокат при деле: их действия важнее звонка.
        menu = block(INTER, 'stock RW_PlayerMenu(playerid, target)')
        self.assertLess(menu.index('RW_PoliceMenuOpen(playerid, target)'), menu.index('RW_LawyerCallFor(playerid, target)'))
        self.assertLess(menu.index('RW_PoliceCaseStart(playerid, target)'), menu.index('RW_LawyerCallFor(playerid, target)'))



if __name__ == '__main__':
    unittest.main()
