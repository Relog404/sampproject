#if defined _railway_garage_included
    #endinput
#endif
#define _railway_garage_included
// The booth and its pickup stand right next to the spawn point so the car
// selection is the first thing a player sees after spawning.
#define GARAGE_X (STATION_X + 2.5)
#define GARAGE_Y (STATION_Y + 2.4)
#define GARAGE_Z (STATION_Z + 0.35)
#define GARAGE_BOOTH_X (STATION_X + 3.6)
#define GARAGE_BOOTH_Y (STATION_Y + 3.4)
#define GARAGE_BOOTH_Z (STATION_Z)
#define GARAGE_RADIUS (6.0)
/* Площадка выдачи: улица восточнее вокзала Unity Station. */
#define GARAGE_PAD_X (1755.0)
#define GARAGE_PAD_Y (-1855.0)
#define GARAGE_PAD_Z (13.6)
new gGaragePickup = -1, gGarageBooth = INVALID_OBJECT_ID;
new Text3D:gGarageLabel = Text3D:INVALID_3DTEXT_ID;
new gKeyTarget[MAX_PLAYERS], gKeyCount[MAX_PLAYERS], gKeyPage[MAX_PLAYERS], gKeyTick[MAX_PLAYERS];
new gKeyList[MAX_PLAYERS][60];
new bool:gGarageReady, bool:gGarageLatch[MAX_PLAYERS];
new gGarageList[MAX_PLAYERS][80], gGarageCount[MAX_PLAYERS], gGaragePage[MAX_PLAYERS], gGarageTick[MAX_PLAYERS];

