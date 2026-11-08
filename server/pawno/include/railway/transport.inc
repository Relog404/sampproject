/*  ============================================================================
 *  railway/transport.inc
 *  Транспорт: топливо, багажник, взлом отмычкой, служба механика и тюнинг.
 *
 *  Машина из своей системы (gVeh) живёт в слотe, у неё есть владелец, ключ,
 *  двери и двигатель. Здесь добавляется то, чего не было:
 *    • топливо — тратится по скорости, заправка на АЗС и механиком;
 *    • багажник — отдельный контейнер на 10 предметов с лимитом по модели;
 *    • отмычка — мини-игра «навык», успех заводит чужую машину;
 *    • механик на службе — ремонт, заправка, эвакуация на штрафстоянку;
 *    • тюнинг — деталь из инвентаря ставится в гараже тюнинга и хранит свой
 *      uid: двигатель 4 уровня даёт +20 км/ч к максимальной скорости.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_transport_included
    #endinput
#endif
#define _railway_transport_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/garage>

/* детали тюнинга: номер = data предмета */
#define TUNE_ENGINE     0
#define TUNE_GEARBOX    1
#define TUNE_BRAKES     2
#define TUNE_SUSP       3
#define TUNE_WHEELS     4

/* колёса: по уровню выбирается компонент */
static const gTuneWheels[] = { 1073, 1074, 1075, 1076, 1077 };

/* Реальные заправки Los Santos: Temple, Ocean Docks, Idlewood, Willowfield. */
static const Float:gGasStations[][3] = {
    { 1004.0, -939.0, 42.2 },      /* Temple */
    { 1595.5, -2235.9, 13.5 },     /* Ocean Docks */
    { 1944.3, -1772.5, 13.4 },     /* Idlewood */
    { 2202.2, -1965.0, 13.6 }      /* Willowfield */
};

new Float:gFuel[VEHICLE_SLOTS];              /* литров в баке                */
new gTune[VEHICLE_SLOTS][TUNE_PARTS];        /* уровень детали 0..5          */
new gTrunkTarget[MAX_PLAYERS];               /* слот машины для меню         */
new gTrunkIds[MAX_PLAYERS][6];               /* uid предметов на странице    */
#define IMPOUND_ROWS            (6)     /* 6 cars per page on the impound lot */

new gTrunkCount[MAX_PLAYERS], gTrunkPage[MAX_PLAYERS];
new gImpoundIds[MAX_PLAYERS][IMPOUND_ROWS], gImpoundCount[MAX_PLAYERS];
new gPickCar[MAX_PLAYERS], gPickStep[MAX_PLAYERS], gPickSeq[MAX_PLAYERS][PICK_STEPS];
new gPickTick[MAX_PLAYERS], gPickStamp[MAX_PLAYERS];
new gTransTick[MAX_PLAYERS];                 /* защита от спама              */
new gTransBusy[MAX_PLAYERS];                 /* тик занятости процедурой     */

forward RW_TransportPulse();
forward RW_PickTimeout(playerid, stamp);
forward RW_TransRepairDone(playerid, vehicleid);
forward RW_TransRefuelDone(playerid, vehicleid, Float:liters);
forward RW_TransTuneDone(playerid, slot);

/*  ----------------------------------- база -------------------------------- */

stock RW_TransportSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS car_tuning (token INT, part INT, level INT, uid INT, stamp INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS trunks (uid INT, token INT, slot INT, type INT, data INT, amount INT, grams INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS impounds (token INT PRIMARY KEY, mechanic VARCHAR(32), reason VARCHAR(64), stamp INT)")) return 0;
    return 1;
}
stock RW_TransportReset(playerid)
{
    gTrunkTarget[playerid] = -1;
    gTrunkPage[playerid] = 0;
    gTrunkCount[playerid] = 0;
    gPickCar[playerid] = 0;
    gPickStep[playerid] = 0;
    gPickStamp[playerid]++;
    gTransBusy[playerid] = 0;
    return 1;
}

/*  --------------------------------- помощники ----------------------------- */

// Токен машины: uid ключа. Он же — ключ для багажника и тюнинга в базе.
stock RW_TransportToken(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    if (gVehGarageId[slot] > 0) return gVehGarageId[slot];
    return gVehKeyUid[slot];
}
stock bool:RW_TransportNear(playerid, vehicleid, Float:range = 6.0)
{
    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    return IsPlayerInRangeOfPoint(playerid, range, x, y, z) ? true : false;
}
stock bool:RW_MechOnDuty(playerid)
{
    return gLogged[playerid] && gDuty[playerid] && gFaction[playerid] == FACTION_MECHANIC;
}
stock bool:RW_TransportBusy(playerid)
{
    return gTransBusy[playerid] > 0;
}
// Занятость процедурой: ремонт, заправка, установка детали.
stock RW_TransportBusyStart(playerid, ms, const what[])
{
    gTransBusy[playerid] = GetTickCount() + ms;
    RW_UI_Close(playerid);
    if (GetPlayerState(playerid) == PLAYER_STATE_ONFOOT) {
        TogglePlayerControllable(playerid, false);
        ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 1, 1, 1, 1, 0);
    }
    new message[128];
    format(message, sizeof message, "%s... (%d seconds)", what, ms / 1000);
    return RW_Info(playerid, message);
}
stock RW_TransportBusyEnd(playerid)
{
    gTransBusy[playerid] = 0;
    ClearAnimations(playerid, 1);
    TogglePlayerControllable(playerid, true);
    return 1;
}

/*  ---------------------------------- топливо ------------------------------ */

