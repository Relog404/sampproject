"""Transport: fuel, trunks, breaking in, the mechanic service and tuning.

Every rule lives on the server: the browser menu and the TextDraw menu call the
same stocks, and the state is kept in SQLite so it survives a restart. SQL from
the Pawn sources is executed here against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

TRANS = pawn('transport')
CORE = pawn('core')
UI = pawn('ui')
INV = pawn('inventory')
INTER = pawn('interaction')
CEFUI = pawn('cefui')
CMDS = pawn('commands')
POLICE = pawn('police')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class TransportTableTest(unittest.TestCase):
    """The tuning, trunk and impound tables, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql(TRANS, 'CREATE TABLE IF NOT EXISTS car_tuning '))
        self.db.execute(sql(TRANS, 'CREATE TABLE IF NOT EXISTS trunks '))
        self.db.execute(sql(TRANS, 'CREATE TABLE IF NOT EXISTS impounds '))

    def tearDown(self):
        self.db.close()

    def columns(self, table):
        return [row[1] for row in self.db.execute('PRAGMA table_info(%s)' % table)]

    def test_table_shapes(self):
        self.assertEqual(self.columns('car_tuning'), ['token', 'part', 'level', 'uid', 'stamp'])
        self.assertEqual(self.columns('trunks'), ['uid', 'token', 'slot', 'type', 'data', 'amount', 'grams'])
        self.assertEqual(self.columns('impounds'), ['token', 'mechanic', 'reason', 'stamp'])

    def test_a_part_is_written_with_the_item_uid(self):
        self.db.execute(sql(TRANS, 'INSERT INTO car_tuning(') % (501, 0, 4, 991, 1000))
        self.db.execute(sql(TRANS, 'INSERT INTO car_tuning(') % (501, 4, 2, 992, 1100))
        rows = self.db.execute(sql(TRANS, 'SELECT part,level FROM car_tuning WHERE token=%d') % 501).fetchall()
        self.assertEqual(sorted(rows), [(0, 4), (4, 2)])

    def test_the_best_level_wins_when_the_same_part_is_fitted_twice(self):
        """RW_TuneLoad keeps the highest level, so an older row cannot downgrade a car."""
        for level in (3, 5, 2):
            self.db.execute(sql(TRANS, 'INSERT INTO car_tuning(') % (501, 0, level, 990 + level, 1000))
        levels = [row[0] for row in self.db.execute('SELECT level FROM car_tuning WHERE token=501 AND part=0')]
        self.assertEqual(max(levels), 5)
        self.assertIn('if (part >= 0 && part < TUNE_PARTS && level > gTune[slot][part]) gTune[slot][part] = level;', TRANS)

    def test_a_trunk_is_ten_numbered_slots_per_car(self):
        for i in range(10):
            self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (100 + i, 501, i, 9, 0, 30, 500))
        rows = self.db.execute(sql(TRANS, 'SELECT uid,type,data,amount,grams FROM trunks WHERE token=%d ORDER BY slot LIMIT %d OFFSET %d') % (501, 5, 0)).fetchall()
        self.assertEqual([row[0] for row in rows], [100, 101, 102, 103, 104])
        page2 = self.db.execute(sql(TRANS, 'SELECT uid,type,data,amount,grams FROM trunks WHERE token=%d ORDER BY slot LIMIT %d OFFSET %d') % (501, 5, 5)).fetchall()
        self.assertEqual([row[0] for row in page2], [105, 106, 107, 108, 109])

    def test_grams_are_summed_to_check_the_limit(self):
        for uid, grams in ((1, 5000), (2, 7000), (3, 2000)):
            self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (uid, 501, uid, 9, 0, 30, grams))
        self.assertEqual(self.db.execute(sql(TRANS, 'SELECT grams FROM trunks WHERE token=%d') % 501).fetchall()[0][0], 5000)
        self.assertEqual(self.db.execute('SELECT SUM(grams) FROM trunks WHERE token=501').fetchone()[0], 14000)
        self.assertIn('if (RW_TrunkGrams(token) + grams > RW_TrunkCapacity(gVeh[slot][vsModel])) return RW_Error(playerid, "It does not fit: the trunk is full.");', TRANS)

    def test_a_free_slot_is_found_by_number(self):
        """RW_TrunkFreeSlot walks the slots and returns the first empty one."""
        self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (1, 501, 0, 9, 0, 30, 500))
        self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (2, 501, 2, 9, 0, 30, 500))
        for slot in range(10):
            rows = self.db.execute(sql(TRANS, 'SELECT uid FROM trunks WHERE token=%d AND slot=%d') % (501, slot)).fetchall()
            if slot in (0, 2):
                self.assertTrue(rows)
            else:
                self.assertFalse(rows)
        self.assertIn('TRUNK_SLOTS', CORE)
        free = block(TRANS, 'stock RW_TrunkFreeSlot(token)')
        self.assertIn('TRUNK_SLOTS', free)
        self.assertIn('return i;', free)

    def test_an_item_leaves_the_trunk_when_taken_back(self):
        self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (77, 501, 3, 12, 1, 40, 900))
        row = self.db.execute(sql(TRANS, 'SELECT type,data,amount FROM trunks WHERE uid=%d AND token=%d') % (77, 501)).fetchone()
        self.assertEqual(row, (12, 1, 40))
        self.db.execute(sql(TRANS, 'DELETE FROM trunks WHERE uid=%d AND token=%d') % (77, 501))
        self.assertEqual(self.db.execute('SELECT uid FROM trunks WHERE token=501').fetchall(), [])

    def test_impound_is_one_row_per_car(self):
        for who in ('Ivan', 'Petr'):
            self.db.execute(sql(TRANS, 'INSERT OR REPLACE INTO impounds(') % (501, who, 1000))
        rows = self.db.execute('SELECT token,mechanic FROM impounds').fetchall()
        self.assertEqual(rows, [(501, 'Petr')])
        self.db.execute(sql(TRANS, 'DELETE FROM impounds WHERE token=%d') % 501)
        self.assertIsNone(self.db.execute(sql(TRANS, 'SELECT token FROM impounds WHERE token=%d') % 501).fetchone())

    def test_the_impound_list_names_the_car(self):
        self.db.execute("CREATE TABLE owned_cars (id INT, token INT, model INT)")
        self.db.execute("INSERT INTO owned_cars VALUES(501,501,560)")
        self.db.execute(sql(TRANS, 'INSERT OR REPLACE INTO impounds(') % (501, 'Ivan', 1000))
        query = sql(TRANS, 'SELECT i.token,c.model,i.mechanic,i.stamp FROM impounds i')
        rows = self.db.execute(query).fetchall()
        self.assertEqual(rows, [(501, 560, 'Ivan', 1000)])
        self.assertIn('db_get_field_assoc(r, "mechanic", who, sizeof who);', TRANS)