stock RW_GarageSchema()
{
    if (!gDBWritable) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS owned_cars (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER NOT NULL, model INTEGER NOT NULL, token INTEGER NOT NULL UNIQUE)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS permanent_keys (uid INTEGER PRIMARY KEY, car INTEGER NOT NULL)")) return 0;
    gGarageReady = true;
    return 1;
}
stock bool:RW_GarageMarkKey(uid, token)
{
    if (!gGarageReady) return false;
    new query[240];
    format(query, sizeof query, "INSERT INTO permanent_keys(uid,car) SELECT %d,id FROM owned_cars WHERE token=%d", uid, token);
    return RW_DB_Exec(query);
}
stock bool:RW_InvHasToken(playerid, token)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid] || token <= 0) return false;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_CAR_KEY && gInv[playerid][i][iiData] == token) return true;
    return false;
}
stock RW_GarageInit()
{
    if (!gGarageReady || !gInvReady) return 0;
    gGarageBooth = CreateObject(3881, GARAGE_BOOTH_X, GARAGE_BOOTH_Y, GARAGE_BOOTH_Z, 0.0, 0.0, 90.0, 150.0);
    gGaragePickup = CreatePickup(1239, 1, GARAGE_X, GARAGE_Y, GARAGE_Z, 0);
    gGarageLabel = Create3DTextLabel("STATION GARAGE\nStep inside to pick a car", 0xF5B04CFF, GARAGE_X, GARAGE_Y, GARAGE_Z + 1.2, 30.0, 0, 0);
    if (gGarageBooth == INVALID_OBJECT_ID) print("[garage] Booth object unavailable; proximity entry still active.");
    if (gGaragePickup == -1) print("[garage] Pickup unavailable; proximity entry still active.");
    if (gGarageLabel == Text3D:INVALID_3DTEXT_ID) print("[garage] Booth label unavailable; booth and pickup still active.");
    return 1;
}
/* Список машин рисует страница: скрывать больше нечего. */
stock RW_GarageHide(playerid)
{
    #pragma unused playerid
    return 1;
}
stock RW_GarageReset(playerid)
{
    gGarageLatch[playerid] = false; gGaragePage[playerid] = 0; gGarageCount[playerid] = 0; gGarageTick[playerid] = 0;
    return 1;
}
stock bool:RW_GarageNear(playerid)
{
    return gLogged[playerid] && gInvLoaded[playerid] && !gInvFitting[playerid] && GetPlayerState(playerid) == PLAYER_STATE_ONFOOT && GetPlayerInterior(playerid) == 0 && GetPlayerVirtualWorld(playerid) == 0 && IsPlayerInRangeOfPoint(playerid, GARAGE_RADIUS, GARAGE_X, GARAGE_Y, GARAGE_Z);
}
stock RW_GarageOpen(playerid, bool:resetPage = true)
{
    if (!RW_GarageNear(playerid) || !gGarageReady) return 1;
    if (gUIInput[playerid]) return 1;
    RW_UI_Close(playerid, true);
    if (resetPage) gGaragePage[playerid] = 0;
    gGarageCount[playerid] = 0;
    new query[400]; format(query, sizeof query, "SELECT DISTINCT c.id,c.model FROM owned_cars c JOIN inventory_items i ON i.data=c.token AND i.type=2 WHERE i.owner=%d ORDER BY c.id LIMIT 80", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Error(playerid, "Garage database unavailable.");
    new count = db_num_rows(r);
    for (new i; i < count; i++) { gGarageList[playerid][gGarageCount[playerid]++] = db_get_field_assoc_int(r, "id"); db_next_row(r); }
    db_free_result(r);
    for (new s; s < VEHICLE_SLOTS && gGarageCount[playerid] < 80; s++)
        if (gVeh[s][vsUsed] && !gVehGarageId[s] && RW_InvHasToken(playerid, gVehKeyUid[s])) gGarageList[playerid][gGarageCount[playerid]++] = -gVehKeyUid[s];
    if (gGaragePage[playerid]*6 >= gGarageCount[playerid]) gGaragePage[playerid] = 0;
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_GarageCef(playerid);
    #endif
    return RW_GarageNative(playerid);
}
/*  Гараж в браузере: шесть машин на странице и кнопки по краям. */
stock RW_GarageCefModel(playerid, index, name[], size = sizeof name)
{
    new id = gGarageList[playerid][index], model;
    if (id > 0) {
        new query[128], DBResult:r;
        format(query, sizeof query, "SELECT model FROM owned_cars WHERE id=%d", id);
        r = db_query(gDB, query);
        if (r != DBResult:0) { model = db_get_field_assoc_int(r, "model"); db_free_result(r); }
    }
    else for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == -id) model = gVeh[s][vsModel];
    RW_VehicleName(model, name, size);
    return model;
}
stock RW_GarageCef(playerid)
{
    new subtitle[64], label[128], name[48], previewName[48], previewModel;
    format(subtitle, sizeof subtitle, "%d машин / страница %d", gGarageCount[playerid], gGaragePage[playerid] + 1);
    if (!RW_CefOpen(playerid, CEF_MODE_GARAGE, "Гараж", subtitle)) return 0;
    for (new row; row < 6; row++) {
        new idx = gGaragePage[playerid] * 6 + row;
        if (idx >= gGarageCount[playerid]) continue;
        name[0] = EOS;
        new model = RW_GarageCefModel(playerid, idx, name, sizeof name);
        if (row == 0) { previewModel = model; format(previewName, sizeof previewName, "%s", name); }
        format(label, sizeof label, "%s", name);
        RW_CefRow(playerid, row, label, gGarageList[playerid][idx] > 0 ? ("в собственности") : ("временный ключ"), "car");
    }
    if (previewModel >= 400 && previewModel <= 611) RW_CefPreview(playerid, previewModel, previewName);
    if (gGaragePage[playerid] > 0) RW_CefAction(playerid, CEF_GARAGE_PREV, "Назад", "");
    if ((gGaragePage[playerid] + 1) * 6 < gGarageCount[playerid]) RW_CefAction(playerid, CEF_GARAGE_NEXT, "Вперёд", "");
    RW_CefAction(playerid, CEF_GARAGE_CLOSE, "Закрыть", "danger");
    return RW_CefEnd(playerid);
}