stock Float:RW_FuelOf(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return -1.0;
    return gFuel[slot];
}
stock RW_FuelSet(vehicleid, Float:liters)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    if (liters < 0.0) liters = 0.0;
    if (liters > FUEL_MAX) liters = FUEL_MAX;
    gFuel[slot] = liters;
    return 1;
}
// На заправке или нет: радиус FUEL_STATION_RANGE от любой АЗС.
stock bool:RW_TransportAtGas(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    for (new i; i < sizeof gGasStations; i++) {
        new Float:dx = gGasStations[i][0] - x, Float:dy = gGasStations[i][1] - y, Float:dz = gGasStations[i][2] - z;
        if (dx*dx + dy*dy + dz*dz <= FUEL_STATION_RANGE*FUEL_STATION_RANGE) return true;
    }
    return false;
}
stock RW_FuelCommand(playerid)
{
    new vehicleid = GetPlayerVehicleID(playerid);
    if (!vehicleid) return RW_Error(playerid, "Sit in a car or stand next to it.");
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "This car has no fuel tank to check.");
    new message[128], liters = floatround(gFuel[slot]);
    format(message, sizeof message, "Fuel: %d of %d litres%s", liters, floatround(FUEL_MAX), RW_TransportAtGas(playerid) ? (" (you are at a gas station: /refuel)") : (""));
    return RW_Info(playerid, message);
}
// Заправка на АЗС за наличные; механик на службе заправляет бесплатно.
stock RW_FuelRefuel(playerid, vehicleid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "This car cannot be refuelled.");
    if (!RW_TransportNear(playerid, vehicleid, 8.0)) return RW_Error(playerid, "Stand closer to the car.");
    if (RW_TransportBusy(playerid)) return RW_Info(playerid, "You are already busy.");
    new Float:need = FUEL_MAX - gFuel[slot];
    if (need <= 0.5) return RW_Info(playerid, "The tank is full.");
    new bool:mech = RW_MechOnDuty(playerid);
    if (!mech && !RW_TransportAtGas(playerid)) {
        if (RW_CefOn(playerid)) return RW_Error(playerid, "Drive to a gas station: the car menu there has a refuel row.");
        return RW_Error(playerid, "Drive to a gas station: /fuel shows the level.");
    }
    new cost = floatround(need) * FUEL_PRICE;
    if (!mech && !RW_TakeMoney(playerid, cost)) return RW_Error(playerid, "Not enough cash for a full tank.");
    new ms = mech ? 5000 : (1000 + floatround(need) * 80);
    SetTimerEx("RW_TransRefuelDone", ms, false, "iif", playerid, vehicleid, need);
    new message[128];
    format(message, sizeof message, "Refuelling %d litres%s", floatround(need), mech ? ("") : (" (paid)"));
    return RW_TransportBusyStart(playerid, ms, message);
}
public RW_TransRefuelDone(playerid, vehicleid, Float:liters)
{
    RW_TransportBusyEnd(playerid);
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "The car is gone.");
    if (!RW_TransportNear(playerid, vehicleid, 10.0)) return RW_Error(playerid, "You walked away from the car.");
    RW_FuelSet(vehicleid, gFuel[slot] + liters);
    new message[96];
    format(message, sizeof message, "Tank: %d litres.", floatround(gFuel[slot]));
    return RW_Ok(playerid, message);
}

/*  --------------------------------- багажник ------------------------------ */
/*  Свой контейнер на 10 предметов: лимит по классу машины, ведётся в граммах. */

