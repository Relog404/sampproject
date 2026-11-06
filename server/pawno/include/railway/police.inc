/*  ============================================================================
 *  railway/police.inc
 *  Полиция (LSPD / Шерифы) и адвокатура.
 *
 *  Предметы: наручники, тазер (2 заряда), дубинка (100 прочности), значок.
 *  Y на игрока — действия полицейского; Y на себя на службе — планшет (MDT).
 *  Арест сажает в КПЗ; освобождение — по сроку, по залогу или через адвоката.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_police_included
    #endinput
#endif
#define _railway_police_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/phone>

#define MAX_CASES           4
#define POLICE_RANGE        3.5        /* дистанция действий Y               */
#define POLICE_STUN_MS      3000
#define TASER_COOLDOWN_MS   6000
#define BATON_DAMAGE        12.0
#define BATON_WEAR          5
#define JAIL_MAX_MINUTES    60
#define MDT_ROWS            6
#define MDT_ROWS_NATIVE     5

/* экраны меню полиции */
#define POLICE_SCR_ACTIONS  0
#define POLICE_SCR_WANTED   1
#define POLICE_SCR_REASON   2
#define POLICE_SCR_ARREST   3

/* вкладки планшета */
#define MDT_CITIZENS        0
#define MDT_VEHICLES        1
#define MDT_ARRESTS         2
#define MDT_FINES           3

new gFaction[MAX_PLAYERS];          /* 0 нет, 1 LSPD, 2 шерифы               */
new gRank[MAX_PLAYERS];
new bool:gDuty[MAX_PLAYERS];
new bool:gCuffed[MAX_PLAYERS];
new gEscort[MAX_PLAYERS];           /* кто ведёт этого игрока (-1 — никто)   */
new gEscorting[MAX_PLAYERS];        /* кого ведёт этот полицейский (-1)      */
new bool:gWalled[MAX_PLAYERS];
new gWantedStars[MAX_PLAYERS];
new gWantedReason[MAX_PLAYERS];
new gJailTime[MAX_PLAYERS];         /* секунд до освобождения                */
new gJailBail[MAX_PLAYERS];
new gJailOfficer[MAX_PLAYERS][MAX_PLAYER_NAME];
new gTrack[MAX_PLAYERS];            /* за кем следит (playerid, -1 — никто)  */
new gTrackTick[MAX_PLAYERS];
new gStun[MAX_PLAYERS];             /* тик окончания оглушения               */
new gTaserTick[MAX_PLAYERS];
new gPoliceTarget[MAX_PLAYERS];     /* игрок, к которому открыто меню        */
new gPoliceScreen[MAX_PLAYERS];
new gPoliceStars[MAX_PLAYERS];
new gPoliceTerm[MAX_PLAYERS];
new gPoliceBail[MAX_PLAYERS];
new gMdtTab[MAX_PLAYERS];
new gMdtPage[MAX_PLAYERS];
new gMdtList[MAX_PLAYERS];
new gMdtIds[MAX_PLAYERS][MDT_ROWS];
new gMdtCount[MAX_PLAYERS];
new gPoliceTick[MAX_PLAYERS];       /* защита от спама                       */
new gLawyerLicense[MAX_PLAYERS];
new gLawyerExpire[MAX_PLAYERS];
new gLawyerPrice[MAX_PLAYERS];
new gLawyerRating[MAX_PLAYERS];
new gCaseId[MAX_PLAYERS];           /* номер дела (0 — вне дела)             */
new gCaseLawyer[MAX_CASES];
new gCasePrisoner[MAX_CASES];
new gCaseOfficer[MAX_CASES];
new gCaseBail[MAX_CASES];

forward RW_PolicePulse();

/* Слоты дел общие на весь сервер, поэтому их надо подготовить один раз. */
stock RW_PoliceInit()
{
    for (new i; i < MAX_CASES; i++) { gCaseLawyer[i] = INVALID_PLAYER_ID; gCasePrisoner[i] = INVALID_PLAYER_ID; gCaseOfficer[i] = INVALID_PLAYER_ID; gCaseBail[i] = 0; }
    return 1;
}
/* Кавычки в нике ломают SQL-запрос: выкидываем их. */
stock RW_PoliceSqlName(const src[], dest[], size = sizeof dest)
{
    new j;
    for (new i; src[i] && j < size - 1; i++) {
        if (src[i] == 39) continue;                 /* апостроф */
        if (src[i] == 37) continue;                 /* знак формата */
        dest[j++] = src[i];
    }
    dest[j] = EOS;
    return 1;
}

/*  --------------------------------- справочники --------------------------- */

stock RW_FactionName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 1: format(dest, size, "LSPD");
        case 2: format(dest, size, "Sheriff");
        default: format(dest, size, "Civilian");
    }
    return 1;
}
// 0 Theft, 1 Assault, 2 Robbery, 3 Car theft, 4 Murder, 5 Other.
stock RW_PoliceReason(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "Theft");
        case 1: format(dest, size, "Assault");
        case 2: format(dest, size, "Robbery");
        case 3: format(dest, size, "Car theft");
        case 4: format(dest, size, "Murder");
        default: format(dest, size, "Other");
    }
    return 1;
}
stock RW_PoliceReasonId(const word[])
{
    if (!strcmp(word, "theft", true)) return 0;
    if (!strcmp(word, "assault", true)) return 1;
    if (!strcmp(word, "robbery", true)) return 2;
    if (!strcmp(word, "car", true)) return 3;
    if (!strcmp(word, "murder", true)) return 4;
    return 5;
}
// Названия предметов для инвентаря.
stock RW_PoliceItemName(type, data, dest[], size = sizeof dest)
{
    #pragma unused data
    switch (type) {
        case ITEM_CUFFS: format(dest, size, "HANDCUFFS");
        case ITEM_TASER: format(dest, size, "TASER");
        case ITEM_BATON: format(dest, size, "BATON");
        case ITEM_BADGE: format(dest, size, "POLICE BADGE");
        default: format(dest, size, "ITEM");
    }
    return 1;
}
stock RW_PoliceBailStep(current)
{
    if (current <= 0) return 500;
    if (current <= 500) return 1500;
    if (current <= 1500) return 5000;
    return 0;
}

/*  ----------------------------------- база -------------------------------- */

stock RW_PoliceSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS officers (player_id INT PRIMARY KEY, faction INT NOT NULL, rank INT NOT NULL DEFAULT 1, duty BOOLEAN)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS wanted (player_id INT PRIMARY KEY, stars INT NOT NULL, reason INT NOT NULL, officer VARCHAR(32), stamp INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS arrests (id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INT, officer VARCHAR(32), minutes INT, bail INT, reason INT, stamp INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS fines (id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INT, officer VARCHAR(32), amount INT, reason INT, paid BOOLEAN, stamp INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS jail (player_id INT PRIMARY KEY, seconds INT, bail INT, reason INT, officer VARCHAR(32))")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS wall_attachments (player_id INT PRIMARY KEY, x REAL, y REAL, z REAL, officer VARCHAR(32), stamp INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS lawyers (player_id INT PRIMARY KEY, license INT, expire INT, price INT, rating INT)")) return 0;
    return 1;
}
stock RW_PoliceReset(playerid)
{
    gFaction[playerid] = 0;
    gRank[playerid] = 0;
    gDuty[playerid] = false;
    gCuffed[playerid] = false;
    gEscort[playerid] = -1;
    gEscorting[playerid] = -1;
    gWalled[playerid] = false;
    gWantedStars[playerid] = 0;
    gWantedReason[playerid] = 0;
    gJailTime[playerid] = 0;
    gJailBail[playerid] = 0;
    gJailOfficer[playerid][0] = EOS;
    gTrack[playerid] = -1;
    gTrackTick[playerid] = 0;
    gStun[playerid] = 0;
    gTaserTick[playerid] = 0;
    gPoliceTarget[playerid] = INVALID_PLAYER_ID;
    gPoliceScreen[playerid] = POLICE_SCR_ACTIONS;
    gPoliceStars[playerid] = 1;
    gPoliceTerm[playerid] = 5;
    gPoliceBail[playerid] = 0;
    gMdtTab[playerid] = MDT_CITIZENS;
    gMdtPage[playerid] = 0;
    gMdtCount[playerid] = 0;
    gPoliceTick[playerid] = 0;
    gLawyerLicense[playerid] = 0;
    gLawyerExpire[playerid] = 0;
    gLawyerPrice[playerid] = 0;
    gLawyerRating[playerid] = 0;
    if (gCaseId[playerid]) RW_PoliceCaseEnd(playerid);
    gCaseId[playerid] = 0;
    return 1;
}
stock RW_PoliceLoad(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[220];
    format(query, sizeof query, "SELECT faction,rank,duty FROM officers WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            gFaction[playerid] = db_get_field_assoc_int(r, "faction");
            gRank[playerid] = db_get_field_assoc_int(r, "rank");
            gDuty[playerid] = db_get_field_assoc_int(r, "duty") ? true : false;
        }
        db_free_result(r);
    }
    format(query, sizeof query, "SELECT stars,reason FROM wanted WHERE player_id=%d", gAccountId[playerid]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            gWantedStars[playerid] = db_get_field_assoc_int(r, "stars");
            gWantedReason[playerid] = db_get_field_assoc_int(r, "reason");
        }
        db_free_result(r);
    }
    format(query, sizeof query, "SELECT seconds,bail,reason,officer FROM jail WHERE player_id=%d", gAccountId[playerid]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            gJailTime[playerid] = db_get_field_assoc_int(r, "seconds");
            gJailBail[playerid] = db_get_field_assoc_int(r, "bail");
            db_get_field_assoc(r, "officer", gJailOfficer[playerid], sizeof gJailOfficer[]);
        }
        db_free_result(r);
    }
    format(query, sizeof query, "SELECT license,expire,price,rating FROM lawyers WHERE player_id=%d", gAccountId[playerid]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            gLawyerLicense[playerid] = db_get_field_assoc_int(r, "license");
            gLawyerExpire[playerid] = db_get_field_assoc_int(r, "expire");
            gLawyerPrice[playerid] = db_get_field_assoc_int(r, "price");
            gLawyerRating[playerid] = db_get_field_assoc_int(r, "rating");
        }
        db_free_result(r);
    }
    if (gLawyerLicense[playerid] && gLawyerExpire[playerid] && gLawyerExpire[playerid] < gettime()) gLawyerLicense[playerid] = 0;
    if (gJailTime[playerid] > 0) RW_PoliceJail(playerid, -1, gJailTime[playerid], gJailBail[playerid], 0, false);
    return 1;
}
// Оставшийся срок сохраняется, чтобы отсидку нельзя было сбросить выходом.
stock RW_PoliceSave(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[220];
    if (gFaction[playerid] > 0) {
        format(query, sizeof query, "INSERT OR REPLACE INTO officers(player_id,faction,rank,duty) VALUES(%d,%d,%d,%d)", gAccountId[playerid], gFaction[playerid], gRank[playerid], gDuty[playerid] ? 1 : 0);
        RW_DB_Exec(query);
    }
    if (gJailTime[playerid] > 0) {
        format(query, sizeof query, "INSERT OR REPLACE INTO jail(player_id,seconds,bail,reason,officer) VALUES(%d,%d,%d,0,'%s')", gAccountId[playerid], gJailTime[playerid], gJailBail[playerid], gJailOfficer[playerid]);
        RW_DB_Exec(query);
    } else {
        format(query, sizeof query, "DELETE FROM jail WHERE player_id=%d", gAccountId[playerid]);
        RW_DB_Exec(query);
    }
    return 1;
}