/* Без браузера: тот же список нативным диалогом, те же номера строк. */
stock RW_GarageNative(playerid)
{
    new subtitle[64], label[128], name[48];
    format(subtitle, sizeof subtitle, "%d машин / страница %d", gGarageCount[playerid], gGaragePage[playerid] + 1);
    RW_UI_Panel(playerid, UI_GARAGE, "Гараж", subtitle);
    for (new row; row < 6; row++) {
        new idx = gGaragePage[playerid] * 6 + row;
        if (idx >= gGarageCount[playerid]) continue;
        name[0] = EOS;
        RW_GarageCefModel(playerid, idx, name, sizeof name);
        format(label, sizeof label, "%s", name);
        RW_UI_Row(playerid, row, label);
    }
    if (gGaragePage[playerid] > 0) RW_UI_Row(playerid, CEF_GARAGE_PREV, "Назад");
    if ((gGaragePage[playerid] + 1) * 6 < gGarageCount[playerid]) RW_UI_Row(playerid, CEF_GARAGE_NEXT, "Вперёд");
    RW_UI_Row(playerid, CEF_GARAGE_CLOSE, "Закрыть");
    return 1;
}
/* Перерисовать: браузер или тексдрав-меню для тех, у кого браузера нет. */
/* Перерисовать: страница или нативный диалог. */
stock RW_GarageRefresh(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_GarageCef(playerid);
    #endif
    return RW_GarageNative(playerid);
}
/* Клик по странице: машина, пролистывание или закрыть. */
stock RW_GarageCefClick(playerid, id)
{
    if (gUIMode[playerid] != UI_GARAGE || !RW_GarageNear(playerid)) return RW_UI_Close(playerid);
    if (id == CEF_GARAGE_CLOSE) return RW_UI_Close(playerid);
    if (GetTickCount() - gGarageTick[playerid] < 1000) return 1;
    gGarageTick[playerid] = GetTickCount();
    if (id == CEF_GARAGE_PREV) { if (gGaragePage[playerid] > 0) gGaragePage[playerid]--; return RW_GarageRefresh(playerid); }
    if (id == CEF_GARAGE_NEXT) { if ((gGaragePage[playerid] + 1) * 6 < gGarageCount[playerid]) gGaragePage[playerid]++; return RW_GarageRefresh(playerid); }
    if (id >= 0 && id < 6) {
        new index = gGaragePage[playerid] * 6 + id;
        if (index < gGarageCount[playerid]) return RW_GarageSpawn(playerid, gGarageList[playerid][index]);
    }
    return 1;
}

