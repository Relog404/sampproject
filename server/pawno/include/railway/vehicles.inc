/*  ============================================================================
 *  railway/vehicles.inc
 *  Персональные машины: спавн по /spcar, владение, ключ в замке, двери,
 *  блокировка движения для чужих и без ключа.
 *
 *  Правила:
 *    • ехать можно только владельцу, у которого вставлен ключ;
 *    • при закрытых дверях в машину сесть нельзя;
 *    • при открытых дверях без ключа сесть можно (пассажиром или за руль),
 *      но двигатель не заводится и машина не едет;
 *    • вставка ключа закрывает двери и заводит двигатель (только владелец).
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
forward RW_Veh_RespawnLater(vehicleid);

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

/* ------------------------------------------------------------------ синхронизация */

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
        /* Машина не из нашей системы — ничего не ограничиваем. */
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
            RW_Info(playerid, "Ключ не вставлен. Вставьте ключ командой /key.");
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
            /* Владелец вышел — ключ остаётся, двигатель глохнет. */
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
        RW_Error(playerid, "Неверный ID. Допустимы стандартные модели 400–611 (без поездов и прицепов).");
        return 0;
    }

    if (GetTickCount() - gLastSpcarTick[playerid] < SPCAR_COOLDOWN_MS) {
        RW_Info(playerid, "Не так быстро — подождите секунду.");
        return 0;
    }
    gLastSpcarTick[playerid] = GetTickCount();

    new oldSlot = gPlayerVehSlot[playerid];
    if (oldSlot >= 0 && gVeh[oldSlot][vsUsed]) {
        RW_Veh_DestroySlot(oldSlot, false);
    }

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
        RW_Error(playerid, "Нет свободных мест для машин на сервере.");
        return 0;
    }

    new c1 = gVehicleColors[random(sizeof gVehicleColors)];
    new c2 = gVehicleColors[random(sizeof gVehicleColors)];

    new vehicleid = CreateVehicle(model, sx, sy, z + 0.25, a, c1, c2, -1);
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid >= MAX_VEHICLES) {
        RW_Error(playerid, "Эту модель нельзя создать. Проверьте ID (400–611).");
        return 0;
    }

    if (GetVehicleModel(vehicleid) != model) {
        DestroyVehicle(vehicleid);
        RW_Error(playerid, "Модель не поддерживается сервером.");
        return 0;
    }

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
    format(string, sizeof string, "Машина {F5B04C}%s{FFFFFF} (ID %d) создана. Владелец — вы.", vname, model);
    SendClientMessage(playerid, COLOR_WHITE, string);
    RW_Info(playerid, "{F5B04C}/door{FFFFFF} — открыть двери, {F5B04C}/key{FFFFFF} — вставить ключ и завести двигатель.");

    return 1;
}

