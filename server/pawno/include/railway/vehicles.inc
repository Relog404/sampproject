/*  ============================================================================
 *  railway/vehicles.inc
 *  ѕерсональные машины: спавн по /spcar, владение, ключ в замке, двери,
 *  блокировка движени€ дл€ чужих и без ключа.
 *
 *  ѕравила:
 *    Х ехать можно только владельцу, у которого вставлен ключ;
 *    Х при закрытых двер€х в машину сесть нельз€;
 *    Х при открытых двер€х без ключа сесть можно (пассажиром или за руль),
 *      но двигатель не заводитс€ и машина не едет;
 *    Х вставка ключа закрывает двери и заводит двигатель (только владелец).
 *  ========================================================================= */

#if defined _railway_vehicles_included
    #endinput
#endif
#define _railway_vehicles_included

#include <railway/core>
#include <railway/ui>
#include <railway/accounts>

#define RW_IS_TRAILER_OR_TRAIN(%0) \
    ((%0) == 449 || (%0) == 450 || (%0) == 537 || (%0) == 538 || (%0) == 569 || (%0) == 570 || \
     (%0) == 584 || (%0) == 590 || (%0) == 591 || (%0) == 592 || (%0) == 593 || (%0) == 606 || \
     (%0) == 607 || (%0) == 608 || (%0) == 610 || (%0) == 611)

static const gVehicleColors[] = {
    0, 1, 2, 3, 6, 8, 12, 23, 40, 41, 46, 52, 79, 88, 91, 100, 116, 123, 126, 137
};

/* ------------------------------------------------------------------ прототипы */
forward RW_Veh_Sync(vehicleid);
forward RW_Veh_SyncForPlayer(vehicleid, playerid);
forward RW_Veh_SyncAll(vehicleid);
forward RW_Veh_SetDoors(vehicleid, bool:open);
forward RW_Veh_SetKey(vehicleid, bool:inserted);
forward RW_Veh_DestroySlot(slot, bool:notifyOwner);
forward bool:RW_Veh_CanDrive(playerid, vehicleid);
forward RW_Veh_Enforce(playerid);
forward RW_Veh_OnEnterDriver(playerid, vehicleid);
forward RW_Veh_OnExitDriver(playerid);
forward RW_Veh_RespawnLater(vehicleid, token);

/* ------------------------------------------------------------------ слоты */

stock RW_Veh_GetFreeSlot()
{
    for (new i = 0; i < VEHICLE_SLOTS; i++) {
        if (!gVeh[i][vsUsed]) {
            return i;
        }
    }
    return -1;
}

stock RW_Veh_SlotByOwner(playerid)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS) {
        return -1;
    }
    return gPlayerVehSlot[playerid];
}

stock RW_Veh_NearOwnedVehicle(playerid, Float:range = KEY_RANGE)
{
    new slot = gPlayerVehSlot[playerid];
    if (slot < 0) {
        return -1;
    }
    if (!gVeh[slot][vsUsed]) {
        return -1;
    }

    new vehicleid = gVeh[slot][vsVehicleId];
    if (vehicleid == INVALID_VEHICLE_ID) {
        return -1;
    }

    if (IsPlayerInAnyVehicle(playerid) && GetPlayerVehicleID(playerid) == vehicleid) {
        return vehicleid;
    }

    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    if (IsPlayerInRangeOfPoint(playerid, range, x, y, z)) {
        return vehicleid;
    }
    return -1;
}

/* ------------------------------------------------------------------ синхронизаци€ */

stock RW_Veh_IsLocked(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) {
        return 0;
    }
    if (gVeh[slot][vsDoors]) {
        return 0;
    }
    return 1;
}

stock RW_Veh_SyncForPlayer(vehicleid, playerid)
{
    if (!IsPlayerConnected(playerid)) {
        return 0;
    }
    SetVehicleParamsForPlayer(vehicleid, playerid, 0, RW_Veh_IsLocked(vehicleid));
    return 1;
}

stock RW_Veh_SyncAll(vehicleid)
{
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (IsPlayerConnected(i)) {
            RW_Veh_SyncForPlayer(vehicleid, i);
        }
    }
    return 1;
}

