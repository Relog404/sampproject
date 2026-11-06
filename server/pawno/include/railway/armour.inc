/*  ============================================================================
 *  railway/armour.inc
 *  Body armour is a separate inventory item (type 7). While it is worn the
 *  vest is drawn on the player and absorbs part of every hit.
 *
 *  HOW TO SHOW YOUR OWN TEXTURE
 *  SA-MP 0.3.7 cannot send files to a client, so the texture itself must
 *  already be in the player's GTA (a mod loader folder, or a TXD replaced
 *  inside gta3.img). The server only decides WHICH model is drawn:
 *    1) your texture replaces the model's own TXD - set the model id in
 *       RW_ArmourModel and leave RW_ArmourTxd / RW_ArmourTexture empty;
 *       the client then draws your texture by itself, nothing else to do;
 *    2) your texture lives in its own TXD - fill in the model id, the TXD
 *       name (without ".txd") and the texture name inside it; the vest is
 *       then built as a server object and that material is set on it.
 *  Both ways need no extra files from the players once their game is set up.
 *  ========================================================================= */

#if defined _railway_armour_included
    #endinput
#endif
#define _railway_armour_included

#include <railway/core>

#define ARMOUR_ATTACH (8)   /* attached-object slot; slot 9 is the backpack */
#define ARMOUR_BONE   (1)   /* spine                                       */
#define ARMOUR_VARIANTS (3)

/*  --- VARIANT TABLE -------------------------------------------------------
 *  variant 1 = light, 2 = standard, 3 = heavy. Item "data" holds the variant.
 *  MODEL:   the object model worn on the player.
 *  TXD:     TXD file name without ".txd", "" = keep the model's own skin.
 *  TEXTURE: texture name inside that TXD, "" = keep the model's own skin.
 *  HP:      armour points it absorbs before it is destroyed.
 *  ABSORB:  percent of the damage taken by the armour.
 *  ------------------------------------------------------------------------ */
stock RW_ArmourModel(variant)
{
    switch (variant) {
        case 1: return 1242;      /* GTA SA body armour                     */
        case 2: return 1242;
        case 3: return 1242;
    }
    return 1242;
}
stock RW_ArmourTxd(variant, dest[], size = sizeof dest)
{
    switch (variant) {
        case 1: format(dest, size, "");
        case 2: format(dest, size, "");   /* example: "war1" for war1.txd      */
        case 3: format(dest, size, "");
    }
    return 1;
}
stock RW_ArmourTexture(variant, dest[], size = sizeof dest)
{
    switch (variant) {
        case 1: format(dest, size, "");
        case 2: format(dest, size, "");   /* example: "armour"                 */
        case 3: format(dest, size, "");
    }
    return 1;
}
stock RW_ArmourHpOf(variant)
{
    switch (variant) {
        case 1: return 50;
        case 2: return 100;
        case 3: return 150;
    }
    return 100;
}
stock RW_ArmourAbsorb(variant)
{
    switch (variant) {
        case 1: return 50;
        case 2: return 65;
        case 3: return 80;
    }
    return 65;
}
stock RW_ArmourGrams(variant)
{
    switch (variant) {
        case 1: return 3000;
        case 2: return 5000;
        case 3: return 7000;
    }
    return 5000;
}
stock RW_ArmourName(variant, dest[], size = sizeof dest)
{
    switch (variant) {
        case 1: format(dest, size, "LIGHT ARMOUR");
        case 2: format(dest, size, "BODY ARMOUR");
        case 3: format(dest, size, "HEAVY ARMOUR");
        default: format(dest, size, "ARMOUR");
    }
    return 1;
}
stock bool:RW_ArmourVariant(variant) { return variant >= 1 && variant <= ARMOUR_VARIANTS; }
stock bool:RW_ArmourWorn(playerid) { return gInvLoaded[playerid] && gInvWorn[playerid] > 0; }