stock RW_Veh_DestroySlot(slot, bool:notifyOwner)
{
    if (slot < 0 || slot >= VEHICLE_SLOTS || !gVeh[slot][vsUsed]) {
        return 0;
    }

    new vehicleid = gVeh[slot][vsVehicleId];
    new owner = gVeh[slot][vsOwnerPlayer];

    if (vehicleid != INVALID_VEHICLE_ID && vehicleid < MAX_VEHICLES) {
        if (gVehSlotOf[vehicleid] == slot) {
            gVehSlotOf[vehicleid] = -1;
        }
        DestroyVehicle(vehicleid);
    }

    if (owner >= 0 && owner < MAX_PLAYERS && gPlayerVehSlot[owner] == slot) {
        gPlayerVehSlot[owner] = -1;
        if (notifyOwner && IsPlayerConnected(owner)) {
            RW_Info(owner, "Ваша машина убрана.");
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

forward RW_Veh_RespawnLater(vehicleid);
public RW_Veh_RespawnLater(vehicleid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot >= 0 && gVeh[slot][vsUsed]) {
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
    /* Новому игроку нужно заново выставить замки на все персональные машины. */
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
        RW_Error(playerid, "Недостаточно прав для этой команды.");
        return 1;
    }

    if (params[0] == EOS) {
        RW_Info(playerid, "Укажите ID модели: {F5B04C}/spcar 411{FFFFFF} (Infernus), /spcar 541 (Bullet), /spcar 560 (Sultan).");
        RW_Info(playerid, "Все стандартные ID: 400–611. Ваша машина — только у вас.");
        return 1;
    }

    if (!RW_IsNumeric(params)) {
        RW_Error(playerid, "ID должен быть числом, например: /spcar 411");
        return 1;
    }

    new model = strval(params);
    return RW_Veh_Spawn(playerid, model);
}

stock RW_Veh_CommandKey(playerid)
{
    if (!gLogged[playerid]) {
        return 0;
    }

    new vehicleid = RW_Veh_NearOwnedVehicle(playerid, KEY_RANGE + 2.0);
    if (vehicleid == -1) {
        RW_Error(playerid, "Рядом нет вашей машины. Подойдите ближе (до 6 м) или сядьте в неё.");
        return 1;
    }

    new slot = gVehSlotOf[vehicleid];

    if (gVeh[slot][vsKey]) {
        gVeh[slot][vsKey] = false;
        gVeh[slot][vsEngine] = false;
        RW_Veh_Sync(vehicleid);
        RW_Veh_SyncAll(vehicleid);
        RW_Info(playerid, "Ключ вынут. Двигатель заглушен.");
    } else {
        RW_Veh_SetKey(vehicleid, true);
        RW_Ok(playerid, "Ключ вставлен. Нажмите E за рулём, чтобы завести.");
    }
    return 1;
}

stock RW_Veh_CommandDoor(playerid)
{
    if (!gLogged[playerid]) {
        return 0;
    }

    new vehicleid = RW_Veh_NearOwnedVehicle(playerid, KEY_RANGE + 2.0);
    if (vehicleid == -1) {
        RW_Error(playerid, "Рядом нет вашей машины. Подойдите ближе (до 6 м).");
        return 1;
    }

    new slot = gVehSlotOf[vehicleid];

    if (gVeh[slot][vsDoors]) {
        RW_Veh_SetDoors(vehicleid, false);
        RW_Info(playerid, "Двери закрыты. В машину больше никто не сядет.");
    } else {
        RW_Veh_SetDoors(vehicleid, true);
        RW_Ok(playerid, "Двери открыты. Сесть может любой, но поехать — только вы с ключом.");
    }
    return 1;
}

stock RW_Veh_CommandMyCar(playerid)
{
    if (!gLogged[playerid]) {
        return 0;
    }

    new slot = gPlayerVehSlot[playerid];
    if (slot < 0 || !gVeh[slot][vsUsed]) {
        RW_Info(playerid, "У вас пока нет машины. Создайте её: {F5B04C}/spcar 411{FFFFFF}.");
        return 1;
    }

    new vehicleid = gVeh[slot][vsVehicleId];
    new vname[48];
    new string[192];

    RW_VehicleName(gVeh[slot][vsModel], vname, sizeof vname);

    format(string, sizeof string, "Машина: {F5B04C}%s{FFFFFF} (ID %d, номер RW-%03d)",
        vname, gVeh[slot][vsModel], slot + 1);
    SendClientMessage(playerid, COLOR_WHITE, string);

    format(string, sizeof string, "Ключ: %s   •   Двери: %s   •   Двигатель: %s",
        gVeh[slot][vsKey] ? "{7BD88F}в замке" : "{FF6B6B}нет",
        gVeh[slot][vsDoors] ? "{7BD88F}открыты" : "{FF6B6B}закрыты",
        gVeh[slot][vsEngine] ? "{7BD88F}работает" : "{8A94A2}заглушен");
    SendClientMessage(playerid, COLOR_GREY, string);

    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    if (!IsPlayerInRangeOfPoint(playerid, 30.0, x, y, z)) {
        RW_Info(playerid, "Машина находится в другой части города.");
    }
    return 1;
}
