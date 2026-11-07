/*  ============================================================================
 *  railway/ems.inc
 *  Скорая помощь: дефибриллятор, адреналин, аптечка, носилки и вызовы 911.
 *
 *  Фракция EMS (FACTION_EMS) хранится в той же таблице officers, что и полиция:
 *  служба, ранг и смена читаются police.inc, а действия делятся по номеру
 *  фракции. Вызов приходит той службе, которой он по адресу: полиция берёт
 *  robbery/shooting/hostages, скорая — injured/fire, «other» видят обе.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_ems_included
    #endinput
#endif
#define _railway_ems_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/phone>
#include <railway/police>

#define EMS_REVIVE_MS       10000      /* дефибриллятор: 10 секунд          */
#define EMS_HEAL_MS         3000       /* аптечка: 3 секунды                */
#define EMS_DEFIB_HP        60.0
#define EMS_MEDKIT_HP       100.0
#define EMS_RANGE           3.5        /* дистанция действий Y              */
#define EMS_PATIENT_RANGE   4.0
#define EMS_CARRY_RANGE     6.0        /* до машины скорой                  */
#define EMS_AMBULANCE       416        /* модель Ambulance                  */
#define EMS_SHAKE_MS        10000      /* тряска после адреналина           */
#define EMS_SHAKE_LEVEL     4000
#define EMS_CALL_ICON       96         /* метка вызова на карте             */

/* экраны меню медика */
#define EMS_SCR_ACTIONS     0

new gEmsCarry[MAX_PLAYERS];            /* кого несёт этот медик (-1 — никого) */
new gEmsCarrier[MAX_PLAYERS];          /* кто несёт этого пациента (-1)      */
new gEmsBusy[MAX_PLAYERS];             /* тик начала процедуры               */
new gEmsShake[MAX_PLAYERS];            /* тик конца тряски                   */
new gEmsShakeStamp[MAX_PLAYERS];
new gEmsTick[MAX_PLAYERS];             /* защита от спама                    */
new gEmsAnim[MAX_PLAYERS];             /* счётчик анимации носилок           */
new gEmsTool[MAX_PLAYERS];             /* чем работаем: дефибриллятор или шприц */
new gEmsScreen[MAX_PLAYERS];
new gEmsTarget[MAX_PLAYERS];
new gEmsCall[MAX_PLAYERS];             /* номер последнего вызова игрока     */

forward RW_EmsPulse();
forward RW_EmsReviveDone(playerid, target);
forward RW_EmsHealDone(playerid, target);
forward RW_EmsShakeDone(playerid, stamp);

/*  ----------------------------------- база -------------------------------- */

stock RW_EmsSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS ems_calls (id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INT, reason INT, x REAL, y REAL, z REAL, stamp INT, taken INT, medic VARCHAR(32))")) return 0;
    return 1;
}
stock RW_EmsReset(playerid)
{
    if (gEmsCarry[playerid] >= 0) gEmsCarrier[gEmsCarry[playerid]] = -1;
    if (gEmsCarrier[playerid] >= 0) gEmsCarry[gEmsCarrier[playerid]] = -1;
    gEmsCarry[playerid] = -1;
    gEmsCarrier[playerid] = -1;
    gEmsBusy[playerid] = 0;
    gEmsShake[playerid] = 0;
    gEmsTick[playerid] = 0;
    gEmsScreen[playerid] = EMS_SCR_ACTIONS;
    gEmsTarget[playerid] = INVALID_PLAYER_ID;
    gEmsCall[playerid] = 0;
    return 1;
}

/*  --------------------------------- помощники ----------------------------- */

