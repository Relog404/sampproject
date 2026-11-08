"""Phone: SIM card, number, balance, calls, SMS, contacts, 911.

The browser page (scriptfiles/cef/railway/phone.html) is only a face: every
rule lives on the server, and players without the plugin use the chat
commands and the native dialog. SQL from the Pawn sources is executed here
against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name, where=''):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

PHONE = pawn('phone')
CEFUI = pawn('cefui')
CORE = pawn('core')
INV = pawn('inventory')
INTER = pawn('interaction')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
PAGES = ROOT / 'server/scriptfiles/cef/railway'
PAGE_JS = (PAGES / 'js/phone.js').read_text(encoding='utf-8')


def sql(source, prefix):
    return next(s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix))


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class PhoneTableTest(unittest.TestCase):
    """The tables from the task, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        for table in ('phone_apps', 'phone_settings', 'sim_cards', 'phone_contacts', 'phone_sms'):
            self.db.execute(sql(PHONE, 'CREATE TABLE IF NOT EXISTS %s ' % table))

    def tearDown(self):
        self.db.close()

    def columns(self, table):
        return [row[1] for row in self.db.execute('PRAGMA table_info(%s)' % table)]

    def test_tables_match_the_requested_schema(self):
        self.assertEqual(self.columns('phone_apps'), ['player_id', 'app_id', 'installed'])
        self.assertEqual(self.columns('phone_settings'), ['player_id', 'wallpaper', 'ringtone', 'dnd'])
        self.assertEqual(self.columns('sim_cards'), ['player_id', 'sim_number', 'tariff', 'balance'])

    def test_inserting_a_sim_gives_a_number(self):
        self.db.execute(sql(PHONE, 'INSERT INTO sim_cards(') % (7, 555123, 'Active', 0))
        row = self.db.execute(sql(PHONE, 'SELECT sim_number,tariff,balance FROM sim_cards') % 7).fetchone()
        self.assertEqual(row, ('555123', 'Active', 0))

    def test_balance_survives_relog(self):
        self.db.execute(sql(PHONE, 'INSERT INTO sim_cards(') % (7, 555123, 'Maxi', 0))
        self.db.execute(sql(PHONE, 'UPDATE sim_cards SET balance=%d') % (240, 'Maxi', 7))
        row = self.db.execute(sql(PHONE, 'SELECT sim_number,tariff,balance FROM sim_cards') % 7).fetchone()
        self.assertEqual(row, ('555123', 'Maxi', 240))

    def test_one_number_is_never_given_twice(self):
        self.db.execute(sql(PHONE, 'INSERT INTO sim_cards(') % (7, 555123, 'Start', 0))
        taken = sql(PHONE, 'SELECT player_id FROM sim_cards WHERE sim_number=%d') % 555123
        self.assertEqual(len(self.db.execute(taken).fetchall()), 1)
        self.assertEqual(len(self.db.execute(taken.replace('555123', '555999')).fetchall()), 0)

    def test_contacts_are_per_account(self):
        self.db.execute(sql(PHONE, 'INSERT INTO phone_contacts(') % (7, 'Alex', 555777))
        self.db.execute(sql(PHONE, 'INSERT INTO phone_contacts(') % (8, 'Alex', 555111))
        rows = self.db.execute(sql(PHONE, 'SELECT name,sim_number FROM phone_contacts WHERE player_id=%d ORDER BY name') % 7).fetchall()
        self.assertEqual(rows, [('Alex', '555777')])

    def test_sms_history_returns_both_directions(self):
        for from_num, to_num, text in ((555123, 555777, 'hi'), (555777, 555123, 'hello'), (555123, 555901, 'other')):
            self.db.execute(sql(PHONE, 'INSERT INTO phone_sms(') % (from_num, to_num, text, 1))
        history = sql(CEFUI, 'SELECT from_num,to_num,body FROM phone_sms WHERE from_num=%d OR to_num=%d')
        rows = self.db.execute(history % (555123, 555123)).fetchall()
        # VARCHAR columns keep the numbers as text; the server reads them with
        # db_get_field_assoc_int, and SQLite compares both sides as text.
        self.assertEqual([(str(f), str(t), b) for f, t, b in rows],
                         [('555123', '555901', 'other'), ('555777', '555123', 'hello'), ('555123', '555777', 'hi')])

    def test_contacts_replace_instead_of_duplicating(self):
        for name in ('Alex', 'Alexey'):
            self.db.execute(sql(PHONE, 'DELETE FROM phone_contacts WHERE player_id=%d AND sim_number=%d') % (7, 555777))
            self.db.execute(sql(PHONE, 'INSERT INTO phone_contacts(') % (7, name, 555777))
        rows = self.db.execute('SELECT name FROM phone_contacts WHERE player_id=7 AND sim_number=555777').fetchall()
        self.assertEqual(rows, [('Alexey',)])