stock RW_Veh_Sync(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0 || !gVeh[slot][vsUsed]) {
        return 0;
    }

    new bool:open = gVeh[slot][vsDoors];
    new engine = gVeh[slot][vsEngine] ? 1 : 0;
    new locked = RW_Veh_IsLocked(vehicleid);

    SetVehicleParamsEx(vehicleid, engine, gVeh[slot][vsLights] ? 1 : 0, 0, locked, 0, 0, 0);
    SetVehicleParamsCarDoors(vehicleid, open ? 1 : 0, open ? 1 : 0, open ? 1 : 0, open ? 1 : 0);

    return 1;
}

/* ------------------------------------------------------------------ двери и ключ */

stock RW_Veh_SetDoors(vehicleid, bool:open)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0) {
        return 0;
    }

    gVeh[slot][vsDoors] = open;
    RW_Veh_Sync(vehicleid);
    RW_Veh_SyncAll(vehicleid);
    return 1;
}

stock RW_Veh_SetKey(vehicleid, bool:inserted)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot < 0) {
        return 0;
    }

    gVeh[slot][vsKey] = inserted;
    if (!inserted) gVeh[slot][vsEngine] = false;

    RW_Veh_Sync(vehicleid);
    RW_Veh_SyncAll(vehicleid);
    return 1;
}

/* ------------------------------------------------------------------ право на движение */

stock bool:RW_Veh_CanDrive(playerid, vehicleid)
{
    new slot = gVehSlotOf[vehicleid];

    if (slot < 0 || !gVeh[slot][vsUsed]) {
        return false;
    }
    if (!gLogged[playerid]) {
        return false;
    }
    if (!gVeh[slot][vsKey] || !gVeh[slot][vsEngine]) {
        return false;
    }
    return true;
}

stock RW_Veh_HudFlags(playerid, vehicleid)
{
    new flags = 0;
    new slot = gVehSlotOf[vehicleid];

    if (slot >= 0 && gVeh[slot][vsUsed]) {
        if (gVeh[slot][vsEngine])  flags |= 1;
        if (gVeh[slot][vsKey])     flags |= 2;
        if (gVeh[slot][vsDoors])   flags |= 4;
        if (gVeh[slot][vsOwnerAcc] == gAccountId[playerid]) {
            flags |= 8;
        }
    }
    if (gNoDrive[playerid]) {
        flags |= 16;
    }
    return flags;
}