stock RW_TrunkCapacity(model)
{
    switch (model) {
        case 448, 461, 462, 463, 468, 471, 481, 509, 510, 521, 522, 523, 581, 586: return 5000;   /* мотоциклы */
        case 415, 429, 411, 451, 477, 480, 494, 502, 503, 506, 541, 555, 558, 559, 560, 561, 562, 565, 587, 602, 603: return 15000; /* спорт */
        case 400, 404, 405, 413, 414, 418, 422, 440, 455, 458, 459, 466, 467, 479, 482, 489, 492, 498, 499, 505, 543, 546, 547, 550, 551, 554, 579, 580, 585, 604, 605, 609: return 80000; /* фургоны */
    }
    return 40000;
}
// Название предмета для списка багажника.
stock RW_TrunkName(type, data, dest[], size = sizeof dest)
{
    if (type == ITEM_BACKPACK) format(dest, size, "BACKPACK");
    else if (type == ITEM_CAR_KEY) format(dest, size, "CAR KEY");
    else if (type == ITEM_WEAPON) RW_WeaponName(data, dest, size);
    else if (type == ITEM_AMMO) { new cname[16]; RW_CaliberName(data, cname, sizeof cname); format(dest, size, "%s AMMO", cname); }
    else if (type == ITEM_BANDAGE) format(dest, size, "BANDAGE");
    else if (type == ITEM_SYRINGE) format(dest, size, "ADRENALINE");
    else if (type == ITEM_ARMOUR) RW_ArmourName(data, dest, size);
    else if (type == ITEM_SIM) { new tname[32]; RW_PhoneTariffName(data, tname, sizeof tname); format(dest, size, "SIM %s", tname); }
    else if (type == ITEM_PHONE) RW_PhoneName(data, dest, size);
    else if (type == ITEM_CUFFS || type == ITEM_TASER || type == ITEM_BATON || type == ITEM_BADGE) RW_PoliceItemName(type, data, dest, size);
    else if (type == ITEM_DEFIB) format(dest, size, "DEFIBRILLATOR");
    else if (type == ITEM_MEDKIT) format(dest, size, "MEDKIT");
    else if (type == ITEM_LOCKPICK) format(dest, size, "LOCKPICK");
    else if (type == ITEM_TUNING) { new pname[24]; RW_TuneName(data, pname, sizeof pname); format(dest, size, "%s L%d", pname, 1); }
    else if (type == ITEM_CHEM) format(dest, size, "PRECURSOR");
    else if (type == ITEM_GREEN) format(dest, size, "GREEN");
    else format(dest, size, "ITEM");
    return 1;
}
stock RW_TrunkGrams(token)
{
    new query[200], total;
    format(query, sizeof query, "SELECT grams FROM trunks WHERE token=%d", token);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) { total += db_get_field_assoc_int(r, "grams"); db_next_row(r); }
        db_free_result(r);
    }
    return total;
}
stock RW_TrunkFreeSlot(token)
{
    for (new i; i < TRUNK_SLOTS; i++) {
        new query[200];
        format(query, sizeof query, "SELECT uid FROM trunks WHERE token=%d AND slot=%d", token, i);
        new DBResult:r = db_query(gDB, query);
        if (r == DBResult:0) return i;
        new taken = db_num_rows(r) > 0;
        db_free_result(r);
        if (!taken) return i;
    }
    return -1;
}
stock RW_TrunkOpen(playerid, vehicleid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is not your car.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    if (!RW_TransportToken(vehicleid)) return RW_Error(playerid, "This car has no trunk.");
    gTrunkTarget[playerid] = gVehSlotOf[vehicleid];
    gTrunkPage[playerid] = 0;
    return RW_TrunkRender(playerid);
}
stock RW_TrunkRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefTrunk(playerid);
    #endif
    return RW_TrunkPanel(playerid);
}
// Возвращает сколько строк показано и заполняет gTrunkIds.
stock RW_TrunkList(playerid, limit)
{
    new slot = gTrunkTarget[playerid];
    for (new i; i < 6; i++) gTrunkIds[playerid][i] = 0;
    if (slot < 0 || !gVeh[slot][vsUsed]) { gTrunkCount[playerid] = 0; return 0; }
    new query[220];
    format(query, sizeof query, "SELECT uid,type,data,amount,grams FROM trunks WHERE token=%d ORDER BY slot LIMIT %d OFFSET %d", RW_TransportToken(gVeh[slot][vsVehicleId]), limit, gTrunkPage[playerid]*limit);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) { gTrunkCount[playerid] = 0; return 0; }
    new rows = db_num_rows(r), index;
    for (new i; i < rows && index < limit; i++) {
        new label[64], name[32];
        gTrunkIds[playerid][index] = db_get_field_assoc_int(r, "uid");
        RW_TrunkName(db_get_field_assoc_int(r, "type"), db_get_field_assoc_int(r, "data"), name, sizeof name);
        format(label, sizeof label, "%s x%d", name, db_get_field_assoc_int(r, "amount"));
        #if RAILWAY_CEF
            if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, "take", "bag");
            else
        #endif
        RW_UI_Row(playerid, index, label);
        index++;
        db_next_row(r);
    }
    db_free_result(r);
    gTrunkCount[playerid] = index;
    return index;
}
stock RW_CefTrunk(playerid)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new vehicleid = gVeh[slot][vsVehicleId], subtitle[64];
    format(subtitle, sizeof subtitle, "%d / %d kg", floatdiv(RW_TrunkGrams(RW_TransportToken(vehicleid)), 1000), floatdiv(RW_TrunkCapacity(gVeh[slot][vsModel]), 1000));
    if (!RW_CefOpen(playerid, CEF_MODE_TRUNK, "Trunk", subtitle)) return RW_TrunkPanel(playerid);
    if (!RW_TrunkList(playerid, 6)) RW_CefRow(playerid, 0, "The trunk is empty", "", "bag");
    RW_CefAction(playerid, 6, "Put the selected item", "");
    if (gTrunkCount[playerid] == 6) RW_CefAction(playerid, 7, "Next page", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_TrunkPanel(playerid)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new vehicleid = gVeh[slot][vsVehicleId], subtitle[64];
    format(subtitle, sizeof subtitle, "%d / %d kg", floatdiv(RW_TrunkGrams(RW_TransportToken(vehicleid)), 1000), floatdiv(RW_TrunkCapacity(gVeh[slot][vsModel]), 1000));
    RW_UI_Panel(playerid, UI_TRUNK, "Trunk", subtitle);
    if (!RW_TrunkList(playerid, 5)) RW_UI_Row(playerid, 0, "The trunk is empty");
    if (gTrunkCount[playerid] == 5) RW_UI_Row(playerid, 5, "Next page >");
    RW_UI_Row(playerid, 6, "Put the selected item");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
// Положить: предмет, выбранный в инвентаре.
stock RW_TrunkPut(playerid)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new vehicleid = gVeh[slot][vsVehicleId], token = RW_TransportToken(vehicleid);
    if (!token) return RW_Error(playerid, "This car has no trunk.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    new uid = gInvSelected[playerid];
    new inv = RW_InvFind(playerid, uid);
    if (inv < 0) return RW_Error(playerid, "Select an item in the inventory first (I, then the item).");
    new type = gInv[playerid][inv][iiType], data = gInv[playerid][inv][iiData], amount = gInv[playerid][inv][iiAmount];
    new grams = RW_InvGrams(type, data, amount);
    if (RW_TrunkGrams(token) + grams > RW_TrunkCapacity(gVeh[slot][vsModel])) return RW_Error(playerid, "It does not fit: the trunk is full.");
    new free = RW_TrunkFreeSlot(token);
    if (free < 0) return RW_Error(playerid, "No free space in the trunk.");
    new query[260];
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", uid, gAccountId[playerid], inv);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not move the item.");
    format(query, sizeof query, "INSERT INTO trunks(uid,token,slot,type,data,amount,grams) VALUES(%d,%d,%d,%d,%d,%d,%d)", uid, token, free, type, data, amount, grams);
    RW_DB_Exec(query);
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][inv][j] = 0;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    new name[32], message[110];
    RW_TrunkName(type, data, name, sizeof name);
    format(message, sizeof message, "%s is in the trunk.", name);
    RW_Ok(playerid, message);
    return RW_TrunkRender(playerid);
}
// Забрать: предмет возвращается в инвентарь.
stock RW_TrunkTake(playerid, row)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new uid = gTrunkIds[playerid][row];
    if (!uid) return 1;
    new vehicleid = gVeh[slot][vsVehicleId], token = RW_TransportToken(vehicleid);
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    new query[220], type, data, amount;
    format(query, sizeof query, "SELECT type,data,amount FROM trunks WHERE uid=%d AND token=%d", uid, token);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0 || db_num_rows(r) == 0) {
        if (r != DBResult:0) db_free_result(r);
        return RW_Error(playerid, "That item is gone.");
    }
    type = db_get_field_assoc_int(r, "type"); data = db_get_field_assoc_int(r, "data"); amount = db_get_field_assoc_int(r, "amount");
    db_free_result(r);
    if (!RW_InvFree(playerid, 0, type, data, amount)) return RW_Error(playerid, "No room for it in your pockets.");
    format(query, sizeof query, "DELETE FROM trunks WHERE uid=%d AND token=%d", uid, token);
    RW_DB_Exec(query);
    RW_InvGrant(playerid, type, data, amount);
    RW_InvAttach(playerid);
    new name[32], message[110];
    RW_TrunkName(type, data, name, sizeof name);
    format(message, sizeof message, "%s taken from the trunk.", name);
    RW_Ok(playerid, message);
    return RW_TrunkRender(playerid);
}
stock RW_TrunkAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 6) return RW_TrunkPut(playerid);
    new limit = cef ? 6 : 5;
    if (row == (cef ? 7 : 5)) {
        if (gTrunkCount[playerid] == limit) gTrunkPage[playerid]++;
        else if (gTrunkPage[playerid] > 0) gTrunkPage[playerid] = 0;
        return RW_TrunkRender(playerid);
    }
    if (row < 0 || row >= limit) return 1;
    return RW_TrunkTake(playerid, row);
}
stock RW_TrunkCefClick(playerid, row) { return RW_TrunkAction(playerid, row, true); }
stock RW_TrunkClick(playerid, row) { return RW_TrunkAction(playerid, row, false); }

