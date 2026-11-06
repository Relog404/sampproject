/*  ============================================================================
 *  railway/skins.inc
 *  Clothing textures that live on the player's computer only.
 *
 *  HOW IT WORKS
 *  Every texture file stays on the client, in the game folder arm/
 *  (arm/war1.txd, arm/vest.txd, ...) and is loaded by the player's mod
 *  loader. The server never sends a single byte of art: it only says
 *  "this item is drawn with texture <name> from TXD <name>", and the client
 *  takes that texture from its own arm/ folder.
 *
 *  TO ADD A TEXTURE
 *    1) drop the .txd into the player's arm/ folder (nothing to install on
 *       the server, nothing to upload);
 *    2) either add a line to the switches below and recompile, or set the
 *       name in game once: /skin 1 mytxd mytexture (admin, saved in the
 *       database - no recompile, works for every item of that id).
 *  Empty TXD or texture = the shape keeps the skin the client has for it.
 *
 *  Note: a plain folder in the game root is not read by GTA itself - the
 *  player needs a mod loader (or the same TXD merged into gta3.img).
 *  ========================================================================= */

#if defined _railway_skins_included
    #endinput
#endif
#define _railway_skins_included

#include <railway/core>

/*  --------------------- COMPILED DEFAULTS ---------------------------------
 *   id | what it is        | shape (model) | TXD in arm/   | texture in it
 *    0 | body armour       | 1242          | war1          | armour
 *    1 | another vest      | 1242          | war1          | vest2
 *    2 | free slot         | 1242          | (empty)       | (empty)
 *  These can be overridden in game with /skin - the database wins.
 *  ------------------------------------------------------------------------ */
