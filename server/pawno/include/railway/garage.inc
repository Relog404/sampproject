#if defined _railway_garage_included
    #endinput
#endif
#define _railway_garage_included
#define GARAGE_X (1735.5)
#define GARAGE_Y (-1941.0)
#define GARAGE_Z (13.55)
#define GARAGE_PAD_X (1742.0)
#define GARAGE_PAD_Y (-1953.0)
#define GARAGE_PAD_Z (13.65)
new gGaragePickup = -1, gGarageBooth = INVALID_OBJECT_ID;
new bool:gGarageReady, bool:gGarageLatch[MAX_PLAYERS], bool:gGarageCreated[MAX_PLAYERS];
new PlayerText:gGarageTD[MAX_PLAYERS][13];
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
    gGarageBooth = CreateObject(3881, 1739.0, -1941.0, 13.55, 0.0, 0.0, 90.0, 150.0);
    gGaragePickup = CreatePickup(1239, 1, GARAGE_X, GARAGE_Y, GARAGE_Z, 0);
    if (gGarageBooth == INVALID_OBJECT_ID) print("[garage] Booth object unavailable; proximity entry still active.");
    if (gGaragePickup == -1) print("[garage] Pickup unavailable; proximity entry still active.");
    return 1;
}
stock RW_GarageHide(playerid)
{
    if (gGarageCreated[playerid]) for (new i; i < 13; i++) PlayerTextDrawHide(playerid, gGarageTD[playerid][i]);
    return 1;
}
stock RW_GarageReset(playerid)
{
    if (gGarageCreated[playerid]) for (new i; i < 13; i++) PlayerTextDrawDestroy(playerid, gGarageTD[playerid][i]);
    gGarageCreated[playerid] = false; gGarageLatch[playerid] = false; gGaragePage[playerid] = 0; gGarageCount[playerid] = 0; gGarageTick[playerid] = 0;
    return 1;
}
stock bool:RW_GarageNear(playerid)
{
    return gLogged[playerid] && gInvLoaded[playerid] && GetPlayerState(playerid) == PLAYER_STATE_ONFOOT && GetPlayerInterior(playerid) == 0 && GetPlayerVirtualWorld(playerid) == 0 && IsPlayerInRangeOfPoint(playerid, 4.0, GARAGE_X, GARAGE_Y, GARAGE_Z);
}
stock RW_GarageLabel(playerid, index, Float:x, Float:y, const text[], bool:button = false)
{
    new buf[128]; format(buf, sizeof buf, "%s", text);
    gGarageTD[playerid][index] = CreatePlayerTextDraw(playerid, x, y, buf);
    PlayerTextDrawFont(playerid, gGarageTD[playerid][index], 1);
    PlayerTextDrawLetterSize(playerid, gGarageTD[playerid][index], 0.22, 1.1);
    PlayerTextDrawColor(playerid, gGarageTD[playerid][index], 0xEEECE7FF);
    PlayerTextDrawSetShadow(playerid, gGarageTD[playerid][index], 0);
    PlayerTextDrawTextSize(playerid, gGarageTD[playerid][index], x+230.0, 18.0);
    if (button) {
        PlayerTextDrawUseBox(playerid, gGarageTD[playerid][index], 1);
        PlayerTextDrawBoxColor(playerid, gGarageTD[playerid][index], 0xD8C39B22);
        PlayerTextDrawSetSelectable(playerid, gGarageTD[playerid][index], 1);
    }
    return 1;
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
    if (!gGarageCreated[playerid]) {
        gGarageTD[playerid][0] = CreatePlayerTextDraw(playerid, 190.0, 90.0, "LD_SPAC:white");
        PlayerTextDrawFont(playerid, gGarageTD[playerid][0], 4);
        PlayerTextDrawTextSize(playerid, gGarageTD[playerid][0], 260.0, 272.0);
        PlayerTextDrawColor(playerid, gGarageTD[playerid][0], 0x141920F8);
        RW_GarageLabel(playerid, 1, 203.0, 104.0, "STATION / GARAGE");
        RW_GarageLabel(playerid, 2, 203.0, 123.0, "SELECT CAR / KEY REQUIRED");
        for (new i; i < 6; i++) RW_GarageLabel(playerid, 3+i, 203.0, 151.0+26.0*i, "_", true);
        RW_GarageLabel(playerid, 9, 203.0, 312.0, "< PREVIOUS", true);
        RW_GarageLabel(playerid, 10, 330.0, 312.0, "NEXT >", true);
        PlayerTextDrawTextSize(playerid, gGarageTD[playerid][9], 310.0, 18.0);
        PlayerTextDrawTextSize(playerid, gGarageTD[playerid][10], 437.0, 18.0);
        RW_GarageLabel(playerid, 11, 203.0, 338.0, "CLOSE / ESC", true);
        RW_GarageLabel(playerid, 12, 203.0, 136.0, "_");
        PlayerTextDrawLetterSize(playerid, gGarageTD[playerid][12], 0.15, 0.75);
        gGarageCreated[playerid] = true;
    }
    for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    gUIMode[playerid] = UI_GARAGE;
    for (new i; i < 13; i++) PlayerTextDrawShow(playerid, gGarageTD[playerid][i]);
    for (new row; row < 6; row++) {
        new idx = gGaragePage[playerid]*6+row;
        if (idx >= gGarageCount[playerid]) { PlayerTextDrawHide(playerid, gGarageTD[playerid][3+row]); continue; }
        new id = gGarageList[playerid][idx], model, name[48], label[128];
        if (id > 0) {
            format(query, sizeof query, "SELECT model FROM owned_cars WHERE id=%d", id);
            r = db_query(gDB, query);
            if (r != DBResult:0) { model = db_get_field_assoc_int(r, "model"); db_free_result(r); }
        } else for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == -id) model = gVeh[s][vsModel];
        RW_VehicleName(model, name, sizeof name);
        format(label, sizeof label, "%s / %s", name, id > 0 ? ("OWNED") : ("TEMP"));
        PlayerTextDrawSetString(playerid, gGarageTD[playerid][3+row], label);
    }
    new label[80]; format(label, sizeof label, "%d CARS / PAGE %d", gGarageCount[playerid], gGaragePage[playerid]+1);
    PlayerTextDrawSetString(playerid, gGarageTD[playerid][12], label);
    RW_UI_SelectBegin(playerid);
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
    RW_UI_Close(playerid);
    return RW_Ok(playerid, "Car ready outside. Use your key to unlock it.");
}
stock RW_GarageClick(playerid, PlayerText:clicked)
{
    if (gUIMode[playerid] != UI_GARAGE || !RW_GarageNear(playerid)) return RW_UI_Close(playerid);
    if (clicked == gGarageTD[playerid][11]) return RW_UI_Close(playerid);
    if (GetTickCount()-gGarageTick[playerid] < 1000) return 1;
    gGarageTick[playerid] = GetTickCount();
    if (clicked == gGarageTD[playerid][9]) { if (gGaragePage[playerid] > 0) gGaragePage[playerid]--; return RW_GarageOpen(playerid, false); }
    if (clicked == gGarageTD[playerid][10]) { if ((gGaragePage[playerid]+1)*6 < gGarageCount[playerid]) gGaragePage[playerid]++; return RW_GarageOpen(playerid, false); }
    for (new row; row < 6; row++) if (clicked == gGarageTD[playerid][3+row]) {
        new index = gGaragePage[playerid]*6+row;
        if (index < gGarageCount[playerid]) return RW_GarageSpawn(playerid, gGarageList[playerid][index]);
    }
    return 1;
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
        if (!IsPlayerInRangeOfPoint(p, 3.0, GARAGE_X, GARAGE_Y, GARAGE_Z)) gGarageLatch[p] = false;
        if (gUIMode[p] == UI_GARAGE && !RW_GarageNear(p)) RW_UI_Close(p);
        if (gGarageReady && !gGarageLatch[p] && RW_GarageNear(p) && IsPlayerInRangeOfPoint(p, 1.3, GARAGE_X, GARAGE_Y, GARAGE_Z)) {
            gGarageLatch[p] = true;
            if (gUIMode[p] == 0) RW_GarageOpen(p);
        }
    }
    return 1;
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
        if (dest < 0 || weight+100 > INV_MAIN_GRAMS) ok = false;
    }
    new uid, car;
    if (ok) { format(query, sizeof query, "INSERT INTO inventory_items(owner,slot,type,data) VALUES(%d,%d,2,0)", owner, dest); ok = RW_InvWrite(query); }
    if (ok) { r = db_query(gDB, "SELECT last_insert_rowid()"); if (r == DBResult:0) ok = false; else { uid = db_get_field_int(r); db_free_result(r); if (uid<=0) ok=false; } }
    if (ok) { format(query, sizeof query, "UPDATE inventory_items SET data=%d WHERE uid=%d", uid, uid); ok = RW_InvWrite(query); }
    if (ok) { format(query, sizeof query, "INSERT INTO owned_cars(owner,model,token) VALUES(%d,%d,%d)", owner, model, uid); ok = RW_InvWrite(query); }
    if (ok) { r = db_query(gDB, "SELECT last_insert_rowid()"); if (r == DBResult:0) ok=false; else { car=db_get_field_int(r); db_free_result(r); if(car<=0) ok=false; } }
    if (ok) { format(query, sizeof query, "INSERT INTO permanent_keys(uid,car) VALUES(%d,%d)", uid, car); ok=RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE garage_give")) {
        RW_DB_Exec("ROLLBACK TO garage_give"); RW_DB_Exec("RELEASE garage_give");
        return RW_Error(playerid, "Grant failed: inventory full, overweight or database error. Nothing was granted.");
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