class PhoneItemTest(unittest.TestCase):
    """The phone is an inventory item; the SIM card is spent when inserted."""

    def test_item_types_are_known_to_the_inventory(self):
        self.assertIn('#define ITEM_SIM                    8', CORE)
        self.assertIn('#define ITEM_PHONE                  9', CORE)
        self.assertRegex(INV, r'#define INV_TYPE_MAX (9|1[0-9]|20)')

    def test_items_have_a_model_weight_and_name(self):
        model = block(INV, 'stock RW_InvModel(type, data = 0)')
        grams = block(INV, 'stock RW_InvGrams(type, data = 0, amount = 1)')
        self.assertIn('if (type == ITEM_SIM) return 1580;', model)
        self.assertIn('if (type == ITEM_PHONE) return 18865;', model)
        self.assertIn('if (type == ITEM_SIM) return 10;', grams)
        self.assertIn('if (type == ITEM_PHONE) return 300;', grams)
        words = block(INV, 'stock RW_InvItemWord(const word[])')
        self.assertIn('"sim"', words)
        self.assertIn('"phone"', words)

    def test_granting_validates_the_variant(self):
        grant = block(INV, 'stock RW_InvGrant(playerid, type, data = 0, amount = 1)')
        self.assertIn('if ((type==ITEM_SIM || type==ITEM_PHONE) && (amount!=1 || data<0 || data>=PHONE_TARIFFS)) return 0;', grant)

    def test_the_card_offers_the_right_buttons(self):
        # подписи кнопок карточки: «Открыть» для телефона, «Вставить» для SIM
        label = block(INV, 'stock RW_InvUseLabel(')
        self.assertIn('"Открыть"', label)
        self.assertIn('"Вставить"', label)
        use = block(INV, 'stock RW_InvUseSelected(playerid)', '\nstock ')
        self.assertIn('if (type == ITEM_PHONE) return RW_PhoneOpen(playerid);', use)
        self.assertIn('if (type == ITEM_SIM) return RW_PhoneInsertSim(playerid);', use)

    def test_inserting_a_sim_removes_it_from_the_bag(self):
        insert = block(PHONE, 'stock RW_PhoneInsertSim(playerid)')
        self.assertIn('new slot = RW_PhoneSimSlot(playerid);', insert)
        self.assertIn('RW_PhoneConsume(playerid, slot)', insert)
        consume = block(PHONE, 'stock RW_PhoneConsume(playerid, slot)')
        self.assertIn('DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d', consume)
        self.assertIn('RW_InvAttach(playerid);', consume)