stock RW_SkinCount() { return 3; }
stock RW_SkinModel(id)
{
    switch (id) {
        case 0: return 1242;      /* GTA SA body armour shape              */
        case 1: return 1242;
        case 2: return 1242;
    }
    return 1242;
}
stock RW_SkinTxdCfg(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "war1");
        case 1: format(dest, size, "war1");
        case 2: format(dest, size, "");
    }
    return 1;
}
stock RW_SkinTextureCfg(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "armour");
        case 1: format(dest, size, "vest2");
        case 2: format(dest, size, "");
    }
    return 1;
}
stock RW_SkinLabel(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "BODY ARMOUR");
        case 1: format(dest, size, "VEST");
        case 2: format(dest, size, "ARMOUR");
        default: format(dest, size, "ARMOUR");
    }
    return 1;
}
/*  --------------------- EFFECTIVE NAMES ----------------------------------- */
stock RW_SkinSchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS skins (id INTEGER PRIMARY KEY, txd TEXT NOT NULL, texture TEXT NOT NULL)") ? 1 : 0;
}
// Compiled defaults first, then whatever the admin saved in the database.
stock RW_SkinLoadAll()
{
    for (new id; id < SKIN_MAX; id++) {
        RW_SkinTxdCfg(id, gSkinTxd[id], sizeof gSkinTxd[]);
        RW_SkinTextureCfg(id, gSkinTexture[id], sizeof gSkinTexture[]);
    }
    new DBResult:r = db_query(gDB, "SELECT id,txd,texture FROM skins");
    if (r == DBResult:0) return 1;
    new count = db_num_rows(r);
    for (new i; i < count; i++) {
        new id = db_get_field_assoc_int(r, "id");
        if (id >= 0 && id < SKIN_MAX) {
            db_get_field_assoc(r, "txd", gSkinTxd[id], sizeof gSkinTxd[]);
            db_get_field_assoc(r, "texture", gSkinTexture[id], sizeof gSkinTexture[]);
        }
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
stock RW_SkinTxd(id, dest[], size = sizeof dest)
{
    if (!RW_SkinValid(id)) return format(dest, size, "");
    return format(dest, size, "%s", gSkinTxd[id]);
}
stock RW_SkinTexture(id, dest[], size = sizeof dest)
{
    if (!RW_SkinValid(id)) return format(dest, size, "");
    return format(dest, size, "%s", gSkinTexture[id]);
}
stock bool:RW_SkinValid(id) { return id >= 0 && id < RW_SkinCount() && id < SKIN_MAX; }
// True when the entry really points at a texture from the client's arm/ folder.
stock bool:RW_SkinTextured(id)
{
    if (!RW_SkinValid(id)) return false;
    return gSkinTxd[id][0] && gSkinTexture[id][0];
}
// Which client-side texture an item uses. One line per clothing type.
stock RW_ItemSkin(type, data)
{
    if (type == ITEM_ARMOUR) return data;
    return -1;
}
// Puts the texture from the client's arm/ folder onto any server object:
// worn vests, dropped items, placed ones.
stock RW_SkinApply(objectid, id)
{
    if (objectid <= 0 || !RW_SkinTextured(id)) return 0;
    SetObjectMaterial(objectid, 0, RW_SkinModel(id), gSkinTxd[id], gSkinTexture[id], 0);
    return 1;
}
// Re-textures everything that already uses this id: worn vests and ground items.
stock RW_SkinRefresh(id)
{
    for (new p; p < MAX_PLAYERS; p++)
        if (IsPlayerConnected(p) && gInvWorn[p] > 0 && gArmourData[p] == id) RW_ArmourAttach(p, true);
    for (new i; i < INV_WORLD; i++)
        if (gGround[i][igUid] > 0 && RW_ItemSkin(gGround[i][igType], gGround[i][igData]) == id) RW_SkinApply(gGround[i][igObject], id);
    return 1;
}
// Names go into SQL, so only harmless characters are allowed.
stock bool:RW_SkinName(const value[])
{
    if (!value[0] || strlen(value) > 31) return false;
    for (new i; value[i]; i++) {
        new c = value[i];
        if (c >= 'a' && c <= 'z') continue;
        if (c >= 'A' && c <= 'Z') continue;
        if (c >= '0' && c <= '9') continue;
        if (c == '_') continue;
        return false;
    }
    return true;
}
// /skin            - list every texture entry
// /skin id txd tex - set one, saved in the database, applied at once
stock RW_SkinCommand(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    new a[32], b[32], c[32], rest[144], tail[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, tail, sizeof tail);
    if (!a[0]) {
        RW_Info(playerid, "Textures come from the client arm/ folder. /skin <id> <txd> <texture> changes a name.");
        for (new id; id < RW_SkinCount(); id++) {
            new label[32], text[110];
            RW_SkinLabel(id, label, sizeof label);
            if (RW_SkinTextured(id)) format(text, sizeof text, "%d %s: %s.txd -> %s", id, label, gSkinTxd[id], gSkinTexture[id]);
            else format(text, sizeof text, "%d %s: stock model skin", id, label);
            SendClientMessage(playerid, COLOR_GREY, text);
        }
        return 1;
    }
    if (!RW_IsNumeric(a) || !RW_SkinValid(strval(a))) return RW_Error(playerid, "Skin ids: 0 to %d.", RW_SkinCount()-1);
    if (!b[0] || !c[0]) return RW_Info(playerid, "/skin <id> <txd> <texture> - both names, without the .txd extension.");
    if (!RW_SkinName(b) || !RW_SkinName(c)) return RW_Error(playerid, "Names: letters, digits and _ only, no .txd.");
    new id = strval(a), query[220];
    format(query, sizeof query, "REPLACE INTO skins(id,txd,texture) VALUES(%d,'%s','%s')", id, b, c);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not save the texture name.");
    format(gSkinTxd[id], 32, "%s", b);
    format(gSkinTexture[id], 32, "%s", c);
    RW_SkinRefresh(id);
    new message[128];
    format(message, sizeof message, "Skin %d now uses %s.txd -> %s from the client arm/ folder.", id, b, c);
    return RW_Ok(playerid, message);
}