stock bool:RW_EmsOnDuty(playerid)
{
    return gLogged[playerid] && gDuty[playerid] && gFaction[playerid] == FACTION_EMS;
}
// Кому вообще нужен этот вызов: robbery/shooting/hostages — полиции,
// injured/fire — скорой, other — обеим службам.
stock bool:RW_EmsWants911(playerid, reason)
{
    if (RW_PoliceOnDuty(playerid)) return (reason == 0 || reason == 1 || reason == 2 || reason == 5) ? true : false;
    if (RW_EmsOnDuty(playerid)) return (reason == 3 || reason == 4 || reason == 5) ? true : false;
    return false;
}
stock RW_EmsItem(playerid, type)
{
    if (!gLogged[playerid]) return -1;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == type) return i;
    return -1;
}
stock bool:RW_EmsNear(playerid, target, Float:range = EMS_RANGE)
{
    if (!IsPlayerConnected(target) || target == playerid) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return IsPlayerInRangeOfPoint(target, range, x, y, z) ? true : false;
}
stock bool:RW_EmsAtHospital(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, HOSPITAL_RANGE, HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z) ? true : false;
}
// Скорая поблизости: ambulance (416) в радиусе EMS_CARRY_RANGE.
stock RW_EmsAmbulance(playerid)
{
    new Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz;
    GetPlayerPos(playerid, x, y, z);
    for (new v = 1; v < MAX_VEHICLES; v++) {
        if (GetVehicleModel(v) != EMS_AMBULANCE) continue;
        GetVehiclePos(v, vx, vy, vz);
        if (VectorSize(vx - x, vy - y, vz - z) > EMS_CARRY_RANGE) continue;
        return v;
    }
    return -1;
}
// Дефибриллятор многоразовый: заряд тратится, пустой прибор исчезает.
stock RW_EmsWear(playerid, slot, loss, const broken[])
{
    if (slot < 0 || gInv[playerid][slot][iiUid] <= 0) return 0;
    new left = gInv[playerid][slot][iiAmount] - loss;
    new query[220];
    if (left <= 0) {
        format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        if (!RW_InvWrite(query)) return 0;
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
        RW_Error(playerid, broken);
    } else {
        format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d AND owner=%d AND slot=%d", left, gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        if (!RW_InvWrite(query)) return 0;
        gInv[playerid][slot][iiAmount] = left;
    }
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return 1;
}

/*  --------------------------------- вызовы -------------------------------- */
// Вызов создаёт и полиция, и скорая: по номеру причины решает RW_EmsWants911.
stock RW_EmsCall(playerid, reason, Float:x, Float:y, Float:z)
{
    if (gAccountId[playerid] <= 0) return 0;
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], query[320];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT INTO ems_calls(player_id,reason,x,y,z,stamp,taken,medic) VALUES(%d,%d,%f,%f,%f,%d,0,'')", gAccountId[playerid], reason, x, y, z, gettime());
    RW_DB_Exec(query);
    /* a_sampdb не отдаёт last_insert_rowid: берём последнюю строку игрока */
    format(query, sizeof query, "SELECT MAX(id) AS last FROM ems_calls WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) gEmsCall[playerid] = db_get_field_assoc_int(r, "last");
        db_free_result(r);
    }
    return gEmsCall[playerid];
}
// Медик (или полицейский) берёт вызов: метка на карте и весточка заявителю.
stock RW_EmsCallAccept(playerid, callid)
{
    if (callid <= 0) return RW_Error(playerid, "That call is already closed.");
    if (!RW_EmsOnDuty(playerid) && !RW_PoliceOnDuty(playerid)) return RW_Error(playerid, "Only the services on duty can take calls.");
    new query[220];
    format(query, sizeof query, "SELECT player_id,reason,x,y,z,taken FROM ems_calls WHERE id=%d", callid);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0 || db_num_rows(r) == 0) {
        if (r != DBResult:0) db_free_result(r);
        return RW_Error(playerid, "That call is already closed.");
    }
    new who = db_get_field_assoc_int(r, "player_id"), reason = db_get_field_assoc_int(r, "reason"), taken = db_get_field_assoc_int(r, "taken");
    new Float:x = db_get_field_assoc_float(r, "x"), Float:y = db_get_field_assoc_float(r, "y"), Float:z = db_get_field_assoc_float(r, "z");
    db_free_result(r);
    if (taken && taken != gAccountId[playerid]) return RW_Info(playerid, "Somebody has already taken that call.");
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], why[24];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    RW_Phone911Reason(reason, why, sizeof why);
    format(query, sizeof query, "UPDATE ems_calls SET taken=%d,medic='%s' WHERE id=%d", gAccountId[playerid], safe, callid);
    RW_DB_Exec(query);
    SetPlayerMapIcon(playerid, EMS_CALL_ICON, x, y, z, 0, 0xFF6B6BFF, MAPICON_GLOBAL);
    new message[160];
    format(message, sizeof message, "Call #%d taken: %s at the marker. /untrack removes it.", callid, why);
    RW_Ok(playerid, message);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != who) continue;
        RW_GetNick(playerid, nick);
        format(message, sizeof message, "%s accepted your 911 call. Help is on the way.", nick);
        RW_Ok(i, message);
    }
    return 1;
}
// Вызов закрывается, когда заявителя подняли или он вышел из игры.
stock RW_EmsCallClose(playerid)
{
    new id = gEmsCall[playerid];
    gEmsCall[playerid] = 0;
    if (id <= 0) return 0;
    new query[160], taken;
    format(query, sizeof query, "SELECT taken FROM ems_calls WHERE id=%d", id);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) taken = db_get_field_assoc_int(r, "taken");
        db_free_result(r);
    }
    format(query, sizeof query, "DELETE FROM ems_calls WHERE id=%d", id);
    RW_DB_Exec(query);
    if (taken > 0) for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != taken) continue;
        RemovePlayerMapIcon(i, EMS_CALL_ICON);
    }
    return 1;
}