/*  --------------------------------- помощники ----------------------------- */

stock bool:RW_PoliceOnDuty(playerid)
{
    return gLogged[playerid] && gDuty[playerid] && gFaction[playerid] > 0;
}
/* Общая проверка: служба, значок и, если указан игрок, дистанция. */
stock RW_PoliceCanAct(playerid, target = -1, Float:range = POLICE_RANGE)
{
    if (!RW_PoliceOnDuty(playerid)) return RW_Error(playerid, "You are not on duty.");
    if (RW_PoliceBadge(playerid) <= 0) return RW_Error(playerid, "You need your police badge in the inventory.");
    if (target >= 0 && !RW_PoliceNear(playerid, target, range)) return RW_Error(playerid, "Stand closer to the player.");
    return 1;
}
stock bool:RW_IsLawyer(playerid)
{
    return gLogged[playerid] && gLawyerLicense[playerid] > 0;
}
// Значок хранится в инвентаре: без него меню фракции недоступно.
stock RW_PoliceBadge(playerid)
{
    if (!gLogged[playerid]) return 0;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_BADGE) return gInv[playerid][i][iiUid];
    return 0;
}
stock RW_PoliceItem(playerid, type)
{
    if (!gLogged[playerid]) return -1;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == type) return i;
    return -1;
}
// Тазер и дубинка изнашиваются; пустой предмет исчезает.
stock RW_PoliceWear(playerid, slot, loss, const broken[])
{
    if (slot < 0 || gInv[playerid][slot][iiUid] <= 0) return 0;
    new left = gInv[playerid][slot][iiAmount] - loss;
    new query[220];
    if (left <= 0) {
        format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        if (!RW_InvWrite(query)) return 0;
        for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
        RW_Error(playerid, broken);
    } else {
        format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d AND owner=%d AND slot=%d", left, gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
        if (!RW_InvWrite(query)) return 0;
        gInv[playerid][slot][iiAmount] = left;
    }
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return 1;
}
stock bool:RW_PoliceNear(playerid, target, Float:range = POLICE_RANGE)
{
    if (!IsPlayerConnected(target) || target == playerid) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    return IsPlayerInRangeOfPoint(target, range, x, y, z) ? true : false;
}
stock bool:RW_PoliceAtStation(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, POLICE_STATION_RANGE, POLICE_STATION_X, POLICE_STATION_Y, POLICE_STATION_Z) ? true : false;
}
stock RW_PoliceFind(const name[])
{
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i]) continue;
        new nick[MAX_PLAYER_NAME];
        RW_GetNick(i, nick);
        if (!strcmp(nick, name, true)) return i;
    }
    return -1;
}

/*  ------------------------------ наручники -------------------------------- */

stock RW_PoliceCuff(playerid, target)
{
    if (!RW_PoliceCanAct(playerid, target)) return 0;
    if (RW_PoliceItem(playerid, ITEM_CUFFS) < 0) return RW_Error(playerid, "You need handcuffs in the inventory.");
    if (gCuffed[target]) return RW_Error(playerid, "He is already cuffed.");
    if (gJailTime[target] > 0) return RW_Error(playerid, "He is already in a cell.");
    gCuffed[target] = true;
    TogglePlayerControllable(target, false);
    SetPlayerSpecialAction(target, SPECIAL_ACTION_NONE);
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s cuffed you.", nick);
    RW_Error(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is cuffed. Y on him for more.", nick);
    return RW_Ok(playerid, message);
}
stock RW_PoliceUncuff(playerid, target)
{
    if (!RW_PoliceCanAct(playerid, target)) return 0;
    if (!gCuffed[target]) return RW_Error(playerid, "He is not cuffed.");
    gCuffed[target] = false;
    gWalled[target] = false;
    if (gEscort[target] >= 0) { gEscorting[gEscort[target]] = -1; gEscort[target] = -1; }
    RW_PoliceStopEscort(playerid, true);
    TogglePlayerControllable(target, true);
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s took the cuffs off.", nick);
    RW_Ok(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is free of the cuffs.", nick);
    new query[160];
    format(query, sizeof query, "DELETE FROM wall_attachments WHERE player_id=%d", gAccountId[target]);
    RW_DB_Exec(query);
    return RW_Ok(playerid, message);
}
// Конвоирование: задержанный идёт за офицером, пока тот его ведёт.
stock RW_PoliceEscort(playerid, target)
{
    if (!RW_PoliceCanAct(playerid, target)) return 0;
    if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");
    if (gEscorting[playerid] >= 0) return RW_Error(playerid, "You are already escorting somebody.");
    gEscort[target] = playerid;
    gEscorting[playerid] = target;
    gWalled[target] = false;
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s is escorting you.", nick);
    RW_Info(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "You are escorting %s.", nick);
    return RW_Ok(playerid, message);
}
stock RW_PoliceStopEscort(playerid, bool:silent = false)
{
    new target = gEscorting[playerid];
    if (target < 0 && !silent) return RW_Error(playerid, "You are not escorting anybody.");
    if (target >= 0) gEscort[target] = -1;
    gEscorting[playerid] = -1;
    if (silent) return 1;
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s stopped escorting you.", nick);
    if (target >= 0 && IsPlayerConnected(target)) RW_Info(target, message);
    format(message, sizeof message, "%s", "Escort stopped.");
    return RW_Ok(playerid, message);
}
// К стенке: задержанный остаётся на месте до решения офицера.
stock RW_PoliceWall(playerid, target)
{
    if (!RW_PoliceCanAct(playerid, target)) return 0;
    if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");
    if (gWalled[target]) return RW_Error(playerid, "He is already attached.");
    gWalled[target] = true;
    gEscort[target] = -1;
    if (gEscorting[playerid] == target) gEscorting[playerid] = -1;
    TogglePlayerControllable(target, false);
    new Float:x, Float:y, Float:z, query[256], nick[MAX_PLAYER_NAME];
    GetPlayerPos(target, x, y, z);
    RW_GetNick(playerid, nick);
    new safe[MAX_PLAYER_NAME];
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT OR REPLACE INTO wall_attachments(player_id,x,y,z,officer,stamp) VALUES(%d,%f,%f,%f,'%s',%d)", gAccountId[target], x, y, z, safe, gettime());
    RW_DB_Exec(query);
    new message[128];
    format(message, sizeof message, "%s attached you to the surroundings.", nick);
    RW_Error(target, message);
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is attached. Y on him to release.", nick);
    return RW_Ok(playerid, message);
}
stock RW_PoliceUnwall(playerid, target)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    if (!gWalled[target]) return RW_Error(playerid, "He is not attached.");
    gWalled[target] = false;
    if (!gCuffed[target]) TogglePlayerControllable(target, true);
    new query[160];
    format(query, sizeof query, "DELETE FROM wall_attachments WHERE player_id=%d", gAccountId[target]);
    RW_DB_Exec(query);
    RW_Info(target, "You were released from the surroundings.");
    return RW_Ok(playerid, "Released.");
}

/*  --------------------------- нелетальное оружие -------------------------- */

stock RW_PoliceStun(playerid, target)
{
    if (!RW_PoliceCanAct(playerid, target, 4.0)) return 0;
    if (gCuffed[target] || gJailTime[target] > 0) return RW_Error(playerid, "He cannot resist right now.");
    new slot = RW_PoliceItem(playerid, ITEM_TASER);
    if (slot >= 0) {
        if (GetTickCount() - gTaserTick[playerid] < TASER_COOLDOWN_MS) return RW_Error(playerid, "The taser is recharging.");
        gTaserTick[playerid] = GetTickCount();
        gStun[target] = GetTickCount() + POLICE_STUN_MS;
        TogglePlayerControllable(target, false);
        ApplyAnimation(target, "PED", "KO_shot_front", 4.1, 0, 0, 0, 1, 0);
        RW_PoliceWear(playerid, slot, 1, "The taser is out of charge and broke.");
        RW_Info(target, "Tased! You cannot move for three seconds.");
        return RW_Ok(playerid, "Taser fired. One charge spent; cuff him while he is down.");
    }
    slot = RW_PoliceItem(playerid, ITEM_BATON);
    if (slot >= 0) {
        new Float:hp;
        GetPlayerHealth(target, hp);
        SetPlayerHealth(target, hp > BATON_DAMAGE ? hp - BATON_DAMAGE : 5.0);
        gStun[target] = GetTickCount() + POLICE_STUN_MS / 2;
        TogglePlayerControllable(target, false);
        RW_PoliceWear(playerid, slot, BATON_WEAR, "The baton broke.");
        RW_Info(target, "Baton hit! You are stunned.");
        return RW_Ok(playerid, "Baton hit.");
    }
    return RW_Error(playerid, "Take a taser or a baton in the inventory.");
}

/*  --------------------------------- розыск -------------------------------- */

stock RW_PoliceWanted(playerid, target, stars, reason)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    if (stars < 0 || stars > 5) return RW_Error(playerid, "Stars: 0 (clear) to 5.");
    if (reason < 0 || reason > 5) reason = 5;
    new query[256], nick[MAX_PLAYER_NAME], why[24];
    RW_GetNick(playerid, nick);
    RW_PoliceReason(reason, why, sizeof why);
    if (stars == 0) {
        gWantedStars[target] = 0;
        gWantedReason[target] = 0;
        format(query, sizeof query, "DELETE FROM wanted WHERE player_id=%d", gAccountId[target]);
        RW_DB_Exec(query);
        RW_Ok(target, "You are no longer wanted.");
        return RW_Ok(playerid, "Wanted cleared.");
    }
    gWantedStars[target] = stars;
    gWantedReason[target] = reason;
    new safe[MAX_PLAYER_NAME];
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT OR REPLACE INTO wanted(player_id,stars,reason,officer,stamp) VALUES(%d,%d,%d,'%s',%d)", gAccountId[target], stars, reason, safe, gettime());
    RW_DB_Exec(query);
    new message[160], tnick[MAX_PLAYER_NAME];
    RW_GetNick(target, tnick);
    format(message, sizeof message, "[Dispatch] %s is wanted: %d stars, %s (%s).", tnick, stars, why, nick);
    for (new i; i < MAX_PLAYERS; i++) if (RW_PoliceOnDuty(i)) SendClientMessage(i, 0x8FD3FFFF, message);
    format(message, sizeof message, "You are wanted: %d stars, %s.", stars, why);
    RW_Error(target, message);
    format(message, sizeof message, "%s: %d stars, %s.", tnick, stars, why);
    return RW_Ok(playerid, message);
}