stock RW_Veh_Enforce(playerid)
{
    if (!IsPlayerConnected(playerid) || !gLogged[playerid]) {
        gNoDrive[playerid] = false;
        return 0;
    }
    if (!IsPlayerInAnyVehicle(playerid) || GetPlayerVehicleSeat(playerid) != 0) {
        gNoDrive[playerid] = false;
        return 0;
    }

    new vehicleid = GetPlayerVehicleID(playerid);
    new slot = gVehSlotOf[vehicleid];

    if (slot < 0 || !gVeh[slot][vsUsed]) {
        /* ћашина не из нашей системы Ч ничего не ограничиваем. */
        gNoDrive[playerid] = false;
        return 0;
    }

    if (RW_Veh_CanDrive(playerid, vehicleid)) {
        if (gNoDrive[playerid]) {
            gNoDrive[playerid] = false;
            RW_Veh_Sync(vehicleid);
        }
        return 1;
    }

    if (!gNoDrive[playerid]) {
        gNoDrive[playerid] = true;
        gVeh[slot][vsEngine] = false;
        RW_Veh_Sync(vehicleid);
    }

    new Float:vx, Float:vy, Float:vz;
    GetVehicleVelocity(vehicleid, vx, vy, vz);
    if (floatsqroot(vx * vx + vy * vy + vz * vz) * 180.0 > 1.0) {
        SetVehicleVelocity(vehicleid, 0.0, 0.0, 0.0);
    }

    if (GetTickCount() - gEnforceLastTick[playerid] > 4000) {
        gEnforceLastTick[playerid] = GetTickCount();

        if (!gVeh[slot][vsKey]) {
            RW_Info(playerid, "No key in the ignition. Use /key to insert it.");
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ вход/выход */

stock RW_Veh_OnEnterDriver(playerid, vehicleid)
{
    new slot = gVehSlotOf[vehicleid];

    RW_UI_SpeedoShow(playerid, vehicleid);

    if (slot < 0 || !gVeh[slot][vsUsed]) {
        return 1;
    }

    gNoDrive[playerid] = !RW_Veh_CanDrive(playerid, vehicleid);
    RW_Veh_Enforce(playerid);
    return 1;
}

stock RW_Veh_OnExitDriver(playerid)
{
    new vehicleid = gSpeedoVehicle[playerid];

    gNoDrive[playerid] = false;
    RW_UI_SpeedoHide(playerid);

    if (vehicleid != INVALID_VEHICLE_ID) {
        new slot = gVehSlotOf[vehicleid];
        if (slot >= 0 && gVeh[slot][vsUsed]) {
            /* ¬ладелец вышел Ч ключ остаЄтс€, двигатель глохнет. */
            gVeh[slot][vsEngine] = false;
            RW_Veh_Sync(vehicleid);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ спавн/удаление */

stock RW_Veh_Spawn(playerid, model)
{
    if (model < 400 || model > 611 || RW_IS_TRAILER_OR_TRAIN(model)) {
        RW_Error(playerid, "Invalid ID. Use supported models 400-611 (no trains or trailers).");
        return 0;
    }

    if (GetTickCount() - gLastSpcarTick[playerid] < SPCAR_COOLDOWN_MS) {
        RW_Info(playerid, "Too fast. Wait a second.");
        return 0;
    }
    gLastSpcarTick[playerid] = GetTickCount();

    new oldSlot = gPlayerVehSlot[playerid];

    new Float:x, Float:y, Float:z, Float:a;

    if (IsPlayerInAnyVehicle(playerid)) {
        new cur = GetPlayerVehicleID(playerid);
        GetVehiclePos(cur, x, y, z);
        GetVehicleZAngle(cur, a);
    } else {
        GetPlayerPos(playerid, x, y, z);
        GetPlayerFacingAngle(playerid, a);
    }

    new Float:sx = x + 4.4 * floatsin(-a, degrees);
    new Float:sy = y + 4.4 * floatcos(-a, degrees);

    new slot = RW_Veh_GetFreeSlot();
    if (slot < 0) {
        RW_Error(playerid, "Server vehicle limit reached.");
        return 0;
    }

    new c1 = gVehicleColors[random(sizeof gVehicleColors)];
    new c2 = gVehicleColors[random(sizeof gVehicleColors)];

    new vehicleid = CreateVehicle(model, sx, sy, z + 0.25, a, c1, c2, -1);
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid >= MAX_VEHICLES) {
        RW_Error(playerid, "Cannot create this model. Check ID (400-611).");
        return 0;
    }

    if (GetVehicleModel(vehicleid) != model) {
        DestroyVehicle(vehicleid);
        RW_Error(playerid, "Model not supported by this server.");
        return 0;
    }

    new keyUid = RW_InvGrant(playerid, ITEM_CAR_KEY);
    if (!keyUid) {
        DestroyVehicle(vehicleid);
        RW_Error(playerid, "A key needs a free main inventory slot and a writable database.");
        return 0;
    }
    if (oldSlot >= 0 && gVeh[oldSlot][vsUsed]) RW_Veh_DestroySlot(oldSlot, false);
    gVehKeyUid[slot] = keyUid;
    gVehGarageId[slot] = 0;
    gVehSpawnStamp[slot] = ++gVehStampCounter;
    RW_MotionReset(vehicleid);

    new nick[MAX_PLAYER_NAME];
    new plate[16];

    RW_GetNick(playerid, nick);

    gVeh[slot][vsUsed] = 1;
    gVeh[slot][vsVehicleId] = vehicleid;
    gVeh[slot][vsOwnerAcc] = gAccountId[playerid];
    gVeh[slot][vsOwnerPlayer] = playerid;
    gVeh[slot][vsModel] = model;
    gVeh[slot][vsLights] = false;
    gVeh[slot][vsKey] = false;
    gVeh[slot][vsDoors] = false;
    gVeh[slot][vsEngine] = false;
    format(gVeh[slot][vsOwnerName], MAX_PLAYER_NAME, "%s", nick);

    gVehSlotOf[vehicleid] = slot;
    gPlayerVehSlot[playerid] = slot;
    gVehSlotCount++;

    format(plate, sizeof plate, "RW-%03d", slot + 1);
    SetVehicleNumberPlate(vehicleid, plate);
    SetVehicleHealth(vehicleid, 1000.0);

    RW_Veh_Sync(vehicleid);
    RW_Veh_SyncAll(vehicleid);

    new string[192];
    new vname[48];
    RW_VehicleName(model, vname, sizeof vname);
    format(string, sizeof string, "Car {F5B04C}%s{FFFFFF} (model %d) created. You own it.", vname, model);
    SendClientMessage(playerid, COLOR_WHITE, string);
    RW_Info(playerid, "{F5B04C}/door{FFFFFF} - unlock; /key - insert key; N - start engine.");
    if(gUIMode[playerid]==UI_INVENTORY) RW_InvRender(playerid);

    return 1;
}

stock RW_Veh_DestroySlot(slot, bool:notifyOwner)
{
    if (slot < 0 || slot >= VEHICLE_SLOTS || !gVeh[slot][vsUsed]) {
        return 0;
    }

    new vehicleid = gVeh[slot][vsVehicleId];
    new owner = gVeh[slot][vsOwnerPlayer];
    if (!gVehGarageId[slot]) RW_InvRevokeKey(gVehKeyUid[slot]);
    gVehGarageId[slot] = 0;
    RW_MotionReset(vehicleid);
    gVehKeyUid[slot] = 0;

    if (vehicleid != INVALID_VEHICLE_ID && vehicleid < MAX_VEHICLES) {
        if (gVehSlotOf[vehicleid] == slot) {
            gVehSlotOf[vehicleid] = -1;
        }
        DestroyVehicle(vehicleid);
    }

    if (owner >= 0 && owner < MAX_PLAYERS && gPlayerVehSlot[owner] == slot) {
        gPlayerVehSlot[owner] = -1;
        if (notifyOwner && IsPlayerConnected(owner)) {
            RW_Info(owner, "Your active car was removed.");
        }
    }

    gVeh[slot][vsUsed] = 0;
    gVeh[slot][vsVehicleId] = INVALID_VEHICLE_ID;
    gVeh[slot][vsOwnerAcc] = -1;
    gVeh[slot][vsOwnerPlayer] = -1;
    gVeh[slot][vsLights] = false;
    gVeh[slot][vsKey] = false;
    gVeh[slot][vsDoors] = false;
    gVeh[slot][vsEngine] = false;

    if (gVehSlotCount > 0) {
        gVehSlotCount--;
    }
    return 1;
}

forward RW_Veh_RespawnLater(vehicleid, token);
public RW_Veh_RespawnLater(vehicleid, token)
{
    if (vehicleid <= 0 || vehicleid >= MAX_VEHICLES || token <= 0) return 1;
    new slot = gVehSlotOf[vehicleid];
    if (slot >= 0 && gVeh[slot][vsUsed] && gVehSpawnStamp[slot] == token) {
        RW_Veh_DestroySlot(slot, true);
    }
    return 1;
}

stock RW_Veh_OnPlayerDisconnect(playerid)
{
    new slot = gPlayerVehSlot[playerid];
    if (slot >= 0 && gVeh[slot][vsUsed]) {
        RW_Veh_DestroySlot(slot, false);
    }
    gPlayerVehSlot[playerid] = -1;
    gNoDrive[playerid] = false;
    return 1;
}

stock RW_Veh_OnPlayerSpawn(playerid)
{
    /* Ќовому игроку нужно заново выставить замки на все персональные машины. */
    for (new i = 0; i < VEHICLE_SLOTS; i++) {
        if (gVeh[i][vsUsed]) {
            RW_Veh_SyncForPlayer(gVeh[i][vsVehicleId], playerid);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ /spcar и т.п. */

stock RW_Veh_CommandSpcar(playerid, const params[])
{
    if (!gLogged[playerid]) {
        return 0;
    }

    if (gAdminLevel[playerid] < SPCAR_MIN_ADMIN_LEVEL) {
        RW_Error(playerid, "Insufficient permissions.");
        return 1;
    }

    if (params[0] == EOS) {
        RW_Info(playerid, "Enter a model ID: {F5B04C}/spcar 411{FFFFFF} (Infernus), /spcar 541 (Bullet), /spcar 560 (Sultan).");
        RW_Info(playerid, "Supported IDs: 400-611. /spcar cars and keys are temporary.");
        return 1;
    }

    if (!RW_IsNumeric(params)) {
        RW_Error(playerid, "ID must be a number, e.g. /spcar 411");
        return 1;
    }

    new model = strval(params);
    return RW_Veh_Spawn(playerid, model);
}

stock RW_Veh_CommandKey(playerid)
{
    return RW_ControlExecute(playerid, RW_ControlTarget(playerid), 2);
}
stock RW_Veh_CommandDoor(playerid)
{
    return RW_ControlExecute(playerid, RW_ControlTarget(playerid), 0);
}

stock RW_Veh_ListCommand(playerid)
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    new line[160], vname[32], count;
    RW_Accent(playerid, "--- {F5B04C}Managed cars{FFFFFF} ---");
    for (new slot; slot < VEHICLE_SLOTS && count < 10; slot++) {
        if (!gVeh[slot][vsUsed]) continue;
        RW_VehicleName(gVeh[slot][vsModel], vname, sizeof vname);
        format(line, sizeof line, "id %d | RW-%03d | %s (%d) | key %d | %s | %s", gVeh[slot][vsVehicleId], slot+1, vname, gVeh[slot][vsModel], gVehKeyUid[slot], gVehGarageId[slot] ? ("permanent") : ("temporary"), gVeh[slot][vsOwnerName]);
        SendClientMessage(playerid, COLOR_GREY, line);
        count++;
    }
    if (!count) SendClientMessage(playerid, COLOR_GREY, "No spawned managed cars. /spcar <model> or /givecar <name> <model>.");
    new DBResult:r = db_query(gDB, "SELECT id,owner,model,token FROM owned_cars ORDER BY id");
    new stored;
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows && stored < 10; i++) {
            new id = db_get_field_assoc_int(r, "id"), model = db_get_field_assoc_int(r, "model"), token = db_get_field_assoc_int(r, "token"), owner = db_get_field_assoc_int(r, "owner"), bool:spawned;
            for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == token) { spawned = true; break; }
            if (!spawned) {
                RW_VehicleName(model, vname, sizeof vname);
                format(line, sizeof line, "c%d | %s (%d) | key %d | stored in the garage | account %d", id, vname, model, token, owner);
                SendClientMessage(playerid, COLOR_DARK_GREY, line);
                stored++;
            }
            db_next_row(r);
        }
        db_free_result(r);
    }
    SendClientMessage(playerid, COLOR_DARK_GREY, "Give a key: /giveitem <playerid> key <id> (or p<plate>, c<garage car id>, or aim and omit it).");
    return 1;
}

stock RW_Veh_CommandMyCar(playerid)
{
    if (!gLogged[playerid]) {
        return 0;
    }

    new slot = gPlayerVehSlot[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) {
        RW_Info(playerid, "No temporary car. Use {F5B04C}/spcar 411{FFFFFF}, or visit the garage for permanent cars.");
        return 1;
    }

    new vehicleid = gVeh[slot][vsVehicleId];
    new vname[48];
    new string[192];

    RW_VehicleName(gVeh[slot][vsModel], vname, sizeof vname);

    format(string, sizeof string, "Car: {F5B04C}%s{FFFFFF} (model %d, plate RW-%03d)",
        vname, gVeh[slot][vsModel], slot + 1);
    SendClientMessage(playerid, COLOR_WHITE, string);

    format(string, sizeof string, "Key: %s / Doors: %s / Engine: %s",
        gVeh[slot][vsKey] ? "{7BD88F}inserted" : "{FF6B6B}none",
        gVeh[slot][vsDoors] ? "{7BD88F}open" : "{FF6B6B}locked",
        gVeh[slot][vsEngine] ? "{7BD88F}running" : "{8A94A2}off");
    SendClientMessage(playerid, COLOR_GREY, string);

    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    if (!IsPlayerInRangeOfPoint(playerid, 30.0, x, y, z)) {
        RW_Info(playerid, "Car is elsewhere in the city.");
    }
    return 1;
}