class TransportItemTest(unittest.TestCase):
    """A lockpick and a tuning part are ordinary inventory items."""

    def test_item_types_are_registered(self):
        self.assertRegex(CORE, r'#define ITEM_LOCKPICK\s+16')
        self.assertRegex(CORE, r'#define ITEM_TUNING\s+17')
        self.assertRegex(INV, r'#define INV_TYPE_MAX 1[78]')

    def test_both_items_have_a_model_and_a_weight(self):
        self.assertRegex(INV, r'if \(type == ITEM_LOCKPICK\) return 1580;')
        self.assertRegex(INV, r'if \(type == ITEM_LOCKPICK\) return 100;')
        self.assertRegex(INV, r'if \(type == ITEM_TUNING\) return RW_TunePartGrams\(data\);')

    def test_both_items_are_accepted_by_word(self):
        self.assertIn('!strcmp(word, "lockpick", true)', INV)
        self.assertIn('!strcmp(word, "tuning", true)', INV)
        self.assertIn('return ITEM_LOCKPICK;', INV)
        self.assertIn('return ITEM_TUNING;', INV)

    def test_a_tuning_part_carries_the_part_number_and_the_level(self):
        self.assertIn('if (type==ITEM_TUNING && (data<0 || data>=TUNE_PARTS || amount<1 || amount>TUNE_MAX_LEVEL)) return 0;', INV)
        self.assertIn('if (data < 0 || data >= TUNE_PARTS) return RW_Error(playerid, "Part: 0 engine, 1 gearbox, 2 brakes, 3 suspension, 4 wheels.");', INV)
        self.assertIn('if (amount < 1 || amount > TUNE_MAX_LEVEL) return RW_Error(playerid, "Level must be 1 to 5.");', INV)

    def test_the_inventory_button_fits_or_picks(self):
        self.assertIn('if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_LOCKPICK) return RW_TransportRow(playerid, RW_TransportCar(playerid));', INV)
        self.assertIn('if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_TUNING) return RW_TuneInstall(playerid, RW_TransportCar(playerid));', INV)
        self.assertIn('else if (type == ITEM_LOCKPICK) format(action, sizeof action, "%s", "PICK");', INV)
        self.assertIn('else if (type == ITEM_TUNING) format(action, sizeof action, "%s", "FIT");', INV)

    def test_titles_name_the_part_and_the_level(self):
        self.assertIn('else if (type == ITEM_TUNING) { new pname[24]; RW_TuneName(data, pname, sizeof pname); format(title, sizeof title, "%s L%d", pname, amount); }', INV)


