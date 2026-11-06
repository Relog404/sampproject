"""Police (LSPD / Sheriff) and the lawyer licence.

The Y menu, the tablet and the phone app are only faces: every rule lives on
the server, and players without the browser plugin use the same functions
through chat commands and TextDraw menus. SQL from the Pawn sources is
executed here against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

POLICE = pawn('police')
CEFUI = pawn('cefui')
CORE = pawn('core')
INV = pawn('inventory')
UI = pawn('ui')
INTER = pawn('interaction')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')
PAGE_JS = (ROOT / 'server/scriptfiles/cef/railway/js/phone.js').read_text(encoding='utf-8')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class PoliceTableTest(unittest.TestCase):
    """The police and lawyer tables, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        for table in ('officers', 'wanted', 'arrests', 'fines', 'jail', 'wall_attachments', 'lawyers'):
            self.db.execute(sql(POLICE, 'CREATE TABLE IF NOT EXISTS %s ' % table))

    def tearDown(self):
        self.db.close()

    def columns(self, table):
        return [row[1] for row in self.db.execute('PRAGMA table_info(%s)' % table)]

    def test_tables_match_the_requested_systems(self):
        self.assertEqual(self.columns('officers'), ['player_id', 'faction', 'rank', 'duty'])
        self.assertEqual(self.columns('wanted'), ['player_id', 'stars', 'reason', 'officer', 'stamp'])
        self.assertEqual(self.columns('arrests'), ['id', 'player_id', 'officer', 'minutes', 'bail', 'reason', 'stamp'])
        self.assertEqual(self.columns('fines'), ['id', 'player_id', 'officer', 'amount', 'reason', 'paid', 'stamp'])
        self.assertEqual(self.columns('jail'), ['player_id', 'seconds', 'bail', 'reason', 'officer'])
        self.assertEqual(self.columns('wall_attachments'), ['player_id', 'x', 'y', 'z', 'officer', 'stamp'])
        self.assertEqual(self.columns('lawyers'), ['player_id', 'license', 'expire', 'price', 'rating'])

    def test_duty_and_faction_survive_relog(self):
        self.db.execute(sql(POLICE, 'INSERT OR REPLACE INTO officers(') % (7, 1, 3, 1))
        row = self.db.execute('SELECT faction,rank,duty FROM officers WHERE player_id=7').fetchone()
        self.assertEqual(row, (1, 3, 1))

    def test_wanted_is_one_row_per_player(self):
        for stars, reason in ((2, 1), (4, 3)):
            self.db.execute(sql(POLICE, 'INSERT OR REPLACE INTO wanted(') % (7, stars, reason, 'Ivan', 1000))
        rows = self.db.execute('SELECT stars,reason FROM wanted WHERE player_id=7').fetchall()
        self.assertEqual(rows, [(4, 3)])
        self.db.execute(sql(POLICE, 'DELETE FROM wanted WHERE player_id=%d') % 7)
        self.assertEqual(self.db.execute('SELECT stars FROM wanted WHERE player_id=7').fetchall(), [])

    def test_arrest_keeps_a_criminal_record(self):
        self.db.execute("CREATE TABLE accounts (id INT, name VARCHAR(32))")
        self.db.execute("INSERT INTO accounts VALUES(7,'Ivan')")
        self.db.execute(sql(POLICE, 'INSERT INTO arrests(') % (7, 'Ivan', 10, 5000, 2, 1000))
        row = self.db.execute(sql(POLICE, 'SELECT r.minutes,r.bail,r.reason,r.officer,a.name AS who FROM arrests r LEFT JOIN accounts a ON a.id=r.player_id WHERE r.id=%d') % 1).fetchone()
        self.assertEqual(row, (10, 5000, 2, 'Ivan', 'Ivan'))

    def test_a_fine_is_paid_once(self):
        for amount in (500, 1500):
            self.db.execute(sql(POLICE, 'INSERT INTO fines(') % (7, 'Ivan', amount, 5, 1000))
        rows = self.db.execute('SELECT SUM(amount) FROM fines WHERE player_id=7 AND paid=0').fetchone()
        self.assertEqual(rows[0], 2000)
        self.db.execute(sql(POLICE, 'UPDATE fines SET paid=1 WHERE player_id=%d AND paid=0') % 7)
        self.assertEqual(self.db.execute('SELECT SUM(amount) FROM fines WHERE player_id=7 AND paid=0').fetchone()[0], None)

    def test_the_sentence_is_saved_so_a_relog_cannot_clear_it(self):
        self.db.execute(sql(POLICE, 'INSERT OR REPLACE INTO jail(') % (7, 600, 5000, 'Ivan'))
        row = self.db.execute('SELECT seconds,bail,officer FROM jail WHERE player_id=7').fetchone()
        self.assertEqual(row, (600, 5000, 'Ivan'))
        self.db.execute(sql(POLICE, 'DELETE FROM jail WHERE player_id=%d') % 7)
        self.assertEqual(self.db.execute('SELECT seconds FROM jail WHERE player_id=7').fetchall(), [])

    def test_the_lawyer_licence_expires(self):
        insert = sql(POLICE, 'INSERT OR REPLACE INTO lawyers(')
        self.db.execute(insert % (7, 1, 2000000000, 1000, 3))
        self.assertEqual(self.db.execute('SELECT license,expire,price,rating FROM lawyers WHERE player_id=7').fetchone(), (1, 2000000000, 1000, 3))
        self.db.execute(insert % (7, 0, 0, 1000, 3))
        self.assertEqual(self.db.execute('SELECT license FROM lawyers WHERE player_id=7').fetchone()[0], 0)
        self.assertIn('if (gLawyerLicense[playerid] && gLawyerExpire[playerid] && gLawyerExpire[playerid] < gettime()) gLawyerLicense[playerid] = 0;', POLICE)

    def test_the_lawyer_list_joins_the_phone_number(self):
        self.db.execute("CREATE TABLE accounts (id INT, name VARCHAR(32))")
        self.db.execute("CREATE TABLE sim_cards (player_id INT, sim_number INT, tariff INT, balance INT)")
        self.db.execute("INSERT INTO accounts VALUES(7,'Ivan')")
        self.db.execute("INSERT INTO sim_cards VALUES(7,555123,1,50)")
        self.db.execute(sql(POLICE, 'INSERT OR REPLACE INTO lawyers(') % (7, 1, 2000000000, 1000, 3))
        rows = self.db.execute(sql(CEFUI, 'SELECT a.name AS who,p.price,p.rating,(SELECT sim_number FROM sim_cards s WHERE s.player_id=p.player_id) AS num FROM lawyers p LEFT JOIN accounts a ON a.id=p.player_id WHERE p.license=1 AND p.expire>%d ORDER BY p.rating DESC LIMIT 20') % 1000).fetchall()
        self.assertEqual(rows, [('Ivan', 1000, 3, 555123)])


