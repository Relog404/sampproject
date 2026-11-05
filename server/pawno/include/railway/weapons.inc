/*  ============================================================================
 *  railway/weapons.inc
 *  Weapons and ammunition live in the inventory, never in the HUD weapon slots.
 *  A weapon must be placed into the ACTIVE slot to be held; rounds are kept in
 *  the weapon itself and come from calibre-specific ammunition stacks.
 *  ========================================================================= */

#if defined _railway_weapons_included
    #endinput
#endif
#define _railway_weapons_included

#include <railway/core>

stock bool:RW_WeaponValid(id) { return id >= 1 && id <= 46; }
stock bool:RW_CaliberValid(cal) { return cal >= 1 && cal <= CAL_COUNT; }

// Calibre of a weapon; 0 means the weapon needs no ammunition.
stock RW_WpCaliber(weaponid)
{
    switch (weaponid) {
        case 22, 23, 28, 29, 32: return CAL_9MM;
        case 24: return CAL_44;
        case 25, 26, 27: return CAL_12GA;
        case 31: return CAL_556;
        case 30, 33: return CAL_762;
        case 34: return CAL_308;
    }
    return 0;
}
// Rounds a weapon holds when fully loaded.
stock RW_WpMag(weaponid)
{
    switch (weaponid) {
        case 22, 23: return 17;
        case 24: return 7;
        case 25, 26: return 8;
        case 27: return 10;
        case 28, 29, 32: return 30;
        case 30: return 30;
        case 31: return 50;
        case 33: return 5;
        case 34: return 10;
    }
    return 0;
}
stock RW_CaliberMax(cal)
{
    switch (cal) {
        case CAL_9MM: return 30;
        case CAL_44: return 20;
        case CAL_12GA: return 15;
        case CAL_556: return 60;
        case CAL_762: return 30;
        case CAL_308: return 15;
    }
    return 0;
}
stock RW_CaliberGrams(cal)   /* grams per round */
{
    switch (cal) {
        case CAL_9MM: return 12;
        case CAL_44: return 25;
        case CAL_12GA: return 40;
        case CAL_556: return 16;
        case CAL_762: return 20;
        case CAL_308: return 30;
    }
    return 10;
}
stock RW_WeaponGrams(weaponid)
{
    switch (weaponid) {
        case 1, 2, 3, 4, 5, 6, 7, 8: return 2000;
        case 9: return 5000;
        case 16, 17, 18: return 600;
        case 22, 23: return 1500;
        case 24: return 2200;
        case 25, 26, 27: return 3400;
        case 28, 29, 32: return 3200;
        case 30, 31: return 4300;
        case 33: return 3900;
        case 34: return 6000;
        case 35, 36: return 12000;
        case 37, 38: return 15000;
        case 41, 42, 43: return 800;
    }
    return 2500;
}
stock RW_WeaponModel(weaponid)
{
    switch (weaponid) {
        case 1: return 331;  case 2: return 333;  case 3: return 334;  case 4: return 335;
        case 5: return 336;  case 6: return 337;  case 7: return 338;  case 8: return 339;
        case 9: return 341;  case 10: return 321; case 11: return 322; case 12: return 323;
        case 13: return 324; case 14: return 325; case 15: return 326; case 16: return 342;
        case 17: return 343; case 18: return 344; case 22: return 346; case 23: return 347;
        case 24: return 348; case 25: return 349; case 26: return 350; case 27: return 351;
        case 28: return 352; case 29: return 353; case 30: return 355; case 31: return 356;
        case 32: return 357; case 33: return 358; case 34: return 359; case 35: return 360;
        case 36: return 361; case 37: return 362; case 38: return 363; case 39: return 364;
        case 40: return 365; case 41: return 366; case 42: return 367; case 43: return 368;
        case 44: return 369; case 45: return 370; case 46: return 371;
    }
    return 1212;
}
stock RW_AmmoModel(cal)
{
    switch (cal) {
        case CAL_12GA: return 2037;
        case CAL_9MM, CAL_44: return 2041;
    }
    return 2358;
}
stock RW_WeaponName(weaponid, dest[], size = sizeof dest)
{
    switch (weaponid) {
        case 1: format(dest, size, "Brass knuckles");
        case 2: format(dest, size, "Golf club");
        case 3: format(dest, size, "Nightstick");
        case 4: format(dest, size, "Knife");
        case 5: format(dest, size, "Baseball bat");
        case 6: format(dest, size, "Shovel");
        case 7: format(dest, size, "Pool cue");
        case 8: format(dest, size, "Katana");
        case 9: format(dest, size, "Chainsaw");
        case 16: format(dest, size, "Grenade");
        case 17: format(dest, size, "Tear gas");
        case 18: format(dest, size, "Molotov");
        case 22: format(dest, size, "Colt 45");
        case 23: format(dest, size, "Silenced 9mm");
        case 24: format(dest, size, "Desert Eagle");
        case 25: format(dest, size, "Shotgun");
        case 26: format(dest, size, "Sawnoff");
        case 27: format(dest, size, "Combat shotgun");
        case 28: format(dest, size, "Uzi");
        case 29: format(dest, size, "MP5");
        case 30: format(dest, size, "AK-47");
        case 31: format(dest, size, "M4");
        case 32: format(dest, size, "Tec-9");
        case 33: format(dest, size, "Rifle");
        case 34: format(dest, size, "Sniper rifle");
        case 35: format(dest, size, "RPG");
        case 36: format(dest, size, "Heat seeker");
        case 37: format(dest, size, "Flame thrower");
        case 38: format(dest, size, "Minigun");
        case 41: format(dest, size, "Spray can");
        case 42: format(dest, size, "Fire extinguisher");
        case 43: format(dest, size, "Camera");
        default: format(dest, size, "Weapon %d", weaponid);
    }
    return 1;
}
stock RW_CaliberName(cal, dest[], size = sizeof dest)
{
    switch (cal) {
        case CAL_9MM: format(dest, size, "9mm");
        case CAL_44: format(dest, size, ".44");
        case CAL_12GA: format(dest, size, "12ga");
        case CAL_556: format(dest, size, "5.56");
        case CAL_762: format(dest, size, "7.62");
        case CAL_308: format(dest, size, ".308");
        default: format(dest, size, "ammo");
    }
    return 1;
}
// The worn weapon is carried, so its weight counts toward the main limit.
stock RW_WpCarriedGrams(playerid)
{
    if (!gWpActive[playerid] || !RW_WeaponValid(gWpWeapon[playerid])) return 0;
    return RW_WeaponGrams(gWpWeapon[playerid]);
}

