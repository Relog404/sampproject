"""Ambulance: defibrillator, adrenaline, medkit, stretcher and 911 calls.

The service lives next to the police: the same `officers` table keeps the
faction, and the tablet is shared, but the actions are different. SQL from
the Pawn sources is executed here against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

EMS = pawn('ems')
POLICE = pawn('police')
MED = pawn('medical')
PHONE = pawn('phone')
CEFUI = pawn('cefui')
CORE = pawn('core')
INV = pawn('inventory')
UI = pawn('ui')
INTER = pawn('interaction')
CMDS = pawn('commands')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class EmsTableTest(unittest.TestCase):
    """The call log, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql(EMS, 'CREATE TABLE IF NOT EXISTS ems_calls '))
        self.db.execute("CREATE TABLE accounts (id INT, name VARCHAR(32))")
        self.db.execute("INSERT INTO accounts VALUES(7,'Ivan')")

    def tearDown(self):
        self.db.close()

    def test_calls_keep_the_place_and_the_reason(self):
        self.assertEqual([row[1] for row in self.db.execute('PRAGMA table_info(ems_calls)')],
                         ['id', 'player_id', 'reason', 'x', 'y', 'z', 'stamp', 'taken', 'medic'])
        self.db.execute(sql(EMS, 'INSERT INTO ems_calls(') % (7, 3, 1200.5, -1300.25, 15.0, 1000))
        row = self.db.execute('SELECT player_id,reason,x,y,z,taken,medic FROM ems_calls WHERE id=1').fetchone()
        self.assertEqual(row, (7, 3, 1200.5, -1300.25, 15.0, 0, ''))

    def test_a_taken_call_keeps_the_medic(self):
        self.db.execute(sql(EMS, 'INSERT INTO ems_calls(') % (7, 3, 1200.5, -1300.25, 15.0, 1000))
        self.db.execute(sql(EMS, "UPDATE ems_calls SET taken=%d,medic='%s' WHERE id=%d") % (9, 'Petr', 1))
        row = self.db.execute('SELECT taken,medic FROM ems_calls WHERE id=1').fetchone()
        self.assertEqual(row, (9, 'Petr'))

    def test_the_call_is_closed_when_the_patient_is_up(self):
        self.db.execute(sql(EMS, 'INSERT INTO ems_calls(') % (7, 3, 1200.5, -1300.25, 15.0, 1000))
        self.db.execute(sql(EMS, 'DELETE FROM ems_calls WHERE id=%d') % 1)
        self.assertEqual(self.db.execute('SELECT id FROM ems_calls').fetchall(), [])

    def test_the_tablet_lists_calls_with_the_caller(self):
        for reason in (3, 0):
            self.db.execute(sql(EMS, 'INSERT INTO ems_calls(') % (7, reason, 1200.5, -1300.25, 15.0, 1000))
        rows = self.db.execute(sql(EMS, 'SELECT c.id,c.reason,c.stamp,c.taken,a.name AS who FROM ems_calls c LEFT JOIN accounts a ON a.id=c.player_id ORDER BY c.id DESC LIMIT %d OFFSET %d') % (6, 0)).fetchall()
        self.assertEqual(rows, [(2, 0, 1000, 0, 'Ivan'), (1, 3, 1000, 0, 'Ivan')])

    def test_the_call_id_is_read_back_without_last_insert_rowid(self):
        call = block(EMS, 'stock RW_EmsCall(playerid, reason, Float:x, Float:y, Float:z)')
        self.assertNotIn('db_last_insert_rowid', call)      # её нет в a_sampdb
        self.assertIn('SELECT MAX(id) AS last FROM ems_calls WHERE player_id=%d', call)