/*  ---------------------------------- взлом -------------------------------- */
/*  Отмычка: три шага, на каждом надо нажать свою цифру. Ошибка или пауза —
 *  отмычка ломается и хозяин получает сообщение об угоне.                    */

stock RW_PickItem(playerid)
{
    if (!gLogged[playerid]) return -1;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_LOCKPICK) return i;
    return -1;
}
stock RW_PickStart(playerid, vehicleid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is your own car: use the key.");
    if (RW_PickItem(playerid) < 0) return RW_Error(playerid, "You need a lockpick in the inventory.");
    if (!RW_TransportNear(playerid, vehicleid, 4.0)) return RW_Error(playerid, "Stand closer to the car.");
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "This car cannot be broken into.");
    if (gVeh[slot][vsDoors]) return RW_Info(playerid, "The door is already open.");
    gPickCar[playerid] = vehicleid;
    gPickStep[playerid] = 0;
    gPickStamp[playerid]++;
    for (new i; i < PICK_STEPS; i++) gPickSeq[playerid][i] = 1 + random(4);
    gPickTick[playerid] = GetTickCount();
    SetTimerEx("RW_PickTimeout", PICK_MS, false, "ii", playerid, gPickStamp[playerid]);
    RW_Info(playerid, "Pick the lock: press the numbers in the order you hear them.");
    return RW_PickRender(playerid);
}
stock RW_PickRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefPick(playerid);
    #endif
    return RW_PickPanel(playerid);
}
stock RW_CefPick(playerid)
{
    new subtitle[48];
    format(subtitle, sizeof subtitle, "Step %d of %d", gPickStep[playerid] + 1, PICK_STEPS);
    if (!RW_CefOpen(playerid, CEF_MODE_LOCKPICK, "Breaking in", subtitle)) return RW_PickPanel(playerid);
    for (new i; i < 4; i++) {
        new label[16];
        format(label, sizeof label, "%d", i + 1);
        RW_CefRow(playerid, i, label, "", "key");
    }
    RW_CefAction(playerid, 8, "Give up", "danger");
    return RW_CefEnd(playerid);
}
stock RW_PickPanel(playerid)
{
    new subtitle[48];
    format(subtitle, sizeof subtitle, "Step %d of %d", gPickStep[playerid] + 1, PICK_STEPS);
    RW_UI_Panel(playerid, UI_LOCKPICK, "Breaking in", subtitle);
    for (new i; i < 4; i++) {
        new label[16];
        format(label, sizeof label, "%d", i + 1);
        RW_UI_Row(playerid, i, label);
    }
    RW_UI_Row(playerid, 7, "Give up");
    return 1;
}
stock RW_PickClick(playerid, row, bool:cef)
{
    if (!gPickCar[playerid]) return 1;
    if (row == (cef ? 8 : 7)) { gPickStamp[playerid]++; gPickCar[playerid] = 0; return RW_UI_Close(playerid); }
    if (row < 0 || row > 3) return 1;
    if (GetTickCount()-gTransTick[playerid] < 200) return 1;
    gTransTick[playerid] = GetTickCount();
    if (row + 1 != gPickSeq[playerid][gPickStep[playerid]]) return RW_PickFail(playerid);
    gPickStep[playerid]++;
    if (gPickStep[playerid] >= PICK_STEPS) return RW_PickSuccess(playerid);
    gPickTick[playerid] = GetTickCount();
    gPickStamp[playerid]++;
    SetTimerEx("RW_PickTimeout", PICK_MS, false, "ii", playerid, gPickStamp[playerid]);
    RW_Info(playerid, "One pin set. Keep going.");
    return RW_PickRender(playerid);
}
stock RW_PickCefClick(playerid, row) { return RW_PickClick(playerid, row, true); }
public RW_PickTimeout(playerid, stamp)
{
    if (stamp != gPickStamp[playerid] || !gPickCar[playerid]) return 1;
    return RW_PickFail(playerid);
}
stock RW_PickFail(playerid)
{
    new vehicleid = gPickCar[playerid];
    gPickCar[playerid] = 0;
    gPickStamp[playerid]++;
    RW_UI_Close(playerid);
    new slot = RW_PickItem(playerid);
    if (slot >= 0) {
        new query[220];
        format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        RW_InvWrite(query);
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
        RW_InvAttach(playerid);
    }
    RW_Error(playerid, "The lockpick snapped.");
    if (vehicleid > 0 && gVehSlotOf[vehicleid] >= 0) RW_PickAlarm(playerid, vehicleid);
    return 1;
}
stock RW_PickSuccess(playerid)
{
    new vehicleid = gPickCar[playerid];
    gPickCar[playerid] = 0;
    gPickStamp[playerid]++;
    RW_UI_Close(playerid);
    new slot = RW_PickItem(playerid);
    if (slot >= 0) {
        new query[220];
        format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        RW_InvWrite(query);
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
        RW_InvAttach(playerid);
    }
    new vslot = gVehSlotOf[vehicleid];
    if (vslot < 0 || !gVeh[vslot][vsUsed]) return RW_Error(playerid, "The car is gone.");
    gVeh[vslot][vsDoors] = true;
    gVeh[vslot][vsKey] = true;
    gVeh[vslot][vsEngine] = true;
    RW_Veh_Sync(vehicleid);
    RW_Veh_SyncAll(vehicleid);
    RW_Ok(playerid, "The door is open and the wires are crossed. Drive while you can.");
    return RW_PickAlarm(playerid, vehicleid);
}
// Хозяин узнаёт об угоне, а полиция получает вызов.
stock RW_PickAlarm(playerid, vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != gVeh[slot][vsOwnerAcc]) continue;
        format(message, sizeof message, "Someone is breaking into your %s: %s.", gVeh[slot][vsOwnerName], nick);
        RW_Error(i, message);
    }
    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    return RW_EmsCall(playerid, 0, x, y, z);
}