/*  ---------------------------------- арест -------------------------------- */

stock RW_PoliceArrest(playerid, target, minutes, bail)
{
    if (!RW_PoliceCanAct(playerid, target)) return 0;
    if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");
    if (!RW_PoliceAtStation(playerid) || !RW_PoliceAtStation(target)) return RW_Error(playerid, "Bring him to the station (40 m).");
    if (minutes < 1 || minutes > JAIL_MAX_MINUTES) return RW_Error(playerid, "Term must be 1 to 60 minutes.");
    if (bail < 0 || bail > 100000) return RW_Error(playerid, "Bail must be 0 to 100000.");
    new seconds = minutes * 60;
    new nick[MAX_PLAYER_NAME], tnick[MAX_PLAYER_NAME], why[24], query[320];
    RW_GetNick(playerid, nick);
    RW_GetNick(target, tnick);
    RW_PoliceReason(gWantedReason[target], why, sizeof why);
    new safe[MAX_PLAYER_NAME];
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT INTO arrests(player_id,officer,minutes,bail,reason,stamp) VALUES(%d,'%s',%d,%d,%d,%d)", gAccountId[target], safe, minutes, bail, gWantedReason[target], gettime());
    RW_DB_Exec(query);
    gWantedStars[target] = 0;
    gWantedReason[target] = 0;
    format(query, sizeof query, "DELETE FROM wanted WHERE player_id=%d", gAccountId[target]);
    RW_DB_Exec(query);
    return RW_PoliceJail(playerid, target, seconds, bail, 0, true);
}
// officer < 0 — посадка восстановлена из базы после перезахода.
stock RW_PoliceJail(playerid, target, seconds, bail, reason, bool:fresh)
{
    #pragma unused reason
    new who = target >= 0 ? target : playerid;
    gJailTime[who] = seconds;
    gJailBail[who] = bail;
    if (target >= 0 && playerid >= 0) {
        new nick[MAX_PLAYER_NAME];
        RW_GetNick(playerid, nick);
        format(gJailOfficer[who], sizeof gJailOfficer[], "%s", nick);
    }
    TogglePlayerControllable(who, true);
    gCuffed[who] = false;
    gWalled[who] = false;
    if (gEscort[who] >= 0) { gEscorting[gEscort[who]] = -1; gEscort[who] = -1; }
    SetPlayerInterior(who, 0);
    SetPlayerVirtualWorld(who, 0);
    SetPlayerPos(who, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z);
    SetPlayerFacingAngle(who, 90.0);
    SetCameraBehindPlayer(who);
    if (fresh) RW_PoliceSave(who);
    new message[200];
    if (target >= 0 && playerid >= 0) {
        new tnick[MAX_PLAYER_NAME];
        RW_GetNick(target, tnick);
        format(message, sizeof message, "%s is jailed for %d minutes. Bail %d.", tnick, seconds / 60, bail);
        RW_Ok(playerid, message);
    }
    format(message, sizeof message, "You are in a cell: %d minutes. Bail %d (/bail).", seconds / 60, bail);
    return RW_Error(who, message);
}
stock RW_PoliceRelease(playerid, const text[])
{
    if (gJailTime[playerid] <= 0) return 0;
    gJailTime[playerid] = 0;
    gJailBail[playerid] = 0;
    gJailOfficer[playerid][0] = EOS;
    RW_PoliceSave(playerid);
    SetPlayerPos(playerid, POLICE_STATION_X + 3.0, POLICE_STATION_Y + 3.0, POLICE_STATION_Z);
    SetPlayerFacingAngle(playerid, 180.0);
    SetCameraBehindPlayer(playerid);
    TogglePlayerControllable(playerid, true);
    new message[160];
    format(message, sizeof message, "%s You are free. %s", RW_TAG, text);
    SendClientMessage(playerid, 0x7BD88FFF, message);
    return 1;
}
// Залог: 10% — арестовавшему офицеру, остальное государству.
stock RW_PoliceBail(playerid)
{
    if (gJailTime[playerid] <= 0) return RW_Error(playerid, "You are not in a cell.");
    if (gJailBail[playerid] <= 0) return RW_Error(playerid, "Bail is not set for this arrest.");
    if (!RW_TakeMoney(playerid, gJailBail[playerid])) return RW_Error(playerid, "Not enough cash for the bail.");
    new officer = RW_PoliceFind(gJailOfficer[playerid]);
    new cut = gJailBail[playerid] / 10;
    if (officer >= 0 && cut > 0) {
        RW_AddMoney(officer, cut);
        new message[160];
        format(message, sizeof message, "Bail paid: your 10%% is %d.", cut);
        RW_Ok(officer, message);
    }
    new message[160];
    format(message, sizeof message, "Bail paid: %d.", gJailBail[playerid]);
    RW_PoliceRelease(playerid, "Walk out and behave.");
    return RW_Ok(playerid, message);
}
// Штраф: квитанция в базе, оплачивается из наличных.
stock RW_PoliceFine(playerid, target, amount, reason)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    if (amount <= 0 || amount > 50000) return RW_Error(playerid, "Fine must be 1 to 50000.");
    if (reason < 0 || reason > 5) reason = 5;
    new nick[MAX_PLAYER_NAME], tnick[MAX_PLAYER_NAME], why[24], query[320];
    RW_GetNick(playerid, nick);
    RW_GetNick(target, tnick);
    RW_PoliceReason(reason, why, sizeof why);
    new safe[MAX_PLAYER_NAME];
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT INTO fines(player_id,officer,amount,reason,paid,stamp) VALUES(%d,'%s',%d,%d,0,%d)", gAccountId[target], safe, amount, reason, gettime());
    RW_DB_Exec(query);
    new message[200];
    format(message, sizeof message, "%s issued a fine to %s: %d (%s).", nick, tnick, amount, why);
    for (new i; i < MAX_PLAYERS; i++) if (RW_PoliceOnDuty(i)) SendClientMessage(i, 0x8FD3FFFF, message);
    format(message, sizeof message, "%s fined you %d for %s. Pay at the station.", nick, amount, why);
    return RW_Error(target, message);
}
stock RW_PolicePayFines(playerid)
{
    if (gAccountId[playerid] <= 0) return RW_Error(playerid, "Log in first.");
    new query[200];
    format(query, sizeof query, "SELECT id,amount FROM fines WHERE player_id=%d AND paid=0", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "No unpaid fines.");
    new rows = db_num_rows(r), total;
    for (new i; i < rows; i++) { total += db_get_field_assoc_int(r, "amount"); db_next_row(r); }
    db_free_result(r);
    if (!rows) return RW_Info(playerid, "No unpaid fines.");
    if (!RW_TakeMoney(playerid, total)) return RW_Error(playerid, "Not enough cash.");
    format(query, sizeof query, "UPDATE fines SET paid=1 WHERE player_id=%d AND paid=0", gAccountId[playerid]);
    RW_DB_Exec(query);
    new message[128];
    format(message, sizeof message, "Paid %d in fines (%d tickets).", total, rows);
    return RW_Ok(playerid, message);
}