class EmsItemTest(unittest.TestCase):
    """The defibrillator and the medkit are inventory items."""

    def test_item_types_are_registered(self):
        self.assertRegex(CORE, r'#define ITEM_DEFIB\s+14')
        self.assertRegex(CORE, r'#define ITEM_MEDKIT\s+15')
        self.assertRegex(CORE, r'#define FACTION_EMS\s+3')
        self.assertRegex(INV, r'#define INV_TYPE_MAX (15|1[5-9]|20)')

    def test_items_have_a_model_and_a_weight(self):
        model = block(INV, 'stock RW_InvModel(type, data = 0)')
        grams = block(INV, 'stock RW_InvGrams(type, data = 0, amount = 1)')
        self.assertIn('if (type == ITEM_DEFIB) return 1580;', model)
        self.assertIn('if (type == ITEM_MEDKIT) return 11738;', model)
        self.assertIn('if (type == ITEM_DEFIB) return 1500;', grams)
        self.assertIn('if (type == ITEM_MEDKIT) return 800;', grams)

    def test_giveitem_knows_the_words(self):
        words = block(INV, 'stock RW_InvItemWord(const word[])')
        for word in ('defib', 'defibrillator', 'medkit'):
            self.assertIn('"%s"' % word, words)

    def test_charges_are_validated(self):
        grant = block(INV, 'stock RW_InvGrant(playerid, type, data = 0, amount = 1)')
        self.assertIn('if (type==ITEM_DEFIB && (data<0 || data>=PHONE_TARIFFS || amount<1 || amount>EMS_DEFIB_CHARGES)) return 0;', grant)
        self.assertIn('if (type==ITEM_MEDKIT && (data<0 || data>=PHONE_TARIFFS || amount!=1)) return 0;', grant)
        self.assertIn('#define EMS_DEFIB_CHARGES           3', CORE)

    def test_giveitem_grants_a_charged_defibrillator(self):
        give = block(INV, 'stock RW_InvGiveCommand(playerid, const params[])', '\nstock ')
        self.assertIn('else if (type == ITEM_DEFIB) amount = EMS_DEFIB_CHARGES;', give)
        self.assertIn('else amount = 1;', give)

    def test_the_card_offers_use(self):
        # подпись кнопки и вес показывает карточка в браузере
        label = block(INV, 'stock RW_InvUseLabel(')
        self.assertIn('"Оживить"', label)
        self.assertIn('"Вылечить"', label)
        card = block(INV, 'stock RW_InvCefContext(playerid, slot)')
        self.assertIn('RW_InvGrams(type, data, amount)', card)
        # Главное действие вынесено в общую функцию: тексдрав-меню и карточка
        # в браузере делают одно и то же.
        use = block(INV, 'stock RW_InvUseSelected(playerid)')
        self.assertIn('if (type == ITEM_DEFIB) return RW_EmsReviveCommand(playerid, "");', use)
        self.assertIn('if (type == ITEM_MEDKIT) return RW_EmsHealCommand(playerid, "");', use)
        self.assertIn('if (action == INV_CEF_ACT_USE) { RW_UI_Close(playerid); return RW_InvUseSelected(playerid); }',
                      block(INV, 'stock RW_InvCefClick(playerid, id)'))