class FuelTest(unittest.TestCase):
    """Fuel burns only with the engine running and is sold at gas stations."""

    def test_constants(self):
        self.assertRegex(CORE, r'#define FUEL_MAX\s+100\.0')
        self.assertRegex(CORE, r'#define FUEL_PRICE\s+3')
        self.assertRegex(CORE, r'#define FUEL_STATION_RANGE\s+12\.0')
        self.assertRegex(CORE, r'#define FUEL_IDLE\s+0\.008')
        self.assertRegex(CORE, r'#define FUEL_SPEED\s+0\.0006')

    def test_the_pulse_burns_fuel_and_stops_the_engine(self):
        pulse = TRANS.split('public RW_TransportPulse()', 1)[1]
        self.assertIn('if (gVeh[s][vsEngine]) {', pulse)
        self.assertIn('gFuel[s] -= (FUEL_IDLE + kmh * FUEL_SPEED);', pulse)
        self.assertIn('gFuel[s] = 0.0;', pulse)
        self.assertIn('RW_Veh_SetKey(vehicleid, false);', pulse)
        self.assertIn('Out of fuel. A mechanic on duty can refuel you, or tow the car to a gas station.', pulse)

    def test_fuel_is_clamped_to_the_tank(self):
        setfn = block(TRANS, 'stock RW_FuelSet(vehicleid, Float:liters)')
        self.assertIn('if (liters < 0.0) liters = 0.0;', setfn)
        self.assertIn('if (liters > FUEL_MAX) liters = FUEL_MAX;', setfn)

    def test_there_are_several_gas_stations(self):
        table = TRANS.split('static const Float:gGasStations[][3] = {', 1)[1].split('};', 1)[0]
        rows = re.findall(r'\{([^}]+)\}', table)
        self.assertEqual(len(rows), 4)
        # Реальные заправки Los Santos: Temple, Ocean Docks, Idlewood, Willowfield.
        for place in ('Temple', 'Ocean Docks', 'Idlewood', 'Willowfield'):
            self.assertIn(place, table)
        for row in rows:
            x, y, z = [float(v) for v in row.split(',')]
            self.assertTrue(0.0 < x < 3000.0, row)      # восток штата
            self.assertTrue(-3000.0 < y < -500.0, row)  # южное побережье
            self.assertTrue(0.0 < z < 100.0, row)

    def test_a_station_is_a_sphere_around_the_pump(self):
        atgas = block(TRANS, 'stock bool:RW_TransportAtGas(playerid)')
        self.assertIn('FUEL_STATION_RANGE*FUEL_STATION_RANGE', atgas)

    def test_refuelling_needs_a_station_or_a_mechanic(self):
        refuel = block(TRANS, 'stock RW_FuelRefuel(playerid, vehicleid)')
        self.assertIn('if (!mech && !RW_TransportAtGas(playerid)) return RW_Error(playerid, "Drive to a gas station: /fuel shows the level.");', refuel)
        self.assertIn('new cost = floatround(need) * FUEL_PRICE;', refuel)
        self.assertIn('if (!mech && !RW_TakeMoney(playerid, cost)) return RW_Error(playerid, "Not enough cash for a full tank.");', refuel)
        self.assertIn('SetTimerEx("RW_TransRefuelDone", ms, false, "iif", playerid, vehicleid, need);', refuel)

    def test_the_tank_is_filled_after_the_wait(self):
        done = TRANS.split('public RW_TransRefuelDone(playerid, vehicleid, Float:liters)', 1)[1].split('\n}', 1)[0]
        self.assertIn('RW_TransportBusyEnd(playerid);', done)
        self.assertIn('RW_FuelSet(vehicleid, gFuel[slot] + liters);', done)

    def test_a_spawned_car_leaves_the_garage_full(self):
        self.assertIn('RW_FuelSet(vehicleid, FUEL_MAX);', pawn('vehicles'))
        self.assertIn('RW_FuelSet(vehicle, FUEL_MAX);', pawn('garage'))