/*  -------------------------------- адвокатура ----------------------------- */

stock RW_PoliceLicense(playerid, target, days)
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 3) return RW_Error(playerid, "Administrators only.");
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Player not found.");
    if (days < 0 || days > 365) return RW_Error(playerid, "License: 0 to 365 days.");
    new expire = days ? gettime() + days * 86400 : 0;
    new query[256];
    format(query, sizeof query, "INSERT OR REPLACE INTO lawyers(player_id,license,expire,price,rating) VALUES(%d,%d,%d,%d,%d)", gAccountId[target], days ? 1 : 0, expire, gLawyerPrice[target], gLawyerRating[target]);
    RW_DB_Exec(query);
    gLawyerLicense[target] = days ? 1 : 0;
    gLawyerExpire[target] = expire;
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(target, nick);
    if (days) {
        format(message, sizeof message, "%s now has a lawyer license for %d days.", nick, days);
        RW_Ok(playerid, message);
        format(message, sizeof message, "You are a licensed lawyer for %d days. /lawyer.", days);
        return RW_Ok(target, message);
    }
    format(message, sizeof message, "%s lost the lawyer license.", nick);
    RW_Ok(playerid, message);
    return RW_Error(target, "Your lawyer license was revoked.");
}
stock RW_PoliceCaseFree()
{
    for (new i = 1; i <= MAX_CASES; i++) if (gCaseLawyer[i-1] == INVALID_PLAYER_ID) return i;
    return 0;
}
// Разговор адвоката, задержанного и офицера: обычный чат идёт втроём.
stock RW_PoliceCaseStart(lawyer, prisoner)
{
    if (!RW_IsLawyer(lawyer)) return RW_Error(lawyer, "You have no lawyer license.");
    if (prisoner < 0 || !IsPlayerConnected(prisoner) || gJailTime[prisoner] <= 0) return RW_Error(lawyer, "He is not in a cell.");
    if (!RW_PoliceNear(lawyer, prisoner, 4.0)) return RW_Error(lawyer, "Stand closer to the player.");
    if (gCaseId[lawyer]) return RW_Error(lawyer, "You already have an open case: /endcase first.");
    new id = RW_PoliceCaseFree();
    if (!id) return RW_Error(lawyer, "Too many open cases right now.");
    new officer = RW_PoliceFind(gJailOfficer[prisoner]);
    gCaseId[lawyer] = id; gCaseId[prisoner] = id; if (officer >= 0) gCaseId[officer] = id;
    gCaseLawyer[id-1] = lawyer;
    gCasePrisoner[id-1] = prisoner;
    gCaseOfficer[id-1] = officer;
    gCaseBail[id-1] = gJailBail[prisoner];
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(lawyer, nick);
    format(message, sizeof message, "%s is your lawyer. Chat now goes to the three of you.", nick);
    RW_Ok(prisoner, message);
    if (officer >= 0) RW_Info(officer, "A lawyer joined the case. Chat now goes to the three of you.");
    format(message, sizeof message, "Case open. Bail is %d: /bailrequest to ask for it.", gJailBail[prisoner]);
    return RW_Ok(lawyer, message);
}
stock RW_PoliceCaseEnd(playerid)
{
    new id = gCaseId[playerid];
    if (!id) return 0;
    new ids[3];
    ids[0] = gCaseLawyer[id-1]; ids[1] = gCasePrisoner[id-1]; ids[2] = gCaseOfficer[id-1];
    for (new i; i < 3; i++) {
        if (ids[i] == INVALID_PLAYER_ID || !IsPlayerConnected(ids[i])) continue;
        gCaseId[ids[i]] = 0;
        if (ids[i] != playerid) RW_Info(ids[i], "The case is closed.");
    }
    gCaseLawyer[id-1] = INVALID_PLAYER_ID;
    gCasePrisoner[id-1] = INVALID_PLAYER_ID;
    gCaseOfficer[id-1] = INVALID_PLAYER_ID;
    gCaseBail[id-1] = 0;
    return 1;
}
stock RW_PoliceCaseSay(playerid, const text[])
{
    new id = gCaseId[playerid];
    if (!id) return 1;
    new nick[MAX_PLAYER_NAME], message[256];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "[Case] %s: %s", nick, text);
    new ids[3];
    ids[0] = gCaseLawyer[id-1]; ids[1] = gCasePrisoner[id-1]; ids[2] = gCaseOfficer[id-1];
    for (new i; i < 3; i++) {
        if (ids[i] == INVALID_PLAYER_ID || !IsPlayerConnected(ids[i]) || !gLogged[ids[i]]) continue;
        SendClientMessage(ids[i], 0xF5B04CFF, message);
    }
    return 0;
}
// Залог через адвоката: 50% адвокату, 10% офицеру, остальное государству.
stock RW_PoliceBailRequest(playerid)
{
    new id = gCaseId[playerid];
    if (!id || gCaseLawyer[id-1] != playerid) return RW_Error(playerid, "Open a case first: Y on the detained player.");
    new prisoner = gCasePrisoner[id-1];
    if (prisoner == INVALID_PLAYER_ID || !IsPlayerConnected(prisoner) || gJailTime[prisoner] <= 0) return RW_Error(playerid, "The client is not in a cell.");
    new bail = gJailBail[prisoner];
    if (bail <= 0) return RW_Error(playerid, "Bail is not set for this arrest.");
    if (!RW_TakeMoney(prisoner, bail)) return RW_Error(playerid, "The client has no cash for the bail.");
    new lawyerCut = bail / 2, officerCut = bail / 10;
    RW_AddMoney(playerid, lawyerCut);
    new officer = gCaseOfficer[id-1];
    if (officer >= 0 && officer != INVALID_PLAYER_ID && IsPlayerConnected(officer) && officerCut > 0) {
        RW_AddMoney(officer, officerCut);
        new message[128];
        format(message, sizeof message, "Bail granted: your 10%% is %d.", officerCut);
        RW_Ok(officer, message);
    }
    gLawyerRating[playerid]++;
    new query[256];
    format(query, sizeof query, "INSERT OR REPLACE INTO lawyers(player_id,license,expire,price,rating) VALUES(%d,%d,%d,%d,%d)", gAccountId[playerid], gLawyerLicense[playerid], gLawyerExpire[playerid], gLawyerPrice[playerid], gLawyerRating[playerid]);
    RW_DB_Exec(query);
    new message[160];
    format(message, sizeof message, "Bail granted: %d (you keep %d). Rating %d.", bail, lawyerCut, gLawyerRating[playerid]);
    RW_Ok(playerid, message);
    RW_PoliceCaseEnd(playerid);
    RW_PoliceRelease(prisoner, "Released on bail through a lawyer.");
    return 1;
}

/* Смерть снимает наручники и конвой, но не срок: он отсиживается в КПЗ. */
stock RW_PoliceOnDeath(playerid)
{
    gStun[playerid] = 0;
    if (gWalled[playerid]) {
        gWalled[playerid] = false;
        new query[128];
        format(query, sizeof query, "DELETE FROM wall_attachments WHERE player_id=%d", gAccountId[playerid]);
        RW_DB_Exec(query);
    }
    if (gEscort[playerid] >= 0) { gEscorting[gEscort[playerid]] = -1; gEscort[playerid] = -1; }
    if (gEscorting[playerid] >= 0) { gEscort[gEscorting[playerid]] = -1; gEscorting[playerid] = -1; }
    gCuffed[playerid] = false;
    return 1;
}

/*  --------------------------------- слежение ------------------------------ */

stock RW_PoliceTrack(playerid, target)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "That citizen is not online.");
    gTrack[playerid] = target;
    gTrackTick[playerid] = 0;
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(target, nick);
    format(message, sizeof message, "Tracking %s. /untrack to stop.", nick);
    return RW_Ok(playerid, message);
}
stock RW_PoliceUntrack(playerid)
{
    if (gTrack[playerid] < 0) return RW_Error(playerid, "You are not tracking anybody.");
    RemovePlayerMapIcon(playerid, 97);
    gTrack[playerid] = -1;
    return RW_Ok(playerid, "Tracking stopped.");
}