class PhoneMechanicsTest(unittest.TestCase):
    def test_tariffs_have_a_price(self):
        names = re.findall(r'case \d: format\(dest, size, "(\w+)"\);', block(PHONE, 'stock RW_PhoneTariffName(id, dest[], size = sizeof dest)'))
        self.assertEqual(names, ['Start', 'Active', 'Maxi'])
        calls = block(PHONE, 'stock RW_PhoneCallCost(tariff)')
        sms = block(PHONE, 'stock RW_PhoneSmsCost(tariff)')
        self.assertEqual(re.findall(r'case \d: return (\d);', calls), ['2', '5'])
        self.assertEqual(re.findall(r'case \d: return (\d);', sms), ['2', '1'])

    def test_numbers_are_six_digits_and_911(self):
        self.assertIn('#define PHONE_NUM_MIN       100000', PHONE)
        self.assertIn('#define PHONE_NUM_MAX       999999', PHONE)
        call = block(PHONE, 'stock RW_PhoneCall(playerid, number)')
        self.assertIn('if (number == 911) return RW_Phone911(playerid, 0);', call)
        self.assertIn('if (number < PHONE_NUM_MIN || number > PHONE_NUM_MAX) return RW_Error(playerid, "Number must be six digits or 911.");', call)

    def test_rejected_calls(self):
        call = block(PHONE, 'stock RW_PhoneCall(playerid, number)')
        for guard in ('if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");',
                      'if (number == gPhoneNum[playerid]) return RW_Error(playerid, "That is your own number.");',
                      'if (gPhoneState[playerid] != PHONE_IDLE) return RW_Error(playerid, "The line is busy.");',
                      'if (gPhoneDnd[target] || gPhoneState[target] != PHONE_IDLE) return RW_Error(playerid, "Subscriber is busy. Try again later.");',
                      'if (target < 0) return RW_Error(playerid, "Subscriber is out of coverage.");'):
            self.assertIn(guard, call)

    def test_the_caller_pays_and_an_empty_balance_ends_the_call(self):
        accept = block(PHONE, 'stock RW_PhoneAccept(playerid)')
        self.assertIn('new cost = RW_PhoneCallCost(gPhoneTariff[caller]);', accept)
        self.assertIn('gPhoneBalance[caller] -= cost;', accept)
        pulse = block(PHONE, 'public RW_PhonePulse()', '\n    return 1;')
        self.assertIn('new payer = gPhoneCaller[p] ? (p) : (partner);', pulse)
        self.assertIn('if (gPhoneBalance[payer] < cost) {', pulse)
        self.assertIn('RW_Error(payer, "Out of credit: the call was dropped.");', pulse)
        self.assertIn('RW_PhoneEnd(p);', pulse)

    def test_ringing_times_out(self):
        pulse = block(PHONE, 'public RW_PhonePulse()', '\n    return 1;')
        self.assertIn('if (GetTickCount() - gPhoneTick[p] > PHONE_RING_MS)', pulse)
        self.assertIn('RW_Info(p, "No answer.");', pulse)

    def test_a_call_is_half_a_conversation_for_bystanders(self):
        say = block(PHONE, 'stock RW_PhoneSay(playerid, const text[])')
        self.assertIn('SendClientMessage(partner, 0xF5B04CFF, message);', say)
        self.assertIn('if (!IsPlayerInRangeOfPoint(i, PHONE_RANGE_SAY, x, y, z)) continue;', say)
        self.assertIn('if (gPhoneState[playerid] == PHONE_TALK) return RW_PhoneSay(playerid, text);', MAIN)

    def test_do_not_disturb_blocks_calls_and_sms(self):
        self.assertIn('if (gPhoneDnd[target] || gPhoneState[target] != PHONE_IDLE)', block(PHONE, 'stock RW_PhoneCall(playerid, number)'))
        self.assertIn('if (target >= 0 && !gPhoneDnd[target])', block(PHONE, 'stock RW_PhoneSms(playerid, number, const text[])'))

    def test_sms_costs_credit_and_is_stored(self):
        sms = block(PHONE, 'stock RW_PhoneSms(playerid, number, const text[])')
        self.assertIn('gPhoneBalance[playerid] -= cost;', sms)
        self.assertIn('INSERT INTO phone_sms(', sms)
        self.assertIn('if (strlen(text) > PHONE_SMS_MAX)', sms)

    def test_911_reaches_everyone_and_marks_the_place(self):
        emergency = block(PHONE, 'stock RW_Phone911(playerid, reason)')
        self.assertIn('SendClientMessage(i, 0xFF6B6BFF, message);', emergency)
        self.assertIn('SetPlayerMapIcon(i, PHONE_911_ICON, x, y, z, 0, 0xFF6B6BFF, MAPICON_GLOBAL);', emergency)
        reasons = re.findall(r'case \d: format\(dest, size, "(\w+)"\);', block(PHONE, 'stock RW_Phone911Reason(id, dest[], size = sizeof dest)'))
        self.assertEqual(reasons, ['Robbery', 'Shooting', 'Hostages', 'Injured', 'Fire'])

    def test_phone_state_is_reset_and_saved(self):
        self.assertIn('RW_PhoneReset(playerid);', MAIN.split('public OnPlayerConnect')[1][:900])
        self.assertIn('RW_PhoneReset(playerid);', MAIN.split('public OnPlayerDisconnect')[1][:900])
        self.assertIn('RW_PhoneSave(playerid);', MAIN.split('public OnPlayerDisconnect')[1][:900])
        self.assertIn('RW_PhoneSchema()', MAIN)
        self.assertIn('SetTimer("RW_PhonePulse", 1000, true);', MAIN)
        self.assertIn('RW_PhoneLoad(playerid);', pawn('accounts'))