class EmsMechanicsTest(unittest.TestCase):
    """Revive, heal, stretcher, ambulance, hospital."""

    def test_the_defibrillator_takes_ten_seconds(self):
        self.assertIn('#define EMS_REVIVE_MS       10000', EMS)
        self.assertIn('#define EMS_DEFIB_HP        60.0', EMS)
        revive = block(EMS, 'stock RW_EmsRevive(playerid, target)')
        self.assertIn('new slot = RW_EmsItem(playerid, ITEM_DEFIB), ms = EMS_REVIVE_MS, tool = ITEM_DEFIB;', revive)
        self.assertIn('SetTimerEx("RW_EmsReviveDone", ms, false, "ii", playerid, target);', revive)
        self.assertIn('RW_EmsWear(playerid, slot, 1, "The defibrillator spent its last charge and broke.");', revive)
        done = block(EMS, 'public RW_EmsReviveDone(playerid, target)', '\n// ')
        self.assertIn('new Float:hp = (gEmsTool[playerid] == ITEM_DEFIB) ? EMS_DEFIB_HP : REVIVE_HP;', done)
        self.assertIn('RW_MedClearDown(target);', done)
        self.assertIn('RW_EmsCallClose(target);', done)

    def test_adrenaline_falls_back_to_the_syringe_and_shakes(self):
        revive = block(EMS, 'stock RW_EmsRevive(playerid, target)')
        self.assertIn('if (slot < 0) { slot = RW_EmsItem(playerid, ITEM_SYRINGE); ms = MED_USE_MS; tool = ITEM_SYRINGE; }', revive)
        self.assertIn('RW_MedConsume(playerid, slot)', revive)
        self.assertIn('#define EMS_SHAKE_MS        10000', EMS)
        shake = block(EMS, 'stock RW_EmsShakeStart(playerid)')
        self.assertIn('SetPlayerDrunkLevel(playerid, EMS_SHAKE_LEVEL);', shake)
        self.assertIn('SetTimerEx("RW_EmsShakeDone", EMS_SHAKE_MS, false, "ii", playerid, gEmsShakeStamp[playerid]);', shake)
        self.assertIn('SetPlayerDrunkLevel(playerid, 0);', block(EMS, 'public RW_EmsShakeDone(playerid, stamp)'))
        # и тот же эффект у гражданского шприца
        self.assertIn('RW_EmsShakeStart(target);', MED)

    def test_the_medkit_heals_to_full(self):
        self.assertIn('#define EMS_HEAL_MS         3000', EMS)
        heal = block(EMS, 'stock RW_EmsHeal(playerid, target)')
        self.assertIn('if (slot < 0) return RW_Error(playerid, "Take a medkit in the inventory.");', heal)
        self.assertIn('SetTimerEx("RW_EmsHealDone", EMS_HEAL_MS, false, "ii", playerid, target);', heal)
        self.assertIn('SetPlayerHealth(target, EMS_MEDKIT_HP);', block(EMS, 'public RW_EmsHealDone(playerid, target)'))

    def test_the_stretcher_belongs_to_the_service(self):
        carry = block(EMS, 'stock RW_EmsCarry(playerid, target)')
        self.assertIn('if (!RW_EmsOnDuty(playerid)) return RW_Error(playerid, "Only the ambulance service on duty can use a stretcher.");', carry)
        self.assertIn('gEmsCarry[playerid] = target;', carry)
        self.assertIn('gEmsCarrier[target] = playerid;', carry)

    def test_loading_needs_an_ambulance(self):
        self.assertIn('#define EMS_AMBULANCE       416', EMS)
        load = block(EMS, 'stock RW_EmsLoadIn(playerid)')
        self.assertIn('new vehicle = RW_EmsAmbulance(playerid);', load)
        self.assertIn('if (vehicle < 0) return RW_Error(playerid, "Bring the patient to an ambulance (model 416).");', load)
        self.assertIn('PutPlayerInVehicle(target, vehicle, seat);', load)
        self.assertIn('if (GetPlayerVehicleID(target) != vehicle) continue;', load)
        near = block(EMS, 'stock RW_EmsAmbulance(playerid)', '\n// ')
        self.assertIn('if (GetVehicleModel(v) != EMS_AMBULANCE) continue;', near)
        self.assertIn('GetVehiclePos(v, vx, vy, vz);', near)

    def test_a_carried_patient_is_kept_alive_and_follows(self):
        pulse = block(EMS, 'public RW_EmsPulse()', '\n    return 1;')
        self.assertIn('SetPlayerPos(patient, x, y, z);', pulse)
        self.assertIn('ApplyAnimation(patient, "CRACK", "crckdeth2"', pulse)
        self.assertIn('gDownEnd[patient] = GetTickCount() + DOWN_MS;', pulse)
        self.assertIn('if (!IsPlayerConnected(patient) || !gDowned[patient] || !RW_EmsNear(p, patient, 12.0))', pulse)

    def test_the_ambulance_holds_the_bleeding_too(self):
        pulse = block(EMS, 'public RW_EmsPulse()', '\n    return 1;')
        self.assertIn('if (vehicle && GetVehicleModel(vehicle) == EMS_AMBULANCE) gDownEnd[p] = GetTickCount() + DOWN_MS;', pulse)

    def test_closing_a_call_only_clears_its_own_marker(self):
        close = block(EMS, 'stock RW_EmsCallClose(playerid)')
        self.assertIn('SELECT taken FROM ems_calls WHERE id=%d', close)
        self.assertIn('if (taken > 0) for (new i;', close)
        self.assertIn('RemovePlayerMapIcon(i, EMS_CALL_ICON);', close)

    def test_the_call_log_needs_a_reason_to_read(self):
        log = block(EMS, 'stock RW_EmsCallsCommand(playerid)')
        self.assertIn('if (!RW_PoliceOnDuty(playerid) && !RW_EmsOnDuty(playerid) && !gEmsCall[playerid])', log)

    def test_the_hospital_admission_gives_full_health(self):
        self.assertIn('#define HOSPITAL_RANGE              40.0', CORE)
        admit = block(EMS, 'stock RW_EmsAdmit(playerid)')
        self.assertIn('if (!RW_EmsAtHospital(playerid)) return RW_Error(playerid, "Bring the patient to the hospital.");', admit)
        self.assertIn('RemovePlayerFromVehicle(target);', admit)
        self.assertIn('SetPlayerHealth(target, 100.0);', admit)
        self.assertIn('RW_EmsCallClose(target);', admit)
        at = block(EMS, 'stock bool:RW_EmsAtHospital(playerid)')
        self.assertIn('IsPlayerInRangeOfPoint(playerid, HOSPITAL_RANGE, HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z)', at)