/*  ---------------------------------- таймер ------------------------------- */

public RW_PolicePulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gLogged[p]) continue;
        /* оглушение тазером и дубинкой */
        if (gStun[p] && GetTickCount() >= gStun[p]) {
            gStun[p] = 0;
            if (!gCuffed[p] && gJailTime[p] <= 0 && gEscort[p] < 0) {
                TogglePlayerControllable(p, true);
                ClearAnimations(p);
            }
        }
        /* конвоирование */
        if (gEscort[p] >= 0) {
            new officer = gEscort[p];
            if (!IsPlayerConnected(officer) || !RW_PoliceNear(officer, p, 8.0)) {
                gEscorting[officer] = -1;
                gEscort[p] = -1;
                if (IsPlayerConnected(officer)) RW_Info(officer, "Escort lost: you were too far.");
            } else {
                new Float:x, Float:y, Float:z, Float:a;
                GetPlayerPos(officer, x, y, z);
                GetPlayerFacingAngle(officer, a);
                x -= 1.3 * floatsin(-a, degrees);
                y -= 1.3 * floatcos(-a, degrees);
                SetPlayerPos(p, x, y, z);
                SetPlayerFacingAngle(p, a);
            }
        }
        /* срок в КПЗ */
        if (gJailTime[p] > 0) {
            if (GetPlayerDistanceFromPoint(p, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z) > 60.0) {
                if (GetPlayerState(p) == PLAYER_STATE_DRIVER || GetPlayerState(p) == PLAYER_STATE_PASSENGER) RemovePlayerFromVehicle(p);
                SetPlayerPos(p, POLICE_CELL_X, POLICE_CELL_Y, POLICE_CELL_Z);
                RW_Error(p, "No escaping from the cell.");
            }
            gJailTime[p]--;
            if (gJailTime[p] <= 0) RW_PoliceRelease(p, "Served the term.");
            else {
                if (gJailTime[p] % 60 == 0) {
                    new message[96];
                    format(message, sizeof message, "%d minutes left.%s", gJailTime[p] / 60, gJailBail[p] > 0 ? (" /bail to pay the bail.") : (""));
                    SendClientMessage(p, COLOR_GREY, message);
                }
                if (gJailTime[p] % 30 == 0) RW_PoliceSave(p);
            }
        }
        /* метка слежения */
        if (gTrack[p] >= 0) {
            new target = gTrack[p];
            if (!IsPlayerConnected(target) || !gLogged[target] || !RW_PoliceOnDuty(p)) RW_PoliceUntrack(p);
            else if (GetTickCount() - gTrackTick[p] > 2000) {
                gTrackTick[p] = GetTickCount();
                new Float:x, Float:y, Float:z;
                GetPlayerPos(target, x, y, z);
                SetPlayerMapIcon(p, 97, x, y, z, 0, 0xFF453AFF, MAPICON_GLOBAL);
            }
        }
    }
    return 1;
}

/*  ---------------------------------- меню --------------------------------- */
/*  Одно меню на всё: браузер для тех, у кого плагин есть, TextDraw — для
 *  остальных. Идентификаторы строк одинаковые, поэтому обработчик один.     */

stock RW_PoliceMenuOpen(playerid, target)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target] || target == playerid) return RW_Error(playerid, "Stand closer to the player.");
    gPoliceTarget[playerid] = target;
    gPoliceScreen[playerid] = POLICE_SCR_ACTIONS;
    return RW_PoliceRender(playerid);
}
stock RW_PoliceRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefPoliceRender(playerid);
    #endif
    return RW_PolicePanel(playerid);
}
stock RW_CefPoliceRender(playerid)
{
    new target = gPoliceTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target)) return RW_UI_Close(playerid);
    new nick[MAX_PLAYER_NAME], subtitle[96];
    RW_GetNick(target, nick);
    new screen = gPoliceScreen[playerid];
    if (screen == POLICE_SCR_ACTIONS) {
        format(subtitle, sizeof subtitle, "Wanted: %d / cuffed: %s", gWantedStars[target], gCuffed[target] ? ("yes") : ("no"));
        if (!RW_CefOpen(playerid, CEF_MODE_POLICE, nick, subtitle)) return RW_PolicePanel(playerid);
        RW_CefRow(playerid, 0, gCuffed[target] ? ("Take the cuffs off") : ("Put the cuffs on"), "", "band");
        RW_CefRow(playerid, 1, gEscorting[playerid] == target ? ("Stop escorting") : ("Escort"), "", "users");
        RW_CefRow(playerid, 2, gWalled[target] ? ("Release from the wall") : ("Attach to the wall"), "", "door");
        RW_CefRow(playerid, 3, gWantedStars[target] ? ("Change wanted") : ("Declare wanted"), gWantedStars[target] ? ("wanted") : ("clean"), "alert");
        RW_CefRow(playerid, 4, gCuffed[target] ? ("Arrest") : ("Arrest (cuff him first)"), "", "clock");
        RW_CefRow(playerid, 5, "Stun: taser or baton", "", "shot");
        RW_CefRow(playerid, 6, "Check his documents", "", "check");
        RW_CefAction(playerid, 7, "Refresh", "");
        RW_CefAction(playerid, 8, "Close", "danger");
        return RW_CefEnd(playerid);
    }
    if (screen == POLICE_SCR_WANTED) {
        if (!RW_CefOpen(playerid, CEF_MODE_POLICE, "Wanted", "How many stars?")) return RW_PolicePanel(playerid);
        for (new i = 1; i <= 5; i++) {
            new label[32];
            format(label, sizeof label, "%d star%s", i, i == 1 ? ("") : ("s"));
            RW_CefRow(playerid, i - 1, label, gPoliceStars[playerid] == i ? ("selected") : (""), "alert");
        }
        RW_CefRow(playerid, 5, "Clear the wanted list", "", "check");
        RW_CefAction(playerid, 6, "Back", "");
        RW_CefAction(playerid, 8, "Close", "danger");
        return RW_CefEnd(playerid);
    }
    if (screen == POLICE_SCR_REASON) {
        if (!RW_CefOpen(playerid, CEF_MODE_POLICE, "Wanted", "Reason for the record")) return RW_PolicePanel(playerid);
        new why[24];
        for (new i; i < 6; i++) {
            RW_PoliceReason(i, why, sizeof why);
            RW_CefRow(playerid, i, why, "", "alert");
        }
        RW_CefAction(playerid, 6, "Back", "");
        RW_CefAction(playerid, 8, "Close", "danger");
        return RW_CefEnd(playerid);
    }
    /* оформление ареста */
    format(subtitle, sizeof subtitle, "Term %d min / bail %d", gPoliceTerm[playerid], gPoliceBail[playerid]);
    if (!RW_CefOpen(playerid, CEF_MODE_POLICE, "Arrest", subtitle)) return RW_PolicePanel(playerid);
    new terms[4] = { 2, 5, 10, 15 };
    new label[40];
    for (new i; i < 4; i++) {
        format(label, sizeof label, "%d minutes", terms[i]);
        RW_CefRow(playerid, i, label, gPoliceTerm[playerid] == terms[i] ? ("selected") : (""), "clock");
    }
    RW_CefRow(playerid, 4, "Bail amount", gPoliceBail[playerid] ? ("set") : ("none"), "bag");
    RW_CefRow(playerid, 5, "Confirm the arrest", "", "check");
    RW_CefAction(playerid, 6, "Back", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
// Тот же список, но для PlayerTextDraw.
stock RW_PolicePanel(playerid)
{
    new target = gPoliceTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target)) return RW_UI_Close(playerid);
    new nick[MAX_PLAYER_NAME], label[64];
    RW_GetNick(target, nick);
    new screen = gPoliceScreen[playerid];
    if (screen == POLICE_SCR_ACTIONS) {
        RW_UI_Panel(playerid, UI_POLICE, nick, "Police actions");
        RW_UI_Row(playerid, 0, gCuffed[target] ? ("Take the cuffs off") : ("Put the cuffs on"));
        RW_UI_Row(playerid, 1, gEscorting[playerid] == target ? ("Stop escorting") : ("Escort"));
        RW_UI_Row(playerid, 2, gWalled[target] ? ("Release from the wall") : ("Attach to the wall"));
        RW_UI_Row(playerid, 3, gWantedStars[target] ? ("Change wanted") : ("Declare wanted"));
        RW_UI_Row(playerid, 4, "Arrest");
        RW_UI_Row(playerid, 5, "Stun: taser or baton");
        RW_UI_Row(playerid, 6, "Check his documents");
        RW_UI_Row(playerid, 7, "Close");
        return 1;
    }
    if (screen == POLICE_SCR_WANTED) {
        RW_UI_Panel(playerid, UI_POLICE, "Wanted", "How many stars?");
        for (new i = 1; i <= 5; i++) {
            format(label, sizeof label, "%d stars%s", i, gPoliceStars[playerid] == i ? (" - selected") : (""));
            RW_UI_Row(playerid, i - 1, label);
        }
        RW_UI_Row(playerid, 5, "Clear the wanted list");
        RW_UI_Row(playerid, 7, "Close");
        return 1;
    }
    if (screen == POLICE_SCR_REASON) {
        RW_UI_Panel(playerid, UI_POLICE, "Wanted", "Reason for the record");
        new why[24];
        for (new i; i < 6; i++) { RW_PoliceReason(i, why, sizeof why); RW_UI_Row(playerid, i, why); }
        RW_UI_Row(playerid, 7, "Close");
        return 1;
    }
    RW_UI_Panel(playerid, UI_POLICE, "Arrest", "Term and bail");
    new terms[4] = { 2, 5, 10, 15 };
    for (new i; i < 4; i++) {
        format(label, sizeof label, "%d minutes%s", terms[i], gPoliceTerm[playerid] == terms[i] ? (" - selected") : (""));
        RW_UI_Row(playerid, i, label);
    }
    format(label, sizeof label, "Bail: %d", gPoliceBail[playerid]);
    RW_UI_Row(playerid, 4, label);
    RW_UI_Row(playerid, 5, "Confirm the arrest");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