/*  ------------------------------- лечение --------------------------------- */
/*  Дефибриллятор поднимает за 10 секунд и даёт 60 HP, адреналин — быстро,
 *  но всего 30 HP и 10 секунд тряски. Аптечка лечит до 100 HP.               */

stock RW_EmsToolName(type, dest[], size = sizeof dest)
{
    switch (type) {
        case ITEM_DEFIB: format(dest, size, "defibrillator");
        case ITEM_MEDKIT: format(dest, size, "medkit");
        default: format(dest, size, "adrenaline");
    }
    return 1;
}
stock RW_EmsBusy(playerid, target, const what[])
{
    gEmsBusy[playerid] = GetTickCount();
    RW_UI_Close(playerid);
    TogglePlayerControllable(playerid, false);
    ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 1, 1, 1, 1, 0);
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(target, nick);
    format(message, sizeof message, "Working on %s: %s...", nick, what);
    return RW_Info(playerid, message);
}
stock RW_EmsFree(playerid)
{
    gEmsBusy[playerid] = 0;
    ClearAnimations(playerid, 1);
    TogglePlayerControllable(playerid, true);
    return 1;
}
stock RW_EmsRevive(playerid, target)
{
    if (target < 0 || !IsPlayerConnected(target) || !gDowned[target]) return RW_Error(playerid, "There is nobody downed here.");
    if (!RW_EmsNear(playerid, target, EMS_PATIENT_RANGE)) return RW_Error(playerid, "Stand closer to the patient.");
    if (gEmsBusy[playerid]) return RW_Info(playerid, "You are already busy with a patient.");
    new slot = RW_EmsItem(playerid, ITEM_DEFIB), ms = EMS_REVIVE_MS, tool = ITEM_DEFIB;
    if (slot < 0) { slot = RW_EmsItem(playerid, ITEM_SYRINGE); ms = MED_USE_MS; tool = ITEM_SYRINGE; }
    if (slot < 0) return RW_Error(playerid, "Take a defibrillator or an adrenaline syringe.");
    if (tool == ITEM_DEFIB) RW_EmsWear(playerid, slot, 1, "The defibrillator spent its last charge and broke.");
    else if (!RW_MedConsume(playerid, slot)) return RW_Error(playerid, "Could not use the syringe.");
    gEmsTool[playerid] = tool;
    SetTimerEx("RW_EmsReviveDone", ms, false, "ii", playerid, target);
    new label[24];
    RW_EmsToolName(tool, label, sizeof label);
    return RW_EmsBusy(playerid, target, label);
}
public RW_EmsReviveDone(playerid, target)
{
    RW_EmsFree(playerid);
    if (!IsPlayerConnected(target) || !gLogged[target] || !gDowned[target]) return RW_Error(playerid, "The patient is gone.");
    if (!RW_EmsNear(playerid, target, EMS_PATIENT_RANGE)) return RW_Error(playerid, "The patient was moved away.");
    new Float:hp = (gEmsTool[playerid] == ITEM_DEFIB) ? EMS_DEFIB_HP : REVIVE_HP;
    RW_MedClearDown(target);
    SetPlayerHealth(target, hp);
    if (gEmsCarrier[target] >= 0) RW_EmsDrop(gEmsCarrier[target]);
    RW_EmsCallClose(target);
    if (gEmsTool[playerid] != ITEM_DEFIB) {
        RW_EmsShakeStart(target);
        RW_Info(target, "Adrenaline kicks in: your hands shake for ten seconds.");
    }
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is back on their feet (%.0f HP).", nick, hp);
    RW_Ok(playerid, message);
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s brought you round.", nick);
    return RW_Ok(target, message);
}
// Аптечка: себе или другому, до 100 HP.
stock RW_EmsHeal(playerid, target)
{
    if (target < 0 || !IsPlayerConnected(target)) return RW_Error(playerid, "Stand closer to the player.");
    if (gDowned[target]) return RW_Error(playerid, "He is downed: revive him first.");
    if (target != playerid && !RW_EmsNear(playerid, target, EMS_PATIENT_RANGE)) return RW_Error(playerid, "Stand closer to the player.");
    if (gEmsBusy[playerid]) return RW_Info(playerid, "You are already busy with a patient.");
    new slot = RW_EmsItem(playerid, ITEM_MEDKIT);
    if (slot < 0) return RW_Error(playerid, "Take a medkit in the inventory.");
    new Float:hp;
    GetPlayerHealth(target, hp);
    if (hp >= EMS_MEDKIT_HP) return RW_Info(playerid, "He is not hurt.");
    if (!RW_MedConsume(playerid, slot)) return RW_Error(playerid, "Could not use the medkit.");
    SetTimerEx("RW_EmsHealDone", EMS_HEAL_MS, false, "ii", playerid, target);
    return RW_EmsBusy(playerid, target, "medkit");
}
public RW_EmsHealDone(playerid, target)
{
    RW_EmsFree(playerid);
    if (!IsPlayerConnected(target) || !gLogged[target] || gDowned[target]) return RW_Error(playerid, "The patient is gone.");
    if (target != playerid && !RW_EmsNear(playerid, target, EMS_PATIENT_RANGE)) return RW_Error(playerid, "The patient was moved away.");
    SetPlayerHealth(target, EMS_MEDKIT_HP);
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is patched up: 100 HP.", nick);
    RW_Ok(playerid, message);
    if (target != playerid) {
        RW_GetNick(playerid, nick);
        format(message, sizeof message, "%s treated your wounds.", nick);
        RW_Ok(target, message);
    }
    return 1;
}
// Тряска после адреналина: 10 секунд шатает, потом отпускает.
stock RW_EmsShakeStart(playerid)
{
    gEmsShakeStamp[playerid]++;
    gEmsShake[playerid] = GetTickCount() + EMS_SHAKE_MS;
    SetPlayerDrunkLevel(playerid, EMS_SHAKE_LEVEL);
    SetTimerEx("RW_EmsShakeDone", EMS_SHAKE_MS, false, "ii", playerid, gEmsShakeStamp[playerid]);
    return 1;
}
public RW_EmsShakeDone(playerid, stamp)
{
    if (stamp != gEmsShakeStamp[playerid]) return 1;
    gEmsShake[playerid] = 0;
    SetPlayerDrunkLevel(playerid, 0);
    return 1;
}