/*  --------------------------------- механик ------------------------------- */
/*  Служба механика: ремонт, заправка и эвакуация на штрафстоянку.            */

stock RW_ServiceOpen(playerid, vehicleid)
{
    if (!RW_MechOnDuty(playerid)) return RW_Error(playerid, "You are not on duty as a mechanic.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "This car cannot be serviced.");
    gTrunkTarget[playerid] = slot;
    return RW_ServiceRender(playerid);
}
stock RW_ServiceRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefService(playerid);
    #endif
    return RW_ServicePanel(playerid);
}
stock RW_CefService(playerid)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new name[48], subtitle[64];
    RW_VehicleName(gVeh[slot][vsModel], name, sizeof name);
    format(subtitle, sizeof subtitle, "Fuel %d / %d", floatround(gFuel[slot]), floatround(FUEL_MAX));
    if (!RW_CefOpen(playerid, CEF_MODE_SERVICE, name, subtitle)) return RW_ServicePanel(playerid);
    RW_CefRow(playerid, 0, "Repair the body", "damage", "settings");
    RW_CefRow(playerid, 1, "Refuel the tank", "litres", "engine");
    RW_CefRow(playerid, 2, "Tow it to the impound lot", "fee", "eject");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_ServicePanel(playerid)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    new name[48], subtitle[64];
    RW_VehicleName(gVeh[slot][vsModel], name, sizeof name);
    format(subtitle, sizeof subtitle, "Fuel %d / %d", floatround(gFuel[slot]), floatround(FUEL_MAX));
    RW_UI_Panel(playerid, UI_SERVICE, name, subtitle);
    RW_UI_Row(playerid, 0, "Repair the body");
    RW_UI_Row(playerid, 1, "Refuel the tank");
    RW_UI_Row(playerid, 2, "Tow it to the impound lot");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_ServiceAction(playerid, row, bool:cef)
{
    new slot = gTrunkTarget[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_UI_Close(playerid);
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    new vehicleid = gVeh[slot][vsVehicleId];
    if (row == 0) { RW_ServiceRepair(playerid, vehicleid); return RW_UI_Close(playerid); }
    if (row == 1) { RW_FuelRefuel(playerid, vehicleid); return RW_UI_Close(playerid); }
    if (row == 2) { RW_ServiceTow(playerid, vehicleid); return RW_UI_Close(playerid); }
    return 1;
}
stock RW_ServiceCefClick(playerid, row) { return RW_ServiceAction(playerid, row, true); }
stock RW_ServiceClick(playerid, row) { return RW_ServiceAction(playerid, row, false); }
// Ремонт: кузов целый, здоровье выше с подвеской.
stock RW_ServiceRepair(playerid, vehicleid)
{
    if (!RW_MechOnDuty(playerid)) return RW_Error(playerid, "You are not on duty as a mechanic.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    if (RW_TransportBusy(playerid)) return RW_Info(playerid, "You are already busy.");
    SetTimerEx("RW_TransRepairDone", 5000, false, "ii", playerid, vehicleid);
    return RW_TransportBusyStart(playerid, 5000, "Repairing the car");
}
public RW_TransRepairDone(playerid, vehicleid)
{
    RW_TransportBusyEnd(playerid);
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "The car is gone.");
    if (!RW_TransportNear(playerid, vehicleid, 10.0)) return RW_Error(playerid, "You walked away from the car.");
    RepairVehicle(vehicleid);
    SetVehicleHealth(vehicleid, 1000.0 + float(gTune[slot][TUNE_SUSP] * 100));
    return RW_Ok(playerid, "The car is repaired.");
}
// Эвакуация: машина уезжает на штрафстоянку, хозяин вызволяет её за деньги.
stock RW_ServiceTow(playerid, vehicleid)
{
    if (!RW_MechOnDuty(playerid)) return RW_Error(playerid, "You are not on duty as a mechanic.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "This car cannot be towed.");
    new token = RW_TransportToken(vehicleid);
    if (!token) return RW_Error(playerid, "This car cannot be towed.");
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == vehicleid && GetPlayerVehicleSeat(i) == 0) return RW_Error(playerid, "There is a driver inside.");
    gVeh[slot][vsKey] = false;
    gVeh[slot][vsEngine] = false;
    gVeh[slot][vsDoors] = false;
    RW_Veh_Sync(vehicleid);
    RW_Veh_SyncAll(vehicleid);
    SetVehiclePos(vehicleid, IMPOUND_X, IMPOUND_Y, IMPOUND_Z);
    SetVehicleZAngle(vehicleid, 90.0);
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], query[260];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT OR REPLACE INTO impounds(token,mechanic,reason,stamp) VALUES(%d,'%s','Towed by a mechanic',%d)", token, safe, gettime());
    RW_DB_Exec(query);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != gVeh[slot][vsOwnerAcc]) continue;
        format(nick, sizeof nick, "%s", RW_CefOn(i)
            ? ("Your car was towed to the impound lot: press Y there to buy it back.")
            : ("Your car was towed to the impound lot. /impound to get it back."));
        RW_Error(i, nick);
    }
    new message[110];
    format(message, sizeof message, "Towed. The owner pays %d to get it back.", IMPOUND_FEE);
    return RW_Ok(playerid, message);
}
// Есть ли у игрока машина на штрафстоянке.
stock bool:RW_OwnsImpounded(playerid)
{
    if (!gLogged[playerid]) return false;
    for (new s; s < VEHICLE_SLOTS; s++) {
        if (!gVeh[s][vsUsed] || gVeh[s][vsOwnerAcc] != gAccountId[playerid]) continue;
        new Float:x, Float:y, Float:z;
        GetVehiclePos(gVeh[s][vsVehicleId], x, y, z);
        if (VectorSize(x - IMPOUND_X, y - IMPOUND_Y, z - IMPOUND_Z) <= 30.0) return true;
    }
    return false;
}
stock RW_ImpoundList(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_MechOnDuty(playerid) && gAdminLevel[playerid] < 2 && !RW_OwnsImpounded(playerid))
        return RW_Error(playerid, "Only a mechanic on duty or the owner can look at the impound lot.");
    new query[240];
    format(query, sizeof query, "SELECT i.token,c.model,i.mechanic,i.stamp FROM impounds i LEFT JOIN owned_cars c ON c.id=i.token OR c.token=i.token");
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "The impound lot is empty.");
    new rows = db_num_rows(r);
    if (!rows) { db_free_result(r); return RW_Info(playerid, "The impound lot is empty."); }
    RW_Accent(playerid, "Impounded cars:");
    for (new i; i < rows; i++) {
        new line[140];
        new who[32];
        db_get_field_assoc(r, "mechanic", who, sizeof who);
        format(line, sizeof line, "model %d (token %d) - towed by %s", db_get_field_assoc_int(r, "model"), db_get_field_assoc_int(r, "token"), who);
        SendClientMessage(playerid, COLOR_GREY, line);
        db_next_row(r);
    }
    db_free_result(r);
    return RW_Info(playerid, RW_CefOn(playerid)
        ? ("The list is on the lot: press Y there and pay from the panel.")
        : ("/unimpound <token> - pay the fee at the impound lot and take the car."));
}
/*  ----------------------------- impound panel ----------------------------- */
/* The list is read from the database on every open, so a click pays for the
   car that stands on that line. /unimpound stays as the chat way. */