// Документы игрока: что о нём известно.
stock RW_PoliceDossier(playerid, target)
{
    new nick[MAX_PLAYER_NAME], why[24], line[128];
    RW_GetNick(target, nick);
    RW_Accent(playerid, "Documents:");
    format(line, sizeof line, "Name: %s / account %d", nick, gAccountId[target]);
    SendClientMessage(playerid, COLOR_GREY, line);
    RW_PoliceReason(gWantedReason[target], why, sizeof why);
    format(line, sizeof line, "Wanted: %d stars (%s)", gWantedStars[target], gWantedStars[target] ? (why) : ("none"));
    SendClientMessage(playerid, COLOR_GREY, line);
    new query[220];
    format(query, sizeof query, "SELECT amount,paid FROM fines WHERE player_id=%d", gAccountId[target]);
    new DBResult:r = db_query(gDB, query);
    new total, unpaid;
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) {
            new amount = db_get_field_assoc_int(r, "amount");
            total += amount;
            if (!db_get_field_assoc_int(r, "paid")) unpaid += amount;
            db_next_row(r);
        }
        db_free_result(r);
    }
    format(line, sizeof line, "Fines: %d total, %d unpaid", total, unpaid);
    SendClientMessage(playerid, COLOR_GREY, line);
    format(query, sizeof query, "SELECT minutes,reason,officer FROM arrests WHERE player_id=%d", gAccountId[target]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        format(line, sizeof line, "Arrests: %d%s", rows, rows ? (" (criminal record)") : (" (clean)"));
        SendClientMessage(playerid, COLOR_GREY, line);
        db_free_result(r);
    }
    if (gJailTime[target] > 0) {
        format(line, sizeof line, "In a cell: %d minutes left, bail %d", gJailTime[target] / 60 + 1, gJailBail[target]);
        SendClientMessage(playerid, COLOR_GREY, line);
    }
    return 1;
}
stock RW_PoliceAction(playerid, row, bool:cef)
{
    new target = gPoliceTarget[playerid];
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target]) return RW_UI_Close(playerid);
    new screen = gPoliceScreen[playerid];
    new closeRow = cef ? 8 : 7;
    if (row == closeRow) return RW_UI_Close(playerid);
    if (screen == POLICE_SCR_ACTIONS) {
        if (row == 0) { if (gCuffed[target]) RW_PoliceUncuff(playerid, target); else RW_PoliceCuff(playerid, target); return RW_PoliceRender(playerid); }
        if (row == 1) { if (gEscorting[playerid] == target) RW_PoliceStopEscort(playerid); else RW_PoliceEscort(playerid, target); return RW_PoliceRender(playerid); }
        if (row == 2) { if (gWalled[target]) RW_PoliceUnwall(playerid, target); else RW_PoliceWall(playerid, target); return RW_PoliceRender(playerid); }
        if (row == 3) { gPoliceScreen[playerid] = POLICE_SCR_WANTED; return RW_PoliceRender(playerid); }
        if (row == 4) {
            if (!gCuffed[target]) return RW_Error(playerid, "Cuff him first.");
            gPoliceScreen[playerid] = POLICE_SCR_ARREST;
            return RW_PoliceRender(playerid);
        }
        if (row == 5) { RW_PoliceStun(playerid, target); return RW_PoliceRender(playerid); }
        if (row == 6) { RW_PoliceDossier(playerid, target); return RW_PoliceRender(playerid); }
        if (row == 7 && cef) return RW_PoliceRender(playerid);
        return 1;
    }
    if (screen == POLICE_SCR_WANTED) {
        if (row >= 0 && row < 5) { gPoliceStars[playerid] = row + 1; gPoliceScreen[playerid] = POLICE_SCR_REASON; return RW_PoliceRender(playerid); }
        if (row == 5) { RW_PoliceWanted(playerid, target, 0, 0); gPoliceScreen[playerid] = POLICE_SCR_ACTIONS; return RW_PoliceRender(playerid); }
        if (row == 6) { gPoliceScreen[playerid] = POLICE_SCR_ACTIONS; return RW_PoliceRender(playerid); }
        if (row == 7 && cef) return RW_PoliceRender(playerid);
        return 1;
    }
    if (screen == POLICE_SCR_REASON) {
        if (row >= 0 && row < 6) {
            RW_PoliceWanted(playerid, target, gPoliceStars[playerid], row);
            gPoliceScreen[playerid] = POLICE_SCR_ACTIONS;
            return RW_PoliceRender(playerid);
        }
        if (row == 6) { gPoliceScreen[playerid] = POLICE_SCR_WANTED; return RW_PoliceRender(playerid); }
        if (row == 7 && cef) return RW_PoliceRender(playerid);
        return 1;
    }
    new terms[4] = { 2, 5, 10, 15 };
    if (row >= 0 && row < 4) { gPoliceTerm[playerid] = terms[row]; return RW_PoliceRender(playerid); }
    if (row == 4) { gPoliceBail[playerid] = RW_PoliceBailStep(gPoliceBail[playerid]); return RW_PoliceRender(playerid); }
    if (row == 5) { RW_PoliceArrest(playerid, target, gPoliceTerm[playerid], gPoliceBail[playerid]); return RW_UI_Close(playerid); }
    if (row == 6) { gPoliceScreen[playerid] = POLICE_SCR_ACTIONS; return RW_PoliceRender(playerid); }
    if (row == 7 && cef) return RW_PoliceRender(playerid);
    return 1;
}
stock RW_PoliceCefClick(playerid, row) { return RW_PoliceAction(playerid, row, true); }
stock RW_PoliceClick(playerid, row) { return RW_PoliceAction(playerid, row, false); }

/*  ------------------------------- планшет MDT ----------------------------- */