stock RW_GarageSpawn(playerid, id)
{
    if (!RW_GarageNear(playerid)) return RW_UI_Close(playerid);
    new token, model, owner, existing = -1;
    if (id > 0) {
        new query[180]; format(query, sizeof query, "SELECT owner,model,token FROM owned_cars WHERE id=%d", id);
        new DBResult:r = db_query(gDB, query);
        if (r == DBResult:0) return 1;
        if (db_num_rows(r) == 1) { token = db_get_field_assoc_int(r, "token"); model = db_get_field_assoc_int(r, "model"); owner = db_get_field_assoc_int(r, "owner"); }
        db_free_result(r);
    } else token = -id;
    if (!RW_InvHasToken(playerid, token)) return RW_Error(playerid, "You no longer have this key.");
    for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == token) { existing = s; model = gVeh[s][vsModel]; owner = gVeh[s][vsOwnerAcc]; }
    if (id < 0 && existing < 0) return RW_Error(playerid, "Temporary car no longer exists.");
    if (model < 400 || model > 611 || RW_IS_TRAILER_OR_TRAIN(model)) return RW_Error(playerid, "Unsupported vehicle model.");
    new vehicle = existing < 0 ? INVALID_VEHICLE_ID : gVeh[existing][vsVehicleId];
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && IsPlayerInAnyVehicle(p) && GetPlayerVehicleID(p) == vehicle) return RW_Error(playerid, "Car is occupied.");
    for (new v = 1; v < MAX_VEHICLES; v++) {
        if (v == vehicle || !GetVehicleModel(v) || GetVehicleVirtualWorld(v) != 0) continue;
        new Float:x, Float:y, Float:z; GetVehiclePos(v, x, y, z);
        if (floatsqroot((x-GARAGE_PAD_X)*(x-GARAGE_PAD_X)+(y-GARAGE_PAD_Y)*(y-GARAGE_PAD_Y)+(z-GARAGE_PAD_Z)*(z-GARAGE_PAD_Z)) < 5.0) return RW_Error(playerid, "Spawn pad blocked. Move the other car first.");
    }
    new slot = existing;
    if (slot < 0) {
        slot = RW_Veh_GetFreeSlot();
        if (slot < 0) return RW_Error(playerid, "Vehicle limit reached.");
        vehicle = CreateVehicle(model, GARAGE_PAD_X, GARAGE_PAD_Y, GARAGE_PAD_Z, 90.0, 1, 1, -1);
        if (vehicle == INVALID_VEHICLE_ID || vehicle <= 0 || vehicle >= MAX_VEHICLES) return RW_Error(playerid, "Cannot create vehicle.");
        gVeh[slot][vsUsed] = 1; gVeh[slot][vsVehicleId] = vehicle; gVeh[slot][vsModel] = model;
        gVeh[slot][vsOwnerAcc] = owner; gVeh[slot][vsOwnerPlayer] = -1;
        for (new p; p < MAX_PLAYERS; p++) if (gLogged[p] && gAccountId[p] == owner) { gVeh[slot][vsOwnerPlayer] = p; RW_GetNick(p, gVeh[slot][vsOwnerName], MAX_PLAYER_NAME); }
        gVehKeyUid[slot] = token; gVehGarageId[slot] = id; gVehSlotOf[vehicle] = slot; gVehSlotCount++;
        RW_MotionReset(vehicle);
    }
    gVehSpawnStamp[slot] = ++gVehStampCounter;
    gVeh[slot][vsKey] = false; gVeh[slot][vsEngine] = false; gVeh[slot][vsDoors] = false; gVeh[slot][vsLights] = false;
    RW_MotionReset(vehicle);
    SetVehiclePos(vehicle, GARAGE_PAD_X, GARAGE_PAD_Y, GARAGE_PAD_Z); SetVehicleZAngle(vehicle, 90.0);
    SetVehicleVelocity(vehicle, 0.0, 0.0, 0.0); SetVehicleHealth(vehicle, 1000.0);
    SetVehicleVirtualWorld(vehicle, 0); LinkVehicleToInterior(vehicle, 0);
    RW_Veh_Sync(vehicle); RW_Veh_SyncAll(vehicle);
    RW_FuelSet(vehicle, FUEL_MAX);
    RW_TuneLoad(vehicle);
    RW_UI_Close(playerid);
    return RW_Ok(playerid, "Car ready outside. Use your key to unlock it.");
}
stock RW_GaragePickup(playerid, pickupid)
{
    if (pickupid != gGaragePickup) return 0;
    if (gGarageLatch[playerid] || !RW_GarageNear(playerid)) return 1;
    gGarageLatch[playerid] = true;
    if (gUIMode[playerid] == 0) RW_GarageOpen(playerid);
    return 1;
}
stock RW_GaragePulse()
{
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p)) {
        if (!IsPlayerInRangeOfPoint(p, 4.0, GARAGE_X, GARAGE_Y, GARAGE_Z)) gGarageLatch[p] = false;
        if (gUIMode[p] == UI_GARAGE && !RW_GarageNear(p)) RW_UI_Close(p);
        if (gGarageReady && !gGarageLatch[p] && RW_GarageNear(p) && IsPlayerInRangeOfPoint(p, 2.0, GARAGE_X, GARAGE_Y, GARAGE_Z)) {
            gGarageLatch[p] = true;
            if (gUIMode[p] == 0) RW_GarageOpen(p);
        }
    }
    return 1;
}
/* ---------------------------------------------- admin key grant by SA-MP id */
/* Список машин рисует страница: скрывать больше нечего. */
stock RW_KeyMenuHide(playerid)
{
    gKeyTarget[playerid] = -1;
    return 1;
}
stock RW_KeyMenuReset(playerid)
{
    gKeyTarget[playerid] = -1; gKeyCount[playerid] = 0; gKeyPage[playerid] = 0; gKeyTick[playerid] = 0;
    return 1;
}
// Spawned cars are listed by their SA-MP vehicle id; stored permanent cars by c<id>.
stock RW_KeyMenuBuild(playerid)
{
    gKeyCount[playerid] = 0;
    for (new s; s < VEHICLE_SLOTS && gKeyCount[playerid] < 60; s++)
        if (gVeh[s][vsUsed] && gVehKeyUid[s] > 0) gKeyList[playerid][gKeyCount[playerid]++] = gVeh[s][vsVehicleId];
    new DBResult:r = db_query(gDB, "SELECT id,model,token FROM owned_cars ORDER BY id");
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows && gKeyCount[playerid] < 60; i++) {
            new token = db_get_field_assoc_int(r, "token"), bool:spawned;
            for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == token) { spawned = true; break; }
            if (!spawned) gKeyList[playerid][gKeyCount[playerid]++] = -db_get_field_assoc_int(r, "id");
            db_next_row(r);
        }
        db_free_result(r);
    }
    if (gKeyPage[playerid]*6 >= gKeyCount[playerid]) gKeyPage[playerid] = 0;
    return 1;
}
/* Подпись строки одна и та же для тексдрав-меню и для страницы. */
stock bool:RW_KeyMenuRow(playerid, row, label[], size = sizeof label)
{
    new idx = gKeyPage[playerid]*6+row, entry, name[48];
    if (idx >= gKeyCount[playerid]) return false;
    entry = gKeyList[playerid][idx];
    if (entry > 0) {
        new slot = RW_IsOwnedSlot(entry);
        if (slot < 0) return false;
        RW_VehicleName(gVeh[slot][vsModel], name, sizeof name);
        format(label, size, "%s / RW-%03d / %d", name, slot+1, entry);
    } else {
        new query[140], model, car = -entry;
        format(query, sizeof query, "SELECT model FROM owned_cars WHERE id=%d", car);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0) { if (db_num_rows(r)) model = db_get_field_assoc_int(r, "model"); db_free_result(r); }
        RW_VehicleName(model, name, sizeof name);
        format(label, size, "%s / c%d / на стоянке", name, car);
    }
    return true;
}
stock RW_KeyMenuOpen(playerid, target)
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    if (gInvFitting[playerid]) return RW_Error(playerid, "Finish or cancel backpack fitting first.");
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Player must be connected and logged in. /givecar <name> <model> works offline.");
    if (!gInvLoaded[target]) return RW_Error(playerid, "Player inventory is not loaded yet.");
    RW_UI_Close(playerid, true);
    gKeyTarget[playerid] = target; gKeyPage[playerid] = 0;
    RW_KeyMenuBuild(playerid);
    if (!gKeyCount[playerid]) { gKeyTarget[playerid] = -1; return RW_Error(playerid, "No managed car has a key right now. /spcar <model> or /givecar first."); }
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_KeyMenuCef(playerid);
    #endif
    return RW_KeyMenuNative(playerid);
    return 1;
}
/*  Меню ключей в браузере: шесть машин, пролистывание, закрыть. */
stock RW_KeyMenuCef(playerid)
{
    new subtitle[64], label[128];
    format(subtitle, sizeof subtitle, "%d машин / страница %d", gKeyCount[playerid], gKeyPage[playerid] + 1);
    if (!RW_CefOpen(playerid, CEF_MODE_KEYS, "Выдать ключ", subtitle)) return 0;
    for (new row; row < 6; row++) {
        if (!RW_KeyMenuRow(playerid, row, label, sizeof label)) continue;
        RW_CefRow(playerid, row, label, gKeyList[playerid][gKeyPage[playerid]*6+row] > 0 ? ("в городе") : ("на стоянке"), "car");
    }
    if (gKeyPage[playerid] > 0) RW_CefAction(playerid, CEF_KEYS_PREV, "Назад", "");
    if ((gKeyPage[playerid] + 1) * 6 < gKeyCount[playerid]) RW_CefAction(playerid, CEF_KEYS_NEXT, "Вперёд", "");
    RW_CefAction(playerid, CEF_KEYS_CLOSE, "Закрыть", "danger");
    return RW_CefEnd(playerid);
}