class TrunkTest(unittest.TestCase):
    """The trunk is a small container that belongs to the car, not to the player."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql(TRANS, 'CREATE TABLE IF NOT EXISTS trunks '))

    def tearDown(self):
        self.db.close()

    def test_the_limit_depends_on_the_class_of_the_car(self):
        capacity = block(TRANS, 'stock RW_TrunkCapacity(model)')
        self.assertIn('return 5000;', capacity)    # мотоцикл
        self.assertIn('return 15000;', capacity)   # спорткар
        self.assertIn('return 80000;', capacity)   # фургон
        self.assertIn('return 40000;', capacity)   # остальные

    def test_only_the_owner_opens_it(self):
        openfn = block(TRANS, 'stock RW_TrunkOpen(playerid, vehicleid)')
        self.assertIn('if (!RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is not your car.");', openfn)
        self.assertIn('if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");', openfn)

    def test_putting_takes_the_selected_item(self):
        put = block(TRANS, 'stock RW_TrunkPut(playerid)')
        self.assertIn('new uid = gInvSelected[playerid];', put)
        self.assertIn('if (inv < 0) return RW_Error(playerid, "Select an item in the inventory first (I, then the item).");', put)
        self.assertIn(sql(TRANS, 'INSERT INTO trunks('), put)
        self.assertIn(sql(TRANS, 'DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d'), put)

    def test_the_item_stays_in_the_trunk_after_a_restart(self):
        self.db.execute(sql(TRANS, 'INSERT INTO trunks(') % (77, 501, 0, 12, 1, 40, 900))
        row = self.db.execute('SELECT uid,type,data,amount,grams FROM trunks WHERE token=501').fetchone()
        self.assertEqual(row, (77, 12, 1, 40, 900))

    def test_taking_it_back_needs_room_in_the_pockets(self):
        take = block(TRANS, 'stock RW_TrunkTake(playerid, row)')
        self.assertIn('if (!RW_InvFree(playerid, 0, type, data, amount)) return RW_Error(playerid, "No room for it in your pockets.");', take)
        self.assertIn('RW_InvGrant(playerid, type, data, amount);', take)

    def test_both_faces_share_one_action_handler(self):
        self.assertIn('stock RW_TrunkCefClick(playerid, row) { return RW_TrunkAction(playerid, row, true); }', TRANS)
        self.assertIn('stock RW_TrunkClick(playerid, row) { return RW_TrunkAction(playerid, row, false); }', TRANS)
        self.assertIn('if (row == 6) return RW_TrunkPut(playerid);', TRANS)
        self.assertIn('if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);', TRANS)

    def test_the_trunk_shows_its_load(self):
        self.assertIn('format(subtitle, sizeof subtitle, "%d / %d kg", floatdiv(RW_TrunkGrams(RW_TransportToken(vehicleid)), 1000), floatdiv(RW_TrunkCapacity(gVeh[slot][vsModel]), 1000));', TRANS)


class LockpickTest(unittest.TestCase):
    """Breaking in is a short mini-game that can cost you the lockpick."""

    def test_the_game_is_three_pins(self):
        self.assertRegex(CORE, r'#define PICK_STEPS\s+3')
        self.assertRegex(CORE, r'#define PICK_MS\s+8000')
        start = block(TRANS, 'stock RW_PickStart(playerid, vehicleid)')
        self.assertIn('for (new i; i < PICK_STEPS; i++) gPickSeq[playerid][i] = 1 + random(4);', start)
        self.assertIn('SetTimerEx("RW_PickTimeout", PICK_MS, false, "ii", playerid, gPickStamp[playerid]);', start)

    def test_you_never_pick_your_own_car(self):
        start = block(TRANS, 'stock RW_PickStart(playerid, vehicleid)')
        self.assertIn('if (RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is your own car: use the key.");', start)
        self.assertIn('if (RW_PickItem(playerid) < 0) return RW_Error(playerid, "You need a lockpick in the inventory.");', start)

    def test_a_wrong_pin_breaks_the_lockpick(self):
        click = block(TRANS, 'stock RW_PickClick(playerid, row, bool:cef)')
        self.assertIn('if (row + 1 != gPickSeq[playerid][gPickStep[playerid]]) return RW_PickFail(playerid);', click)
        fail = block(TRANS, 'stock RW_PickFail(playerid)')
        self.assertIn(sql(TRANS, 'DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d'), fail)
        self.assertIn('RW_Error(playerid, "The lockpick snapped.");', fail)
        self.assertIn('RW_PickAlarm(playerid, vehicleid);', fail)

    def test_running_out_of_time_breaks_it_too(self):
        timeout = TRANS.split('public RW_PickTimeout(playerid, stamp)', 1)[1].split('\n}', 1)[0]
        self.assertIn('if (stamp != gPickStamp[playerid] || !gPickCar[playerid]) return 1;', timeout)
        self.assertIn('return RW_PickFail(playerid);', timeout)

    def test_success_opens_the_car_and_warns_the_city(self):
        done = block(TRANS, 'stock RW_PickSuccess(playerid)')
        self.assertIn('gVeh[vslot][vsDoors] = true;', done)
        self.assertIn('gVeh[vslot][vsEngine] = true;', done)
        self.assertIn('RW_Veh_SyncAll(vehicleid);', done)
        self.assertIn('return RW_PickAlarm(playerid, vehicleid);', done)

    def test_the_owner_and_the_police_hear_about_it(self):
        alarm = block(TRANS, 'stock RW_PickAlarm(playerid, vehicleid)')
        self.assertTrue('RW_EmsCall' in alarm or 'RW_Police' in alarm or 'gWanted' in alarm)

    def test_you_can_give_up(self):
        click = block(TRANS, 'stock RW_PickClick(playerid, row, bool:cef)')
        self.assertIn('if (row == (cef ? 8 : 7)) { gPickStamp[playerid]++; gPickCar[playerid] = 0; return RW_UI_Close(playerid); }', click)

    def test_clicks_are_throttled(self):
        click = block(TRANS, 'stock RW_PickClick(playerid, row, bool:cef)')
        self.assertIn('if (GetTickCount()-gTransTick[playerid] < 200) return 1;', click)


class ServiceTest(unittest.TestCase):
    """Repair, refuel and tow are a mechanic's job."""

    def test_the_mechanic_is_a_faction(self):
        self.assertRegex(CORE, r'#define FACTION_MECHANIC\s+4')
        self.assertIn('case 4: format(dest, size, "Mechanic");', POLICE)
        self.assertIn('if (faction < 0 || faction > 4) return RW_Error(playerid, "Faction: 0 civilian, 1 LSPD, 2 Sheriff, 3 ambulance, 4 mechanic.");', POLICE)
        self.assertIn('return gLogged[playerid] && gDuty[playerid] && gFaction[playerid] == FACTION_MECHANIC;', TRANS)

    def test_the_service_needs_a_mechanic_on_duty(self):
        openfn = block(TRANS, 'stock RW_ServiceOpen(playerid, vehicleid)')
        self.assertIn('if (!RW_MechOnDuty(playerid)) return RW_Error(playerid, "You are not on duty as a mechanic.");', openfn)
        self.assertIn('if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");', openfn)

    def test_the_menu_offers_three_jobs(self):
        self.assertIn('RW_UI_Row(playerid, 0, "Repair the body");', TRANS)
        self.assertIn('RW_UI_Row(playerid, 1, "Refuel the tank");', TRANS)
        self.assertIn('RW_UI_Row(playerid, 2, "Tow it to the impound lot");', TRANS)
        self.assertIn('RW_CefRow(playerid, 0, "Repair the body", "damage", "settings");', TRANS)
        self.assertIn('RW_CefRow(playerid, 1, "Refuel the tank", "litres", "engine");', TRANS)
        self.assertIn('RW_CefRow(playerid, 2, "Tow it to the impound lot", "fee", "eject");', TRANS)

    def test_a_car_with_a_driver_is_never_towed(self):
        tow = block(TRANS, 'stock RW_ServiceTow(playerid, vehicleid)')
        self.assertIn('GetPlayerVehicleSeat(i) == 0', tow)
        self.assertIn('return RW_Error(playerid, "There is a driver inside.");', tow)
        self.assertIn('SetVehiclePos(vehicleid, IMPOUND_X, IMPOUND_Y, IMPOUND_Z);', tow)
        self.assertIn(sql(TRANS, 'INSERT OR REPLACE INTO impounds('), tow)

    def test_the_owner_is_told_where_the_car_went(self):
        tow = block(TRANS, 'stock RW_ServiceTow(playerid, vehicleid)')
        self.assertIn('gAccountId[i] != gVeh[slot][vsOwnerAcc]', tow)
        self.assertIn('Your car was towed to the impound lot. /impound to get it back.', tow)

    def test_the_impound_lot_is_behind_a_fee(self):
        self.assertRegex(CORE, r'#define IMPOUND_FEE\s+500')
        release = block(TRANS, 'stock RW_ImpoundRelease(playerid, const params[])')
        self.assertIn('if (!RW_TakeMoney(playerid, IMPOUND_FEE)) return RW_Error(playerid, "Not enough cash for the fee.");', release)
        self.assertIn(sql(TRANS, 'DELETE FROM impounds WHERE token=%d'), release)

    def test_only_the_mechanic_the_owner_or_an_administrator_looks_at_the_lot(self):
        listing = block(TRANS, 'stock RW_ImpoundList(playerid)')
        self.assertIn('if (!RW_MechOnDuty(playerid) && gAdminLevel[playerid] < 2 && !RW_OwnsImpounded(playerid))', listing)
        release = block(TRANS, 'stock RW_ImpoundRelease(playerid, const params[])')
        self.assertIn('IsPlayerInRangeOfPoint(playerid, 20.0, IMPOUND_X, IMPOUND_Y, IMPOUND_Z)', release)

    def test_releases_come_back_next_to_the_player(self):
        release = block(TRANS, 'stock RW_ImpoundRelease(playerid, const params[])')
        self.assertIn('SetVehiclePos(gVeh[s][vsVehicleId], x + 4.0 * floatsin(-a, degrees), y + 4.0 * floatcos(-a, degrees), z + 0.5);', release)