/*  --------------------------------- носилки ------------------------------- */
/*  Медик на службе несёт пациента на себе, грузит в скорую (416) и сдаёт
 *  в больнице: там он поднимается с полным здоровьем.                        */

stock RW_EmsCarry(playerid, target)
{
    if (!RW_EmsOnDuty(playerid)) return RW_Error(playerid, "Only the ambulance service on duty can use a stretcher.");
    if (target < 0 || !IsPlayerConnected(target) || !gDowned[target]) return RW_Error(playerid, "There is nobody downed here.");
    if (!RW_EmsNear(playerid, target, EMS_PATIENT_RANGE)) return RW_Error(playerid, "Stand closer to the patient.");
    if (gEmsCarry[playerid] >= 0) return RW_Error(playerid, "You are already carrying somebody.");
    if (gEmsCarrier[target] >= 0) return RW_Error(playerid, "Somebody is carrying him already.");
    if (gEmsBusy[playerid]) return RW_Info(playerid, "Finish what you are doing first.");
    gEmsCarry[playerid] = target;
    gEmsCarrier[target] = playerid;
    gEmsAnim[playerid] = 0;
    ApplyAnimation(target, "CRACK", "crckdeth2", 4.1, 1, 1, 1, 1, 0);
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s is carrying you to the ambulance.", nick);
    RW_Info(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "You are carrying %s. /load puts him in the ambulance.", nick);
    return RW_Ok(playerid, message);
}
stock RW_EmsDrop(playerid)
{
    new target = gEmsCarry[playerid];
    if (target < 0) return RW_Error(playerid, "You are not carrying anybody.");
    gEmsCarry[playerid] = -1;
    if (target >= 0) gEmsCarrier[target] = -1;
    if (IsPlayerConnected(target) && gDowned[target]) {
        ClearAnimations(target, 1);
        ApplyAnimation(target, "CRACK", "crckdeth2", 4.1, 1, 1, 1, 1, 0);
    }
    return RW_Ok(playerid, "You put the patient down.");
}
// Погрузка в скорую: сажаем пациента пассажиром.
stock RW_EmsLoadIn(playerid)
{
    new target = gEmsCarry[playerid];
    if (target < 0) return RW_Error(playerid, "You are not carrying anybody.");
    if (!RW_EmsOnDuty(playerid)) return RW_Error(playerid, "Only the ambulance service on duty can use a stretcher.");
    new vehicle = RW_EmsAmbulance(playerid);
    if (vehicle < 0) return RW_Error(playerid, "Bring the patient to an ambulance (model 416).");
    for (new seat = 1; seat <= 3; seat++) {
        ClearAnimations(target, 1);
        TogglePlayerControllable(target, true);
        PutPlayerInVehicle(target, vehicle, seat);
        TogglePlayerControllable(target, false);
        if (GetPlayerVehicleID(target) != vehicle) continue;
        gEmsCarry[playerid] = -1;
        gEmsCarrier[target] = -1;
        new nick[MAX_PLAYER_NAME], message[128];
        RW_GetNick(playerid, nick);
        format(message, sizeof message, "%s put you in the ambulance.", nick);
        RW_Info(target, message);
        RW_GetNick(target, nick);
        format(message, sizeof message, "%s is in the ambulance. Drive to the hospital, then /admit.", nick);
        return RW_Ok(playerid, message);
    }
    return RW_Error(playerid, "There is no free seat in the ambulance.");
}
// Сдача в больнице: пациент поднимается со 100 HP.
stock RW_EmsAdmit(playerid)
{
    if (!RW_EmsOnDuty(playerid)) return RW_Error(playerid, "Only the ambulance service on duty can admit patients.");
    new target = gEmsCarry[playerid];
    if (target < 0) {
        new vehicle = RW_EmsAmbulance(playerid);
        if (vehicle >= 0) for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gDowned[i] && GetPlayerVehicleID(i) == vehicle) { target = i; break; }
    }
    if (target < 0) return RW_Error(playerid, "Carry the patient here or bring him in the ambulance.");
    if (!RW_EmsAtHospital(playerid)) return RW_Error(playerid, "Bring the patient to the hospital.");
    if (!gDowned[target]) return RW_Error(playerid, "The patient is not downed anymore.");
    if (GetPlayerVehicleID(target)) RemovePlayerFromVehicle(target);
    if (gEmsCarrier[target] >= 0) RW_EmsDrop(gEmsCarrier[target]);
    RW_MedClearDown(target);
    SetPlayerHealth(target, 100.0);
    TogglePlayerControllable(target, true);
    RW_EmsCallClose(target);
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "You woke up in the hospital: %s brought you in.", nick);
    RW_Ok(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is admitted: 100 HP, the call is closed.", nick);
    RW_Ok(playerid, message);
    for (new i; i < MAX_PLAYERS; i++) if (RW_EmsOnDuty(i) && i != playerid) SendClientMessage(i, 0x7BD88FFF, message);
    return 1;
}