/* Без браузера: тот же список нативным диалогом, те же номера строк. */
stock RW_KeyMenuNative(playerid)
{
    new subtitle[64], label[128];
    format(subtitle, sizeof subtitle, "%d машин / страница %d", gKeyCount[playerid], gKeyPage[playerid] + 1);
    RW_UI_Panel(playerid, UI_KEYMENU, "Выдать ключ", subtitle);
    for (new row; row < 6; row++) {
        if (!RW_KeyMenuRow(playerid, row, label, sizeof label)) continue;
        RW_UI_Row(playerid, row, label);
    }
    if (gKeyPage[playerid] > 0) RW_UI_Row(playerid, CEF_KEYS_PREV, "Назад");
    if ((gKeyPage[playerid] + 1) * 6 < gKeyCount[playerid]) RW_UI_Row(playerid, CEF_KEYS_NEXT, "Вперёд");
    RW_UI_Row(playerid, CEF_KEYS_CLOSE, "Закрыть");
    return 1;
}
/* Выдать ключ на выбранную машину: одно и то же для обоих меню. */
stock RW_KeyMenuGrant(playerid, row)
{
    new index = gKeyPage[playerid]*6+row, target = gKeyTarget[playerid];
    if (index >= gKeyCount[playerid]) return 1;
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target] || !gInvLoaded[target]) { RW_UI_Close(playerid); return RW_Error(playerid, "Target player is no longer available."); }
    new entry = gKeyList[playerid][index], data;
    if (entry > 0) {
        new slot = RW_IsOwnedSlot(entry);
        if (slot < 0 || !gVeh[slot][vsUsed] || gVehKeyUid[slot] <= 0) return RW_Error(playerid, "That car is gone. Pick another one.");
        data = gVehKeyUid[slot];
    } else {
        new query[140];
        format(query, sizeof query, "SELECT token FROM owned_cars WHERE id=%d", -entry);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0) { if (db_num_rows(r)) data = db_get_field_assoc_int(r, "token"); db_free_result(r); }
        if (data <= 0) return RW_Error(playerid, "That car record is gone. Pick another one.");
    }
    new uid = RW_InvGrant(target, ITEM_CAR_KEY, data);
    if (!uid) return RW_InvExplainGrant(playerid, target, ITEM_CAR_KEY, data);
    printf("[garage] admin account=%d grants key token=%d uid=%d to account=%d from menu", gAccountId[playerid], data, uid, gAccountId[target]);
    RW_Ok(target, "Car key received. /inv");
    new message[128]; format(message, sizeof message, "Key for token %d granted to id %d (item %d).", data, target, uid);
    RW_Ok(playerid, message);
    return RW_UI_Close(playerid);
}
/* Клик по странице: машина, пролистывание или закрыть. */
stock RW_KeyMenuCefClick(playerid, id)
{
    if (gUIMode[playerid] != UI_KEYMENU) return 0;
    if (id == CEF_KEYS_CLOSE) return RW_UI_Close(playerid);
    if (GetTickCount() - gKeyTick[playerid] < 800) return 1;
    gKeyTick[playerid] = GetTickCount();
    if (id == CEF_KEYS_PREV || id == CEF_KEYS_NEXT) {
        if (id == CEF_KEYS_PREV) { if (gKeyPage[playerid] > 0) gKeyPage[playerid]--; }
        else if ((gKeyPage[playerid] + 1) * 6 < gKeyCount[playerid]) gKeyPage[playerid]++;
        RW_KeyMenuBuild(playerid);
        #if RAILWAY_CEF
            if (RW_CefOn(playerid)) return RW_KeyMenuCef(playerid);
        #endif
        return RW_KeyMenuNative(playerid);
    }
    if (id >= 0 && id < 6) return RW_KeyMenuGrant(playerid, id);
    return 1;
}