class TuneTest(unittest.TestCase):
    """A part is an item: it is carried in the trunk and fitted in a garage."""

    def test_constants(self):
        self.assertRegex(CORE, r'#define TUNE_PARTS\s+5')
        self.assertRegex(CORE, r'#define TUNE_MAX_LEVEL\s+5')
        self.assertRegex(CORE, r'#define TUNE_SPEED_STEP\s+5\.0')
        self.assertRegex(CORE, r'#define TUNE_INSTALL_MS\s+10000')
        for name, value in (('TUNE_ENGINE', 0), ('TUNE_GEARBOX', 1), ('TUNE_BRAKES', 2), ('TUNE_SUSP', 3), ('TUNE_WHEELS', 4)):
            self.assertRegex(TRANS, r'#define %s\s+%d' % (name, value))

    def test_the_engine_raises_the_top_speed(self):
        self.assertIn('return RW_TuneBaseSpeed(gVeh[slot][vsModel]) + TUNE_SPEED_STEP * float(gTune[slot][TUNE_ENGINE]);', TRANS)
        self.assertIn('case 415, 429, 411, 451', TRANS)

    def test_fitting_happens_at_the_garage_and_only_to_your_car(self):
        install = block(TRANS, 'stock RW_TuneInstall(playerid, vehicleid)')
        self.assertIn('if (!RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is not your car.");', install)
        self.assertIn('if (!RW_TuneNear(playerid)) return RW_Error(playerid, "Tuning is done at the tuning garage.");', install)
        self.assertIn('if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER) return RW_Error(playerid, "Step out of the car first.");', install)
        self.assertIn('if (inv < 0 || gInv[playerid][inv][iiType] != ITEM_TUNING) return RW_Error(playerid, "Select a tuning part in the inventory (I, then the part).");', install)

    def test_a_weaker_part_is_refused(self):
        install = block(TRANS, 'stock RW_TuneInstall(playerid, vehicleid)')
        self.assertIn('if (level <= gTune[vslot][part]) return RW_Error(playerid, "That part is no better than the installed one.");', install)

    def test_the_part_is_consumed_and_the_car_keeps_the_level(self):
        done = TRANS.split('public RW_TransTuneDone(playerid, slot)', 1)[1].split('\n}', 1)[0]
        self.assertIn(sql(TRANS, 'INSERT INTO car_tuning('), done)
        self.assertIn(sql(TRANS, 'DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d'), done)
        self.assertIn('gTune[vslot][part] = level;', done)
        self.assertIn('RW_TuneApply(vehicleid);', done)

    def test_levels_are_loaded_with_the_car(self):
        self.assertIn('RW_TuneLoad(vehicleid);', pawn('vehicles'))
        self.assertIn('RW_TuneLoad(vehicle);', pawn('garage'))

    def test_wheels_are_visible(self):
        self.assertIn('static const gTuneWheels[] = { 1073, 1074, 1075, 1076, 1077 };', TRANS)
        applyfn = block(TRANS, 'stock RW_TuneApply(vehicleid)')
        self.assertIn('AddVehicleComponent(vehicleid, gTuneWheels[level - 1]);', applyfn)

    def test_gearbox_and_brakes_change_the_drive(self):
        pulse = TRANS.split('public RW_TransportPulse()', 1)[1]
        self.assertIn('gTune[s][TUNE_GEARBOX]', pulse)
        self.assertIn('gTune[s][TUNE_BRAKES]', pulse)
        self.assertIn('SetVehicleVelocity(vehicleid, vx*scale, vy*scale, vz*scale);', pulse)