/*  ---------------------------------- таймер ------------------------------- */

public RW_EmsPulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gLogged[p]) continue;
        if (gEmsCarry[p] >= 0) {
            new patient = gEmsCarry[p];
            if (!IsPlayerConnected(patient) || !gDowned[patient] || !RW_EmsNear(p, patient, 12.0)) {
                RW_EmsDrop(p);
                continue;
            }
            new Float:x, Float:y, Float:z, Float:a;
            GetPlayerPos(p, x, y, z);
            GetPlayerFacingAngle(p, a);
            x += 1.4 * floatsin(-a, degrees);
            y += 1.4 * floatcos(-a, degrees);
            SetPlayerPos(patient, x, y, z);
            SetPlayerFacingAngle(patient, a + 90.0);
            if ((gEmsAnim[p] % 5) == 0) ApplyAnimation(patient, "CRACK", "crckdeth2", 4.1, 1, 1, 1, 1, 0);
            gEmsAnim[p]++;
            /* пока пациента несут, он не истекает кровью */
            gDownEnd[patient] = GetTickCount() + DOWN_MS;
        }
        /* в салоне скорой время на спасение тоже не идёт */
        if (gDowned[p]) {
            new vehicle = GetPlayerVehicleID(p);
            if (vehicle && GetVehicleModel(vehicle) == EMS_AMBULANCE) gDownEnd[p] = GetTickCount() + DOWN_MS;
        }
    }
    return 1;
}