class PhoneYMenuTest(unittest.TestCase):
    def test_y_picks_a_context(self):
        open_ = block(INTER, 'stock RW_ControlOpen(playerid, bool:settings)')
        self.assertIn('new target = RW_PlayerTarget(playerid);', open_)
        self.assertIn('if (target >= 0) return RW_PlayerMenu(playerid, target);', open_)
        self.assertIn('return RW_SelfMenu(playerid);', open_)

    def test_y_on_a_player_swaps_numbers_then_calls(self):
        menu = block(INTER, 'stock RW_PlayerMenu(playerid, target)')
        self.assertIn('if (RW_PhoneKnows(playerid, gPhoneNum[target])) return RW_PhoneCall(playerid, gPhoneNum[target]);', menu)
        self.assertIn('RW_PhoneContactAdd(playerid, nick, gPhoneNum[target]);', menu)
        self.assertIn('RW_PhoneContactAdd(target, mynick, gPhoneNum[playerid]);', menu)

    def test_y_on_yourself_opens_the_phone(self):
        self_menu = block(INTER, 'stock RW_SelfMenu(playerid)')
        self.assertIn('return RW_PhoneOpen(playerid);', self_menu)
        self.assertIn('No SIM card', self_menu)

    def test_the_native_menu_clicks(self):
        click = block(INTER, 'stock RW_UI_Click(playerid, row)', '\n// Camera ray')
        self.assertIn('} else if (mode == UI_PHONE || mode == UI_PHONE_CONT) {', click)
        self.assertIn('return RW_PhoneClick(playerid, row, mode);', click)
        self.assertIn('#define UI_PHONE 11', pawn('ui'))


class PhoneCommandTest(unittest.TestCase):
    def test_commands_exist(self):
        for line in ('if (!strcmp(cmd, "phone", true)) return RW_PhoneOpen(playerid);',
                     'if (!strcmp(cmd, "call", true)) return RW_PhoneCallCommand(playerid, params);',
                     'if (!strcmp(cmd, "sms", true)) return RW_PhoneSmsCommand(playerid, params);',
                     'if (!strcmp(cmd, "pickup", true)) return RW_PhoneAccept(playerid);',
                     'if (!strcmp(cmd, "hangup", true)) return RW_PhoneEnd(playerid);',
                     'if (!strcmp(cmd, "contacts", true)) return RW_PhoneContactsCommand(playerid);',
                     'if (!strcmp(cmd, "addcontact", true)) return RW_PhoneContactCommand(playerid, params);',
                     'if (!strcmp(cmd, "sim", true)) return RW_PhoneInsertSim(playerid);',
                     'if (!strcmp(cmd, "topup", true)) return RW_PhoneTopUpCommand(playerid, params);',
                     'if (!strcmp(cmd, "tariff", true)) return RW_PhoneTariffCommand(playerid, params);',
                     'if (!strcmp(cmd, "911", true)) return RW_Phone911Command(playerid, params);'):
            self.assertIn(line, CMDS)

    def test_the_help_lists_them(self):
        help_ = block(CMDS, 'stock RW_CommandHelp(playerid)', '\n    return ')
        self.assertIn('/phone', help_)
        self.assertIn('/911', help_)

    def test_arguments_are_checked(self):
        call_cmd = block(PHONE, 'stock RW_PhoneCallCommand(playerid, const params[])')
        self.assertIn('if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /call <number> or /call 911");', call_cmd)
        self.assertIn('return RW_PhoneCall(playerid, strval(word));', call_cmd)
        self.assertIn('if (amount > 10000)', block(PHONE, 'stock RW_PhoneTopUp(playerid, amount)'))
        self.assertIn('if (!RW_TakeMoney(playerid, amount))', block(PHONE, 'stock RW_PhoneTopUp(playerid, amount)'))


