#if defined _railway_inventory_included
    #endinput
#endif
#define _railway_inventory_included

#define INV_MAIN 50
#define INV_BAG 30
#define INV_SLOTS 80
#define INV_NEAR 50
#define INV_WORLD 500
#define INV_TDS 165
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
new bool:gInvContext[MAX_PLAYERS];
new gInvEquipped[MAX_PLAYERS];
new bool:gInvFitting[MAX_PLAYERS];
new gInvFitSkin[MAX_PLAYERS], gInvFitUid[MAX_PLAYERS];
forward RW_InvPulse();

stock RW_InvModel(type) { return type == ITEM_BACKPACK ? 3026 : 11746; }
stock RW_InvGrams(type) { return type == ITEM_BACKPACK ? 1000 : 100; }
stock RW_InvPack(playerid)
{
    return gInvEquipped[playerid]>0 ? INV_SLOTS : -1;
}
stock RW_InvWeight(playerid, section)
{
    new weight, start = section ? INV_MAIN : 0, end = section ? INV_SLOTS : INV_MAIN;
    for (new i = start; i < end; i++) if (gInv[playerid][i][iiUid]) weight += RW_InvGrams(gInv[playerid][i][iiType]);
    if (!section && gInvEquipped[playerid]>0) weight += RW_InvGrams(ITEM_BACKPACK);
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
stock RW_InvAttach(playerid, bool:force = false)
{
    new bool:hasBag = gInvLoaded[playerid] && gInvEquipped[playerid]>0;
    if ((!hasBag || force) && IsPlayerAttachedObjectSlotUsed(playerid, INV_ATTACH)) RemovePlayerAttachedObject(playerid, INV_ATTACH);
    if (hasBag && !IsPlayerAttachedObjectSlotUsed(playerid, INV_ATTACH)) {
        // Model 3026's spine-space axes: no old sideways 90-degree rotation.
        new Float:x=-0.11,Float:y=-0.08,Float:z=0.0,Float:rx,Float:ry,Float:rz,query[200];
        format(query,sizeof query,"SELECT x,y,z,rx,ry,rz FROM backpack_fit WHERE owner=%d AND skin=%d",gAccountId[playerid],GetPlayerSkin(playerid));
        new DBResult:r=db_query(gDB,query);
        if(r!=DBResult:0) {
            if(db_num_rows(r)) { x=db_get_field_assoc_float(r,"x"); y=db_get_field_assoc_float(r,"y"); z=db_get_field_assoc_float(r,"z"); rx=db_get_field_assoc_float(r,"rx"); ry=db_get_field_assoc_float(r,"ry"); rz=db_get_field_assoc_float(r,"rz"); }
            db_free_result(r);
        }
        SetPlayerAttachedObject(playerid, INV_ATTACH, 3026, 1, x,y,z,rx,ry,rz,1.0,1.0,1.0);
    }
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
    if (!RW_InvEquipmentSchema()) return 0;
    // Personal cars are session-only. Never let a recycled vehicle ID revive a key.
    if (!RW_DB_Exec("DELETE FROM inventory_items WHERE type=2 AND data NOT IN (SELECT token FROM owned_cars)")) return 0;
    new DBResult:r = db_query(gDB, "SELECT uid,type,data,x,y,z FROM inventory_items WHERE owner IS NULL AND uid NOT IN(SELECT uid FROM inventory_equipment) ORDER BY uid");
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
    SetTimer("RW_GroundPulse", 150, true);
    return 1;
}
stock RW_InvLoad(playerid)
{
    gInvLoaded[playerid] = false;
    if (!gInvReady) return 0;
    for (new i; i < INV_SLOTS; i++) for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][i][j] = 0;
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
    if (!RW_InvEquipmentLoad(playerid)) valid = 0;
    if (RW_InvWeight(playerid, 0) > INV_MAIN_GRAMS || RW_InvWeight(playerid, 1) > INV_BAG_GRAMS || (RW_InvPack(playerid) < 0 && RW_InvWeight(playerid, 1) > 0)) valid = 0;
    if (!valid) { print("[inventory] Invalid inventory state; data preserved, player inventory locked."); return 0; }
    gInvLoaded[playerid] = true;
    return 1;
}
stock RW_InvHide(playerid)
{
    if (gInvCreated[playerid]) for (new i; i < INV_TDS; i++) PlayerTextDrawHide(playerid, gInvTD[playerid][i]);
    gInvSelected[playerid] = 0; gInvContext[playerid] = false;
    return 1;
}
stock RW_InvReset(playerid)
{
    if (gInvCreated[playerid]) for (new i; i < INV_TDS; i++) PlayerTextDrawDestroy(playerid, gInvTD[playerid][i]);
    gInvEquipped[playerid] = 0; gInvFitting[playerid] = false;
    gInvContext[playerid] = false;
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
    if ((type==ITEM_BACKPACK && data!=0) || data<0 || (type==ITEM_CAR_KEY && data>0 && !RW_InvKeyTokenKnown(data))) return 0;
    new slot = RW_InvFree(playerid, 0, type);
    if (slot < 0) return 0;
    if (!RW_DB_Exec("SAVEPOINT inventory_grant")) return 0;
    new query[256]; format(query, sizeof query, "INSERT INTO inventory_items(owner,slot,type,data) VALUES(%d,%d,%d,%d)", gAccountId[playerid], slot, type, data);
    new bool:ok = RW_InvWrite(query), uid;
    if (ok) {
        format(query,sizeof query,"SELECT uid FROM inventory_items WHERE owner=%d AND slot=%d AND type=%d",gAccountId[playerid],slot,type);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0) { if(db_num_rows(r)==1) uid = db_get_field_assoc_int(r, "uid"); db_free_result(r); }
        if (uid <= 0) ok = false;
    }
    if (ok && type == ITEM_CAR_KEY && data == 0) {
        data = uid;
        format(query, sizeof query, "UPDATE inventory_items SET data=%d WHERE uid=%d", data, uid);
        ok = RW_InvWrite(query);
    }
    if (ok && type == ITEM_CAR_KEY) ok = RW_GarageMarkKey(uid, data);
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
    for (new p; p < MAX_PLAYERS; p++) if (gInvLoaded[p]) {
        new bool:changed;
        for (new i; i < INV_SLOTS; i++) if (gInv[p][i][iiType] == ITEM_CAR_KEY && gInv[p][i][iiData] == token) {
            for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[p][i][j] = 0;
            changed=true;
        }
        if(changed && gUIMode[p]==UI_INVENTORY) RW_InvRender(p);
    }
    for (new i; i < INV_WORLD; i++) if (gGround[i][igType] == ITEM_CAR_KEY && gGround[i][igData] == token) {
        if (IsValidObject(gGround[i][igObject])) DestroyObject(gGround[i][igObject]);
        gGround[i][igUid] = 0;
    }
    return 1;
}
stock bool:RW_InvCanRemove(playerid, slot)
{
    if (gInv[playerid][slot][iiType] == ITEM_CAR_KEY) for (new v; v < VEHICLE_SLOTS; v++)
        if (gVeh[v][vsUsed] && gVeh[v][vsKey] && gVehKeyUid[v] == gInv[playerid][slot][iiData]) {
            RW_Error(playerid, "Remove the key from the ignition first."); return false;
        }
    return true;
}
stock RW_InvTransfer(playerid, section)
{
    if (!gInvFromGround[playerid] && gInvSelected[playerid] == gInvEquipped[playerid] && gInvEquipped[playerid]>0) {
        if(section==0) return RW_InvUnequip(playerid);
        return 1;
    }
    new uid = gInvSelected[playerid], query[320];
    if (gInvFromGround[playerid]) {
        new ground = RW_InvGroundFind(uid);
        if (!RW_InvNearGround(playerid, ground)) return RW_Error(playerid, "Item is no longer available.");
        new dest = RW_InvFree(playerid, section, gGround[ground][igType]);
        if (dest < 0) return RW_Error(playerid, "No space or weight limit exceeded.");
        format(query, sizeof query, "UPDATE inventory_items SET owner=%d,slot=%d,x=0,y=0,z=0 WHERE uid=%d AND owner IS NULL AND uid NOT IN(SELECT uid FROM inventory_equipment)", gAccountId[playerid], dest, uid);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not save the item.");
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
        if (dest < 0) return RW_Error(playerid, "No space or weight limit exceeded.");
        format(query, sizeof query, "UPDATE inventory_items SET slot=%d WHERE uid=%d AND owner=%d AND slot=%d", dest, uid, gAccountId[playerid], source);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not save the transfer.");
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
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return RW_Error(playerid, "Drop items on foot, outdoors, in the main world only.");
    new ground = -1;
    for (new i; i < INV_WORLD; i++) if (!gGround[i][igUid]) { ground = i; break; }
    if (ground < 0) return RW_Error(playerid, "Too many items on the ground.");
    new Float:x, Float:y, Float:z, Float:a;
    GetPlayerPos(playerid, x, y, z); GetPlayerFacingAngle(playerid, a);
    x += 0.6*floatsin(-a, degrees); y += 0.6*floatcos(-a, degrees); z -= 0.9;
    new object = CreateObject(RW_InvModel(gInv[playerid][source][iiType]), x, y, z, 0.0, 0.0, a, 70.0);
    if (object == INVALID_OBJECT_ID) return RW_Error(playerid, "Could not create the world item.");
    new query[320]; format(query, sizeof query, "UPDATE inventory_items SET owner=NULL,slot=-1,x=%f,y=%f,z=%f WHERE uid=%d AND owner=%d AND slot=%d", x, y, z, gInv[playerid][source][iiUid], gAccountId[playerid], source);
    if (!RW_InvWrite(query)) { DestroyObject(object); return RW_Error(playerid, "Could not save the item."); }
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
    RW_InvLabel(playerid, 147, 253.0, 304.0, "BACK / 7 EQUIP SOON", 143.0);
    PlayerTextDrawSetSelectable(playerid,gInvTD[playerid][131],1);
    RW_InvLabel(playerid, 148, 595.0, 377.0, "X", 22.0, true);
    RW_InvLabel(playerid, 149, 285.0, 377.0, "MAIN", 62.0, true);
    RW_InvLabel(playerid, 150, 360.0, 377.0, "BAG", 62.0, true);
    RW_InvLabel(playerid, 151, 435.0, 377.0, "DROP", 62.0, true);
    RW_InvLabel(playerid, 152, 510.0, 377.0, "USE", 62.0, true);
    RW_InvLabel(playerid, 153, 20.0, 377.0, "SELECT ITEM", 255.0);
    RW_InvLabel(playerid, 154, 420.0, 240.0, "CLICK ITEM / ACTIONS", 195.0);
    RW_InvSprite(playerid, 155, 420.0, 261.0, 198.0, 108.0, 0x171F2CFF);
    RW_InvLabel(playerid, 156, 430.0, 266.0, "ITEM", 175.0);
    RW_InvLabel(playerid, 157, 430.0, 281.0, "DESCRIPTION", 175.0);
    RW_InvLabel(playerid, 158, 430.0, 296.0, "STATS", 175.0);
    RW_InvLabel(playerid, 159, 430.0, 313.0, "DROP", 80.0, true);
    RW_InvLabel(playerid, 160, 530.0, 313.0, "WEAR", 80.0, true);
    RW_InvLabel(playerid, 161, 430.0, 330.0, "TO BAG", 80.0, true);
    RW_InvLabel(playerid, 162, 530.0, 330.0, "TO MAIN", 80.0, true);
    RW_InvLabel(playerid, 163, 430.0, 347.0, "USE KEY", 80.0, true);
    RW_InvLabel(playerid, 164, 530.0, 347.0, "CLOSE", 80.0, true);
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
stock RW_InvContextRender(playerid)
{
    for (new i = 155; i < INV_TDS; i++) PlayerTextDrawHide(playerid, gInvTD[playerid][i]);
    if (!gInvContext[playerid] || !gInvSelected[playerid]) return 1;
    new slot = RW_InvFind(playerid, gInvSelected[playerid]), ground = RW_InvGroundFind(gInvSelected[playerid]), type, data;
    if (gInvFromGround[playerid]) { if (!RW_InvNearGround(playerid, ground)) return 1; type=gGround[ground][igType]; data=gGround[ground][igData]; }
    else { if(slot<0) { if(gInvSelected[playerid]!=gInvEquipped[playerid]) return 1; type=ITEM_BACKPACK; } else { type=gInv[playerid][slot][iiType]; data=gInv[playerid][slot][iiData]; } }
    PlayerTextDrawSetString(playerid, gInvTD[playerid][156], type == ITEM_BACKPACK ? ("BACKPACK") : ("CAR KEY"));
    new stats[128], desc[128];
    if (type == ITEM_BACKPACK) { format(desc,sizeof desc,"%s",gInvSelected[playerid]==gInvEquipped[playerid] ? ("Equipped / BACK slot") : ("Equip in BACK slot")); format(stats,sizeof stats,"1kg / +30 slots / 40kg"); }
    else {
        new model, bool:permanent = RW_GarageKeyModel(data, model);
        format(desc,sizeof desc,"%s vehicle access", permanent ? ("Permanent") : ("Temporary"));
        format(stats,sizeof stats,"0.1kg / model %d",model);
    }
    PlayerTextDrawSetString(playerid,gInvTD[playerid][157],desc);
    PlayerTextDrawSetString(playerid,gInvTD[playerid][158],stats);
    for(new i=155;i<=158;i++) PlayerTextDrawShow(playerid,gInvTD[playerid][i]);
    PlayerTextDrawShow(playerid,gInvTD[playerid][164]);
    if (!gInvFromGround[playerid] && gInvSelected[playerid]!=gInvEquipped[playerid]) {
        PlayerTextDrawShow(playerid,gInvTD[playerid][159]);
        if(type==ITEM_BACKPACK) PlayerTextDrawShow(playerid,gInvTD[playerid][160]);
        if(type==ITEM_CAR_KEY) PlayerTextDrawShow(playerid,gInvTD[playerid][163]);
    }
    if ((gInvFromGround[playerid] || slot<INV_MAIN) && RW_InvPack(playerid)>=0 && type!=ITEM_BACKPACK) PlayerTextDrawShow(playerid,gInvTD[playerid][161]);
    if (gInvFromGround[playerid] || slot>=INV_MAIN || (gInvEquipped[playerid]>0 && gInvSelected[playerid]==gInvEquipped[playerid])) PlayerTextDrawShow(playerid,gInvTD[playerid][162]);
    return 1;
}
stock RW_InvRender(playerid)
{
    if (gUIMode[playerid] != UI_INVENTORY || !gInvCreated[playerid]) return 1;
    new hasBag = RW_InvPack(playerid) >= 0;
    RW_InvCell(playerid,131,hasBag ? ITEM_BACKPACK : 0,gInvSelected[playerid]>0 && gInvSelected[playerid]==gInvEquipped[playerid]);
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
        else if(gInvEquipped[playerid]>0 && gInvSelected[playerid]==gInvEquipped[playerid]) type=ITEM_BACKPACK;
    }
    if (!type) { gInvSelected[playerid] = 0; format(text, sizeof text, "SELECT ITEM"); }
    else if (type == ITEM_BACKPACK) format(text, sizeof text, "BACKPACK / 1kg / +30");
    else {
        new vehicle = -1;
        for (new v; v < VEHICLE_SLOTS; v++) if (gVeh[v][vsUsed] && gVehKeyUid[v] == data) vehicle = gVeh[v][vsVehicleId];
        if (vehicle < 0) format(text, sizeof text, "KEY / GARAGED / 0.1kg");
        else format(text, sizeof text, "KEY / CAR %d / 0.1kg", vehicle);
    }
    PlayerTextDrawSetString(playerid, gInvTD[playerid][153], text);
    return RW_InvContextRender(playerid);
}
stock RW_InvOpen(playerid)
{
    if (gUIMode[playerid] == UI_INVENTORY) return RW_UI_Close(playerid);
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return RW_Error(playerid, "Inventory unavailable: check the database.");
    new pstate = GetPlayerState(playerid);
    if (pstate != PLAYER_STATE_ONFOOT && pstate != PLAYER_STATE_DRIVER && pstate != PLAYER_STATE_PASSENGER) return 1;
    if (gUIInput[playerid] || gInvFitting[playerid]) return 1;
    RW_UI_Close(playerid, true);
    for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    RW_InvCreate(playerid);
    gUIMode[playerid] = UI_INVENTORY;
    // Backgrounds are also CREATED first: SA-MP renders by pool ID.
    for (new i = 139; i <= 142; i++) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    PlayerTextDrawSetPreviewModel(playerid, gInvTD[playerid][130], GetPlayerSkin(playerid));
    for (new i = 130; i < 139; i++) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    for (new i = 143; i < 155; i++) if (i<149 || i>152) PlayerTextDrawShow(playerid, gInvTD[playerid][i]);
    RW_InvRender(playerid);
    RW_UI_SelectBegin(playerid);
    return 1;
}
stock RW_InvClick(playerid, PlayerText:clicked)
{
    if (gUIMode[playerid] != UI_INVENTORY || !gLogged[playerid] || !gInvLoaded[playerid]) return 0;
    for (new i; i < 130; i++) if (clicked == gInvTD[playerid][i]) {
        if (i >= 50 && i < 80 && RW_InvPack(playerid) < 0) return 1;
        gInvContext[playerid] = true;
        gInvFromGround[playerid] = i >= 80;
        gInvSelected[playerid] = i < 80 ? gInv[playerid][i][iiUid] : gInvNear[playerid][i-80];
        return RW_InvRender(playerid);
    }
    if (clicked == gInvTD[playerid][131]) {
        if(gInvEquipped[playerid]>0) { gInvSelected[playerid]=gInvEquipped[playerid]; gInvFromGround[playerid]=false; gInvContext[playerid]=true; return RW_InvRender(playerid); }
        return RW_InvEquip(playerid);
    }
    if (clicked == gInvTD[playerid][164]) { gInvContext[playerid] = false; return RW_InvRender(playerid); }
    if (clicked == gInvTD[playerid][148]) return RW_UI_Close(playerid);
    if (GetTickCount()-gInvTick[playerid] < 250) return 1;
    gInvTick[playerid] = GetTickCount();
    if (!gInvSelected[playerid] || !gInvContext[playerid]) return 1;
    if (clicked == gInvTD[playerid][160] && !gInvFromGround[playerid]) {
        new slot=RW_InvFind(playerid,gInvSelected[playerid]);
        if(slot>=0 && gInv[playerid][slot][iiType]==ITEM_BACKPACK) return RW_InvEquip(playerid);
    }
    if (clicked == gInvTD[playerid][162]) return RW_InvTransfer(playerid, 0);
    if (clicked == gInvTD[playerid][161]) return RW_InvTransfer(playerid, 1);
    if (clicked == gInvTD[playerid][159]) return RW_InvDrop(playerid);
    if (clicked == gInvTD[playerid][163] && !gInvFromGround[playerid]) {
        new slot = RW_InvFind(playerid, gInvSelected[playerid]);
        if (slot >= 0 && gInv[playerid][slot][iiType] == ITEM_CAR_KEY) {
            new token = gInv[playerid][slot][iiData];
            for (new v; v < VEHICLE_SLOTS; v++) if (gVeh[v][vsUsed] && gVehKeyUid[v] == token) {
                RW_UI_Close(playerid); return RW_ControlExecute(playerid, gVeh[v][vsVehicleId], 2);
            }
        } else if (slot >= 0) return RW_Info(playerid, "Backpack already equipped.");
    }
    return 1;
}
public RW_InvPulse()
{
    RW_GaragePulse();
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
// Accepted words so a correctly meant command is not rejected as a typo:
// 1/bag/backpack, 2/key/carkey, and a vehicle as <id>, p<plate number> or c<garage car id>.
stock RW_InvItemWord(const word[])
{
    if (!word[0]) return -1;
    if (!strcmp(word, "1", true) || !strcmp(word, "bag", true) || !strcmp(word, "backpack", true)) return ITEM_BACKPACK;
    if (!strcmp(word, "2", true) || !strcmp(word, "key", true) || !strcmp(word, "carkey", true) || !strcmp(word, "keys", true)) return ITEM_CAR_KEY;
    return -1;
}
// Resolves the vehicle argument and reports the exact reason when it cannot.
// Returns the vehicle slot, VEHICLE_SLOTS for a stored permanent car, or -1.
stock RW_InvGiveVehicle(playerid, const arg[], &data)
{
    data = 0;
    new vehicle = -1, message[160];
    if (!arg[0] || !strcmp(arg, "me", true) || !strcmp(arg, "aim", true)) {
        vehicle = RW_ControlTarget(playerid);
        if (vehicle <= 0 || vehicle >= MAX_VEHICLES) { RW_Error(playerid, "Sit in a managed car or aim at one within 6 metres; /cars lists them."); return -1; }
    } else if (arg[0] == 'p' || arg[0] == 'P') {
        new plate = strval(arg[1]);
        if (plate <= 0 || plate > VEHICLE_SLOTS || !gVeh[plate-1][vsUsed] || gVehKeyUid[plate-1] <= 0) { RW_Error(playerid, "No spawned car with that plate. /cars lists managed cars and plates."); return -1; }
        data = gVehKeyUid[plate-1];
        return plate - 1;
    } else if (arg[0] == 'c' || arg[0] == 'C') {
        new car = strval(arg[1]), query[128];
        if (car <= 0) { RW_Error(playerid, "Use c<garage car id> from /cars, for example c3."); return -1; }
        format(query, sizeof query, "SELECT token FROM owned_cars WHERE id=%d", car);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0) { if (db_num_rows(r)) data = db_get_field_assoc_int(r, "token"); db_free_result(r); }
        if (data <= 0) { RW_Error(playerid, "No permanent car with that id. /cars lists garage cars."); return -1; }
        return VEHICLE_SLOTS;
    } else if (RW_IsNumeric(arg)) {
        vehicle = strval(arg);
    } else {
        RW_Error(playerid, "Vehicle must be an id, p<plate number> or c<garage car id>. /cars lists them.");
        return -1;
    }
    if (vehicle <= 0 || vehicle >= MAX_VEHICLES) { RW_Error(playerid, "No vehicle with that id. /cars lists managed cars and their ids."); return -1; }
    new slot = RW_IsOwnedSlot(vehicle);
    if (slot >= 0 && gVeh[slot][vsUsed] && gVehKeyUid[slot] > 0) { data = gVehKeyUid[slot]; return slot; }
    new number = strval(arg);
    if (RW_IsNumeric(arg) && number >= 1 && number <= VEHICLE_SLOTS && gVeh[number-1][vsUsed] && gVehKeyUid[number-1] > 0) {
        format(message, sizeof message, "Vehicle %d has no server key, but plate RW-%03d is vehicle id %d. Use /giveitem <id> key %d", vehicle, number, gVeh[number-1][vsVehicleId], gVeh[number-1][vsVehicleId]);
        RW_Error(playerid, message);
        return -1;
    }
    if (GetVehicleModel(vehicle)) format(message, sizeof message, "Vehicle %d exists but has no key managed by the server (/spcar and garage cars only).", vehicle);
    else format(message, sizeof message, "No vehicle with id %d. /cars lists managed cars and their ids.", vehicle);
    RW_Error(playerid, message);
    return -1;
}
// Reports why a grant failed instead of one generic message.
stock RW_InvExplainGrant(playerid, target, type, data)
{
    new message[160];
    if (type == ITEM_CAR_KEY && data > 0 && !RW_InvKeyTokenKnown(data)) return RW_Error(playerid, "That car's key token no longer exists. /cars lists valid cars. Nothing was granted.");
    if (RW_InvFree(target, 0, type) < 0) {
        new grams = RW_InvWeight(target, 0) + RW_InvGrams(type);
        if (grams > INV_MAIN_GRAMS)
            format(message, sizeof message, "Target would carry %.1f kg of %.1f kg in MAIN. Nothing was granted.", float(grams)/1000.0, float(INV_MAIN_GRAMS)/1000.0);
        else
            format(message, sizeof message, "Target has no free MAIN slot (50 slots, keys and backpacks only go to MAIN). Nothing was granted.");
        return RW_Error(playerid, message);
    }
    return RW_Error(playerid, "Database error; nothing was granted. Run dbcheck in the server console.");
}
stock RW_InvGiveCommand(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    new a[32], b[32], c[32], extra[32], rest[144], tail[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    if (!a[0] || !b[0] || extra[0] || strlen(a) > 5 || strlen(b) > 12 || strlen(c) > 8)
        return RW_Info(playerid, "/giveitem <playerid> <bag|key> [vehicle id, p<plate> or c<garage car id>]");
    if (!RW_IsNumeric(a)) return RW_Error(playerid, "Player id must be a number, for example /giveitem 0 key 7");
    new target = strval(a), type = RW_InvItemWord(b), data;
    if (type < 0) return RW_Error(playerid, "Items: bag (backpack) or key (car key).");
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Player must be connected and logged in. /givecar <name> <model> works offline.");
    if (!gInvLoaded[target]) return RW_Error(playerid, "Player inventory is not loaded yet. Ask them to reconnect.");
    if (type == ITEM_CAR_KEY) {
        if (RW_InvGiveVehicle(playerid, c, data) < 0) return 1;
    } else if (c[0]) return RW_Error(playerid, "A backpack takes no vehicle. Use /giveitem <playerid> bag.");
    new uid = RW_InvGrant(target, type, data);
    if (!uid) return RW_InvExplainGrant(playerid, target, type, data);
    printf("[inventory] admin account=%d gives type=%d data=%d uid=%d to account=%d", gAccountId[playerid], type, data, uid, gAccountId[target]);
    RW_Ok(target, "Item received. /inv");
    new message[144];
    if (type == ITEM_CAR_KEY) format(message, sizeof message, "Key granted (item %d, car token %d). It starts that car.", uid, data);
    else format(message, sizeof message, "Backpack granted (item %d). Equip it in the BACK slot to unlock 30 bag slots.", uid);
    return RW_Ok(playerid, message);
}

// Equipment owns the same item UID; its inventory row is unowned but excluded
// from ground queries. No item is copied or deleted when it is equipped.
stock RW_InvEquipmentSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS inventory_equipment (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS backpack_fit (owner INTEGER, skin INTEGER, x REAL, y REAL, z REAL, rx REAL, ry REAL, rz REAL, PRIMARY KEY(owner,skin))")) return 0;
    if (!RW_DB_Exec("SAVEPOINT equipment_upgrade")) return 0;
    // Only migrate old accounts with occupied BAG slots: never strand their items.
    new bool:ok = RW_DB_Exec("INSERT INTO inventory_equipment(owner,uid) SELECT i.owner,MIN(i.uid) FROM inventory_items i WHERE i.owner IS NOT NULL AND i.type=1 AND i.slot<50 AND EXISTS(SELECT 1 FROM inventory_items b WHERE b.owner=i.owner AND b.slot>=50) AND NOT EXISTS(SELECT 1 FROM inventory_equipment e WHERE e.owner=i.owner) GROUP BY i.owner");
    if (ok) ok = RW_DB_Exec("UPDATE inventory_items SET owner=NULL,slot=-1 WHERE uid IN(SELECT uid FROM inventory_equipment e WHERE e.owner=inventory_items.owner) AND owner IS NOT NULL");
    if (!ok || !RW_DB_Exec("RELEASE equipment_upgrade")) { RW_DB_Exec("ROLLBACK TO equipment_upgrade"); RW_DB_Exec("RELEASE equipment_upgrade"); return 0; }
    return 1;
}
stock RW_InvEquipmentLoad(playerid)
{
    gInvEquipped[playerid] = 0;
    new query[280]; format(query,sizeof query,"SELECT e.uid,i.type,i.slot,(i.owner IS NULL) AS unowned FROM inventory_equipment e LEFT JOIN inventory_items i ON i.uid=e.uid WHERE e.owner=%d",gAccountId[playerid]);
    new DBResult:r=db_query(gDB,query);
    if(r==DBResult:0) return 0;
    new count=db_num_rows(r), valid=1;
    if(count==1) {
        new uid=db_get_field_assoc_int(r,"uid");
        if(uid<=0 || db_get_field_assoc_int(r,"type")!=ITEM_BACKPACK || db_get_field_assoc_int(r,"slot")!=-1 || !db_get_field_assoc_int(r,"unowned")) valid=0;
        else gInvEquipped[playerid]=uid;
    } else if(count>1) valid=0;
    db_free_result(r);
    return valid;
}
stock RW_InvEquip(playerid)
{
    if(!gInvLoaded[playerid] || gInvFromGround[playerid]) return 1;
    if(gInvEquipped[playerid]) return RW_Error(playerid,"Remove the equipped backpack first.");
    new slot=RW_InvFind(playerid,gInvSelected[playerid]);
    if(slot<0 || slot>=INV_MAIN || gInv[playerid][slot][iiType]!=ITEM_BACKPACK) return RW_Error(playerid,"Select a backpack in MAIN, then click the BACK equipment slot.");
    if(!RW_DB_Exec("SAVEPOINT equip_backpack")) return RW_Error(playerid,"Database unavailable.");
    new uid=gInv[playerid][slot][iiUid], query[260];
    format(query,sizeof query,"INSERT INTO inventory_equipment(owner,uid) VALUES(%d,%d)",gAccountId[playerid],uid);
    new bool:ok=RW_InvWrite(query);
    if(ok) {
        format(query,sizeof query,"UPDATE inventory_items SET owner=NULL,slot=-1 WHERE uid=%d AND owner=%d AND slot=%d AND type=1",uid,gAccountId[playerid],slot);
        ok=RW_InvWrite(query);
    }
    if(!ok || !RW_DB_Exec("RELEASE equip_backpack")) { RW_DB_Exec("ROLLBACK TO equip_backpack"); RW_DB_Exec("RELEASE equip_backpack"); return RW_Error(playerid,"Could not equip backpack."); }
    for(new E_INV_ITEM:j;j<E_INV_ITEM;j++) gInv[playerid][slot][j]=0;
    gInvEquipped[playerid]=uid;
    RW_InvAttach(playerid,true);
    return RW_InvRender(playerid);
}
stock RW_InvUnequip(playerid)
{
    if(!gInvLoaded[playerid] || !gInvEquipped[playerid]) return 1;
    if(RW_InvWeight(playerid,1)>0) return RW_Error(playerid,"Empty the backpack before removing it.");
    new dest=-1;
    for(new i;i<INV_MAIN;i++) if(!gInv[playerid][i][iiUid]) { dest=i; break; }
    if(dest<0) return RW_Error(playerid,"Free a MAIN slot before removing the backpack.");
    if(!RW_DB_Exec("SAVEPOINT unequip_backpack")) return RW_Error(playerid,"Database unavailable.");
    new uid=gInvEquipped[playerid],query[300];
    format(query,sizeof query,"UPDATE inventory_items SET owner=%d,slot=%d WHERE uid=%d AND owner IS NULL AND slot=-1 AND EXISTS(SELECT 1 FROM inventory_equipment WHERE owner=%d AND uid=%d)",gAccountId[playerid],dest,uid,gAccountId[playerid],uid);
    new bool:ok=RW_InvWrite(query);
    if(ok) { format(query,sizeof query,"DELETE FROM inventory_equipment WHERE owner=%d AND uid=%d",gAccountId[playerid],uid); ok=RW_InvWrite(query); }
    if(!ok || !RW_DB_Exec("RELEASE unequip_backpack")) { RW_DB_Exec("ROLLBACK TO unequip_backpack"); RW_DB_Exec("RELEASE unequip_backpack"); return RW_Error(playerid,"Could not remove backpack."); }
    gInv[playerid][dest][iiUid]=uid; gInv[playerid][dest][iiType]=ITEM_BACKPACK; gInv[playerid][dest][iiData]=0;
    gInvEquipped[playerid]=0;
    RW_InvAttach(playerid,true);
    return RW_InvRender(playerid);
}
stock RW_InvFitCommand(playerid)
{
    if(!gLogged[playerid] || gAdminLevel[playerid]<1 || !gInvEquipped[playerid]) return RW_Error(playerid,"Admin with an equipped backpack required.");
    if(GetPlayerState(playerid)!=PLAYER_STATE_ONFOOT) return RW_Error(playerid,"Stand on foot to adjust your backpack.");
    RW_UI_Close(playerid);
    gInvFitSkin[playerid]=GetPlayerSkin(playerid); gInvFitUid[playerid]=gInvEquipped[playerid];
    gInvFitting[playerid]=true;
    EditAttachedObject(playerid,INV_ATTACH);
    return RW_Info(playerid,"Adjust backpack position for your skin, then save. Cancel restores the previous fit.");
}
stock RW_InvFitSave(playerid, response, index, model, bone, Float:x,Float:y,Float:z,Float:rx,Float:ry,Float:rz,Float:sx,Float:sy,Float:sz)
{
    if(index!=INV_ATTACH || !gInvFitting[playerid]) return 0;
    if(response==EDIT_RESPONSE_UPDATE) return 1;
    gInvFitting[playerid]=false;
    if(response==EDIT_RESPONSE_FINAL && gLogged[playerid] && gAdminLevel[playerid]>=1 && gInvEquipped[playerid]==gInvFitUid[playerid] && gInvEquipped[playerid]>0 && GetPlayerSkin(playerid)==gInvFitSkin[playerid] && model==3026 && bone==1) {
        if(!(x>=-0.6 && x<=0.6 && y>=-0.6 && y<=0.6 && z>=-0.6 && z<=0.6 && rx>=-360.0 && rx<=360.0 && ry>=-360.0 && ry<=360.0 && rz>=-360.0 && rz<=360.0)) { RW_InvAttach(playerid,true); return RW_Error(playerid,"Backpack fit outside allowed range."); }
        new query[350]; format(query,sizeof query,"INSERT OR REPLACE INTO backpack_fit(owner,skin,x,y,z,rx,ry,rz) VALUES(%d,%d,%f,%f,%f,%f,%f,%f)",gAccountId[playerid],GetPlayerSkin(playerid),x,y,z,rx,ry,rz);
        if(!gDBWritable || !RW_DB_Exec(query)) RW_Error(playerid,"Could not save backpack fit.");
        else RW_Ok(playerid,"Backpack fit saved for this skin. Scale remains 1:1.");
    }
    // Scaling is deliberately fixed; don't accept a client-edited giant model.
    #pragma unused sx,sy,sz
    RW_InvAttach(playerid,true);
    return 1;
}

stock bool:RW_InvKeyTokenKnown(token)
{
    if(token<=0) return false;
    for(new s;s<VEHICLE_SLOTS;s++) if(gVeh[s][vsUsed] && gVehKeyUid[s]==token) return true;
    new query[140]; format(query,sizeof query,"SELECT id FROM owned_cars WHERE token=%d",token);
    new DBResult:r=db_query(gDB,query);
    if(r==DBResult:0) return false;
    new bool:known=db_num_rows(r)==1; db_free_result(r); return known;
}