/*  ---------------------------------- меню --------------------------------- */
/*  Одно меню на всё: браузер для тех, у кого плагин есть, нативный диалог —
 *  для остальных. Идентификаторы строк одинаковые, поэтому обработчик один. */

stock RW_EmsMenuOpen(playerid, target)
{
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target] || target == playerid) return RW_Error(playerid, "Stand closer to the player.");
    if (RW_EmsItem(playerid, ITEM_DEFIB) < 0 && RW_EmsItem(playerid, ITEM_SYRINGE) < 0 && RW_EmsItem(playerid, ITEM_MEDKIT) < 0) {
        if (!RW_EmsOnDuty(playerid)) return RW_Error(playerid, "Take a defibrillator, an adrenaline syringe or a medkit.");
    }
    gEmsTarget[playerid] = target;
    gEmsScreen[playerid] = EMS_SCR_ACTIONS;
    return RW_EmsRender(playerid);
}
stock RW_EmsRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefEmsRender(playerid);
    #endif
    return RW_EmsPanel(playerid);
}
stock RW_CefEmsRender(playerid)
{
    new target = gEmsTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target)) return RW_UI_Close(playerid);
    new nick[MAX_PLAYER_NAME], Float:hp;
    RW_GetNick(target, nick);
    GetPlayerHealth(target, hp);
    if (!RW_CefOpen(playerid, CEF_MODE_EMS, nick, gDowned[target] ? ("Patient") : ("Civilian"))) return RW_EmsPanel(playerid);
    if (gDowned[target]) {
        RW_CefRow(playerid, 0, "Revive: defibrillator or adrenaline", "", "shot");
        if (RW_EmsOnDuty(playerid)) RW_CefRow(playerid, 2, "Carry him on a stretcher", "", "users");
        if (RW_EmsOnDuty(playerid) && gEmsCarry[playerid] == target) RW_CefRow(playerid, 3, "Put him in the ambulance", "", "car");
    } else {
        RW_CefRow(playerid, 1, "Treat him with a medkit", "", "band");
    }
    if (RW_EmsOnDuty(playerid)) RW_CefRow(playerid, 4, "Admit him to the hospital", "", "map");
    RW_CefAction(playerid, 7, "Refresh", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_EmsPanel(playerid)
{
    new target = gEmsTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target)) return RW_UI_Close(playerid);
    new nick[MAX_PLAYER_NAME], Float:hp;
    RW_GetNick(target, nick);
    GetPlayerHealth(target, hp);
    RW_UI_Panel(playerid, UI_EMS, nick, gDowned[target] ? ("Patient") : ("Civilian"));
    if (gDowned[target]) {
        RW_UI_Row(playerid, 0, "Revive: defibrillator or adrenaline");
        if (RW_EmsOnDuty(playerid)) RW_UI_Row(playerid, 2, "Carry him on a stretcher");
        if (RW_EmsOnDuty(playerid) && gEmsCarry[playerid] == target) RW_UI_Row(playerid, 3, "Put him in the ambulance");
    } else {
        RW_UI_Row(playerid, 1, "Treat him with a medkit");
    }
    if (RW_EmsOnDuty(playerid)) RW_UI_Row(playerid, 4, "Admit him to the hospital");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_EmsAction(playerid, row, bool:cef)
{
    new target = gEmsTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target]) return RW_UI_Close(playerid);
    new closeRow = cef ? 8 : 7;
    if (row == closeRow) return RW_UI_Close(playerid);
    if (row == 0) { RW_EmsRevive(playerid, target); return RW_UI_Close(playerid); }
    if (row == 1) { RW_EmsHeal(playerid, target); return RW_UI_Close(playerid); }
    if (row == 2) {
        if (gEmsCarry[playerid] == target) { RW_EmsDrop(playerid); return RW_EmsRender(playerid); }
        RW_EmsCarry(playerid, target);
        return RW_UI_Close(playerid);
    }
    if (row == 3) { RW_EmsLoadIn(playerid); return RW_UI_Close(playerid); }
    if (row == 4) { RW_EmsAdmit(playerid); return RW_UI_Close(playerid); }
    return 1;
}
stock RW_EmsCefClick(playerid, row) { return RW_EmsAction(playerid, row, true); }
stock RW_EmsClick(playerid, row) { return RW_EmsAction(playerid, row, false); }

