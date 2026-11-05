#if defined _railway_inventory_included
    #endinput
#endif
#define _railway_inventory_included

#define INV_MAIN 50
#define INV_BAG 30
#define INV_SLOTS 80
#define INV_NEAR 50
#define INV_WORLD 500
#define INV_TDS 155
#define ITEM_BACKPACK 1
#define ITEM_CAR_KEY 2
#define INV_ATTACH 9
#define INV_MAIN_GRAMS 50000
#define INV_BAG_GRAMS 40000

enum E_INV_ITEM { iiUid, iiType, iiData };
enum E_INV_GROUND { igUid, igType, igData, igObject, Float:igX, Float:igY, Float:igZ };
new gInv[MAX_PLAYERS][INV_SLOTS][E_INV_ITEM];
new gGround[INV_WORLD][E_INV_GROUND];
new PlayerText:gInvTD[MAX_PLAYERS][INV_TDS];
new bool:gInvCreated[MAX_PLAYERS], bool:gInvLoaded[MAX_PLAYERS], bool:gInvReady;
new gInvNear[MAX_PLAYERS][INV_NEAR], gInvSelected[MAX_PLAYERS], bool:gInvFromGround[MAX_PLAYERS];
new gInvTick[MAX_PLAYERS];
forward RW_InvPulse();

stock RW_InvModel(type) { return type == ITEM_BACKPACK ? 3026 : 11746; }
stock RW_InvGrams(type) { return type == ITEM_BACKPACK ? 1000 : 100; }
stock RW_InvPack(playerid)
{
    for (new i; i < INV_MAIN; i++) if (gInv[playerid][i][iiType] == ITEM_BACKPACK) return i;
    return -1;
}
stock RW_InvWeight(playerid, section)
{
    new weight, start = section ? INV_MAIN : 0, end = section ? INV_SLOTS : INV_MAIN;
    for (new i = start; i < end; i++) if (gInv[playerid][i][iiUid]) weight += RW_InvGrams(gInv[playerid][i][iiType]);
    return weight;
}
stock RW_InvFree(playerid, section, type)
{
    if (section && (RW_InvPack(playerid) < 0 || type == ITEM_BACKPACK)) return -1;
    if (RW_InvWeight(playerid, section) + RW_InvGrams(type) > (section ? INV_BAG_GRAMS : INV_MAIN_GRAMS)) return -1;
    new start = section ? INV_MAIN : 0, end = section ? INV_SLOTS : INV_MAIN;
    for (new i = start; i < end; i++) if (!gInv[playerid][i][iiUid]) return i;
    return -1;
}
stock RW_InvFind(playerid, uid)
{
    if (uid <= 0) return -1;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiUid] == uid) return i;
    return -1;
}
stock RW_InvGroundFind(uid)
{
    if (uid <= 0) return -1;
    for (new i; i < INV_WORLD; i++) if (gGround[i][igUid] == uid) return i;
    return -1;
}
stock bool:RW_InvNearGround(playerid, index)
{
    return index >= 0 && index < INV_WORLD && gGround[index][igUid] > 0 &&
        GetPlayerState(playerid) == PLAYER_STATE_ONFOOT && GetPlayerInterior(playerid) == 0 &&
        GetPlayerVirtualWorld(playerid) == 0 &&
        IsPlayerInRangeOfPoint(playerid, 3.0, gGround[index][igX], gGround[index][igY], gGround[index][igZ]);
}
stock RW_InvAttach(playerid)
{
    new bool:hasBag = gInvLoaded[playerid] && RW_InvPack(playerid) >= 0;
    if (!hasBag && IsPlayerAttachedObjectSlotUsed(playerid, INV_ATTACH)) RemovePlayerAttachedObject(playerid, INV_ATTACH);
    if (hasBag && !IsPlayerAttachedObjectSlotUsed(playerid, INV_ATTACH))
        SetPlayerAttachedObject(playerid, INV_ATTACH, 3026, 1, 0.0, -0.16, 0.0, 0.0, 90.0, 0.0, 1.0, 1.0, 1.0);
    return 1;
}
stock bool:RW_InvWrite(query[])
{
    if (!gInvReady || !gDBWritable || !RW_DB_Exec("SAVEPOINT inventory_mutation")) return false;
    new bool:ok = RW_DB_Exec(query);
    if (ok) {
        new DBResult:r = db_query(gDB, "SELECT changes()");
        if (r == DBResult:0) ok = false;
        else { ok = db_get_field_int(r, 0) == 1; db_free_result(r); }
    }
    if (!ok) { RW_DB_Exec("ROLLBACK TO inventory_mutation"); RW_DB_Exec("RELEASE inventory_mutation"); return false; }
    if (!RW_DB_Exec("RELEASE inventory_mutation")) { RW_DB_Exec("ROLLBACK TO inventory_mutation"); RW_DB_Exec("RELEASE inventory_mutation"); return false; }
    return true;
}