class TransportWiringTest(unittest.TestCase):
    """The slice is wired into the menus, the commands and the game mode."""

    def test_the_mode_knows_the_slice(self):
        self.assertIn('#include <railway/transport>', MAIN)
        self.assertIn('RW_TransportSchema()', MAIN)
        self.assertIn('SetTimer("RW_TransportPulse", 1000, true);', MAIN)
        self.assertIn('RW_TransportReset(playerid);', MAIN)

    def test_menu_modes_are_registered(self):
        self.assertRegex(UI, r'#define UI_TRUNK 1[0-9]')
        self.assertRegex(UI, r'#define UI_LOCKPICK 1[0-9]')
        self.assertRegex(UI, r'#define UI_SERVICE 1[0-9]')
        self.assertRegex(CORE, r'#define CEF_MODE_TRUNK\s+\(6\)')
        self.assertRegex(CORE, r'#define CEF_MODE_LOCKPICK\s+\(7\)')
        self.assertRegex(CORE, r'#define CEF_MODE_SERVICE\s+\(8\)')

    def test_the_car_menu_grows_a_transport_row(self):
        self.assertIn('if (RW_TransportRowLabel(playerid, v, transLabel, sizeof transLabel)) RW_UI_Row(playerid, 6, transLabel);', INTER)
        self.assertIn('if (RW_TransportRowLabel(playerid, v, transLabel, sizeof transLabel)) RW_CefRow(playerid, 6, transLabel, "", "bag");', INTER)
        self.assertIn('else if (row == 6) return RW_TransportRow(playerid, gControlVehicle[playerid]);', INTER)
        self.assertIn('if (id == 6) return RW_TransportRow(playerid, v);', INTER)

    def test_the_row_label_depends_on_who_you_are(self):
        label = block(TRANS, 'stock RW_TransportRowLabel(playerid, vehicleid, dest[], size = sizeof dest)')
        self.assertIn('if (RW_IsVehicleOwner(playerid, vehicleid)) {', label)
        self.assertIn('if (RW_MechOnDuty(playerid)) { format(dest, size, "Service: repair, refuel, tow"); return 1; }', label)
        self.assertIn('if (RW_PickItem(playerid) >= 0) { format(dest, size, "Break in with a lockpick"); return 1; }', label)

    def test_clicks_reach_the_right_menu(self):
        self.assertIn('else if (mode == UI_TRUNK) {', INTER)
        self.assertIn('return RW_TrunkClick(playerid, row);', INTER)
        self.assertIn('return RW_PickClick(playerid, row, false);', INTER)
        self.assertIn('return RW_ServiceClick(playerid, row);', INTER)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_TRUNK) return RW_TrunkCefClick(playerid, id);', CEFUI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_LOCKPICK) return RW_PickCefClick(playerid, id);', CEFUI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_SERVICE) return RW_ServiceCefClick(playerid, id);', CEFUI)

    def test_commands_exist_in_both_faces(self):
        for cmd, stock in (('fuel', 'RW_FuelCommand'), ('refuel', 'RW_RefuelCommand'), ('trunk', 'RW_TrunkCommand'),
                           ('repair', 'RW_RepairCommand'), ('tow', 'RW_TowCommand'), ('tune', 'RW_TuneCommand'),
                           ('impound', 'RW_ImpoundList'), ('unimpound', 'RW_UnimpoundCommand')):
            self.assertIn('if (!strcmp(cmd, "%s", true)) return %s(playerid' % (cmd, stock), CMDS)
        self.assertIn('Транспорт: /fuel, /refuel, /trunk, /tune, /repair, /tow, /impound, /unimpound; Y на машине.', CMDS)

    def test_every_menu_has_a_text_draw_twin(self):
        for cef, td in (('RW_CefTrunk', 'RW_TrunkPanel'), ('RW_CefPick', 'RW_PickPanel'), ('RW_CefService', 'RW_ServicePanel')):
            self.assertIn('stock %s(playerid)' % cef, TRANS)
            self.assertIn('stock %s(playerid)' % td, TRANS)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefTrunk(playerid);', TRANS)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefPick(playerid);', TRANS)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefService(playerid);', TRANS)

    def test_no_cyrillic_reaches_the_player(self):
        """Comments are Russian, every string the player can read is English."""
        code = re.sub(r'/\*[\s\S]*?\*/', '', TRANS)
        for line in code.splitlines():
            line = line.split('//', 1)[0]
            self.assertNotRegex(line, r'[А-Яа-я]')


if __name__ == '__main__':
    unittest.main()