stock RW_MdtOpen(playerid)
{
    if (!RW_PoliceCanAct(playerid)) return 0;
    gMdtPage[playerid] = 0;
    gMdtList[playerid] = 0;
    return RW_MdtRender(playerid);
}
stock RW_MdtRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { if (gMdtList[playerid]) return RW_CefMdtList(playerid); return RW_CefMdtRender(playerid); }
    #endif
    if (gMdtList[playerid]) return RW_MdtPanelList(playerid);
    return RW_MdtPanel(playerid);
}
stock RW_MdtTabName(tab, dest[], size = sizeof dest)
{
    switch (tab) {
        case MDT_CITIZENS: format(dest, size, "Citizens");
        case MDT_VEHICLES: format(dest, size, "Vehicles");
        case MDT_ARRESTS: format(dest, size, "Arrests");
        default: format(dest, size, "Fines");
    }
    return 1;
}
// Запрос и отрисовка строк списка. limit — сколько строк помещается в меню.
stock RW_MdtList(playerid, limit)
{
    for (new i; i < MDT_ROWS; i++) gMdtIds[playerid][i] = 0;
    new query[340];
    if (gMdtTab[playerid] == MDT_CITIZENS) {
        format(query, sizeof query, "SELECT a.id,a.name,(SELECT stars FROM wanted w WHERE w.player_id=a.id) AS stars FROM accounts a ORDER BY a.name LIMIT %d OFFSET %d", limit, gMdtPage[playerid]*limit);
    } else if (gMdtTab[playerid] == MDT_VEHICLES) {
        format(query, sizeof query, "SELECT c.id,c.model,c.token,a.name AS owner FROM owned_cars c LEFT JOIN accounts a ON a.id=c.owner ORDER BY c.id DESC LIMIT %d OFFSET %d", limit, gMdtPage[playerid]*limit);
    } else if (gMdtTab[playerid] == MDT_ARRESTS) {
        format(query, sizeof query, "SELECT r.id,r.minutes,r.bail,r.officer,a.name AS who FROM arrests r LEFT JOIN accounts a ON a.id=r.player_id ORDER BY r.id DESC LIMIT %d OFFSET %d", limit, gMdtPage[playerid]*limit);
    } else {
        format(query, sizeof query, "SELECT f.id,f.amount,f.paid,f.officer,a.name AS who FROM fines f LEFT JOIN accounts a ON a.id=f.player_id ORDER BY f.id DESC LIMIT %d OFFSET %d", limit, gMdtPage[playerid]*limit);
    }
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) { gMdtCount[playerid] = 0; return 0; }
    new rows = db_num_rows(r), index;
    for (new i; i < rows && index < limit; i++) {
        new label[72];
        gMdtIds[playerid][index] = db_get_field_assoc_int(r, "id");
        if (gMdtTab[playerid] == MDT_CITIZENS) {
            new name[MAX_PLAYER_NAME], stars = db_get_field_assoc_int(r, "stars");
            db_get_field_assoc(r, "name", name, sizeof name);
            format(label, sizeof label, "%s%s", name, stars ? (" - WANTED") : (""));
            #if RAILWAY_CEF
                if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, stars ? ("wanted") : ("clean"), "users", "", stars ? ("state-on") : ("state-off"));
                else
            #endif
            RW_UI_Row(playerid, index, label);
        } else if (gMdtTab[playerid] == MDT_VEHICLES) {
            new name[MAX_PLAYER_NAME], model = db_get_field_assoc_int(r, "model");
            db_get_field_assoc(r, "owner", name, sizeof name);
            format(label, sizeof label, "model %d - %s", model, name[0] ? name : ("unknown"));
            #if RAILWAY_CEF
                if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, "", "car");
                else
            #endif
            RW_UI_Row(playerid, index, label);
        } else if (gMdtTab[playerid] == MDT_ARRESTS) {
            new name[MAX_PLAYER_NAME], minutes = db_get_field_assoc_int(r, "minutes"), bail = db_get_field_assoc_int(r, "bail");
            db_get_field_assoc(r, "who", name, sizeof name);
            format(label, sizeof label, "%s - %d min, bail %d", name[0] ? name : ("?"), minutes, bail);
            #if RAILWAY_CEF
                if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, "", "clock");
                else
            #endif
            RW_UI_Row(playerid, index, label);
        } else {
            new name[MAX_PLAYER_NAME], amount = db_get_field_assoc_int(r, "amount"), paid = db_get_field_assoc_int(r, "paid");
            db_get_field_assoc(r, "who", name, sizeof name);
            format(label, sizeof label, "%s - %d %s", name[0] ? name : ("?"), amount, paid ? ("(paid)") : ("(unpaid)"));
            #if RAILWAY_CEF
                if (RW_CefOn(playerid)) RW_CefRow(playerid, index, label, paid ? ("paid") : ("unpaid"), "bag", "", paid ? ("state-on") : ("state-off"));
                else
            #endif
            RW_UI_Row(playerid, index, label);
        }
        index++;
        db_next_row(r);
    }
    db_free_result(r);
    gMdtCount[playerid] = index;
    return index;
}
// Первый экран планшета: вкладки.
stock RW_CefMdtRender(playerid)
{
    if (!RW_CefOpen(playerid, CEF_MODE_MDT, "MDT", "Police database")) return RW_MdtPanel(playerid);
    RW_CefRow(playerid, 0, "Citizens", "names", "users");
    RW_CefRow(playerid, 1, "Vehicles", "garage", "car");
    RW_CefRow(playerid, 2, "Arrests", "last arrests", "clock");
    RW_CefRow(playerid, 3, "Fines", "last fines", "bag");
    RW_CefRow(playerid, 4, gTrack[playerid] >= 0 ? ("Stop tracking") : ("Tracking"), gTrack[playerid] >= 0 ? ("on") : ("off"), "map");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_MdtPanel(playerid)
{
    RW_UI_Panel(playerid, UI_MDT, "MDT", "Police database");
    RW_UI_Row(playerid, 0, "Citizens");
    RW_UI_Row(playerid, 1, "Vehicles");
    RW_UI_Row(playerid, 2, "Arrests");
    RW_UI_Row(playerid, 3, "Fines");
    RW_UI_Row(playerid, 4, gTrack[playerid] >= 0 ? ("Stop tracking") : ("Tracking"));
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
// Список выбранной вкладки.
stock RW_CefMdtList(playerid)
{
    new tab[24];
    RW_MdtTabName(gMdtTab[playerid], tab, sizeof tab);
    if (!RW_CefOpen(playerid, CEF_MODE_MDT, "MDT", tab)) return RW_MdtPanelList(playerid);
    if (!RW_MdtList(playerid, MDT_ROWS)) RW_CefRow(playerid, 0, "Nothing found", "", "close");
    RW_CefAction(playerid, 6, "Back", "");
    if (gMdtCount[playerid] == MDT_ROWS) RW_CefAction(playerid, 7, "Next page", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_MdtPanelList(playerid)
{
    new tab[24];
    RW_MdtTabName(gMdtTab[playerid], tab, sizeof tab);
    RW_UI_Panel(playerid, UI_MDT, "MDT", tab);
    if (!RW_MdtList(playerid, MDT_ROWS_NATIVE)) RW_UI_Row(playerid, 0, "Nothing found");
    if (gMdtCount[playerid] == MDT_ROWS_NATIVE) RW_UI_Row(playerid, 5, "Next page >");
    RW_UI_Row(playerid, 6, "Back to tabs");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_MdtTabClick(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 4) {
        if (gTrack[playerid] >= 0) { RW_PoliceUntrack(playerid); return RW_MdtRender(playerid); }
        return RW_Info(playerid, "Open the Citizens tab and pick a name to start tracking.");
    }
    if (row < 0 || row > 3) return 1;
    gMdtTab[playerid] = row;
    gMdtPage[playerid] = 0;
    gMdtList[playerid] = 1;
    return RW_MdtRender(playerid);
}
stock RW_MdtAction(playerid, row, bool:cef)
{
    if (!gMdtList[playerid]) return RW_MdtTabClick(playerid, row, cef);
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 6) { gMdtList[playerid] = 0; gMdtPage[playerid] = 0; return RW_MdtRender(playerid); }
    new limit = cef ? MDT_ROWS : MDT_ROWS_NATIVE;
    if (row == (cef ? 7 : 5)) {
        if (gMdtCount[playerid] >= limit) gMdtPage[playerid]++;
        return RW_MdtRender(playerid);
    }
    if (row < 0 || row >= limit || !gMdtIds[playerid][row]) return 1;
    new id = gMdtIds[playerid][row];
    if (gMdtTab[playerid] == MDT_CITIZENS) {
        new target = -1;
        for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gLogged[i] && gAccountId[i] == id) { target = i; break; }
        if (target < 0) return RW_Error(playerid, "That citizen is offline: tracking needs him in the city.");
        RW_PoliceTrack(playerid, target);
        return RW_MdtRender(playerid);
    }
    new line[190], query[300];
    if (gMdtTab[playerid] == MDT_VEHICLES) {
        format(query, sizeof query, "SELECT c.model,c.token,a.name AS owner FROM owned_cars c LEFT JOIN accounts a ON a.id=c.owner WHERE c.id=%d", id);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0 && db_num_rows(r) > 0) {
            new name[MAX_PLAYER_NAME];
            db_get_field_assoc(r, "owner", name, sizeof name);
            format(line, sizeof line, "Car: model %d, token %d, owner %s", db_get_field_assoc_int(r, "model"), db_get_field_assoc_int(r, "token"), name[0] ? name : ("unknown"));
            SendClientMessage(playerid, COLOR_GREY, line);
        }
        if (r != DBResult:0) db_free_result(r);
        return RW_MdtRender(playerid);
    }
    if (gMdtTab[playerid] == MDT_ARRESTS) {
        format(query, sizeof query, "SELECT r.minutes,r.bail,r.reason,r.officer,a.name AS who FROM arrests r LEFT JOIN accounts a ON a.id=r.player_id WHERE r.id=%d", id);
        new DBResult:r = db_query(gDB, query);
        if (r != DBResult:0 && db_num_rows(r) > 0) {
            new name[MAX_PLAYER_NAME], officer[32], why[24];
            db_get_field_assoc(r, "who", name, sizeof name);
            db_get_field_assoc(r, "officer", officer, sizeof officer);
            RW_PoliceReason(db_get_field_assoc_int(r, "reason"), why, sizeof why);
            format(line, sizeof line, "Arrest: %s got %d min, bail %d, %s (officer %s)", name[0] ? name : ("?"), db_get_field_assoc_int(r, "minutes"), db_get_field_assoc_int(r, "bail"), why, officer);
            SendClientMessage(playerid, COLOR_GREY, line);
        }
        if (r != DBResult:0) db_free_result(r);
        return RW_MdtRender(playerid);
    }
    format(query, sizeof query, "SELECT f.amount,f.paid,f.reason,f.officer,a.name AS who FROM fines f LEFT JOIN accounts a ON a.id=f.player_id WHERE f.id=%d", id);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0 && db_num_rows(r) > 0) {
        new name[MAX_PLAYER_NAME], officer[32], why[24];
        db_get_field_assoc(r, "who", name, sizeof name);
        db_get_field_assoc(r, "officer", officer, sizeof officer);
        RW_PoliceReason(db_get_field_assoc_int(r, "reason"), why, sizeof why);
        format(line, sizeof line, "Fine: %s owes %d, %s, %s (officer %s)", name[0] ? name : ("?"), db_get_field_assoc_int(r, "amount"), db_get_field_assoc_int(r, "paid") ? ("paid") : ("unpaid"), why, officer);
        SendClientMessage(playerid, COLOR_GREY, line);
    }
    if (r != DBResult:0) db_free_result(r);
    return RW_MdtRender(playerid);
}
stock RW_MdtCefClick(playerid, row) { return RW_MdtAction(playerid, row, true); }
stock RW_MdtClick(playerid, row) { return RW_MdtAction(playerid, row, false); }