class PoliceItemTest(unittest.TestCase):
    """Cuffs, taser, baton and badge are inventory items."""

    def test_item_types_are_registered(self):
        for name, value in (('ITEM_CUFFS', 10), ('ITEM_TASER', 11), ('ITEM_BATON', 12), ('ITEM_BADGE', 13)):
            self.assertRegex(CORE, r'#define %s\s+%d' % (name, value))
        self.assertRegex(INV, r'#define INV_TYPE_MAX (13|1[3-9])')

    def test_items_have_a_model_and_a_weight(self):
        model = block(INV, 'stock RW_InvModel(type, data = 0)')
        grams = block(INV, 'stock RW_InvGrams(type, data = 0, amount = 1)')
        self.assertIn('if (type == ITEM_CUFFS) return 19418;', model)
        self.assertIn('if (type == ITEM_TASER) return 348;', model)
        self.assertIn('if (type == ITEM_BATON) return 333;', model)
        self.assertIn('if (type == ITEM_BADGE) return 1580;', model)
        self.assertIn('if (type == ITEM_CUFFS) return 200;', grams)
        self.assertIn('if (type == ITEM_TASER) return 400;', grams)
        self.assertIn('if (type == ITEM_BATON) return 900;', grams)
        self.assertIn('if (type == ITEM_BADGE) return 50;', grams)

    def test_giveitem_knows_the_words(self):
        words = block(INV, 'stock RW_InvItemWord(const word[])')
        for word in ('cuffs', 'taser', 'baton', 'badge'):
            self.assertIn('"%s"' % word, words)

    def test_charges_and_durability_are_validated(self):
        grant = block(INV, 'stock RW_InvGrant(playerid, type, data = 0, amount = 1)')
        self.assertIn('if (type==ITEM_TASER && (data<0 || data>=PHONE_TARIFFS || amount<1 || amount>2)) return 0;', grant)
        self.assertIn('if (type==ITEM_BATON && (data<0 || data>=PHONE_TARIFFS || amount<1 || amount>100)) return 0;', grant)
        self.assertIn('if ((type==ITEM_CUFFS || type==ITEM_BADGE) && (amount!=1 || data<0 || data>=PHONE_TARIFFS)) return 0;', grant)

    def test_giveitem_grants_charges_not_copies(self):
        give = block(INV, 'stock RW_InvGiveCommand(playerid, const params[])', '\nstock ')
        self.assertIn('if (type == ITEM_TASER) amount = 2;', give)
        self.assertIn('else if (type == ITEM_BATON) amount = 100;', give)

    def test_the_card_offers_the_right_buttons(self):
        render = block(INV, 'stock RW_InvContextRender(playerid)', '\nstock ')
        names = block(INV, 'stock RW_InvContextRender(playerid)', '\nstock ')
        self.assertIn('RW_PoliceItemName(type, data, title, sizeof title);', names)
        for line in ('else if (type == ITEM_CUFFS) format(action, sizeof action, "%s", "CUFF");',
                     'else if (type == ITEM_TASER || type == ITEM_BATON) format(action, sizeof action, "%s", "STUN");',
                     'else if (type == ITEM_BADGE) format(action, sizeof action, "%s", "MDT");'):
            self.assertIn(line, render)
        click = block(INV, 'stock RW_InvClick(playerid, PlayerText:clicked)', '\npublic ')
        self.assertIn('if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_CUFFS) return RW_PoliceCuffCommand(playerid, "");', click)
        self.assertIn('if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_BADGE) return RW_MdtOpen(playerid);', click)

    def test_item_names(self):
        naming = block(POLICE, 'stock RW_PoliceItemName(type, data, dest[], size = sizeof dest)')
        self.assertEqual(re.findall(r'case ITEM_\w+: format\(dest, size, "([^"]+)"\);', naming),
                         ['HANDCUFFS', 'TASER', 'BATON', 'POLICE BADGE'])