/*  ---------------------- вкладка «Вызовы» общего планшета ------------------ */
// Список открытых вызовов: строка планшета отдаётся полиции и скорой.
stock RW_EmsMdtList(playerid, limit)
{
    for (new i; i < MDT_ROWS; i++) gMdtIds[playerid][i] = 0;
    new query[260];
    format(query, sizeof query, "SELECT c.id,c.reason,c.stamp,c.taken,a.name AS who FROM ems_calls c LEFT JOIN accounts a ON a.id=c.player_id ORDER BY c.id DESC LIMIT %d OFFSET %d", limit, gMdtPage[playerid]*limit);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) { gMdtCount[playerid] = 0; return 0; }
    new rows = db_num_rows(r), index;
    for (new i; i < rows && index < limit; i++) {
        new label[80], name[MAX_PLAYER_NAME], why[24];
        gMdtIds[playerid][index] = db_get_field_assoc_int(r, "id");
        db_get_field_assoc(r, "who", name, sizeof name);
        RW_Phone911Reason(db_get_field_assoc_int(r, "reason"), why, sizeof why);
        new minutes = (gettime() - db_get_field_assoc_int(r, "stamp")) / 60;
        if (minutes < 0) minutes = 0;
        format(label, sizeof label, "%s - %s, %d min ago", name[0] ? name : ("?"), why, minutes);
        #if RAILWAY_CEF
            if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, db_get_field_assoc_int(r, "taken") ? ("taken") : ("open"), "alert", "", db_get_field_assoc_int(r, "taken") ? ("state-on") : ("state-off"));
            else
        #endif
        RW_UI_Row(playerid, index, label);
        index++;
        db_next_row(r);
    }
    db_free_result(r);
    gMdtCount[playerid] = index;
    return index;
}
stock RW_EmsMdtClick(playerid, callid)
{
    RW_EmsCallAccept(playerid, callid);
    return RW_MdtRender(playerid);
}

/*  --------------------------------- команды -------------------------------- */