stock RW_KeyMenuCommand(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    new a[32], rest[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    if (!a[0] || rest[0] || strlen(a) > 5 || !RW_IsNumeric(a)) return RW_Info(playerid, "/givekey <playerid> - choose the car by SA-MP id; /cars lists them");
    return RW_KeyMenuOpen(playerid, strval(a));
}

stock RW_GarageGive(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1 || !gGarageReady || !gInvReady) return RW_Error(playerid, "Admin only / garage unavailable.");
    new targetText[32], rest[144], modelText[16], extra[144];
    RW_ParseCommand(params, targetText, sizeof targetText, rest, sizeof rest);
    RW_ParseCommand(rest, modelText, sizeof modelText, extra, sizeof extra);
    if (!targetText[0] || !RW_IsNumeric(modelText) || strlen(modelText)>3 || extra[0]) return RW_Info(playerid, "/givecar <playerid|account_name> <modelid>");
    new model = strval(modelText), owner, target = -1, query[400];
    if (model < 400 || model > 611 || RW_IS_TRAILER_OR_TRAIN(model)) return RW_Error(playerid, "Invalid vehicle model.");
    if (RW_IsNumeric(targetText)) {
        if (strlen(targetText)>4) return RW_Error(playerid, "Invalid player ID.");
        target = strval(targetText);
        if (target < 0 || target >= MAX_PLAYERS || !gLogged[target]) return RW_Error(playerid, "Player must be online and logged in. Use a name for offline accounts.");
        owner = gAccountId[target];
    } else {
        if (!RW_ValidNick(targetText) || !RW_SafeSqlText(targetText)) return RW_Error(playerid, "Invalid account name.");
        format(query, sizeof query, "SELECT id FROM accounts WHERE name='%s' COLLATE NOCASE", targetText);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0) { if (db_num_rows(r)) owner = db_get_field_assoc_int(r, "id"); db_free_result(r); }
        for (new p; p < MAX_PLAYERS; p++) if (gLogged[p] && gAccountId[p] == owner) target = p;
    }
    if (owner <= 0) return RW_Error(playerid, "Account not found.");
    if (target >= 0 && !gInvLoaded[target]) return RW_Error(playerid, "Target inventory unavailable.");
    if (!RW_DB_Exec("SAVEPOINT garage_give")) return RW_Error(playerid, "Database unavailable.");
    format(query, sizeof query, "SELECT slot,type FROM inventory_items WHERE owner=%d AND slot<50", owner);
    new DBResult:r = db_query(gDB, query), used[50], weight, dest = -1;
    new bool:ok = r != DBResult:0;
    if (ok) {
        new count = db_num_rows(r);
        for (new i; i < count; i++) { new slot = db_get_field_assoc_int(r, "slot"); if (slot >= 0 && slot < 50) used[slot] = 1; weight += RW_InvGrams(db_get_field_assoc_int(r, "type")); db_next_row(r); }
        db_free_result(r);
        for (new i; i < 50; i++) if (!used[i]) { dest = i; break; }
        format(query,sizeof query,"SELECT COUNT(*) AS equipped FROM inventory_equipment WHERE owner=%d",owner);
        r=db_query(gDB,query);
        if(r==DBResult:0) ok=false;
        else { new equipped=db_get_field_assoc_int(r,"equipped"); db_free_result(r); if(equipped>1) ok=false; else weight+=equipped*RW_InvGrams(ITEM_BACKPACK); }
        if (dest < 0 || weight+100 > INV_MAIN_GRAMS) ok = false;
    }
    new uid, car;
    if (ok) { format(query, sizeof query, "INSERT INTO inventory_items(owner,slot,type,data) VALUES(%d,%d,2,0)", owner, dest); ok = RW_InvWrite(query); }
    if (ok) {
        format(query,sizeof query,"SELECT uid FROM inventory_items WHERE owner=%d AND slot=%d AND type=2 AND data=0",owner,dest);
        r = db_query(gDB,query); if(r==DBResult:0) ok=false;
        else { if(db_num_rows(r)==1) uid=db_get_field_assoc_int(r,"uid"); db_free_result(r); if(uid<=0) ok=false; }
    }
    if (ok) { format(query, sizeof query, "UPDATE inventory_items SET data=%d WHERE uid=%d", uid, uid); ok = RW_InvWrite(query); }
    if (ok) { format(query, sizeof query, "INSERT INTO owned_cars(owner,model,token) VALUES(%d,%d,%d)", owner, model, uid); ok = RW_InvWrite(query); }
    if (ok) {
        format(query,sizeof query,"SELECT id FROM owned_cars WHERE token=%d AND owner=%d",uid,owner);
        r=db_query(gDB,query); if(r==DBResult:0) ok=false;
        else { if(db_num_rows(r)==1) car=db_get_field_assoc_int(r,"id"); db_free_result(r); if(car<=0) ok=false; }
    }
    if (ok) { format(query, sizeof query, "INSERT INTO permanent_keys(uid,car) VALUES(%d,%d)", uid, car); ok=RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE garage_give")) {
        RW_DB_Exec("ROLLBACK TO garage_give"); RW_DB_Exec("RELEASE garage_give");
        printf("[garage] givecar failed for account=%d by admin account=%d (dest=%d weight=%d ok=%d)", owner, gAccountId[playerid], dest, weight, ok);
        if (!ok && dest < 0) return RW_Error(playerid, "No free MAIN slot in that account inventory (50 slots). Nothing was granted.");
        if (!ok && weight + 100 > INV_MAIN_GRAMS) { new failed[160]; format(failed, sizeof failed, "That account would carry %.1f kg of %.1f kg in MAIN. Nothing was granted.", float(weight+100)/1000.0, float(INV_MAIN_GRAMS)/1000.0); return RW_Error(playerid, failed); }
        return RW_Error(playerid, "Database error; nothing was granted. Run dbcheck in the server console.");
    }
    if (target >= 0) {
        if (!RW_InvLoad(target)) RW_Error(target, "Inventory reload failed. Reconnect / contact an admin.");
        else { RW_InvAttach(target); if (gUIMode[target] == UI_INVENTORY) RW_InvRender(target); }
        RW_Ok(target, "Permanent car and key received. Visit the station garage.");
    }
    printf("[garage] admin=%d grants model=%d car=%d to account=%d", gAccountId[playerid], model, car, owner);
    return RW_Ok(playerid, "Permanent car and key granted.");
}
stock RW_GarageExit()
{
    if (gGaragePickup != -1) DestroyPickup(gGaragePickup);
    if (gGarageLabel != Text3D:INVALID_3DTEXT_ID) Delete3DTextLabel(gGarageLabel);
    if (IsValidObject(gGarageBooth)) DestroyObject(gGarageBooth);
    return 1;
}

stock bool:RW_GarageKeyModel(token, &model)
{
    new query[140]; format(query,sizeof query,"SELECT model FROM owned_cars WHERE token=%d",token);
    new DBResult:r=db_query(gDB,query), bool:permanent;
    if(r!=DBResult:0) { if(db_num_rows(r)) { model=db_get_field_assoc_int(r,"model"); permanent=true; } db_free_result(r); }
    if(!permanent) for(new s;s<VEHICLE_SLOTS;s++) if(gVeh[s][vsUsed] && gVehKeyUid[s]==token) model=gVeh[s][vsModel];
    return permanent;
}