class PoliceMechanicsTest(unittest.TestCase):
    """Cuffs, escort, the wall, stun, wanted, arrest, bail, fines, tracking."""

    def test_every_action_needs_duty_and_a_badge(self):
        guard = block(POLICE, 'stock RW_PoliceCanAct(playerid, target = -1, Float:range = POLICE_RANGE)')
        self.assertIn('if (!RW_PoliceOnDuty(playerid)) return RW_Error(playerid, "You are not on duty.");', guard)
        self.assertIn('if (RW_PoliceBadge(playerid) <= 0) return RW_Error(playerid, "You need your police badge in the inventory.");', guard)
        self.assertIn('if (target >= 0 && !RW_PoliceNear(playerid, target, range)) return RW_Error(playerid, "Stand closer to the player.");', guard)
        duty = block(POLICE, 'stock bool:RW_PoliceOnDuty(playerid)', '\nstock ')
        # фракция 3 — скорая: её служебные действия другие
        self.assertIn('return gLogged[playerid] && gDuty[playerid] && (gFaction[playerid] == FACTION_LSPD || gFaction[playerid] == FACTION_SHERIFF);', duty)

    def test_cuffing_needs_cuffs_and_freezes_the_player(self):
        cuff = block(POLICE, 'stock RW_PoliceCuff(playerid, target)')
        self.assertIn('if (!RW_PoliceCanAct(playerid, target)) return 0;', cuff)
        self.assertIn('if (RW_PoliceItem(playerid, ITEM_CUFFS) < 0) return RW_Error(playerid, "You need handcuffs in the inventory.");', cuff)
        self.assertIn('TogglePlayerControllable(target, false);', cuff)
        uncuff = block(POLICE, 'stock RW_PoliceUncuff(playerid, target)')
        self.assertIn('TogglePlayerControllable(target, true);', uncuff)
        self.assertIn('DELETE FROM wall_attachments WHERE player_id=%d', uncuff)

    def test_escort_follows_the_officer_and_breaks_at_a_distance(self):
        escort = block(POLICE, 'stock RW_PoliceEscort(playerid, target)')
        self.assertIn('if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");', escort)
        self.assertIn('gEscort[target] = playerid;', escort)
        pulse = block(POLICE, 'public RW_PolicePulse()', '\n    return 1;')
        self.assertIn('if (!IsPlayerConnected(officer) || !RW_PoliceNear(officer, p, 8.0))', pulse)
        self.assertIn('SetPlayerPos(p, x, y, z);', pulse)
        self.assertIn('SetPlayerFacingAngle(p, a);', pulse)

    def test_the_wall_attachment_is_stored_in_the_database(self):
        wall = block(POLICE, 'stock RW_PoliceWall(playerid, target)')
        self.assertIn('INSERT OR REPLACE INTO wall_attachments(', wall)
        self.assertIn('TogglePlayerControllable(target, false);', wall)
        unwall = block(POLICE, 'stock RW_PoliceUnwall(playerid, target)')
        self.assertIn('DELETE FROM wall_attachments WHERE player_id=%d', unwall)

    def test_the_taser_holds_two_charges_and_the_baton_wears_out(self):
        self.assertIn('#define POLICE_STUN_MS      3000', POLICE)
        self.assertIn('#define TASER_COOLDOWN_MS   6000', POLICE)
        self.assertIn('#define BATON_WEAR          5', POLICE)
        stun = block(POLICE, 'stock RW_PoliceStun(playerid, target)')
        self.assertIn('if (GetTickCount() - gTaserTick[playerid] < TASER_COOLDOWN_MS)', stun)
        self.assertIn('gStun[target] = GetTickCount() + POLICE_STUN_MS;', stun)
        self.assertIn('RW_PoliceWear(playerid, slot, 1, "The taser is out of charge and broke.");', stun)
        self.assertIn('RW_PoliceWear(playerid, slot, BATON_WEAR, "The baton broke.");', stun)
        wear = block(POLICE, 'stock RW_PoliceWear(playerid, slot, loss, const broken[])')
        self.assertIn('DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d', wear)

    def test_the_stun_button_needs_a_player_nearby(self):
        command = block(POLICE, 'stock RW_PoliceStunCommand(playerid)')
        self.assertIn('new target = RW_PlayerTarget(playerid);', command)
        self.assertIn('if (target < 0) return RW_Error(playerid, "Stand closer to the player.");', command)
        click = block(INV, 'stock RW_InvClick(playerid, PlayerText:clicked)', '\npublic ')
        self.assertIn('if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_TASER) return RW_PoliceStunCommand(playerid);', click)

    def test_death_clears_the_cuffs_but_not_the_sentence(self):
        death = block(POLICE, 'stock RW_PoliceOnDeath(playerid)')
        self.assertIn('gCuffed[playerid] = false;', death)
        self.assertIn('DELETE FROM wall_attachments WHERE player_id=%d', death)
        self.assertNotIn('gJailTime[playerid] = 0;', death)
        self.assertIn('RW_PoliceOnDeath(playerid);', MAIN.split('public OnPlayerDeath')[1][:600])

    def test_stun_releases_after_three_seconds(self):
        pulse = block(POLICE, 'public RW_PolicePulse()', '\n    return 1;')
        self.assertIn('if (gStun[p] && GetTickCount() >= gStun[p])', pulse)
        self.assertIn('TogglePlayerControllable(p, true);', pulse)
        self.assertIn('ClearAnimations(p);', pulse)

    def test_wanted_carries_stars_and_a_reason(self):
        wanted = block(POLICE, 'stock RW_PoliceWanted(playerid, target, stars, reason)')
        self.assertIn('if (stars < 0 || stars > 5)', wanted)
        self.assertIn('INSERT OR REPLACE INTO wanted(', wanted)
        self.assertIn('DELETE FROM wanted WHERE player_id=%d', wanted)
        reasons = re.findall(r'case \d: format\(dest, size, "([^"]+)"\);', block(POLICE, 'stock RW_PoliceReason(id, dest[], size = sizeof dest)'))
        self.assertEqual(reasons, ['Theft', 'Assault', 'Robbery', 'Car theft', 'Murder'])

    def test_arrest_needs_cuffs_and_the_station(self):
        arrest = block(POLICE, 'stock RW_PoliceArrest(playerid, target, minutes, bail)')
        self.assertIn('if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");', arrest)
        self.assertIn('if (!RW_PoliceAtStation(playerid) || !RW_PoliceAtStation(target))', arrest)
        self.assertIn('if (minutes < 1 || minutes > JAIL_MAX_MINUTES)', arrest)
        self.assertIn('if (bail < 0 || bail > 100000)', arrest)
        self.assertIn('INSERT INTO arrests(', arrest)
        self.assertIn('DELETE FROM wanted WHERE player_id=%d', arrest)
        self.assertIn('#define JAIL_MAX_MINUTES    60', POLICE)
        self.assertIn('return RW_PoliceJail(playerid, target, seconds, bail, 0, true);', arrest)

    def test_the_sentence_cannot_be_escaped(self):
        jail = block(POLICE, 'stock RW_PoliceJail(playerid, target, seconds, bail, reason, bool:fresh)')
        self.assertIn('SetPlayerPos(who, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z);', jail)
        self.assertIn('if (fresh) RW_PoliceSave(who);', jail)
        pulse = block(POLICE, 'public RW_PolicePulse()', '\n    return 1;')
        self.assertIn('if (GetPlayerDistanceFromPoint(p, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z) > 60.0)', pulse)
        self.assertIn('SetPlayerPos(p, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z);', pulse)
        self.assertIn('if (gJailTime[p] % 30 == 0) RW_PoliceSave(p);', pulse)
        self.assertIn('if (gJailTime[p] <= 0) RW_PoliceRelease(p, "Served the term.");', pulse)

    def test_bail_pays_ten_percent_to_the_officer(self):
        bail = block(POLICE, 'stock RW_PoliceBail(playerid)')
        self.assertIn('if (gJailBail[playerid] <= 0) return RW_Error(playerid, "Bail is not set for this arrest.");', bail)
        self.assertIn('new cut = gJailBail[playerid] / 10;', bail)
        self.assertIn('RW_AddMoney(officer, cut);', bail)
        self.assertIn('RW_PoliceRelease(playerid, "Walk out and behave.");', bail)

    def test_fines_are_issued_and_paid_at_the_station(self):
        fine = block(POLICE, 'stock RW_PoliceFine(playerid, target, amount, reason)')
        self.assertIn('if (amount <= 0 || amount > 50000)', fine)
        self.assertIn('INSERT INTO fines(', fine)
        pay = block(POLICE, 'stock RW_PolicePayFines(playerid)')
        self.assertIn('SELECT id,amount FROM fines WHERE player_id=%d AND paid=0', pay)
        self.assertIn('UPDATE fines SET paid=1 WHERE player_id=%d AND paid=0', pay)

    def test_tracking_marks_the_citizen_on_the_map(self):
        track = block(POLICE, 'stock RW_PoliceTrack(playerid, target)')
        self.assertIn('gTrack[playerid] = target;', track)
        untrack = block(POLICE, 'stock RW_PoliceUntrack(playerid)')
        self.assertIn('RemovePlayerMapIcon(playerid, 97);', untrack)
        pulse = block(POLICE, 'public RW_PolicePulse()', '\n    return 1;')
        self.assertIn('SetPlayerMapIcon(p, 97, x, y, z, 0, 0xFF453AFF, MAPICON_GLOBAL);', pulse)
        self.assertIn('if (!IsPlayerConnected(target) || !gLogged[target] || !RW_PoliceOnDuty(p)) RW_PoliceUntrack(p);', pulse)

    def test_officer_names_cannot_break_the_query(self):
        self.assertIn('stock RW_PoliceSqlName(const src[], dest[], size = sizeof dest)', POLICE)
        for query in ('INSERT OR REPLACE INTO wanted(', 'INSERT INTO arrests(', 'INSERT INTO fines('):
            self.assertIn('RW_PoliceSqlName(nick, safe, sizeof safe);', block(POLICE, 'stock RW_PoliceWanted(playerid, target, stars, reason)') if 'wanted' in query else POLICE)