stock bool:RW_WeaponsSchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS inventory_active (owner INTEGER PRIMARY KEY, uid INTEGER NOT NULL UNIQUE)");
}

// Weapons, ammunition and medical items need more item types and a round
// counter. The old table restricts types with CHECK(type IN (1,2)), so it is
// rebuilt row by row; the previous copy is dropped only after every uid, type,
// data, slot and owner is verified to match. Any failure leaves the old table
// untouched and simply disables the new features.
stock bool:RW_InvUpgradeSchema()
{
    new DBResult:r = db_query(gDB, "SELECT sql FROM sqlite_master WHERE type='table' AND name='inventory_items'");
    if (r == DBResult:0) return false;
    new def[1400]; def[0] = EOS;
    if (db_num_rows(r) > 0) db_get_field_assoc(r, "sql", def, sizeof def);
    db_free_result(r);
    if (!def[0]) return false;
    if (strfind(def, "amount", true) >= 0 && strfind(def, "CHECK(type IN", true) < 0) return true;
    print("[inventory] Upgrading item table for weapons, ammunition and medical items; data preserved.");
    new query[1600];
    if (!RW_DB_Exec("SAVEPOINT inventory_upgrade")) return false;
    if (!RW_DB_Exec("ALTER TABLE inventory_items RENAME TO inventory_items_prev")) { RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade"); return false; }
    format(query, sizeof query, "CREATE TABLE inventory_items (uid INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER, slot INTEGER NOT NULL, type INTEGER NOT NULL, data INTEGER NOT NULL DEFAULT 0, amount INTEGER NOT NULL DEFAULT 1, x REAL NOT NULL DEFAULT 0, y REAL NOT NULL DEFAULT 0, z REAL NOT NULL DEFAULT 0, UNIQUE(owner,slot), CHECK((owner IS NULL AND slot=-1) OR (owner>0 AND slot>=0 AND slot<80)))");
    if (!RW_DB_Exec(query)) { RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade"); return false; }
    format(query, sizeof query, "INSERT INTO inventory_items(uid,owner,slot,type,data,amount,x,y,z) SELECT uid,owner,slot,type,data,1,x,y,z FROM inventory_items_prev ORDER BY uid");
    if (!RW_DB_Exec(query)) { RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade"); return false; }
    format(query, sizeof query, "SELECT (SELECT COUNT(*) FROM inventory_items_prev) AS old_count, (SELECT COUNT(*) FROM inventory_items) AS new_count, (SELECT COUNT(*) FROM inventory_items_prev p LEFT JOIN inventory_items n ON n.uid=p.uid WHERE n.uid IS NULL OR n.type<>p.type OR n.data<>p.data OR n.slot<>p.slot OR IFNULL(n.owner,-9)<>IFNULL(p.owner,-9)) AS lost");
    r = db_query(gDB, query);
    if (r == DBResult:0) { RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade"); return false; }
    new old_count, new_count, lost;
    if (db_num_rows(r) == 1) {
        old_count = db_get_field_assoc_int(r, "old_count");
        new_count = db_get_field_assoc_int(r, "new_count");
        lost = db_get_field_assoc_int(r, "lost");
    } else lost = 1;
    db_free_result(r);
    if (lost != 0 || old_count != new_count) {
        printf("[inventory] Upgrade verification failed (old=%d new=%d lost=%d); old table restored.", old_count, new_count, lost);
        RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade");
        return false;
    }
    if (!RW_DB_Exec("DROP TABLE inventory_items_prev") || !RW_DB_Exec("RELEASE inventory_upgrade")) {
        RW_DB_Exec("ROLLBACK TO inventory_upgrade"); RW_DB_Exec("RELEASE inventory_upgrade");
        return false;
    }
    printf("[inventory] Item table upgraded; %d rows preserved.", new_count);
    return true;
}

// Restores which weapon the account was holding. One active weapon per account.
stock bool:RW_WpLoadState(playerid)
{
    gWpActive[playerid] = 0; gWpWeapon[playerid] = 0; gWpAmmo[playerid] = 0; gWpDirty[playerid] = false;
    new query[220]; format(query, sizeof query, "SELECT i.uid,i.data,i.amount FROM inventory_active a JOIN inventory_items i ON i.uid=a.uid WHERE a.owner=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return false;
    new count = db_num_rows(r);
    if (count == 1) {
        gWpActive[playerid] = db_get_field_assoc_int(r, "uid");
        gWpWeapon[playerid] = db_get_field_assoc_int(r, "data");
        gWpAmmo[playerid] = db_get_field_assoc_int(r, "amount");
    }
    db_free_result(r);
    if (count > 1) return false;
    if (gWpActive[playerid] <= 0) return true;
    if (!RW_WeaponValid(gWpWeapon[playerid]) || gWpAmmo[playerid] < 0 || gWpAmmo[playerid] > RW_WpMag(gWpWeapon[playerid])) return false;
    return true;
}
// Hands reflect the active slot only; nothing is kept in the HUD weapon slots.
stock RW_WpSync(playerid)
{
    ResetPlayerWeapons(playerid);
    if (gWpActive[playerid] > 0 && RW_WeaponValid(gWpWeapon[playerid]) && gWpAmmo[playerid] > 0) {
        GivePlayerWeapon(playerid, gWpWeapon[playerid], gWpAmmo[playerid]);
        SetPlayerArmedWeapon(playerid, gWpWeapon[playerid]);
    } else if (gWpActive[playerid] > 0 && RW_WeaponValid(gWpWeapon[playerid])) {
        GivePlayerWeapon(playerid, gWpWeapon[playerid], 0);
    }
    return 1;
}
stock RW_WpFlush(playerid)
{
    if (!gWpDirty[playerid] || gWpActive[playerid] <= 0 || !gLogged[playerid]) return 0;
    new query[220]; format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d AND owner IS NULL AND slot=-1", gWpAmmo[playerid], gWpActive[playerid]);
    if (RW_InvWrite(query)) gWpDirty[playerid] = false;
    return 1;
}
stock RW_WpReset(playerid)
{
    gWpActive[playerid] = 0; gWpWeapon[playerid] = 0; gWpAmmo[playerid] = 0; gWpDirty[playerid] = false;
    return 1;
}
stock RW_WpEquip(playerid, uid)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return 0;
    new slot = RW_InvFind(playerid, uid);
    if (slot < 0) return 0;
    if (gInv[playerid][slot][iiType] != ITEM_WEAPON) return RW_Error(playerid, "Only a weapon goes into the active slot.");
    if (gWpActive[playerid] > 0) return RW_Error(playerid, "Unequip the current weapon first.");
    new weapon = gInv[playerid][slot][iiData], rounds = gInv[playerid][slot][iiAmount];
    if (!RW_WeaponValid(weapon)) return RW_Error(playerid, "Unknown weapon.");
    if (rounds < 0 || rounds > RW_WpMag(weapon)) return RW_Error(playerid, "This weapon has a damaged round counter.");
    if (GetPlayerState(playerid) == PLAYER_STATE_WASTED || GetPlayerState(playerid) == PLAYER_STATE_SPECTATING) return RW_Error(playerid, "You cannot equip a weapon now.");
    new query[320];
    if (!RW_DB_Exec("SAVEPOINT weapon_equip")) return RW_Error(playerid, "Database unavailable.");
    format(query, sizeof query, "UPDATE inventory_items SET owner=NULL,slot=-1 WHERE uid=%d AND owner=%d AND slot=%d", uid, gAccountId[playerid], slot);
    new bool:ok = RW_InvWrite(query);
    if (ok) { format(query, sizeof query, "INSERT INTO inventory_active(owner,uid) VALUES(%d,%d)", gAccountId[playerid], uid); ok = RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE weapon_equip")) {
        RW_DB_Exec("ROLLBACK TO weapon_equip"); RW_DB_Exec("RELEASE weapon_equip");
        return RW_Error(playerid, "Could not equip the weapon. Nothing changed.");
    }
    gWpActive[playerid] = uid; gWpWeapon[playerid] = weapon; gWpAmmo[playerid] = rounds; gWpDirty[playerid] = false;
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_WpSync(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    new name[32], message[110]; RW_WeaponName(weapon, name, sizeof name);
    if (rounds > 0) format(message, sizeof message, "%s equipped with %d rounds.", name, rounds);
    else if (RW_WpCaliber(weapon)) {
        new cbuf[16]; RW_CaliberName(RW_WpCaliber(weapon), cbuf, sizeof cbuf);
        format(message, sizeof message, "%s equipped, empty. Load %s ammunition.", name, cbuf);
    }
    else format(message, sizeof message, "%s equipped.", name);
    return RW_Ok(playerid, message);
}
stock RW_WpUnequip(playerid)
{
    if (gWpActive[playerid] <= 0) return RW_Error(playerid, "No weapon is equipped.");
    new dest = RW_InvFree(playerid, 0, ITEM_WEAPON, gWpWeapon[playerid], 1);
    if (dest < 0) return RW_Error(playerid, "No free MAIN slot or weight limit reached.");
    new uid = gWpActive[playerid], weapon = gWpWeapon[playerid], rounds = gWpAmmo[playerid], query[320];
    if (!RW_DB_Exec("SAVEPOINT weapon_unequip")) return RW_Error(playerid, "Database unavailable.");
    format(query, sizeof query, "UPDATE inventory_items SET owner=%d,slot=%d,amount=%d WHERE uid=%d AND owner IS NULL AND slot=-1", gAccountId[playerid], dest, rounds, uid);
    new bool:ok = RW_InvWrite(query);
    if (ok) { format(query, sizeof query, "DELETE FROM inventory_active WHERE owner=%d AND uid=%d", gAccountId[playerid], uid); ok = RW_InvWrite(query); }
    if (!ok || !RW_DB_Exec("RELEASE weapon_unequip")) {
        RW_DB_Exec("ROLLBACK TO weapon_unequip"); RW_DB_Exec("RELEASE weapon_unequip");
        return RW_Error(playerid, "Could not unequip the weapon. Nothing changed.");
    }
    gInv[playerid][dest][iiUid] = uid; gInv[playerid][dest][iiType] = ITEM_WEAPON; gInv[playerid][dest][iiData] = weapon; gInv[playerid][dest][iiAmount] = rounds;
    gWpActive[playerid] = 0; gWpWeapon[playerid] = 0; gWpAmmo[playerid] = 0; gWpDirty[playerid] = false;
    RW_WpSync(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return RW_Ok(playerid, "Weapon returned to the inventory.");
}
// Moves rounds from ammunition stacks into the equipped weapon.
stock RW_WpReload(playerid)
{
    if (gWpActive[playerid] <= 0) return RW_Error(playerid, "Equip a weapon first.");
    new caliber = RW_WpCaliber(gWpWeapon[playerid]);
    if (!caliber) return RW_Error(playerid, "This weapon needs no ammunition.");
    new mag = RW_WpMag(gWpWeapon[playerid]);
    if (gWpAmmo[playerid] >= mag) return RW_Info(playerid, "The magazine is already full.");
    new need = mag - gWpAmmo[playerid], taken;
    for (new i; i < INV_SLOTS && taken < need; i++) {
        if (gInv[playerid][i][iiType] != ITEM_AMMO || gInv[playerid][i][iiData] != caliber || gInv[playerid][i][iiAmount] <= 0) continue;
        new take = need - taken;
        if (take > gInv[playerid][i][iiAmount]) take = gInv[playerid][i][iiAmount];
        new query[220];
        if (take >= gInv[playerid][i][iiAmount]) {
            format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d AND type=%d", gInv[playerid][i][iiUid], gAccountId[playerid], i, ITEM_AMMO);
            if (!RW_InvWrite(query)) continue;
            gInv[playerid][i][iiUid] = 0; gInv[playerid][i][iiType] = 0; gInv[playerid][i][iiData] = 0; gInv[playerid][i][iiAmount] = 0;
        } else {
            format(query, sizeof query, "UPDATE inventory_items SET amount=amount-%d WHERE uid=%d AND owner=%d AND slot=%d AND type=%d AND amount>=%d", take, gInv[playerid][i][iiUid], gAccountId[playerid], i, ITEM_AMMO, take);
            if (!RW_InvWrite(query)) continue;
            gInv[playerid][i][iiAmount] -= take;
        }
        taken += take;
    }
    if (!taken) {
        new cname[16], message[110]; RW_CaliberName(caliber, cname, sizeof cname);
        format(message, sizeof message, "No %s ammunition in the inventory.", cname);
        return RW_Error(playerid, message);
    }
    gWpAmmo[playerid] += taken; gWpDirty[playerid] = true;
    RW_WpFlush(playerid);
    RW_WpSync(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    new cname[16], message[110]; RW_CaliberName(caliber, cname, sizeof cname);
    format(message, sizeof message, "Loaded %d %s rounds (%d / %d).", taken, cname, gWpAmmo[playerid], mag);
    return RW_Ok(playerid, message);
}
// Context action: load a specific stack into the equipped weapon.
stock RW_WpReloadFrom(playerid, slot)
{
    if (gInv[playerid][slot][iiType] != ITEM_AMMO) return RW_Error(playerid, "Select an ammunition stack.");
    if (gWpActive[playerid] <= 0) return RW_Error(playerid, "Equip a weapon first.");
    new cal = gInv[playerid][slot][iiData];
    if (cal != RW_WpCaliber(gWpWeapon[playerid])) {
        new cname[16], wcal[16], message[128];
        RW_CaliberName(cal, cname, sizeof cname);
        RW_CaliberName(RW_WpCaliber(gWpWeapon[playerid]), wcal, sizeof wcal);
        format(message, sizeof message, "That is %s ammunition; the weapon takes %s.", cname, wcal);
        return RW_Error(playerid, message);
    }
    new mag = RW_WpMag(gWpWeapon[playerid]);
    if (gWpAmmo[playerid] >= mag) return RW_Info(playerid, "The magazine is already full.");
    new take = mag - gWpAmmo[playerid], query[220];
    if (take > gInv[playerid][slot][iiAmount]) take = gInv[playerid][slot][iiAmount];
    if (take >= gInv[playerid][slot][iiAmount]) {
        format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d AND type=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot, ITEM_AMMO);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not update the ammunition.");
        gInv[playerid][slot][iiUid] = 0; gInv[playerid][slot][iiType] = 0; gInv[playerid][slot][iiData] = 0; gInv[playerid][slot][iiAmount] = 0;
    } else {
        format(query, sizeof query, "UPDATE inventory_items SET amount=amount-%d WHERE uid=%d AND owner=%d AND slot=%d AND type=%d AND amount>=%d", take, gInv[playerid][slot][iiUid], gAccountId[playerid], slot, ITEM_AMMO, take);
        if (!RW_InvWrite(query)) return RW_Error(playerid, "Could not update the ammunition.");
        gInv[playerid][slot][iiAmount] -= take;
    }
    gWpAmmo[playerid] += take; gWpDirty[playerid] = true;
    RW_WpFlush(playerid);
    RW_WpSync(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    new cname[16], message[110]; RW_CaliberName(cal, cname, sizeof cname);
    format(message, sizeof message, "Loaded %d %s rounds (%d / %d).", take, cname, gWpAmmo[playerid], mag);
    return RW_Ok(playerid, message);
}
// Every fired round is taken from the inventory, not from the HUD.
stock RW_WpShot(playerid, weaponid)
{
    if (gWpActive[playerid] <= 0 || weaponid != gWpWeapon[playerid]) return 0;
    new caliber = RW_WpCaliber(weaponid);
    if (!caliber) return 1;                     // melee and thrown weapons
    if (gWpAmmo[playerid] <= 0) {
        RW_WpSync(playerid);
        return RW_Error(playerid, "No rounds loaded. Reload with ammunition.");
    }
    gWpAmmo[playerid]--; gWpDirty[playerid] = true;
    SetPlayerAmmo(playerid, weaponid, gWpAmmo[playerid]);
    if (gWpAmmo[playerid] <= 0) {
        RW_WpReload(playerid);
        if (gWpAmmo[playerid] <= 0) RW_WpSync(playerid);
    }
    return 1;
}