class PhoneBrowserTest(unittest.TestCase):
    """The page and the server must agree on the event names."""

    def emitted(self):
        names = set()
        for source in (PHONE, CEFUI):
            for chunk in source.split('CEF_EmitEvent(')[1:]:
                names |= set(re.findall(r'"(phone:[^"]+)"', chunk.split(');')[0]))
        return names

    def handled(self):
        return set(re.findall(r"RW\.on\('(phone:[^']+)'", PAGE_JS))

    def sent(self):
        return set(re.findall(r"RW\.emit\('(phone:[^']+)'", PAGE_JS))

    def registered(self):
        return set(re.findall(r'CEF_RegisterEvent\("(phone:[^"]+)"', CEFUI))

    def test_page_files_exist(self):
        for name in ('phone.html', 'css/phone.css', 'js/phone.js'):
            self.assertTrue((PAGES / name).exists(), name)
        self.assertIn('css/phone.css', (PAGES / 'phone.html').read_text(encoding='utf-8'))

    def test_browser_layer_sits_between_menu_and_toasts(self):
        self.assertIn('#define CEF_UI_PHONE        (102)', CEFUI)
        self.assertIn('#define CEF_UI_PHONE_URL    "http://cef/railway/phone.html"', CEFUI)
        start = block(CEFUI, 'stock RW_CefStart(playerid)')
        self.assertIn('CEF_SetBrowserLayer(playerid, CEF_UI_PHONE, 150);', start)
        self.assertIn('CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, false);', start)
        self.assertIn('CEF_DestroyBrowser(playerid, CEF_UI_PHONE);', block(CEFUI, 'stock RW_CefReset(playerid)'))

    def test_every_server_event_is_rendered(self):
        self.assertEqual(self.emitted() - self.handled(), set())

    def test_every_page_action_is_registered(self):
        self.assertTrue(self.sent(), 'the page must talk back to the server')
        self.assertEqual(self.sent() - self.registered(), set())

    def test_the_phone_opens_only_with_a_phone_and_a_sim(self):
        open_ = block(PHONE, 'stock RW_PhoneOpen(playerid)')
        self.assertIn('if (RW_PhoneHasItem(playerid) <= 0)', open_)
        self.assertIn('if (gPhoneNum[playerid] <= 0)', open_)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefPhoneOpen(playerid);', open_)
        self.assertIn('return RW_PhoneMenu(playerid);', open_)

    def test_state_is_pushed_when_it_changes(self):
        self.assertIn('RW_CefPhoneState(playerid);', block(PHONE, 'stock RW_PhoneTopUp(playerid, amount)'))
        self.assertIn('RW_CefPhoneState(playerid);', block(PHONE, 'stock RW_PhoneSms(playerid, number, const text[])'))
        self.assertIn('RW_CefPhoneContacts(playerid);', block(PHONE, 'stock RW_PhoneContactAdd(playerid, const name[], number)'))

    def test_page_preview_shows_a_demo_phone(self):
        self.assertIn("if (window.location.search.indexOf('fake=1') >= 0) demo();", PAGE_JS)
        self.assertIn('if (!RW.ready()) demo();', PAGE_JS)
        self.assertIn("RW.fake('phone:state'", PAGE_JS)

    def test_the_home_bar_closes_the_handset(self):
        """Полоска под экраном — кнопка: домой, с главного экрана — закрыть."""
        self.assertIn("if (el.homeBtn) {", PAGE_JS)
        self.assertIn("el.homeBtn.addEventListener('click', function () {", PAGE_JS)
        self.assertIn("if (screen === 'home') RW.emit('phone:close');", PAGE_JS)
        self.assertIn('else back();', PAGE_JS)
        self.assertIn("var stage = document.querySelector('.stage');", PAGE_JS)
        self.assertIn("if (event.target === stage) RW.emit('phone:close');", PAGE_JS)

    def test_the_bar_really_looks_like_a_button(self):
        css = (PAGES / 'css/phone.css').read_text(encoding='utf-8')
        bar = css.split('.home-bar i {')[1].split('}')[0]
        self.assertIn('cursor: pointer;', bar)
        self.assertIn('.home-bar i:hover { background: rgba(255, 255, 255, 0.7); }', css)

    def test_closing_the_phone_frees_the_mouse(self):
        closed = block(CEFUI, 'public RW_CefPhoneClosed(playerid, browserid)')
        self.assertIn('gPhoneShown[playerid] = false;', closed)
        self.assertIn('CEF_FocusBrowser(playerid, CEF_UI_PHONE, false);', closed)
        self.assertIn('CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, false);', closed)
        self.assertIn('CEF_RegisterEvent("phone:close", "RW_CefPhoneClosed");', CEFUI)

    def test_apps_cover_the_requested_list(self):
        names = re.findall(r'case \d+: format\(dest, size, "([^"]+)"\);', block(PHONE, 'stock RW_PhoneAppName(id, dest[], size = sizeof dest)'))
        self.assertEqual(names, ['Phone', 'Messages', 'Bank', 'Weather', 'Group', 'Lawyers',
                                 'Uber Drive', 'Weazel News', 'Auto Plus', 'Camera', 'Settings',
                                 'Operator', 'Radio'])
        # Приложение знает свой номер: радио — тринадцатое.
        self.assertIn('#define PHONE_APPS          13', PHONE)
        self.assertIn('#define PHONE_APP_RADIO            12', CORE)
        self.assertIn('case 12: format(dest, size, "radio");', block(PHONE, 'stock RW_PhoneAppIcon(id, dest[], size = sizeof dest)'))


if __name__ == '__main__':
    unittest.main()