/*  --------------------------------- команды -------------------------------- */
// Цель команды: номер игрока или ближайший, если номер не указан.
stock RW_PoliceArgTarget(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word)) {
        new target = RW_PlayerTarget(playerid);
        if (target < 0) { RW_Error(playerid, "Stand closer to the player or give his id."); return -1; }
        return target;
    }
    if (!RW_IsNumeric(word)) {
        new target = RW_PoliceFind(word);
        if (target < 0) { RW_Error(playerid, "Player with that name is not online."); return -1; }
        return target;
    }
    new target = strval(word);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) {
        RW_Error(playerid, "Player with that id is not online.");
        return -1;
    }
    return target;
}
stock RW_PoliceDutyCommand(playerid)
{
    if (gFaction[playerid] <= 0) return RW_Error(playerid, "You are not a police officer.");
    gDuty[playerid] = !gDuty[playerid];
    RW_PoliceSave(playerid);
    new fname[24], message[128];
    RW_FactionName(gFaction[playerid], fname, sizeof fname);
    if (gDuty[playerid]) {
        SetPlayerColor(playerid, 0x4A8FE7FF);
        format(message, sizeof message, "%s: on duty. Y on a player for actions, Y on yourself for the tablet.", fname);
        return RW_Ok(playerid, message);
    }
    SetPlayerColor(playerid, (gAdminLevel[playerid] > 0) ? 0xF5B04CFF : 0x8FD3FFFF);
    if (gTrack[playerid] >= 0) RW_PoliceUntrack(playerid);
    format(message, sizeof message, "%s: off duty.", fname);
    return RW_Ok(playerid, message);
}
stock RW_PoliceFactionCommand(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 3) return RW_Error(playerid, "Administrators only.");
    new a[32], b[32], c[32], rest[144], tail[144], extra[32];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    new target = RW_PoliceArgTarget(playerid, a);
    if (target < 0) return 1;
    if (!b[0] || !RW_IsNumeric(b)) return RW_Error(playerid, "Use: /faction <playerid> <0-2> [rank]. 0 - dismiss.");
    new faction = strval(b), rank = c[0] && RW_IsNumeric(c) ? strval(c) : 1;
    if (faction < 0 || faction > 2) return RW_Error(playerid, "Faction: 0 civilian, 1 LSPD, 2 Sheriff.");
    if (rank < 1 || rank > 10) return RW_Error(playerid, "Rank must be 1 to 10.");
    gFaction[target] = faction;
    gRank[target] = rank;
    if (!faction) { gDuty[target] = false; if (gTrack[target] >= 0) RW_PoliceUntrack(target); }
    RW_PoliceSave(target);
    new nick[MAX_PLAYER_NAME], fname[24], message[160];
    RW_GetNick(target, nick);
    RW_FactionName(faction, fname, sizeof fname);
    format(message, sizeof message, "%s is now %s (rank %d).", nick, fname, rank);
    RW_Ok(playerid, message);
    if (target != playerid) {
        format(message, sizeof message, "You are %s, rank %d. /duty to start the shift.", fname, rank);
        RW_Ok(target, message);
    }
    return 1;
}
stock RW_PoliceFactionsCommand(playerid)
{
    new count, line[128];
    RW_Accent(playerid, "On duty:");
    for (new i; i < MAX_PLAYERS; i++) {
        if (!RW_PoliceOnDuty(i)) continue;
        new nick[MAX_PLAYER_NAME], fname[24];
        RW_GetNick(i, nick);
        RW_FactionName(gFaction[i], fname, sizeof fname);
        format(line, sizeof line, "%s [%d] - %s, rank %d", nick, i, fname, gRank[i]);
        SendClientMessage(playerid, COLOR_GREY, line);
        count++;
    }
    if (!count) return RW_Info(playerid, "Nobody is on duty right now.");
    return 1;
}
stock RW_PoliceCuffCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    return RW_PoliceCuff(playerid, target);
}
stock RW_PoliceUncuffCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    return RW_PoliceUncuff(playerid, target);
}
stock RW_PoliceEscortCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    if (gEscorting[playerid] == target) return RW_PoliceStopEscort(playerid);
    return RW_PoliceEscort(playerid, target);
}
stock RW_PoliceWallCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    if (gWalled[target]) return RW_PoliceUnwall(playerid, target);
    return RW_PoliceWall(playerid, target);
}
stock RW_PoliceWantedCommand(playerid, const params[])
{
    new a[32], b[32], c[32], rest[144], tail[144], extra[32];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    if (!a[0] || !b[0]) return RW_Error(playerid, "Use: /wanted <playerid> <0-5 stars> [reason 0-5]");
    new target = RW_PoliceArgTarget(playerid, a);
    if (target < 0) return 1;
    if (!RW_IsNumeric(b)) return RW_Error(playerid, "Stars: 0 (clear) to 5.");
    new reason = c[0] ? RW_PoliceReasonId(c) : 5;
    if (c[0] && RW_IsNumeric(c)) reason = strval(c);
    return RW_PoliceWanted(playerid, target, strval(b), reason);
}
stock RW_PoliceArrestCommand(playerid, const params[])
{
    new a[32], b[32], c[32], rest[144], tail[144], extra[32];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    new target = RW_PoliceArgTarget(playerid, a);
    if (target < 0) return 1;
    new minutes = b[0] && RW_IsNumeric(b) ? strval(b) : 5;
    new bail = c[0] && RW_IsNumeric(c) ? strval(c) : 0;
    return RW_PoliceArrest(playerid, target, minutes, bail);
}
stock RW_PoliceReleaseCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    if (gJailTime[target] <= 0) return RW_Error(playerid, "He is not in a cell.");
    RW_PoliceRelease(target, "Released by an officer.");
    new nick[MAX_PLAYER_NAME], tnick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    RW_GetNick(target, tnick);
    format(message, sizeof message, "%s released %s.", nick, tnick);
    return RW_Ok(playerid, message);
}
stock RW_PoliceFineCommand(playerid, const params[])
{
    new a[32], b[32], c[32], rest[144], tail[144], extra[32];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, tail, sizeof tail);
    RW_ParseCommand(tail, c, sizeof c, extra, sizeof extra);
    if (!a[0] || !b[0]) return RW_Error(playerid, "Use: /fine <playerid> <amount> [reason 0-5]");
    new target = RW_PoliceArgTarget(playerid, a);
    if (target < 0) return 1;
    if (!RW_IsNumeric(b)) return RW_Error(playerid, "Amount must be a number.");
    new reason = c[0] ? (RW_IsNumeric(c) ? strval(c) : RW_PoliceReasonId(c)) : 5;
    return RW_PoliceFine(playerid, target, strval(b), reason);
}
stock RW_PoliceTrackCommand(playerid, const params[])
{
    new target = RW_PoliceArgTarget(playerid, params);
    if (target < 0) return 1;
    return RW_PoliceTrack(playerid, target);
}
stock RW_PoliceStunCommand(playerid)
{
    new target = RW_PlayerTarget(playerid);
    if (target < 0) return RW_Error(playerid, "Stand closer to the player.");
    return RW_PoliceStun(playerid, target);
}
stock RW_LawyerCommand(playerid)
{
    if (!RW_IsLawyer(playerid)) return RW_Error(playerid, "You have no lawyer license. /lawyer is for licensed lawyers.");
    new line[160];
    RW_Accent(playerid, "Lawyer:");
    format(line, sizeof line, "License until %d, price %d, rating %d.", gLawyerExpire[playerid], gLawyerPrice[playerid], gLawyerRating[playerid]);
    SendClientMessage(playerid, COLOR_GREY, line);
    SendClientMessage(playerid, COLOR_GREY, "Y on a detained player - take the case; chat then goes to the three of you.");
    SendClientMessage(playerid, COLOR_GREY, "/bailrequest - ask for bail (you keep 50%); /endcase - close the case; /lawprice <amount> - your fee.");
    return 1;
}
stock RW_LawyerLicenseCommand(playerid, const params[])
{
    new a[32], b[32], rest[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    RW_ParseCommand(rest, b, sizeof b, rest, sizeof rest);
    if (!a[0]) return RW_Error(playerid, "Use: /setlawyer <playerid> <days>. 0 revokes the license.");
    new target = RW_PoliceArgTarget(playerid, a);
    if (target < 0) return 1;
    if (!b[0] || !RW_IsNumeric(b)) return RW_Error(playerid, "Use: /setlawyer <playerid> <days>. 0 revokes the license.");
    return RW_PoliceLicense(playerid, target, strval(b));
}
stock RW_LawyerPriceCommand(playerid, const params[])
{
    new a[32], rest[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    if (!RW_IsLawyer(playerid)) return RW_Error(playerid, "You have no lawyer license.");
    if (!a[0] || !RW_IsNumeric(a)) return RW_Error(playerid, "Use: /lawprice <amount>");
    new price = strval(a);
    if (price < 0 || price > 50000) return RW_Error(playerid, "Price must be 0 to 50000.");
    gLawyerPrice[playerid] = price;
    new query[200];
    format(query, sizeof query, "INSERT OR REPLACE INTO lawyers(player_id,license,expire,price,rating) VALUES(%d,%d,%d,%d,%d)", gAccountId[playerid], gLawyerLicense[playerid], gLawyerExpire[playerid], price, gLawyerRating[playerid]);
    RW_DB_Exec(query);
    new message[96];
    format(message, sizeof message, "Your fee is %d.", price);
    return RW_Ok(playerid, message);
}