// Цель команды: номер игрока, ник или ближайший, если ничего не указано.
stock RW_EmsArgTarget(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word)) {
        new target = -1, Float:best = EMS_PATIENT_RANGE*EMS_PATIENT_RANGE, Float:x, Float:y, Float:z, Float:dx, Float:dy, Float:dz;
        GetPlayerPos(playerid, x, y, z);
        for (new p; p < MAX_PLAYERS; p++) {
            if (p == playerid || !IsPlayerConnected(p) || !gLogged[p]) continue;
            GetPlayerPos(p, dx, dy, dz);
            dx -= x; dy -= y; dz -= z;
            new Float:d = dx*dx + dy*dy + dz*dz;
            if (d <= best) { best = d; target = p; }
        }
        if (target < 0) { RW_Error(playerid, "Stand closer to the player or give his id."); return -1; }
        return target;
    }
    if (!RW_IsNumeric(word)) {
        new target = RW_PoliceFind(word);
        if (target < 0) { RW_Error(playerid, "Player with that name is not online."); return -1; }
        return target;
    }
    new target = strval(word);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) {
        RW_Error(playerid, "Player with that id is not online.");
        return -1;
    }
    return target;
}
stock RW_EmsReviveCommand(playerid, const params[])
{
    new target = RW_EmsArgTarget(playerid, params);
    if (target < 0) return 1;
    return RW_EmsRevive(playerid, target);
}
stock RW_EmsHealCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word)) return RW_EmsHeal(playerid, playerid);
    new target = RW_EmsArgTarget(playerid, params);
    if (target < 0) return 1;
    return RW_EmsHeal(playerid, target);
}
stock RW_EmsCarryCommand(playerid, const params[])
{
    new target = RW_EmsArgTarget(playerid, params);
    if (target < 0) return 1;
    if (gEmsCarry[playerid] == target) return RW_EmsDrop(playerid);
    return RW_EmsCarry(playerid, target);
}
stock RW_EmsListCommand(playerid)
{
    new count, line[128];
    RW_Accent(playerid, "Ambulance on duty:");
    for (new i; i < MAX_PLAYERS; i++) {
        if (!RW_EmsOnDuty(i)) continue;
        new nick[MAX_PLAYER_NAME];
        RW_GetNick(i, nick);
        format(line, sizeof line, "%s [%d] - rank %d%s", nick, i, gRank[i], gEmsCarry[i] >= 0 ? (", carrying a patient") : (""));
        SendClientMessage(playerid, COLOR_GREY, line);
        count++;
    }
    if (!count) return RW_Info(playerid, "No ambulance is on duty right now.");
    return 1;
}
stock RW_EmsCallsCommand(playerid)
{
    if (!RW_PoliceOnDuty(playerid) && !RW_EmsOnDuty(playerid) && !gEmsCall[playerid]) return RW_Error(playerid, "Only the services and the caller read the call log.");
    new query[220];
    format(query, sizeof query, "SELECT c.id,c.reason,c.stamp,c.taken,a.name AS who FROM ems_calls c LEFT JOIN accounts a ON a.id=c.player_id ORDER BY c.id DESC LIMIT 8");
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "No open calls.");
    new rows = db_num_rows(r);
    if (!rows) { db_free_result(r); return RW_Info(playerid, "No open calls."); }
    RW_Accent(playerid, "Emergency calls:");
    for (new i; i < rows; i++) {
        new name[MAX_PLAYER_NAME], why[24], line[128];
        db_get_field_assoc(r, "who", name, sizeof name);
        RW_Phone911Reason(db_get_field_assoc_int(r, "reason"), why, sizeof why);
        new minutes = (gettime() - db_get_field_assoc_int(r, "stamp")) / 60;
        if (minutes < 0) minutes = 0;
        format(line, sizeof line, "#%d %s - %s, %d min ago%s", db_get_field_assoc_int(r, "id"), name[0] ? name : ("?"), why, minutes, db_get_field_assoc_int(r, "taken") ? (" (taken)") : (""));
        SendClientMessage(playerid, COLOR_GREY, line);
        db_next_row(r);
    }
    db_free_result(r);
    return RW_Info(playerid, "/take <id> - take a call, /mdt - the tablet.");
}
stock RW_EmsTakeCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /take <call id>. /calls lists them.");
    return RW_EmsCallAccept(playerid, strval(word));
}