class EmsCallTest(unittest.TestCase):
    """911 goes to the service that fits the reason."""

    def test_the_reason_chooses_the_service(self):
        routing = block(EMS, 'stock bool:RW_EmsWants911(playerid, reason)', '\nstock ')
        self.assertIn('if (RW_PoliceOnDuty(playerid)) return (reason == 0 || reason == 1 || reason == 2 || reason == 5)', routing)
        self.assertIn('if (RW_EmsOnDuty(playerid)) return (reason == 3 || reason == 4 || reason == 5)', routing)

    def test_a_911_call_is_written_to_the_log(self):
        emergency = block(PHONE, 'stock RW_Phone911(playerid, reason)')
        self.assertIn('RW_EmsCall(playerid, reason, x, y, z);', emergency)
        self.assertIn('if (!pass && !RW_EmsWants911(i, reason)) continue;', emergency)
        self.assertIn('for (pass = 0; pass < 2 && !sent; pass++)', emergency)

    def test_the_downed_menu_calls_the_ambulance_with_injured(self):
        # вызов один для тексдрав-меню и для страницы раненого
        call = block(MED, 'stock RW_MedCall(playerid)')
        self.assertIn('RW_EmsCall(playerid, 3, gDownX[playerid], gDownY[playerid], gDownZ[playerid])', call)
        # обе кнопки вызывают те же функции: тексдрав-меню удалено
        self.assertNotIn('RW_MedClick', MED)
        self.assertNotIn('gMedTD', MED)
        # страница: две кнопки и таймер кровотечения
        screen = block(MED, 'stock RW_MedCef(playerid)')
        self.assertIn('RW_UI_Panel(playerid, UI_DOWNED, "Вы ранены", subtitle);', screen)
        self.assertIn('RW_UI_Row(playerid, 0, "Вызвать скорую");', screen)
        self.assertIn('RW_UI_Row(playerid, 1, "Подождать ещё 7 минут");', screen)
        self.assertIn('Кровотечение: %s', screen)

    def test_taking_a_call_marks_it_on_the_map(self):
        accept = block(EMS, 'stock RW_EmsCallAccept(playerid, callid)')
        self.assertIn('SetPlayerMapIcon(playerid, EMS_CALL_ICON, x, y, z, 0, 0xFF6B6BFF, MAPICON_GLOBAL);', accept)
        self.assertIn('UPDATE ems_calls SET taken=%d,medic=', accept)
        self.assertIn('RW_EmsShakeStart', EMS)
        self.assertIn('#define EMS_CALL_ICON       96', EMS)

    def test_the_tablet_tab_is_shared(self):
        self.assertIn('#define MDT_CALLS                   (4)', CORE)
        self.assertIn('} else if (gMdtTab[playerid] == MDT_CALLS) {\n        return RW_EmsMdtList(playerid, limit);', POLICE)
        self.assertIn('if (gMdtTab[playerid] == MDT_CALLS) return RW_EmsMdtClick(playerid, id);', POLICE)
        self.assertIn('RW_CefRow(playerid, 4, "Calls", "911", "alert");', POLICE)
        self.assertIn('if (RW_PoliceOnDuty(playerid)) RW_CefRow(playerid, 5,', POLICE)

    def test_the_medic_opens_the_tablet_with_a_medkit(self):
        open_ = block(POLICE, 'stock RW_MdtOpen(playerid)')
        self.assertIn('new bool:police = RW_PoliceOnDuty(playerid), bool:medic = RW_EmsOnDuty(playerid);', open_)
        self.assertIn('if (medic && RW_EmsItem(playerid, ITEM_MEDKIT) < 0)', open_)
        self.assertIn('gMdtTab[playerid] = police ? MDT_CITIZENS : MDT_CALLS;', open_)