stock RW_InvInit()
{
    if (gDB == DB:0 || !gDBWritable) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS inventory_items (uid INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER, slot INTEGER NOT NULL, type INTEGER NOT NULL CHECK(type IN (1,2)), data INTEGER NOT NULL DEFAULT 0, x REAL NOT NULL DEFAULT 0, y REAL NOT NULL DEFAULT 0, z REAL NOT NULL DEFAULT 0, UNIQUE(owner,slot), CHECK((owner IS NULL AND slot=-1) OR (owner>0 AND slot>=0 AND slot<80)))")) return 0;
    // Personal cars are session-only. Never let a recycled vehicle ID revive a key.
    if (!RW_DB_Exec("DELETE FROM inventory_items WHERE type=2")) return 0;
    new DBResult:r = db_query(gDB, "SELECT uid,type,data,x,y,z FROM inventory_items WHERE owner IS NULL ORDER BY uid");
    if (r == DBResult:0) return 0;
    new count = db_num_rows(r);
    if (count > INV_WORLD) { db_free_result(r); print("[inventory] Ground capacity exceeded; inventory disabled, data preserved."); return 0; }
    for (new i; i < count; i++) {
        new uid = db_get_field_assoc_int(r, "uid");
        if (uid <= 0) { db_free_result(r); RW_InvExit(); print("[inventory] Invalid ground UID; data preserved."); return 0; }
        gGround[i][igUid] = uid;
        gGround[i][igType] = db_get_field_assoc_int(r, "type");
        gGround[i][igData] = db_get_field_assoc_int(r, "data");
        gGround[i][igX] = db_get_field_assoc_float(r, "x");
        gGround[i][igY] = db_get_field_assoc_float(r, "y");
        gGround[i][igZ] = db_get_field_assoc_float(r, "z");
        gGround[i][igObject] = CreateObject(RW_InvModel(gGround[i][igType]), gGround[i][igX], gGround[i][igY], gGround[i][igZ], 0.0, 0.0, 0.0, 70.0);
        if (gGround[i][igObject] == INVALID_OBJECT_ID) {
            db_free_result(r);
            RW_InvExit();
            print("[inventory] Cannot restore ground objects; data preserved, inventory disabled.");
            return 0;
        }
        db_next_row(r);
    }
    db_free_result(r);
    gInvReady = true;
    SetTimer("RW_InvPulse", 1000, true);
    return 1;
}
stock RW_InvLoad(playerid)
{
    gInvLoaded[playerid] = false;
    if (!gInvReady) return 0;
    new query[160]; format(query, sizeof query, "SELECT uid,slot,type,data FROM inventory_items WHERE owner=%d ORDER BY slot", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new count = db_num_rows(r), valid = 1;
    for (new i; i < count; i++) {
        new slot = db_get_field_assoc_int(r, "slot"), type = db_get_field_assoc_int(r, "type");
        if (slot < 0 || slot >= INV_SLOTS || (type != ITEM_BACKPACK && type != ITEM_CAR_KEY) || (slot >= INV_MAIN && type == ITEM_BACKPACK)) { valid = 0; break; }
        new uid = db_get_field_assoc_int(r, "uid");
        if (uid <= 0) { valid = 0; break; }
        gInv[playerid][slot][iiUid] = uid;
        gInv[playerid][slot][iiType] = type;
        gInv[playerid][slot][iiData] = db_get_field_assoc_int(r, "data");
        db_next_row(r);
    }
    db_free_result(r);
    if (RW_InvWeight(playerid, 0) > INV_MAIN_GRAMS || RW_InvWeight(playerid, 1) > INV_BAG_GRAMS || (RW_InvPack(playerid) < 0 && RW_InvWeight(playerid, 1) > 0)) valid = 0;
    if (!valid) { print("[inventory] Invalid inventory state; data preserved, player inventory locked."); return 0; }
    gInvLoaded[playerid] = true;
    return 1;
}
stock RW_InvHide(playerid)
{
    if (gInvCreated[playerid]) for (new i; i < INV_TDS; i++) PlayerTextDrawHide(playerid, gInvTD[playerid][i]);
    gInvSelected[playerid] = 0;
    return 1;
}
stock RW_InvReset(playerid)
{
    if (gInvCreated[playerid]) for (new i; i < INV_TDS; i++) PlayerTextDrawDestroy(playerid, gInvTD[playerid][i]);
    gInvCreated[playerid] = false; gInvLoaded[playerid] = false; gInvSelected[playerid] = 0; gInvTick[playerid] = 0;
    for (new i; i < INV_SLOTS; i++) for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][i][j] = 0;
    for (new i; i < INV_NEAR; i++) gInvNear[playerid][i] = 0;
    if (IsPlayerConnected(playerid) && IsPlayerAttachedObjectSlotUsed(playerid, INV_ATTACH)) RemovePlayerAttachedObject(playerid, INV_ATTACH);
    return 1;
}
// Returns a durable item UID, or zero; no in-memory grant before COMMIT.
stock RW_InvGrant(playerid, type, data = 0)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid] || (type != ITEM_BACKPACK && type != ITEM_CAR_KEY)) return 0;
    new slot = RW_InvFree(playerid, 0, type);
    if (slot < 0) return 0;
    if (!RW_DB_Exec("SAVEPOINT inventory_grant")) return 0;
    new query[256]; format(query, sizeof query, "INSERT INTO inventory_items(owner,slot,type,data) VALUES(%d,%d,%d,%d)", gAccountId[playerid], slot, type, data);
    new bool:ok = RW_InvWrite(query), uid;
    if (ok) {
        new DBResult:r = db_query(gDB, "SELECT last_insert_rowid()");
        if (r != DBResult:0) { uid = db_get_field_int(r, 0); db_free_result(r); }
        if (uid <= 0) ok = false;
    }
    if (ok && type == ITEM_CAR_KEY && data == 0) {
        data = uid;
        format(query, sizeof query, "UPDATE inventory_items SET data=%d WHERE uid=%d", data, uid);
        ok = RW_InvWrite(query);
    }
    if (!ok) { RW_DB_Exec("ROLLBACK TO inventory_grant"); RW_DB_Exec("RELEASE inventory_grant"); return 0; }
    if (!RW_DB_Exec("RELEASE inventory_grant")) { RW_DB_Exec("ROLLBACK TO inventory_grant"); RW_DB_Exec("RELEASE inventory_grant"); return 0; }
    gInv[playerid][slot][iiUid] = uid; gInv[playerid][slot][iiType] = type; gInv[playerid][slot][iiData] = data;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return uid;
}
stock bool:RW_InvHasKey(playerid, vehicleid)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return false;
    new slot = RW_IsOwnedSlot(vehicleid);
    if (slot < 0 || !gVeh[slot][vsUsed] || gVehKeyUid[slot] <= 0) return false;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_CAR_KEY && gInv[playerid][i][iiData] == gVehKeyUid[slot]) return true;
    return false;
}
stock RW_InvRevokeKey(token)
{
    if (token <= 0) return 1;
    new query[160]; format(query, sizeof query, "DELETE FROM inventory_items WHERE type=2 AND data=%d", token);
    if (!RW_DB_Exec(query)) { print("[inventory] Key cleanup failed; stale keys cannot authorize another car."); return 0; }
    for (new p; p < MAX_PLAYERS; p++) if (gInvLoaded[p]) for (new i; i < INV_SLOTS; i++) if (gInv[p][i][iiType] == ITEM_CAR_KEY && gInv[p][i][iiData] == token) {
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[p][i][j] = 0;
    }
    for (new i; i < INV_WORLD; i++) if (gGround[i][igType] == ITEM_CAR_KEY && gGround[i][igData] == token) {
        if (IsValidObject(gGround[i][igObject])) DestroyObject(gGround[i][igObject]);
        gGround[i][igUid] = 0;
    }
    return 1;
}
stock bool:RW_InvCanRemove(playerid, slot)
{
    if (gInv[playerid][slot][iiType] == ITEM_BACKPACK && RW_InvPack(playerid) == slot) {
        new other;
        for (new i; i < INV_MAIN; i++) if (i != slot && gInv[playerid][i][iiType] == ITEM_BACKPACK) other++;
        if (!other && RW_InvWeight(playerid, 1) > 0) { RW_Error(playerid, "Сначала освободите рюкзак."); return false; }
    }
    if (gInv[playerid][slot][iiType] == ITEM_CAR_KEY) for (new v; v < VEHICLE_SLOTS; v++)
        if (gVeh[v][vsUsed] && gVeh[v][vsKey] && gVehKeyUid[v] == gInv[playerid][slot][iiData]) {
            RW_Error(playerid, "Сначала выньте ключ из замка."); return false;
        }
    return true;
}
stock RW_InvTransfer(playerid, section)
{
    new uid = gInvSelected[playerid], query[320];
    if (gInvFromGround[playerid]) {
        new ground = RW_InvGroundFind(uid);
        if (!RW_InvNearGround(playerid, ground)) return RW_Error(playerid, "Предмет уже недоступен.");
        new dest = RW_InvFree(playerid, section, gGround[ground][igType]);
        if (dest < 0) return RW_Error(playerid, "Нет места или превышен вес.");
        format(query, sizeof query, "UPDATE inventory_items SET owner=%d,slot=%d,x=0,y=0,z=0 WHERE uid=%d AND owner IS NULL", gAccountId[playerid], dest, uid);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось сохранить предмет.");
        gInv[playerid][dest][iiUid] = uid; gInv[playerid][dest][iiType] = gGround[ground][igType]; gInv[playerid][dest][iiData] = gGround[ground][igData];
        if (IsValidObject(gGround[ground][igObject])) DestroyObject(gGround[ground][igObject]);
        gGround[ground][igUid] = 0;
        gInvFromGround[playerid] = false;
    } else {
        new source = RW_InvFind(playerid, uid);
        if (source < 0) return 1;
        if ((source >= INV_MAIN) == (section != 0)) return 1;
        if (!RW_InvCanRemove(playerid, source)) return 1;
        new dest = RW_InvFree(playerid, section, gInv[playerid][source][iiType]);
        if (dest < 0) return RW_Error(playerid, "Нет места или превышен вес.");
        format(query, sizeof query, "UPDATE inventory_items SET slot=%d WHERE uid=%d AND owner=%d AND slot=%d", dest, uid, gAccountId[playerid], source);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось сохранить перенос.");
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) { gInv[playerid][dest][j] = gInv[playerid][source][j]; gInv[playerid][source][j] = 0; }
    }
    RW_InvAttach(playerid);
    return RW_InvRender(playerid);
}
stock RW_InvDrop(playerid)
{
    if (gInvFromGround[playerid]) return 1;
    new source = RW_InvFind(playerid, gInvSelected[playerid]);
    if (source < 0 || !RW_InvCanRemove(playerid, source)) return 1;
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return RW_Error(playerid, "Выбрасывать можно пешком на улице, в основном мире.");
    new ground = -1;
    for (new i; i < INV_WORLD; i++) if (!gGround[i][igUid]) { ground = i; break; }
    if (ground < 0) return RW_Error(playerid, "На земле слишком много предметов.");
    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z); GetPlayerFacingAngle(playerid, a);
    x += 0.6*floatsin(-a, degrees); y += 0.6*floatcos(-a, degrees); z -= 0.9;
    new object = CreateObject(RW_InvModel(gInv[playerid][source][iiType]), x, y, z, 0.0, 0.0, a, 70.0);
    if (object == INVALID_OBJECT_ID) return RW_Error(playerid, "Не удалось создать предмет в мире.");
    new query[320]; format(query, sizeof query, "UPDATE inventory_items SET owner=NULL,slot=-1,x=%f,y=%f,z=%f WHERE uid=%d AND owner=%d AND slot=%d", x, y, z, gInv[playerid][source][iiUid], gAccountId[playerid], source);
    if (!RW_InvWrite(query)) { DestroyObject(object); return RW_Error(playerid, "Не удалось сохранить предмет."); }
    gGround[ground][igUid] = gInv[playerid][source][iiUid]; gGround[ground][igType] = gInv[playerid][source][iiType]; gGround[ground][igData] = gInv[playerid][source][iiData];
    gGround[ground][igObject] = object; gGround[ground][igX] = x; gGround[ground][igY] = y; gGround[ground][igZ] = z;
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][source][j] = 0;
    gInvSelected[playerid] = 0;
    RW_InvAttach(playerid);
    return RW_InvRender(playerid);
}
stock RW_InvLabel(playerid, index, Float:x, Float:y, const text[], Float:width, bool:button = false)
{
    new label[144]; format(label, sizeof label, "%s", text);
    gInvTD[playerid][index] = CreatePlayerTextDraw(playerid, x, y, label);
    PlayerTextDrawFont(playerid, gInvTD[playerid][index], 1);
    PlayerTextDrawLetterSize(playerid, gInvTD[playerid][index], 0.18, 0.95);
    PlayerTextDrawColor(playerid, gInvTD[playerid][index], 0xEEECE7FF);
    PlayerTextDrawSetShadow(playerid, gInvTD[playerid][index], 0);
    PlayerTextDrawTextSize(playerid, gInvTD[playerid][index], x+width, 14.0);
    if (button) { PlayerTextDrawUseBox(playerid, gInvTD[playerid][index], 1); PlayerTextDrawBoxColor(playerid, gInvTD[playerid][index], 0xD8C39B30); PlayerTextDrawSetSelectable(playerid, gInvTD[playerid][index], 1); }
    return 1;
}
stock RW_InvSprite(playerid, index, Float:x, Float:y, Float:w, Float:h, color, bool:selectable = false)
{
    gInvTD[playerid][index] = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, gInvTD[playerid][index], 4);
    PlayerTextDrawTextSize(playerid, gInvTD[playerid][index], w, h);
    PlayerTextDrawColor(playerid, gInvTD[playerid][index], color);
    PlayerTextDrawSetSelectable(playerid, gInvTD[playerid][index], selectable);
    return 1;
}
stock RW_InvCreate(playerid)
{
    if (gInvCreated[playerid]) return 1;
    RW_InvSprite(playerid, 139, 10.0, 42.0, 620.0, 365.0, 0x11151DF5);
    RW_InvSprite(playerid, 140, 10.0, 42.0, 620.0, 2.0, 0xD8C39BFF);
    RW_InvSprite(playerid, 141, 229.0, 79.0, 1.0, 275.0, 0xFFFFFF12);
    RW_InvSprite(playerid, 142, 409.0, 79.0, 1.0, 275.0, 0xFFFFFF12);
    for (new i; i < 130; i++) {
        new Float:x, Float:y;
        if (i < 50) { x = 20.0+(i%10)*20.0; y = 88.0+(i/10)*27.0; }
        else if (i < 80) { x = 20.0+((i-50)%10)*20.0; y = 267.0+((i-50)/10)*27.0; }
        else { x = 420.0+((i-80)%10)*20.0; y = 88.0+((i-80)/10)*27.0; }
        RW_InvSprite(playerid, i, x, y, 18.0, 24.0, 0xFFFFFF12, true);
    }
    RW_InvSprite(playerid, 130, 267.0, 105.0, 102.0, 180.0, 0xFFFFFFFF);
    PlayerTextDrawFont(playerid, gInvTD[playerid][130], 5);
    PlayerTextDrawBackgroundColor(playerid, gInvTD[playerid][130], 0x14192000);
    PlayerTextDrawSetPreviewRot(playerid, gInvTD[playerid][130], 0.0, 0.0, 15.0, 1.0);
    for (new i; i < 8; i++) RW_InvSprite(playerid, 131+i, i < 4 ? 240.0 : 376.0, 100.0+float(i%4)*46.0, 24.0, 30.0, 0xD8C39B22);
    RW_InvLabel(playerid, 143, 20.0, 56.0, "INVENTORY", 196.0);
    RW_InvLabel(playerid, 144, 20.0, 246.0, "BACKPACK", 196.0);
    RW_InvLabel(playerid, 145, 420.0, 56.0, "NEARBY / 3m", 195.0);
    RW_InvLabel(playerid, 146, 265.0, 56.0, "CHARACTER", 130.0);
    RW_InvLabel(playerid, 147, 253.0, 304.0, "8 EQUIP / SOON", 143.0);
    RW_InvLabel(playerid, 148, 595.0, 377.0, "X", 22.0, true);
    RW_InvLabel(playerid, 149, 285.0, 377.0, "MAIN", 62.0, true);
    RW_InvLabel(playerid, 150, 360.0, 377.0, "BAG", 62.0, true);
    RW_InvLabel(playerid, 151, 435.0, 377.0, "DROP", 62.0, true);
    RW_InvLabel(playerid, 152, 510.0, 377.0, "USE", 62.0, true);
    RW_InvLabel(playerid, 153, 20.0, 377.0, "SELECT ITEM", 255.0);
    RW_InvLabel(playerid, 154, 420.0, 240.0, "SELECT > MAIN / BAG", 195.0);
    gInvCreated[playerid] = true;
    return 1;
}
stock RW_InvCell(playerid, index, type, bool:selected)
{
    PlayerTextDrawHide(playerid, gInvTD[playerid][index]);
    if (type) {
        PlayerTextDrawFont(playerid, gInvTD[playerid][index], 5);
        PlayerTextDrawColor(playerid, gInvTD[playerid][index], 0xFFFFFFFF);
        PlayerTextDrawBackgroundColor(playerid, gInvTD[playerid][index], selected ? 0xD8C39B99 : 0x232A36FF);
        PlayerTextDrawSetPreviewModel(playerid, gInvTD[playerid][index], RW_InvModel(type));
        PlayerTextDrawSetPreviewRot(playerid, gInvTD[playerid][index], type == ITEM_CAR_KEY ? 90.0 : 0.0, 0.0, 25.0, 1.0);
    } else {
        PlayerTextDrawFont(playerid, gInvTD[playerid][index], 4);
        PlayerTextDrawSetString(playerid, gInvTD[playerid][index], "LD_SPAC:white");
        PlayerTextDrawColor(playerid, gInvTD[playerid][index], 0xFFFFFF12);
    }
    PlayerTextDrawShow(playerid, gInvTD[playerid][index]);
    return 1;
}
stock RW_InvRender(playerid)
{
    if (gUIMode[playerid] != UI_INVENTORY || !gInvCreated[playerid]) return 1;
    new hasBag = RW_InvPack(playerid) >= 0;
    for (new i; i < INV_SLOTS; i++) {
        if (i >= INV_MAIN && !hasBag) { PlayerTextDrawHide(playerid, gInvTD[playerid][i]); continue; }
        RW_InvCell(playerid, i, gInv[playerid][i][iiType], !gInvFromGround[playerid] && gInv[playerid][i][iiUid] > 0 && gInv[playerid][i][iiUid] == gInvSelected[playerid]);
    }
    new count;
    for (new i; i < INV_WORLD && count < INV_NEAR; i++) if (RW_InvNearGround(playerid, i)) gInvNear[playerid][count++] = gGround[i][igUid];
    for (new i = count; i < INV_NEAR; i++) gInvNear[playerid][i] = 0;
    for (new i; i < INV_NEAR; i++) {
        new ground = RW_InvGroundFind(gInvNear[playerid][i]);
        RW_InvCell(playerid, 80+i, ground < 0 ? 0 : gGround[ground][igType], gInvFromGround[playerid] && gInvNear[playerid][i] > 0 && gInvNear[playerid][i] == gInvSelected[playerid]);
    }
    new text[128];
    format(text, sizeof text, "INVENTORY  %.1f / 50kg", float(RW_InvWeight(playerid, 0))/1000.0);
    PlayerTextDrawSetString(playerid, gInvTD[playerid][143], text);
    if (hasBag) format(text, sizeof text, "BACKPACK  %.1f / 40kg", float(RW_InvWeight(playerid, 1))/1000.0);
    else format(text, sizeof text, "BACKPACK / LOCKED");
    PlayerTextDrawSetString(playerid, gInvTD[playerid][144], text);
    format(text, sizeof text, "NEARBY  %d / 50  / 3m", count);
    PlayerTextDrawSetString(playerid, gInvTD[playerid][145], text);
    new type, data;
    if (gInvFromGround[playerid]) {
        new ground = RW_InvGroundFind(gInvSelected[playerid]);
        if (RW_InvNearGround(playerid, ground)) { type = gGround[ground][igType]; data = gGround[ground][igData]; }
    } else {
        new slot = RW_InvFind(playerid, gInvSelected[playerid]);
        if (slot >= 0) { type = gInv[playerid][slot][iiType]; data = gInv[playerid][slot][iiData]; }
    }
    if (!type) { gInvSelected[playerid] = 0; format(text, sizeof text, "SELECT ITEM"); }
    else if (type == ITEM_BACKPACK) format(text, sizeof text, "BACKPACK / 1kg / +30");
    else {
        new vehicle = -1;
        for (new v; v < VEHICLE_SLOTS; v++) if (gVeh[v][vsUsed] && gVehKeyUid[v] == data) vehicle = gVeh[v][vsVehicleId];
        format(text, sizeof text, "KEY / CAR %d / 0.1kg", vehicle);
    }
    PlayerTextDrawSetString(playerid, gInvTD[playerid][153], text);
    return 1;
}
stock RW_InvOpen(playerid)
{
    if (gUIMode[playerid] == UI_INVENTORY) return RW_UI_Close(playerid);
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь недоступен: проверьте базу.");
    new pstate = GetPlayerState(playerid);
    if (pstate != PLAYER_STATE_ONFOOT && pstate != PLAYER_STATE_DRIVER && pstate != PLAYER_STATE_PASSENGER) return 1;
    if (gUIInput[playerid]) return 1;
    RW_UI_Close(playerid);
    for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    RW_InvCreate(playerid);
    gUIMode[playerid] = UI_INVENTORY;
    // Backgrounds are also CREATED first: SA-MP renders by pool ID.
    for (new i = 139; i <= 142; i++) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    PlayerTextDrawSetPreviewModel(playerid, gInvTD[playerid][130], GetPlayerSkin(playerid));
    for (new i = 130; i < 139; i++) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    for (new i = 143; i < INV_TDS; i++) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    RW_InvRender(playerid);
    SelectTextDraw(playerid, 0xD8C39BFF);
    return 1;
}
stock RW_InvClick(playerid, PlayerText:clicked)
{
    if (gUIMode[playerid] != UI_INVENTORY || !gLogged[playerid] || !gInvLoaded[playerid]) return 0;
    for (new i; i < 130; i++) if (clicked == gInvTD[playerid][i]) {
        if (i >= 50 && i < 80 && RW_InvPack(playerid) < 0) return 1;
        gInvFromGround[playerid] = i >= 80;
        gInvSelected[playerid] = i < 80 ? gInv[playerid][i][iiUid] : gInvNear[playerid][i-80];
        return RW_InvRender(playerid);
    }
    if (clicked == gInvTD[playerid][148]) return RW_UI_Close(playerid);
    if (GetTickCount()-gInvTick[playerid] < 250) return 1;
    gInvTick[playerid] = GetTickCount();
    if (!gInvSelected[playerid]) return 1;
    if (clicked == gInvTD[playerid][149]) return RW_InvTransfer(playerid, 0);
    if (clicked == gInvTD[playerid][150]) return RW_InvTransfer(playerid, 1);
    if (clicked == gInvTD[playerid][151]) return RW_InvDrop(playerid);
    if (clicked == gInvTD[playerid][152] && !gInvFromGround[playerid]) {
        new slot = RW_InvFind(playerid, gInvSelected[playerid]);
        if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_CAR_KEY) {
            new token = gInv[playerid][slot][iiData];
            for (new v; v < VEHICLE_SLOTS; v++) if (gVeh[v][vsUsed] && gVehKeyUid[v] == token) {
                RW_UI_Close(playerid); return RW_ControlExecute(playerid, gVeh[v][vsVehicleId], 2);
            }
        } else if (slot >= 0) return RW_Info(playerid, "Рюкзак уже надет.");
    }
    return 1;
}
public RW_InvPulse()
{
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gUIMode[p] == UI_INVENTORY) {
        new pstate = GetPlayerState(p);
        if (!gLogged[p] || (pstate != PLAYER_STATE_ONFOOT && pstate != PLAYER_STATE_DRIVER && pstate != PLAYER_STATE_PASSENGER)) RW_UI_Close(p);
        else RW_InvRender(p);
    }
    return 1;
}
stock RW_InvExit()
{
    for (new i; i < INV_WORLD; i++) if (gGround[i][igUid] && IsValidObject(gGround[i][igObject])) DestroyObject(gGround[i][igObject]);
    return 1;
}
stock RW_InvGiveCommand(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Только для администрации.");
    new a[24], b[24], c[24], extra[24], rest[144], tail[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    if (!RW_IsNumeric(a) || !RW_IsNumeric(b) || extra[0] || (c[0] && !RW_IsNumeric(c)) || strlen(a)>5 || strlen(b)>2 || strlen(c)>5)
        return RW_Info(playerid, "/giveitem <playerid> <1=bag,2=key> [vehicleid]");
    new target = strval(a), type = strval(b), data;
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target] || !gInvLoaded[target]) return RW_Error(playerid, "Игрок недоступен.");
    if (type != ITEM_BACKPACK && type != ITEM_CAR_KEY) return RW_Error(playerid, "Предметы: 1 — рюкзак, 2 — ключ авто.");
    if (type == ITEM_CAR_KEY) {
        new vehicle = strval(c), slot = RW_IsOwnedSlot(vehicle);
        if (!c[0] || slot < 0 || !gVeh[slot][vsUsed] || gVehKeyUid[slot] <= 0) return RW_Error(playerid, "Укажите ID существующей персональной машины.");
        data = gVehKeyUid[slot];
    } else if (c[0]) return RW_Error(playerid, "Для рюкзака ID машины не нужен.");
    if (!RW_InvGrant(target, type, data)) return RW_Error(playerid, "Нет места, превышен вес или ошибка БД.");
    printf("[inventory] admin account=%d gives type=%d data=%d to account=%d", gAccountId[playerid], type, data, gAccountId[target]);
    RW_Ok(target, "Получен предмет. /inv");
    return RW_Ok(playerid, "Предмет выдан.");
}