class LawyerTest(unittest.TestCase):
    """Licence, case chat and bail through a lawyer."""

    def test_the_licence_is_issued_by_the_government(self):
        licence = block(POLICE, 'stock RW_PoliceLicense(playerid, target, days)')
        self.assertIn('if (!gLogged[playerid] || gAdminLevel[playerid] < 3) return RW_Error(playerid, "Administrators only.");', licence)
        self.assertIn('if (days < 0 || days > 365)', licence)
        self.assertIn('new expire = days ? gettime() + days * 86400 : 0;', licence)

    def test_a_case_is_opened_from_the_cell(self):
        start = block(POLICE, 'stock RW_PoliceCaseStart(lawyer, prisoner)')
        self.assertIn('if (!RW_IsLawyer(lawyer)) return RW_Error(lawyer, "You have no lawyer license.");', start)
        self.assertIn('if (prisoner < 0 || !IsPlayerConnected(prisoner) || gJailTime[prisoner] <= 0)', start)
        self.assertIn('if (!RW_PoliceNear(lawyer, prisoner, 4.0))', start)
        self.assertIn('gCaseId[lawyer] = id;', start)
        self.assertIn('#define MAX_CASES           4', POLICE)

    def test_case_chat_reaches_the_three_of_them(self):
        say = block(POLICE, 'stock RW_PoliceCaseSay(playerid, const text[])')
        self.assertIn('ids[0] = gCaseLawyer[id-1]; ids[1] = gCasePrisoner[id-1]; ids[2] = gCaseOfficer[id-1];', say)
        self.assertIn('return 0;', say)
        self.assertIn('if (gCaseId[playerid]) return RW_PoliceCaseSay(playerid, text);', MAIN)

    def test_bail_through_a_lawyer_is_split(self):
        request = block(POLICE, 'stock RW_PoliceBailRequest(playerid)')
        self.assertIn('new lawyerCut = bail / 2, officerCut = bail / 10;', request)
        self.assertIn('RW_AddMoney(playerid, lawyerCut);', request)
        self.assertIn('RW_AddMoney(officer, officerCut);', request)
        self.assertIn('RW_PoliceCaseEnd(playerid);', request)
        self.assertIn('RW_PoliceRelease(prisoner, "Released on bail through a lawyer.");', request)

    def test_closing_a_case_frees_everybody(self):
        end = block(POLICE, 'stock RW_PoliceCaseEnd(playerid)')
        self.assertIn('gCaseId[ids[i]] = 0;', end)
        self.assertIn('gCaseLawyer[id-1] = INVALID_PLAYER_ID;', end)
        self.assertIn('gCasePrisoner[id-1] = INVALID_PLAYER_ID;', end)
        self.assertIn('gCaseOfficer[id-1] = INVALID_PLAYER_ID;', end)
        self.assertIn('if (gCaseId[playerid]) RW_PoliceCaseEnd(playerid);', block(POLICE, 'stock RW_PoliceReset(playerid)'))

    def test_the_case_slots_start_empty(self):
        init = block(POLICE, 'stock RW_PoliceInit()')
        self.assertIn('gCaseLawyer[i] = INVALID_PLAYER_ID;', init)
        self.assertIn('RW_PoliceInit();', MAIN)