stock bool:RW_ImpoundNear(playerid)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return VectorSize(x - IMPOUND_X, y - IMPOUND_Y, z - IMPOUND_Z) <= 20.0;
}
stock bool:RW_ImpoundMayOpen(playerid)
{
    if (!gLogged[playerid]) return false;
    if (RW_MechOnDuty(playerid) || gAdminLevel[playerid] >= 2) return true;
    return RW_OwnsImpounded(playerid);
}
stock RW_ImpoundRows(playerid)
{
    new count, query[260];
    for (new i; i < IMPOUND_ROWS; i++) gImpoundIds[playerid][i] = 0;
    format(query, sizeof query,
        "SELECT i.token,c.model,i.mechanic FROM impounds i LEFT JOIN owned_cars c ON c.id=i.token OR c.token=i.token ORDER BY i.stamp DESC LIMIT %d",
        IMPOUND_ROWS);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        while (count < IMPOUND_ROWS && db_num_rows(r) > count) {
            new name[48], who[32], label[96], note[72];
            RW_VehicleName(db_get_field_assoc_int(r, "model"), name, sizeof name);
            db_get_field_assoc(r, "mechanic", who, sizeof who);
            format(label, sizeof label, "%s (token %d)", name, db_get_field_assoc_int(r, "token"));
            format(note, sizeof note, "towed by %s, fee %d", who, IMPOUND_FEE);
            gImpoundIds[playerid][count] = db_get_field_assoc_int(r, "token");
            #if RAILWAY_CEF
                if (RW_CefOn(playerid)) { RW_CefRow(playerid, count, label, "", "car", note); count++; db_next_row(r); continue; }
            #endif
            RW_UI_Row(playerid, count, label, "car");
            count++;
            db_next_row(r);
        }
        db_free_result(r);
    }
    gImpoundCount[playerid] = count;
    if (!count) {
        #if RAILWAY_CEF
            if (RW_CefOn(playerid)) { RW_CefRow(playerid, 0, "The impound lot is empty", "", "car", "Come back later"); return 0; }
        #endif
        RW_UI_Row(playerid, 0, "The impound lot is empty", "car");
    }
    return count;
}
stock RW_ImpoundCef(playerid)
{
    new subtitle[64];
    format(subtitle, sizeof subtitle, "Fee %d for a car, the lot at 1635 / -1700", IMPOUND_FEE);
    if (!RW_CefOpen(playerid, CEF_MODE_IMPOUND, "Impound lot", subtitle)) return RW_ImpoundPanel(playerid);
    RW_ImpoundRows(playerid);
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_ImpoundPanel(playerid)
{
    new subtitle[64];
    format(subtitle, sizeof subtitle, "Fee %d for a car, the lot at 1635 / -1700", IMPOUND_FEE);
    RW_UI_Panel(playerid, UI_IMPOUND, "Impound lot", subtitle);
    RW_ImpoundRows(playerid);
    RW_UI_Row(playerid, 7, "Close", "close");
    return 1;
}
stock RW_ImpoundRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_ImpoundCef(playerid);
    #endif
    return RW_ImpoundPanel(playerid);
}
stock RW_ImpoundOpen(playerid)
{
    if (!RW_ImpoundMayOpen(playerid)) return RW_Error(playerid, "Only a mechanic on duty or the owner can look at the impound lot.");
    return RW_ImpoundRender(playerid);
}
stock RW_ImpoundAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row < 0 || row >= IMPOUND_ROWS) return 1;
    new token = gImpoundIds[playerid][row];
    if (token <= 0) return 1;
    RW_ImpoundTake(playerid, token);
    return RW_ImpoundRender(playerid);
}
stock RW_ImpoundCefClick(playerid, row) { return RW_ImpoundAction(playerid, row, true); }
stock RW_ImpoundClick(playerid, row) { return RW_ImpoundAction(playerid, row, false); }

/* The fee and the release: one function for the click and for the command. */
stock RW_ImpoundTake(playerid, token)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (token <= 0) return RW_Error(playerid, "That car is not impounded.");
    new query[240];
    format(query, sizeof query, "SELECT token FROM impounds WHERE token=%d", token);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0 || db_num_rows(r) == 0) {
        if (r != DBResult:0) db_free_result(r);
        return RW_Error(playerid, "That car is not impounded.");
    }
    db_free_result(r);
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_MechOnDuty(playerid) && gAdminLevel[playerid] < 2
        && !IsPlayerInRangeOfPoint(playerid, 20.0, IMPOUND_X, IMPOUND_Y, IMPOUND_Z))
        return RW_Error(playerid, "Come to the impound lot to pay the fee.");
    if (!RW_TakeMoney(playerid, IMPOUND_FEE)) return RW_Error(playerid, "Not enough cash for the fee.");
    format(query, sizeof query, "DELETE FROM impounds WHERE token=%d", token);
    RW_DB_Exec(query);
    for (new s; s < VEHICLE_SLOTS; s++) {
        if (!gVeh[s][vsUsed] || RW_TransportToken(gVeh[s][vsVehicleId]) != token) continue;
        new Float:x, Float:y, Float:z, Float:a;
        GetPlayerPos(playerid, x, y, z);
        GetPlayerFacingAngle(playerid, a);
        SetVehiclePos(gVeh[s][vsVehicleId], x + 4.0 * floatsin(-a, degrees), y + 4.0 * floatcos(-a, degrees), z + 0.5);
        RW_Ok(playerid, "The car is released and waits outside.");
        return 1;
    }
    return RW_Ok(playerid, "Paid: the car is out of the impound. Take it from the garage.");
}
stock RW_ImpoundRelease(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /unimpound <token>. /impound lists them.");
    return RW_ImpoundTake(playerid, strval(word));
}