class EmsInterfaceTest(unittest.TestCase):
    """Y menu, tablet, commands."""

    def test_y_reaches_the_patient(self):
        menu = block(INTER, 'stock RW_PlayerMenu(playerid, target)', '\nstock ')
        self.assertIn('if (gDowned[target] || (RW_EmsOnDuty(playerid) && RW_EmsItem(playerid, ITEM_MEDKIT) >= 0)) return RW_EmsMenuOpen(playerid, target);', menu)
        self_menu = block(INTER, 'stock RW_SelfMenu(playerid)', '\nstock ')
        self.assertIn('if (RW_EmsOnDuty(playerid) && RW_EmsItem(playerid, ITEM_MEDKIT) >= 0) return RW_MdtOpen(playerid);', self_menu)

    def test_both_interfaces_share_one_click_handler(self):
        click = block(INTER, 'stock RW_UI_Click(playerid, row)', '\nstock ')
        self.assertIn('} else if (mode == UI_EMS) {', click)
        self.assertIn('return RW_EmsClick(playerid, row);', click)
        self.assertIn('#define UI_EMS 15', UI)
        self.assertIn('#define CEF_MODE_EMS                (5)', CORE)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_EMS) return RW_EmsCefClick(playerid, id);', CEFUI)
        self.assertIn('stock RW_EmsCefClick(playerid, row) { return RW_EmsAction(playerid, row, true); }', EMS)

    def test_commands_are_wired(self):
        chain = block(CMDS, 'stock RW_CommandProcess(playerid, cmdtext[])', '\nstock ')
        for line in ('if (!strcmp(cmd, "heal", true)) return RW_EmsHealCommand(playerid, params);',
                     'if (!strcmp(cmd, "revive", true)) return RW_EmsReviveCommand(playerid, params);',
                     'if (!strcmp(cmd, "carry", true)) return RW_EmsCarryCommand(playerid, params);',
                     'if (!strcmp(cmd, "drop", true)) return RW_EmsDrop(playerid);',
                     'if (!strcmp(cmd, "load", true)) return RW_EmsLoadIn(playerid);',
                     'if (!strcmp(cmd, "admit", true)) return RW_EmsAdmit(playerid);',
                     'if (!strcmp(cmd, "ems", true)) return RW_EmsListCommand(playerid);',
                     'if (!strcmp(cmd, "calls", true)) return RW_EmsCallsCommand(playerid);',
                     'if (!strcmp(cmd, "take", true)) return RW_EmsTakeCommand(playerid, params);'):
            self.assertIn(line, chain)
        self.assertIn('/revive, /heal, /carry', block(CMDS, 'stock RW_CommandHelp(playerid)', '\nstock '))

    def test_the_module_is_loaded_and_ticked(self):
        self.assertIn('#include <railway/ems>', MAIN)
        self.assertIn('RW_EmsSchema()', MAIN)
        self.assertIn('SetTimer("RW_EmsPulse", 1000, true);', MAIN)
        self.assertIn('RW_EmsReset(playerid);', MAIN.split('public OnPlayerConnect')[1][:1200])
        self.assertIn('RW_EmsCallClose(playerid);', MAIN.split('public OnPlayerDisconnect')[1][:1200])


if __name__ == '__main__':
    unittest.main()