class PoliceInterfaceTest(unittest.TestCase):
    """Y menu, tablet, commands and the browser."""

    def test_y_on_a_player_opens_the_police_menu(self):
        menu = block(INTER, 'stock RW_PlayerMenu(playerid, target)', '\nstock ')
        self.assertIn('if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0) return RW_PoliceMenuOpen(playerid, target);', menu)
        self.assertIn('if (RW_IsLawyer(playerid) && gJailTime[target] > 0) return RW_PoliceCaseStart(playerid, target);', menu)

    def test_y_on_yourself_opens_the_tablet(self):
        self_menu = block(INTER, 'stock RW_SelfMenu(playerid)', '\nstock ')
        self.assertIn('if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0) return RW_MdtOpen(playerid);', self_menu)

    def test_both_interfaces_share_one_click_handler(self):
        click = block(INTER, 'stock RW_UI_Click(playerid, row)', '\nstock ')
        self.assertIn('} else if (mode == UI_POLICE) {', click)
        self.assertIn('return RW_PoliceClick(playerid, row);', click)
        self.assertIn('} else if (mode == UI_MDT) {', click)
        self.assertIn('return RW_MdtClick(playerid, row);', click)
        self.assertIn('#define UI_POLICE 13', UI)
        self.assertIn('#define UI_MDT 14', UI)
        self.assertIn('#define CEF_MODE_POLICE             (3)', CORE)
        self.assertIn('#define CEF_MODE_MDT                (4)', CORE)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_POLICE) return RW_PoliceCefClick(playerid, id);', CEFUI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_MDT) return RW_MdtCefClick(playerid, id);', CEFUI)
        self.assertIn('stock RW_PoliceCefClick(playerid, row) { return RW_PoliceAction(playerid, row, true); }', POLICE)
        self.assertIn('stock RW_MdtCefClick(playerid, row) { return RW_MdtAction(playerid, row, true); }', POLICE)

    def test_the_tablet_has_four_tabs(self):
        self.assertIn('#define MDT_CITIZENS        0', POLICE)
        self.assertIn('#define MDT_FINES           3', POLICE)
        for query in ('FROM accounts a ORDER BY a.name LIMIT %d OFFSET %d',
                      'FROM owned_cars c LEFT JOIN accounts a ON a.id=c.owner ORDER BY c.id DESC LIMIT %d OFFSET %d',
                      'FROM arrests r LEFT JOIN accounts a ON a.id=r.player_id ORDER BY r.id DESC LIMIT %d OFFSET %d',
                      'FROM fines f LEFT JOIN accounts a ON a.id=f.player_id ORDER BY f.id DESC LIMIT %d OFFSET %d'):
            self.assertIn(query, POLICE)

    def test_the_lawyer_app_only_works_from_a_cell(self):
        app = block(CEFUI, 'public RW_CefPhoneApp(playerid, browserid, app)', '\npublic ')
        self.assertIn('if (gJailTime[playerid] <= 0)', app)
        self.assertIn('return RW_CefPhoneLawyers(playerid);', app)
        self.assertIn('CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:lawyer", CEF_STR(name), CEF_INT(db_get_field_assoc_int(r, "rating")), CEF_INT(db_get_field_assoc_int(r, "price")), CEF_INT(db_get_field_assoc_int(r, "num")));', block(CEFUI, 'stock RW_CefPhoneLawyers(playerid)', '\npublic '))
        self.assertIn('CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:lawyerend");', CEFUI)
        self.assertIn("RW.on('phone:lawyer', function (name, rating, price, number) {", PAGE_JS)
        self.assertIn("RW.on('phone:lawyerend', function () {", PAGE_JS)
        self.assertIn('if (id === 5) return screenLawyers();', PAGE_JS)

    def test_commands_are_wired(self):
        chain = block(CMDS, 'stock RW_CommandProcess(playerid, cmdtext[])', '\nstock ')
        for line in ('if (!strcmp(cmd, "duty", true)) return RW_PoliceDutyCommand(playerid);',
                     'if (!strcmp(cmd, "faction", true)) return RW_PoliceFactionCommand(playerid, params);',
                     'if (!strcmp(cmd, "mdt", true)) return RW_MdtOpen(playerid);',
                     'if (!strcmp(cmd, "cuff", true)) return RW_PoliceCuffCommand(playerid, params);',
                     'if (!strcmp(cmd, "escort", true)) return RW_PoliceEscortCommand(playerid, params);',
                     'if (!strcmp(cmd, "wall", true)) return RW_PoliceWallCommand(playerid, params);',
                     'if (!strcmp(cmd, "wanted", true)) return RW_PoliceWantedCommand(playerid, params);',
                     'if (!strcmp(cmd, "arrest", true)) return RW_PoliceArrestCommand(playerid, params);',
                     'if (!strcmp(cmd, "release", true)) return RW_PoliceReleaseCommand(playerid, params);',
                     'if (!strcmp(cmd, "bail", true)) return RW_PoliceBail(playerid);',
                     'if (!strcmp(cmd, "fine", true)) return RW_PoliceFineCommand(playerid, params);',
                     'if (!strcmp(cmd, "fines", true)) return RW_PolicePayFines(playerid);',
                     'if (!strcmp(cmd, "track", true)) return RW_PoliceTrackCommand(playerid, params);',
                     'if (!strcmp(cmd, "untrack", true)) return RW_PoliceUntrack(playerid);',
                     'if (!strcmp(cmd, "lawyer", true)) return RW_LawyerCommand(playerid);',
                     'if (!strcmp(cmd, "setlawyer", true)) return RW_LawyerLicenseCommand(playerid, params);',
                     'if (!strcmp(cmd, "lawprice", true)) return RW_LawyerPriceCommand(playerid, params);',
                     'if (!strcmp(cmd, "bailrequest", true)) return RW_PoliceBailRequest(playerid);',
                     'if (!strcmp(cmd, "endcase", true)) return RW_PoliceCaseEnd(playerid);'):
            self.assertIn(line, chain)
        help_text = block(CMDS, 'stock RW_CommandHelp(playerid)', '\nstock ')
        self.assertIn('/duty, /mdt, /cuff', help_text)
        self.assertIn('/lawyer, /bailrequest', help_text)

    def test_the_module_is_loaded_saved_and_ticked(self):
        self.assertIn('#include <railway/police>', MAIN)
        self.assertIn('RW_PoliceSchema()', MAIN)
        self.assertIn('RW_PoliceInit();', MAIN)
        self.assertIn('SetTimer("RW_PolicePulse", 1000, true);', MAIN)
        connect = MAIN.split('public OnPlayerConnect')[1][:1200]
        self.assertIn('RW_PoliceReset(playerid);', connect)
        disconnect = MAIN.split('public OnPlayerDisconnect')[1][:1200]
        self.assertIn('RW_PoliceSave(playerid);', disconnect)
        self.assertIn('RW_PoliceLoad(playerid);', pawn('accounts'))

    def test_the_station_and_the_cells_have_coordinates(self):
        for name in ('POLICE_STATION_X', 'POLICE_STATION_Y', 'POLICE_STATION_Z', 'POLICE_STATION_RANGE',
                     'POLICE_CELL_X', 'POLICE_CELL_Y', 'POLICE_CELL_Z'):
            self.assertRegex(CORE, r'#define %s\s' % name)


if __name__ == '__main__':
    unittest.main()