/*  ---------------------------------- тюнинг ------------------------------- */
/*  Деталь хранит свой uid: ставится в гараже тюнинга, уровень выше старого,
 *  двигатель 4 уровня даёт +20 км/ч к максимальной скорости.                 */

// Вес детали: двигатель тяжелее всего, колёса легче.
stock RW_TunePartGrams(part)
{
    switch (part) {
        case TUNE_ENGINE: return 40000;
        case TUNE_GEARBOX: return 25000;
        case TUNE_BRAKES: return 15000;
        case TUNE_SUSP: return 30000;
        default: return 20000;          /* колёса */
    }
    return 20000;
}
stock RW_TuneName(part, dest[], size = sizeof dest)
{
    switch (part) {
        case TUNE_ENGINE: format(dest, size, "ENGINE");
        case TUNE_GEARBOX: format(dest, size, "GEARBOX");
        case TUNE_BRAKES: format(dest, size, "BRAKES");
        case TUNE_SUSP: format(dest, size, "SUSPENSION");
        default: format(dest, size, "WHEELS");
    }
    return 1;
}
// Базовая скорость модели: от неё считается предел с тюнингом.
stock Float:RW_TuneBaseSpeed(model)
{
    switch (model) {
        case 448, 461, 462, 463, 468, 471, 481, 509, 510, 521, 522, 523, 581, 586: return 150.0;
        case 415, 429, 411, 451, 477, 480, 494, 502, 503, 506, 541, 555, 558, 559, 560, 561, 562, 565, 587, 602, 603: return 230.0;
        case 400, 404, 405, 413, 414, 418, 422, 440, 455, 458, 459, 466, 467, 479, 482, 489, 492, 498, 499, 505, 543, 546, 547, 550, 551, 554, 579, 580, 585, 604, 605, 609: return 170.0;
    }
    return 180.0;
}
stock Float:RW_TuneMaxSpeed(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0.0;
    return RW_TuneBaseSpeed(gVeh[slot][vsModel]) + TUNE_SPEED_STEP * float(gTune[slot][TUNE_ENGINE]);
}
stock RW_TuneLevel(vehicleid, part)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed] || part < 0 || part >= TUNE_PARTS) return 0;
    return gTune[slot][part];
}
// Уровни читаются из базы при появлении машины.
stock RW_TuneLoad(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    for (new i; i < TUNE_PARTS; i++) gTune[slot][i] = 0;
    new token = RW_TransportToken(vehicleid);
    if (!token) return 0;
    new query[220];
    format(query, sizeof query, "SELECT part,level FROM car_tuning WHERE token=%d", token);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    for (new i; i < rows; i++) {
        new part = db_get_field_assoc_int(r, "part"), level = db_get_field_assoc_int(r, "level");
        if (part >= 0 && part < TUNE_PARTS && level > gTune[slot][part]) gTune[slot][part] = level;
        db_next_row(r);
    }
    db_free_result(r);
    RW_TuneApply(vehicleid);
    return 1;
}
stock RW_TuneApply(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    new level = gTune[slot][TUNE_WHEELS];
    if (level > 0) AddVehicleComponent(vehicleid, gTuneWheels[level - 1]);
    return 1;
}
stock bool:RW_TuneNear(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, TUNING_RANGE, TUNING_X, TUNING_Y, TUNING_Z) ? true : false;
}
stock RW_TuneInstall(playerid, vehicleid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_IsVehicleOwner(playerid, vehicleid)) return RW_Error(playerid, "It is not your car.");
    if (!RW_TuneNear(playerid)) return RW_Error(playerid, "Tuning is done at the tuning garage.");
    if (!RW_TransportNear(playerid, vehicleid)) return RW_Error(playerid, "Stand closer to the car.");
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER) return RW_Error(playerid, "Step out of the car first.");
    if (RW_TransportBusy(playerid)) return RW_Info(playerid, "You are already busy.");
    new uid = gInvSelected[playerid], inv = RW_InvFind(playerid, uid);
    if (inv < 0 || gInv[playerid][inv][iiType] != ITEM_TUNING) return RW_Error(playerid, "Select a tuning part in the inventory (I, then the part).");
    new part = gInv[playerid][inv][iiData], level = gInv[playerid][inv][iiAmount], vslot = gVehSlotOf[vehicleid];
    if (part < 0 || part >= TUNE_PARTS) return RW_Error(playerid, "Unknown part.");
    if (level < 1 || level > TUNE_MAX_LEVEL) return RW_Error(playerid, "The part level is wrong.");
    if (level <= gTune[vslot][part]) return RW_Error(playerid, "That part is no better than the installed one.");
    if (!RW_TransportToken(vehicleid)) return RW_Error(playerid, "This car cannot be tuned.");
    gTrunkTarget[playerid] = vslot;
    SetTimerEx("RW_TransTuneDone", TUNE_INSTALL_MS, false, "ii", playerid, inv);
    new pname[24], what[64];
    RW_TuneName(part, pname, sizeof pname);
    format(what, sizeof what, "Installing %s level %d", pname, level);
    return RW_TransportBusyStart(playerid, TUNE_INSTALL_MS, what);
}
public RW_TransTuneDone(playerid, slot)
{
    RW_TransportBusyEnd(playerid);
    new vslot = gTrunkTarget[playerid];
    if (vslot < 0 || !gVeh[vslot][vsUsed]) return RW_Error(playerid, "The car is gone.");
    new vehicleid = gVeh[vslot][vsVehicleId];
    if (!RW_TransportNear(playerid, vehicleid, 10.0)) return RW_Error(playerid, "You walked away from the car.");
    if (slot < 0 || gInv[playerid][slot][iiType] != ITEM_TUNING) return RW_Error(playerid, "The part is gone.");
    new part = gInv[playerid][slot][iiData], level = gInv[playerid][slot][iiAmount], uid = gInv[playerid][slot][iiUid];
    new query[260];
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", uid, gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not fit the part.");
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_InvAttach(playerid);
    format(query, sizeof query, "INSERT INTO car_tuning(token,part,level,uid,stamp) VALUES(%d,%d,%d,%d,%d)", RW_TransportToken(vehicleid), part, level, uid, gettime());
    RW_DB_Exec(query);
    gTune[vslot][part] = level;
    RW_TuneApply(vehicleid);
    new pname[24], message[128];
    RW_TuneName(part, pname, sizeof pname);
    format(message, sizeof message, "%s level %d installed. Top speed %d km/h.", pname, level, floatround(RW_TuneMaxSpeed(vehicleid)));
    return RW_Ok(playerid, message);
}