stock RW_ArmourSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS inventory_wear (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE, hp INTEGER NOT NULL DEFAULT 0)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS armour_fit (owner INTEGER, skin INTEGER, x REAL, y REAL, z REAL, rx REAL, ry REAL, rz REAL, PRIMARY KEY(owner,skin))")) return 0;
    return 1;
}
// Per-skin fit, then the defaults for a vest on the spine.
stock RW_ArmourFit(playerid, &Float:x, &Float:y, &Float:z, &Float:rx, &Float:ry, &Float:rz)
{
    x = 0.0; y = 0.06; z = 0.0; rx = 0.0; ry = 0.0; rz = 0.0;
    if (gAccountId[playerid] <= 0) return 1;
    new query[200];
    format(query, sizeof query, "SELECT x,y,z,rx,ry,rz FROM armour_fit WHERE owner=%d AND skin=%d", gAccountId[playerid], GetPlayerSkin(playerid));
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 1;
    if (db_num_rows(r)) {
        x = db_get_field_assoc_float(r, "x"); y = db_get_field_assoc_float(r, "y"); z = db_get_field_assoc_float(r, "z");
        rx = db_get_field_assoc_float(r, "rx"); ry = db_get_field_assoc_float(r, "ry"); rz = db_get_field_assoc_float(r, "rz");
    }
    db_free_result(r);
    return 1;
}
stock RW_ArmourObjectClear(playerid)
{
    if (gArmourObject[playerid] > 0 && IsValidObject(gArmourObject[playerid])) DestroyObject(gArmourObject[playerid]);
    gArmourObject[playerid] = 0;
    return 1;
}
// Draws the worn vest. A named TXD uses a server object, otherwise the lighter
// attached object is enough - it keeps whatever skin the client has for it.
stock RW_ArmourAttach(playerid, bool:force = false)
{
    new bool:worn = RW_ArmourWorn(playerid);
    if ((!worn || force) && IsPlayerAttachedObjectSlotUsed(playerid, ARMOUR_ATTACH)) RemovePlayerAttachedObject(playerid, ARMOUR_ATTACH);
    if ((!worn || force) && !gArmourFitting[playerid]) RW_ArmourObjectClear(playerid);
    if (!worn) return 1;
    new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, txd[32], texture[32];
    RW_ArmourFit(playerid, x, y, z, rx, ry, rz);
    RW_ArmourTxd(gArmourData[playerid], txd, sizeof txd);
    RW_ArmourTexture(gArmourData[playerid], texture, sizeof texture);
    if (txd[0] && texture[0] && !gArmourFitting[playerid]) {
        new model = RW_ArmourModel(gArmourData[playerid]);
        new object = CreateObject(model, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 300.0);
        if (object == INVALID_OBJECT_ID) return 0;
        SetObjectMaterial(object, 0, model, txd, texture, 0);
        AttachObjectToPlayer(object, playerid, x, y, z, rx, ry, rz);
        gArmourObject[playerid] = object;
        return 1;
    }
    SetPlayerAttachedObject(playerid, ARMOUR_ATTACH, RW_ArmourModel(gArmourData[playerid]), ARMOUR_BONE, x, y, z, rx, ry, rz, 1.0, 1.0, 1.0);
    return 1;
}
stock RW_ArmourSaveHp(playerid)
{
    if (gAccountId[playerid] <= 0 || gInvWorn[playerid] <= 0) return 1;
    new query[160];
    format(query, sizeof query, "UPDATE inventory_wear SET hp=%d WHERE owner=%d AND uid=%d", gArmourHp[playerid], gAccountId[playerid], gInvWorn[playerid]);
    return RW_InvWrite(query) ? 1 : 0;
}
stock RW_ArmourWear(playerid, slot)
{
    if (!gInvLoaded[playerid] || gInvFromGround[playerid]) return 1;
    if (gInvWorn[playerid]) return RW_Error(playerid, "Take the worn armour off first.");
    if (gDowned[playerid]) return RW_Error(playerid, "You cannot put armour on while downed.");
    if (slot < 0 || slot >= INV_MAIN || gInv[playerid][slot][iiType] != ITEM_ARMOUR) return RW_Error(playerid, "Select body armour in MAIN, then press WEAR.");
    new variant = gInv[playerid][slot][iiData];
    if (!RW_ArmourVariant(variant)) return RW_Error(playerid, "This armour has an unknown variant.");
    new uid = gInv[playerid][slot][iiUid], hp = RW_ArmourHpOf(variant), query[300];
    if (!RW_DB_Exec("SAVEPOINT wear_armour")) return RW_Error(playerid, "Database unavailable.");
    format(query, sizeof query, "INSERT INTO inventory_wear(owner,uid,hp) VALUES(%d,%d,%d)", gAccountId[playerid], uid, hp);
    new bool:ok = RW_InvWrite(query);
    if (ok) {
        format(query, sizeof query, "UPDATE inventory_items SET owner=NULL,slot=-1 WHERE uid=%d AND owner=%d AND slot=%d AND type=%d", uid, gAccountId[playerid], slot, ITEM_ARMOUR);
        ok = RW_InvWrite(query);
    }
    if (!ok || !RW_DB_Exec("RELEASE wear_armour")) { RW_DB_Exec("ROLLBACK TO wear_armour"); RW_DB_Exec("RELEASE wear_armour"); return RW_Error(playerid, "Could not put the armour on."); }
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    gInvWorn[playerid] = uid; gArmourData[playerid] = variant; gArmourHp[playerid] = hp;
    gInvSelected[playerid] = 0; gInvContext[playerid] = false;
    RW_ArmourAttach(playerid, true);
    SetPlayerArmour(playerid, float(hp));
    new name[32]; RW_ArmourName(variant, name, sizeof name);
    new message[96]; format(message, sizeof message, "%s on: %d points, absorbs %d%%.", name, hp, RW_ArmourAbsorb(variant));
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return RW_Ok(playerid, message);
}
stock RW_ArmourRemove(playerid)
{
    if (!gInvLoaded[playerid] || !gInvWorn[playerid]) return 1;
    new dest = -1;
    for (new i; i < INV_MAIN; i++) if (!gInv[playerid][i][iiUid]) { dest = i; break; }
    if (dest < 0) return RW_Error(playerid, "Free a MAIN slot before taking the armour off.");
    if (RW_InvWeight(playerid, 0) + RW_InvGrams(ITEM_ARMOUR, gArmourData[playerid]) > INV_MAIN_GRAMS) return RW_Error(playerid, "No room for the armour in MAIN.");
    new uid = gInvWorn[playerid], variant = gArmourData[playerid], query[300];
    if (!RW_DB_Exec("SAVEPOINT unwear_armour")) return RW_Error(playerid, "Database unavailable.");
    format(query, sizeof query, "UPDATE inventory_items SET owner=%d,slot=%d WHERE uid=%d AND owner IS NULL AND slot=-1 AND EXISTS(SELECT 1 FROM inventory_wear WHERE owner=%d AND uid=%d)", gAccountId[playerid], dest, uid, gAccountId[playerid], uid);
    new bool:ok = RW_InvWrite(query);
    if (ok) { format(query, sizeof query, "DELETE FROM inventory_wear WHERE owner=%d AND uid=%d", gAccountId[playerid], uid); ok = RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE unwear_armour")) { RW_DB_Exec("ROLLBACK TO unwear_armour"); RW_DB_Exec("RELEASE unwear_armour"); return RW_Error(playerid, "Could not take the armour off."); }
    gInv[playerid][dest][iiUid] = uid; gInv[playerid][dest][iiType] = ITEM_ARMOUR; gInv[playerid][dest][iiData] = variant; gInv[playerid][dest][iiAmount] = 1;
    gInvWorn[playerid] = 0; gArmourData[playerid] = 0; gArmourHp[playerid] = 0;
    gInvSelected[playerid] = 0; gInvContext[playerid] = false;
    RW_ArmourAttach(playerid, true);
    SetPlayerArmour(playerid, 0.0);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return RW_Ok(playerid, "Armour taken off.");
}
// The vest is gone for good when its points run out.
stock RW_ArmourBreak(playerid)
{
    if (!gInvWorn[playerid]) return 1;
    if (!RW_DB_Exec("SAVEPOINT break_armour")) return 0;
    new uid = gInvWorn[playerid], query[220];
    format(query, sizeof query, "DELETE FROM inventory_wear WHERE owner=%d AND uid=%d", gAccountId[playerid], uid);
    new bool:ok = RW_InvWrite(query);
    if (ok) { format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner IS NULL AND slot=-1 AND type=%d", uid, ITEM_ARMOUR); ok = RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE break_armour")) { RW_DB_Exec("ROLLBACK TO break_armour"); RW_DB_Exec("RELEASE break_armour"); return 0; }
    gInvWorn[playerid] = 0; gArmourData[playerid] = 0; gArmourHp[playerid] = 0;
    gInvSelected[playerid] = 0; gInvContext[playerid] = false;
    RW_ArmourAttach(playerid, true);
    SetPlayerArmour(playerid, 0.0);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return RW_Error(playerid, "Your armour was destroyed.");
}
// Called from OnPlayerTakeDamage: takes the armour's share and heals it back,
// which is the only way to soften a hit in SA-MP 0.3.7.
stock RW_ArmourDamage(playerid, Float:amount)
{
    if (!RW_ArmourWorn(playerid) || gArmourHp[playerid] <= 0) return 0;
    if (gDowned[playerid] || amount <= 0.0) return 0;
    new taken = floatround(amount * float(RW_ArmourAbsorb(gArmourData[playerid])) / 100.0);
    if (taken < 1) taken = 1;
    if (taken > gArmourHp[playerid]) taken = gArmourHp[playerid];
    gArmourHp[playerid] -= taken;
    new Float:hp; GetPlayerHealth(playerid, hp);
    hp += float(taken);
    if (hp > 100.0) hp = 100.0;
    SetPlayerHealth(playerid, hp);
    SetPlayerArmour(playerid, float(gArmourHp[playerid]));
    RW_ArmourSaveHp(playerid);
    if (gArmourHp[playerid] <= 0) return RW_ArmourBreak(playerid);
    return taken;
}
stock RW_ArmourLoad(playerid)
{
    gInvWorn[playerid] = 0; gArmourData[playerid] = 0; gArmourHp[playerid] = 0;
    if (gAccountId[playerid] <= 0) return 0;
    new query[280];
    format(query, sizeof query, "SELECT w.uid,w.hp,i.type,i.data,(i.owner IS NULL) AS unowned FROM inventory_wear w LEFT JOIN inventory_items i ON i.uid=w.uid WHERE w.owner=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new count = db_num_rows(r), valid = 1;
    if (count == 1) {
        new uid = db_get_field_assoc_int(r, "uid"), type = db_get_field_assoc_int(r, "type");
        new data = db_get_field_assoc_int(r, "data"), hp = db_get_field_assoc_int(r, "hp");
        if (uid <= 0 || type != ITEM_ARMOUR || !RW_ArmourVariant(data) || !db_get_field_assoc_int(r, "unowned") || hp < 0) valid = 0;
        else {
            new maximum = RW_ArmourHpOf(data);
            gInvWorn[playerid] = uid; gArmourData[playerid] = data; gArmourHp[playerid] = hp > maximum ? maximum : hp;
        }
    } else if (count > 1) valid = 0;
    db_free_result(r);
    if (!valid) { gInvWorn[playerid] = 0; gArmourData[playerid] = 0; gArmourHp[playerid] = 0; }
    return valid;
}
stock RW_ArmourSpawn(playerid)
{
    if (!RW_ArmourWorn(playerid)) return 1;
    RW_ArmourAttach(playerid, true);
    SetPlayerArmour(playerid, float(gArmourHp[playerid]));
    return 1;
}
stock RW_ArmourReset(playerid)
{
    RW_ArmourObjectClear(playerid);
    if (IsPlayerConnected(playerid) && IsPlayerAttachedObjectSlotUsed(playerid, ARMOUR_ATTACH)) RemovePlayerAttachedObject(playerid, ARMOUR_ATTACH);
    gInvWorn[playerid] = 0; gArmourData[playerid] = 0; gArmourHp[playerid] = 0;
    gArmourFitting[playerid] = false;
    return 1;
}
// /armfit: admin tool, saves the vest position for the current skin.
stock RW_ArmourFitCommand(playerid)
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1 || !RW_ArmourWorn(playerid)) return RW_Error(playerid, "Admin with worn armour required.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Stand on foot to adjust your armour.");
    RW_UI_Close(playerid);
    gArmourFitSkin[playerid] = GetPlayerSkin(playerid); gArmourFitUid[playerid] = gInvWorn[playerid];
    gArmourFitting[playerid] = true;
    RW_ArmourAttach(playerid, true);   /* fit the attached object, not the server one */
    EditAttachedObject(playerid, ARMOUR_ATTACH);
    return RW_Info(playerid, "Adjust the armour for your skin, then save. Cancel keeps the old fit.");
}
stock RW_ArmourFitSave(playerid, response, index, model, bone, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, Float:sx, Float:sy, Float:sz)
{
    #pragma unused model, bone, sx, sy, sz
    if (!gArmourFitting[playerid]) return 0;
    gArmourFitting[playerid] = false;
    if (response != EDIT_RESPONSE_FINAL || !gLogged[playerid] || gAdminLevel[playerid] < 1 || gInvWorn[playerid] != gArmourFitUid[playerid]) return RW_ArmourAttach(playerid, true);
    if (index != ARMOUR_ATTACH || gArmourFitSkin[playerid] != GetPlayerSkin(playerid)) return RW_ArmourAttach(playerid, true);
    new query[300];
    format(query, sizeof query, "REPLACE INTO armour_fit(owner,skin,x,y,z,rx,ry,rz) VALUES(%d,%d,%f,%f,%f,%f,%f,%f)", gAccountId[playerid], gArmourFitSkin[playerid], x, y, z, rx, ry, rz);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not save the fit.");
    RW_ArmourAttach(playerid, true);
    return RW_Ok(playerid, "Armour fit saved for this skin.");
}
