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
 *    2) add one line to the three switches below: the shape (model), the
 *       TXD name WITHOUT ".txd", and the texture name inside that TXD.
 *  That is all: worn items, dropped items and placed items then use it.
 *
 *  Note: a plain folder in the game root is not read by GTA itself - the
 *  player needs a mod loader (or the same TXD merged into gta3.img).
 *  ========================================================================= */

#if defined _railway_skins_included
    #endinput
#endif
#define _railway_skins_included

#include <railway/core>

#define SKIN_MAX (16)

/*  --------------------- YOUR TEXTURES -------------------------------------
 *   id | what it is        | shape (model) | TXD in arm/   | texture in it
 *    0 | body armour       | 1242          | war1          | armour
 *    1 | another vest      | 1242          | war1          | vest2
 *    2 | free slot         | 1242          | (empty)       | (empty)
 *  Empty TXD or texture = the shape keeps the skin the client has for it.
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
stock RW_SkinTxd(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "war1");
        case 1: format(dest, size, "war1");
        case 2: format(dest, size, "");
    }
    return 1;
}
stock RW_SkinTexture(id, dest[], size = sizeof dest)
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
stock bool:RW_SkinValid(id) { return id >= 0 && id < RW_SkinCount() && id < SKIN_MAX; }
// True when the entry really points at a texture from the client's arm/ folder.
stock bool:RW_SkinTextured(id)
{
    if (!RW_SkinValid(id)) return false;
    new txd[32], texture[32];
    RW_SkinTxd(id, txd, sizeof txd);
    RW_SkinTexture(id, texture, sizeof texture);
    return txd[0] && texture[0];
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
    new txd[32], texture[32];
    RW_SkinTxd(id, txd, sizeof txd);
    RW_SkinTexture(id, texture, sizeof texture);
    SetObjectMaterial(objectid, 0, RW_SkinModel(id), txd, texture, 0);
    return 1;
}