/*  --------------------------- строка меню машины -------------------------- */
// Чем для вас эта машина: багажник, служба или взлом.
stock RW_TransportRowLabel(playerid, vehicleid, dest[], size = sizeof dest)
{
    dest[0] = EOS;
    if (vehicleid <= 0 || gVehSlotOf[vehicleid] < 0) return 0;
    if (RW_IsVehicleOwner(playerid, vehicleid)) {
        format(dest, size, "Trunk (%d kg)", floatdiv(RW_TrunkGrams(RW_TransportToken(vehicleid)), 1000));
        return 1;
    }
    if (RW_MechOnDuty(playerid)) { format(dest, size, "Service: repair, refuel, tow"); return 1; }
    if (RW_PickItem(playerid) >= 0) { format(dest, size, "Break in with a lockpick"); return 1; }
    return 0;
}
stock RW_TransportRow(playerid, vehicleid)
{
    if (RW_IsVehicleOwner(playerid, vehicleid)) return RW_TrunkOpen(playerid, vehicleid);
    if (RW_MechOnDuty(playerid)) return RW_ServiceOpen(playerid, vehicleid);
    if (RW_PickItem(playerid) >= 0) return RW_PickStart(playerid, vehicleid);
    return RW_Error(playerid, "Nothing to do with this car: you need the key, a lockpick or a mechanic's job.");
}

/*  --------------------------------- команды -------------------------------- */

stock RW_TransportCar(playerid)
{
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid > 0 && gVehSlotOf[vehicleid] >= 0) return vehicleid;
    new Float:x, Float:y, Float:z, Float:best = 8.0, found;
    GetPlayerPos(playerid, x, y, z);
    for (new s; s < VEHICLE_SLOTS; s++) {
        if (!gVeh[s][vsUsed]) continue;
        new Float:vx, Float:vy, Float:vz;
        GetVehiclePos(gVeh[s][vsVehicleId], vx, vy, vz);
        new Float:d = VectorSize(vx - x, vy - y, vz - z);
        if (d < best) { best = d; found = gVeh[s][vsVehicleId]; }
    }
    if (!found) { RW_Error(playerid, "Sit in a car or stand next to one."); return 0; }
    return found;
}
stock RW_TrunkCommand(playerid)
{
    new vehicleid = RW_TransportCar(playerid);
    if (!vehicleid) return 1;
    return RW_TrunkOpen(playerid, vehicleid);
}
stock RW_RepairCommand(playerid)
{
    new vehicleid = RW_TransportCar(playerid);
    if (!vehicleid) return 1;
    return RW_ServiceRepair(playerid, vehicleid);
}
stock RW_RefuelCommand(playerid)
{
    new vehicleid = RW_TransportCar(playerid);
    if (!vehicleid) return 1;
    return RW_FuelRefuel(playerid, vehicleid);
}
stock RW_TowCommand(playerid)
{
    new vehicleid = RW_TransportCar(playerid);
    if (!vehicleid) return 1;
    return RW_ServiceTow(playerid, vehicleid);
}
stock RW_TuneCommand(playerid)
{
    new vehicleid = RW_TransportCar(playerid);
    if (!vehicleid) return 1;
    return RW_TuneInstall(playerid, vehicleid);
}
stock RW_UnimpoundCommand(playerid, const params[])
{
    return RW_ImpoundRelease(playerid, params);
}

/*  ---------------------------------- таймер ------------------------------- */
public RW_TransportPulse()
{
    for (new s; s < VEHICLE_SLOTS; s++) {
        if (!gVeh[s][vsUsed]) continue;
        new vehicleid = gVeh[s][vsVehicleId];
        if (vehicleid <= 0 || !GetVehicleModel(vehicleid)) continue;
        new Float:vx, Float:vy, Float:vz;
        GetVehicleVelocity(vehicleid, vx, vy, vz);
        new Float:kmh = floatsqroot(vx*vx + vy*vy + vz*vz) * 180.0;
        /* топливо тратится только с заведённым двигателем */
        if (gVeh[s][vsEngine]) {
            gFuel[s] -= (FUEL_IDLE + kmh * FUEL_SPEED);
            if (gFuel[s] <= 0.0) {
                gFuel[s] = 0.0;
                gVeh[s][vsKey] = false;
                RW_Veh_SetKey(vehicleid, false);
                for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == vehicleid && GetPlayerVehicleSeat(i) == 0) RW_Error(i, "Out of fuel. A mechanic on duty can refuel you, or tow the car to a gas station.");
            }
        }
        /* предел скорости от двигателя и подкрутка от коробки и тормозов */
        new driver = -1;
        for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == vehicleid && GetPlayerVehicleSeat(i) == 0) { driver = i; break; }
        new Float:limit = RW_TuneMaxSpeed(vehicleid);
        if (limit > 0.0 && kmh > limit + 2.0) {
            new Float:scale = limit / kmh;
            SetVehicleVelocity(vehicleid, vx*scale, vy*scale, vz*scale);
        } else if (driver >= 0 && kmh > 2.0 && kmh < limit) {
            new keys, updown, leftright;
            GetPlayerKeys(driver, keys, updown, leftright);
            new gearbox = gTune[s][TUNE_GEARBOX], brakes = gTune[s][TUNE_BRAKES];
            if (gearbox > 0 && (keys & KEY_UP)) {
                new Float:gain = 1.0 + 0.004 * float(gearbox);
                SetVehicleVelocity(vehicleid, vx*gain, vy*gain, vz*gain);
            } else if (brakes > 0 && (keys & KEY_DOWN)) {
                new Float:loss = 1.0 - 0.01 * float(brakes);
                SetVehicleVelocity(vehicleid, vx*loss, vy*loss, vz*loss);
            }
        }
    }
    return 1;
}
