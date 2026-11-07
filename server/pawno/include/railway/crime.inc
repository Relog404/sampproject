/*  ============================================================================
 *  railway/crime.inc
 *  Криминал: ограбления по типу цели, розыск, наркота Green и крафт.
 *
 *  Вся логика серверная: цель ищется по реальным точкам карты, добыча зависит
 *  от типа цели (магазин, склад, банк), делится на банду и поднимает розыск.
 *  Розыск хранится в SQLite и сам спадает со временем — перезапуск сервера
 *  его не обнуляет. Прекурсоры и Green — предметы инвентаря, крафт идёт
 *  на лабораториях, продажа — у скупщика.
 *
 *  Интерфейс один: страница браузера (CEF_MODE_CRIME) или нативный диалог
 *  (UI_CRIME), а все действия доступны и командами.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_crime_included
    #endinput
#endif
#define _railway_crime_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/economy>
#include <railway/police>
#include <railway/medical>

/* ---------------------------------- цели ---------------------------------- */
#define CRIME_KIND_SHOP         0       /* магазин 24/7    */
#define CRIME_KIND_WAREHOUSE    1       /* пакгауз в порту */
#define CRIME_KIND_BANK         2       /* банковский терминал */
#define CRIME_KINDS             3
#define CRIME_RANGE             14.0    /* подойти к цели  */
#define CRIME_CREW_RANGE        25.0    /* кто в банде     */
#define CRIME_CREW_SHOP         10      /* до 10 человек   */
#define CRIME_CREW_BANK         30      /* до 30 человек   */
#define CRIME_COOLDOWN          900     /* 15 минут на точку */
#define CRIME_SECONDS_SHOP      20
#define CRIME_SECONDS_WAREHOUSE 35
#define CRIME_SECONDS_BANK      60

/* --------------------------------- розыск --------------------------------- */
#define CRIME_WANTED_MAX        5
#define CRIME_WANTED_DECAY      600     /* минус уровень за 10 минут */
#define CRIME_WANTED_SHOP       1
#define CRIME_WANTED_WAREHOUSE  2
#define CRIME_WANTED_BANK       3
#define CRIME_REASON_ROBBERY    2       /* причина в базе полиции   */
#define CRIME_REASON_THEFT      0
#define CRIME_REASON_DRUGS      5

/* --------------------------------- наркота -------------------------------- */
#define CRIME_GREEN_MINUTES     5       /* окно передозировки */
#define CRIME_GREEN_DOSES       3       /* больше — отключка  */
#define CRIME_GREEN_EFFECT      60      /* секунд эффекта     */
#define CRIME_GREEN_HEAL        25.0
#define CRIME_GREEN_REGEN       4
#define CRIME_CHEM_PER_DOSE     3       /* прекурсоров на дозу */
#define CRIME_CRAFT_SECONDS     30
#define CRIME_SELL_MIN          700
#define CRIME_SELL_MAX          1400
#define CRIME_SELL_RANGE        8.0

/* Реальные точки карты: пакгаузы Ocean Docks, лаборатории и скупщик. */
static const Float:gCrimeWarehouse[][3] = {
    {  2745.0, -2372.0, 13.6 },     /* Ocean Docks, пакгаузы       */
    {  2670.0, -2330.0, 13.6 }      /* контейнерный терминал       */
};
static const Float:gCrimeLab[][3] = {
    {   250.0,  -150.0,   6.0 },    /* ферма, амбар                */
    {  1595.5, -2235.9,  13.5 }     /* Ocean Docks                 */
};
static const Float:gCrimeBuyer[3] = { 2244.4, -1665.4, 13.5 };  /* Ganton */

static const gCrimeName[CRIME_KINDS][] = {
    "Магазин 24/7", "Пакгауз в порту", "Банковский терминал"
};
static const gCrimeSeconds[CRIME_KINDS] = { CRIME_SECONDS_SHOP, CRIME_SECONDS_WAREHOUSE, CRIME_SECONDS_BANK };
static const gCrimeWanted[CRIME_KINDS] = { CRIME_WANTED_SHOP, CRIME_WANTED_WAREHOUSE, CRIME_WANTED_BANK };

new gWantedStamp[MAX_PLAYERS];
new gCrimeKind[MAX_PLAYERS], gCrimeIndex[MAX_PLAYERS];
new gCrimeStamp[CRIME_KINDS][8];        /* откат по каждой точке */
new bool:gCrimeBusy[MAX_PLAYERS];
new bool:gCrafting[MAX_PLAYERS];
new gDrugUntil[MAX_PLAYERS], gDrugDose[MAX_PLAYERS][CRIME_GREEN_DOSES + 1];
new gCrimeTick[MAX_PLAYERS];

forward RW_CrimePulse();
forward RW_CrimeFinish(playerid);
forward RW_CrimeCancel(playerid);
forward RW_CrimeCraftDone(playerid);

/*  --------------------------------- схема --------------------------------- */

stock RW_CrimeSchema()
{
    /* Розыск уже хранится в таблице полиции: отдельной таблицы не нужно. */
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS wanted (player_id INT PRIMARY KEY, stars INT NOT NULL, reason INT NOT NULL, officer VARCHAR(32), stamp INT)")) return 0;
    return 1;
}
stock RW_CrimeReset(playerid)
{
    gWantedStamp[playerid] = 0;
    gCrimeKind[playerid] = 0;
    gCrimeIndex[playerid] = 0;
    gCrimeBusy[playerid] = false;
    gCrafting[playerid] = false;
    gDrugUntil[playerid] = 0;
    gCrimeTick[playerid] = 0;
    for (new i; i <= CRIME_GREEN_DOSES; i++) gDrugDose[playerid][i] = 0;
    return 1;
}
stock RW_CrimeLoad(playerid)
{
    gWantedStamp[playerid] = 0;
    if (gAccountId[playerid] <= 0) return 0;
    new query[160];
    format(query, sizeof query, "SELECT stamp FROM wanted WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) gWantedStamp[playerid] = db_get_field_assoc_int(r, "stamp");
    db_free_result(r);
    return 1;
}
stock RW_CrimeSaveStamp(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[180];
    format(query, sizeof query, "UPDATE wanted SET stamp=%d WHERE player_id=%d", gettime(), gAccountId[playerid]);
    return RW_DB_Exec(query);
}

/*  --------------------------------- розыск -------------------------------- */

stock RW_CrimeWantedSet(playerid, stars, reason)
{
    if (stars < 0) stars = 0;
    if (stars > CRIME_WANTED_MAX) stars = CRIME_WANTED_MAX;
    gWantedStars[playerid] = stars;
    gWantedStamp[playerid] = gettime();
    if (gAccountId[playerid] <= 0) return 1;
    new query[240];
    if (stars == 0) {
        format(query, sizeof query, "DELETE FROM wanted WHERE player_id=%d", gAccountId[playerid]);
        return RW_DB_Exec(query);
    }
    gWantedReason[playerid] = reason;
    format(query, sizeof query, "INSERT OR REPLACE INTO wanted(player_id,stars,reason,officer,stamp) VALUES(%d,%d,%d,'dispatch',%d)", gAccountId[playerid], stars, reason, gettime());
    return RW_DB_Exec(query);
}
stock RW_CrimeWantedAdd(playerid, level, reason)
{
    return RW_CrimeWantedSet(playerid, gWantedStars[playerid] + level, reason);
}
stock RW_CrimeWantedText(level, dest[], size = sizeof dest)
{
    if (level <= 0) format(dest, size, "не в розыске");
    else if (level == 1) format(dest, size, "1 звезда");
    else if (level < 5) format(dest, size, "%d звезды", level);
    else format(dest, size, "%d звёзд", level);
    return 1;
}
stock RW_WantedCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new target = playerid;
    if (params[0]) {
        target = strval(params);
        if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return RW_Error(playerid, "Нет такого игрока.");
        if (!RW_PoliceOnDuty(playerid) || RW_PoliceBadge(playerid) <= 0) return RW_Error(playerid, "Проверять розыск может полицейский на службе.");
    }
    new name[MAX_PLAYER_NAME], heat[24], message[180];
    RW_GetNick(target, name);
    RW_CrimeWantedText(gWantedStars[target], heat, sizeof heat);
    format(message, sizeof message, "Розыск: %s — %s. Спадает по одной звезде за 10 минут.", name, heat);
    return RW_Info(playerid, message);
}

/*  -------------------------------- поиск цели ----------------------------- */

/* Возвращает вид цели и её номер; -1, если рядом ничего нет. */
stock RW_CrimeNear(playerid, &kind, &index)
{
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new Float:px, Float:py, Float:pz;
    for (new i; i < RW_ShopPoints(); i++) {
        RW_ShopPoint(i, px, py, pz);
        if (IsPlayerInRangeOfPoint(playerid, CRIME_RANGE, px, py, pz)) { kind = CRIME_KIND_SHOP; index = i; return 1; }
    }
    for (new i; i < sizeof gCrimeWarehouse; i++)
        if (IsPlayerInRangeOfPoint(playerid, CRIME_RANGE, gCrimeWarehouse[i][0], gCrimeWarehouse[i][1], gCrimeWarehouse[i][2])) { kind = CRIME_KIND_WAREHOUSE; index = i; return 1; }
    for (new i; i < RW_AtmPoints(); i++) {
        RW_AtmPoint(i, px, py, pz);
        if (IsPlayerInRangeOfPoint(playerid, CRIME_RANGE, px, py, pz)) { kind = CRIME_KIND_BANK; index = i; return 1; }
    }
    return -1;
}
stock bool:RW_CrimeWindow()
{
    new hour, minute, second;
    gettime(hour, minute, second);
    #pragma unused minute, second
    /* Ночью кассы вывозят, зато склады и терминалы доступны круглые сутки. */
    return (hour >= 12 || hour < 1) ? true : false;
}
/* Сколько человек в деле: рядом, не полиция и не в отключке. */
stock RW_CrimeCrew(playerid, list[], limit)
{
    new Float:x, Float:y, Float:z, count;
    GetPlayerPos(playerid, x, y, z);
    list[count++] = playerid;
    for (new p; p < MAX_PLAYERS && count < limit; p++) {
        if (p == playerid || !IsPlayerConnected(p) || !gLogged[p]) continue;
        if (RW_PoliceOnDuty(p) && RW_PoliceBadge(p) > 0) continue;
        if (gDowned[p]) continue;
        if (!IsPlayerInRangeOfPoint(p, CRIME_CREW_RANGE, x, y, z)) continue;
        list[count++] = p;
    }
    return count;
}

/*  ------------------------------- ограбление ------------------------------ */

stock RW_CrimeHeist(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gCrimeBusy[playerid]) return RW_Error(playerid, "Вы уже заняты делом.");
    if (gCrafting[playerid]) return RW_Error(playerid, "Сначала закончите варку.");
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER || GetPlayerState(playerid) == PLAYER_STATE_PASSENGER)
        return RW_Error(playerid, "Выйдите из машины.");
    if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0)
        return RW_Error(playerid, "Полицейский на службе не грабит.");
    if (GetTickCount() - gCrimeTick[playerid] < 2000) return 1;
    new kind, index;
    if (RW_CrimeNear(playerid, kind, index) < 0) return RW_Error(playerid, "Рядом нет цели: магазин 24/7, пакгауз в порту или банковский терминал.");
    if (kind == CRIME_KIND_SHOP && !RW_CrimeWindow())
        return RW_Error(playerid, "Кассу днём не взять: приходите с 12:00 до 01:00. Склад и терминал доступны всегда.");
    if (gettime() < gCrimeStamp[kind][index]) return RW_Error(playerid, "Эту точку уже брали. Подождите или ищите другую.");
    gCrimeTick[playerid] = GetTickCount();
    gCrimeKind[playerid] = kind;
    gCrimeIndex[playerid] = index;
    gCrimeBusy[playerid] = true;
    TogglePlayerControllable(playerid, false);
    SetTimerEx("RW_CrimeFinish", gCrimeSeconds[kind] * 1000, false, "i", playerid);
    new message[180];
    format(message, sizeof message, "Начато: %s. Держитесь %d секунд и не уходите — банда рядом получит свою долю.", gCrimeName[kind], gCrimeSeconds[kind]);
    RW_Ok(playerid, message);
    for (new p; p < MAX_PLAYERS; p++)
        if (IsPlayerConnected(p) && gLogged[p] && RW_PoliceOnDuty(p) && RW_PoliceBadge(p) > 0) {
            new Float:x, Float:y, Float:z, nick[MAX_PLAYER_NAME];
            GetPlayerPos(playerid, x, y, z);
            RW_GetNick(playerid, nick);
            format(message, sizeof message, "Тревога: %s грабят (%d, %d)! Туда: %s.", gCrimeName[kind], floatround(x), floatround(y), nick);
            RW_Error(p, message);
        }
    return 1;
}
public RW_CrimeFinish(playerid)
{
    if (!IsPlayerConnected(playerid) || !gCrimeBusy[playerid]) return 1;
    new kind = gCrimeKind[playerid], index = gCrimeIndex[playerid];
    TogglePlayerControllable(playerid, true);
    gCrimeBusy[playerid] = false;
    gCrimeStamp[kind][index] = gettime() + CRIME_COOLDOWN;
    new limit = (kind == CRIME_KIND_BANK) ? (CRIME_CREW_BANK) : (CRIME_CREW_SHOP);
    new crew[MAX_PLAYERS], count = RW_CrimeCrew(playerid, crew, limit);
    new cash[CRIME_KINDS] = { 400, 250, 1500 };
    new total = (cash[kind] + random(cash[kind] / 2 + 1)) * count;
    new share = total / count;
    new money[32], message[220];
    for (new i; i < count; i++) {
        RW_AddMoney(crew[i], share);
        RW_CrimeWantedAdd(crew[i], gCrimeWanted[kind], CRIME_REASON_ROBBERY);
        RW_CrimeLoot(crew[i], kind);
        RW_StoryEvent(crew[i], STORY_EV_ROB);
        RW_StoryEarned(crew[i], share);
    }
    RW_FormatMoney(share, money, sizeof money);
    format(message, sizeof message, "Дело сделано: %s. Доля на каждого — %s, участников %d. Розыск +%d.", gCrimeName[kind], money, count, gCrimeWanted[kind]);
    RW_Ok(playerid, message);
    for (new i = 1; i < count; i++) {
        RW_FormatMoney(share, money, sizeof money);
        format(message, sizeof message, "Ваша доля: %s. Розыск +%d.", money, gCrimeWanted[kind]);
        RW_Info(crew[i], message);
    }
    return 1;
}
public RW_CrimeCancel(playerid)
{
    if (!gCrimeBusy[playerid] && !gCrafting[playerid]) return 1;
    gCrimeBusy[playerid] = false;
    gCrafting[playerid] = false;
    if (IsPlayerConnected(playerid)) TogglePlayerControllable(playerid, true);
    return 1;
}
/* Добыча зависит от типа цели: наличные одни, содержимое — разное. */
stock RW_CrimeLoot(playerid, kind)
{
    if (kind == CRIME_KIND_SHOP) {
        new roll = random(100);
        if (roll < 35) RW_InvGrant(playerid, ITEM_BANDAGE);
        else if (roll < 60) RW_InvGrant(playerid, ITEM_AMMO, random(6) + 1, 30);
        return 1;
    }
    if (kind == CRIME_KIND_WAREHOUSE) {
        new amount = 1 + random(3);
        RW_CrimeGive(playerid, ITEM_CHEM, amount);
        if (random(100) < 20) RW_InvGrant(playerid, ITEM_TUNING, random(TUNE_PARTS), 1);
        return 1;
    }
    if (kind == CRIME_KIND_BANK) {
        if (random(100) < 40) RW_InvGrant(playerid, ITEM_ARMOUR, RW_CrimeArmour(), 1);
        if (random(100) < 25) RW_CrimeGive(playerid, ITEM_CHEM, 2);
        return 1;
    }
    return 1;
}
stock RW_CrimeArmour() { return random(3) + 1; }
/* Выдача пачками: RW_InvGrant кладёт одну штуку, остальное добивается тут. */
stock RW_CrimeGive(playerid, type, amount)
{
    new uid = RW_InvGrant(playerid, type, 0, 1);
    if (uid <= 0) return 0;
    if (amount <= 1) return uid;
    new query[160];
    format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d", amount, uid);
    if (!RW_InvWrite(query)) return uid;
    new slot = RW_InvFind(playerid, uid);
    if (slot >= 0) gInv[playerid][slot][iiAmount] = amount;
    return uid;
}

/*  ---------------------------------- крафт -------------------------------- */

stock RW_CraftCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gCrimeBusy[playerid] || gCrafting[playerid]) return RW_Error(playerid, "Вы уже заняты делом.");
    new lab = RW_CrimeLabNear(playerid);
    if (lab < 0) return RW_Error(playerid, "Варят только в лаборатории: амбар на ферме или пакгауз в Ocean Docks.");
    if (RW_CrimeCount(playerid, ITEM_CHEM) < CRIME_CHEM_PER_DOSE)
        return RW_Error(playerid, "Нужно %d прекурсора: они падают со склада в порту.", CRIME_CHEM_PER_DOSE);
    if (!RW_CrimeConsume(playerid, ITEM_CHEM, CRIME_CHEM_PER_DOSE))
        return RW_Error(playerid, "Не удалось взять прекурсоры.");
    gCrafting[playerid] = true;
    TogglePlayerControllable(playerid, false);
    SetTimerEx("RW_CrimeCraftDone", CRIME_CRAFT_SECONDS * 1000, false, "i", playerid);
    if (random(100) < 30) {
        new Float:x, Float:y, Float:z, nick[MAX_PLAYER_NAME], message[180];
        GetPlayerPos(playerid, x, y, z);
        RW_GetNick(playerid, nick);
        format(message, sizeof message, "Наводка: в лаборатории (%d, %d) варят Green. Кто-то из своих: %s.", floatround(x), floatround(y), nick);
        for (new p; p < MAX_PLAYERS; p++)
            if (IsPlayerConnected(p) && gLogged[p] && RW_PoliceOnDuty(p) && RW_PoliceBadge(p) > 0) RW_Error(p, message);
    }
    new message[160];
    format(message, sizeof message, "Варка началась: %d секунд. Не отходите.", CRIME_CRAFT_SECONDS);
    return RW_Ok(playerid, message);
}
public RW_CrimeCraftDone(playerid)
{
    if (!IsPlayerConnected(playerid) || !gCrafting[playerid]) return 1;
    gCrafting[playerid] = false;
    TogglePlayerControllable(playerid, true);
    if (RW_CrimeGive(playerid, ITEM_GREEN, 1) <= 0) {
        RW_CrimeGive(playerid, ITEM_CHEM, CRIME_CHEM_PER_DOSE);
        return RW_Error(playerid, "В инвентаре нет места. Прекурсоры вернули.");
    }
    RW_CrimeWantedAdd(playerid, 1, CRIME_REASON_DRUGS);
    return RW_Ok(playerid, "Готово: доза Green. Розыск +1.");
}
stock RW_CrimeLabNear(playerid)
{
    for (new i; i < sizeof gCrimeLab; i++)
        if (IsPlayerInRangeOfPoint(playerid, CRIME_SELL_RANGE, gCrimeLab[i][0], gCrimeLab[i][1], gCrimeLab[i][2])) return i;
    return -1;
}
stock RW_CrimeBuyerNear(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, CRIME_SELL_RANGE, gCrimeBuyer[0], gCrimeBuyer[1], gCrimeBuyer[2]) ? 1 : 0;
}
/* Сколько штук такого типа лежит в инвентаре. */
stock RW_CrimeCount(playerid, type)
{
    new total;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == type) total += gInv[playerid][i][iiAmount];
    return total;
}
/* Списание предмета из инвентаря: удаляет строки из базы и память. */
stock RW_CrimeConsume(playerid, type, amount)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid] || amount <= 0) return 0;
    if (RW_CrimeCount(playerid, type) < amount) return 0;
    new left = amount, query[160];
    for (new i; i < INV_SLOTS && left > 0; i++) {
        if (gInv[playerid][i][iiType] != type) continue;
        new have = gInv[playerid][i][iiAmount];
        if (have <= 0) continue;
        if (have <= left) {
            format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][i][iiUid], gAccountId[playerid], i);
            if (!RW_InvWrite(query)) return 0;
            for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][i][j] = 0;
            left -= have;
        } else {
            format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d", have - left, gInv[playerid][i][iiUid]);
            if (!RW_InvWrite(query)) return 0;
            gInv[playerid][i][iiAmount] = have - left;
            left = 0;
        }
    }
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return left == 0;
}

/*  --------------------------------- наркота ------------------------------- */

stock RW_GreenCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    if (RW_CrimeCount(playerid, ITEM_GREEN) < 1) return RW_Error(playerid, "Нет ни одной дозы Green.");
    if (GetTickCount() - gCrimeTick[playerid] < 2000) return 1;
    gCrimeTick[playerid] = GetTickCount();
    new now = gettime(), doses;
    for (new i; i <= CRIME_GREEN_DOSES; i++) if (now - gDrugDose[playerid][i] < CRIME_GREEN_MINUTES * 60) doses++;
    if (doses >= CRIME_GREEN_DOSES) {
        /* Передоз: отключка вместо лечения. */
        new Float:x, Float:y, Float:z;
        GetPlayerPos(playerid, x, y, z);
        RW_CrimeConsume(playerid, ITEM_GREEN, 1);
        RW_CrimeWantedAdd(playerid, 1, CRIME_REASON_DRUGS);
        SetPlayerDrunkLevel(playerid, 50000);
        new health;
        GetPlayerHealth(playerid, health);
        SetPlayerHealth(playerid, 5.0);
        #pragma unused health
        return RW_MedDown(playerid, x, y, z);
    }
    RW_CrimeConsume(playerid, ITEM_GREEN, 1);
    for (new i; i < CRIME_GREEN_DOSES; i++) gDrugDose[playerid][i] = gDrugDose[playerid][i + 1];
    gDrugDose[playerid][CRIME_GREEN_DOSES] = now;
    gDrugUntil[playerid] = now + CRIME_GREEN_EFFECT;
    new Float:health;
    GetPlayerHealth(playerid, health);
    if (health < 100.0) {
        health += CRIME_GREEN_HEAL;
        if (health > 100.0) health = 100.0;
        SetPlayerHealth(playerid, health);
    }
    SetPlayerDrunkLevel(playerid, 20000);
    return RW_Ok(playerid, "Green пошёл: силы вернутся за минуту. Часто — нельзя, отключка.");
}
stock RW_SellDrugsCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!RW_CrimeBuyerNear(playerid)) return RW_Error(playerid, "Скупщик ждёт в Гантоне, у магазина на 2244, -1665.");
    new amount = RW_CrimeCount(playerid, ITEM_GREEN);
    if (amount < 1) return RW_Error(playerid, "Нечего продавать.");
    if (GetTickCount() - gCrimeTick[playerid] < 2000) return 1;
    gCrimeTick[playerid] = GetTickCount();
    new price = CRIME_SELL_MIN + random(CRIME_SELL_MAX - CRIME_SELL_MIN + 1);
    new total = price * amount;
    if (!RW_CrimeConsume(playerid, ITEM_GREEN, amount)) return RW_Error(playerid, "Не удалось забрать дозы.");
    RW_AddMoney(playerid, total);
    RW_StoryEarned(playerid, total);
    new money[32], message[180];
    RW_FormatMoney(total, money, sizeof money);
    format(message, sizeof message, "Продано %d доз по %d: %s наличными.", amount, price, money);
    RW_Ok(playerid, message);
    /* Полиция рядом — скупщик под колпаком. */
    for (new p; p < MAX_PLAYERS; p++)
        if (IsPlayerConnected(p) && p != playerid && RW_PoliceOnDuty(p) && RW_PoliceBadge(p) > 0
            && IsPlayerInRangeOfPoint(p, 60.0, gCrimeBuyer[0], gCrimeBuyer[1], gCrimeBuyer[2])) {
            RW_CrimeWantedAdd(playerid, 1, CRIME_REASON_DRUGS);
            new nick[MAX_PLAYER_NAME], note[160];
            RW_GetNick(playerid, nick);
            format(note, sizeof note, "У скупщика в Гантоне торгуют Green: %s.", nick);
            RW_Error(p, note);
            break;
        }
    return 1;
}

/*  ---------------------------------- таймер ------------------------------- */

public RW_CrimePulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gLogged[p]) continue;
        if (gDrugUntil[p] > 0 && gettime() < gDrugUntil[p]) {
            new Float:health;
            GetPlayerHealth(p, health);
            if (health > 0.0 && health < 100.0 && !gDowned[p]) {
                health += CRIME_GREEN_REGEN;
                if (health > 100.0) health = 100.0;
                SetPlayerHealth(p, health);
            }
        } else if (gDrugUntil[p] > 0) {
            gDrugUntil[p] = 0;
            SetPlayerDrunkLevel(p, 0);
        }
        if (gWantedStars[p] > 0 && gettime() - gWantedStamp[p] >= CRIME_WANTED_DECAY) {
            RW_CrimeWantedSet(p, gWantedStars[p] - 1, gWantedReason[p]);
        }
    }
    return 1;
}

/*  --------------------------------- панель -------------------------------- */

stock RW_CrimeRowOut(playerid, row, const text[], const arg[], const icon[])
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefRow(playerid, row, text, arg, icon); return 1; }
    #else
        #pragma unused arg, icon
    #endif
    new line[160];
    format(line, sizeof line, "%s / %s", text, arg);
    RW_UI_Row(playerid, row, line);
    return 1;
}
stock RW_CrimeRows(playerid)
{
    new kind, index, near = RW_CrimeNear(playerid, kind, index);
    new row[96], arg[96], heat[24];
    RW_CrimeWantedText(gWantedStars[playerid], heat, sizeof heat);
    if (near >= 0) format(row, sizeof row, "Цель рядом: %s", gCrimeName[kind]);
    else format(row, sizeof row, "Цели рядом нет");
    if (near >= 0 && gettime() < gCrimeStamp[kind][index]) format(arg, sizeof arg, "уже брали, отдыхает");
    else if (near >= 0) format(arg, sizeof arg, "%d сек, до %d человек", gCrimeSeconds[kind], (kind == CRIME_KIND_BANK) ? (CRIME_CREW_BANK) : (CRIME_CREW_SHOP));
    else format(arg, sizeof arg, "магазин, пакгауз, терминал");
    RW_CrimeRowOut(playerid, 0, row, arg, "target");
    format(row, sizeof row, "Розыск: %s", heat);
    format(arg, sizeof arg, "спадает за 10 минут");
    RW_CrimeRowOut(playerid, 1, row, arg, "law");
    format(row, sizeof row, "Прекурсоры: %d шт.", RW_CrimeCount(playerid, ITEM_CHEM));
    format(arg, sizeof arg, "из них варится Green (%d на дозу)", CRIME_CHEM_PER_DOSE);
    RW_CrimeRowOut(playerid, 2, row, arg, "box");
    format(row, sizeof row, "Green: %d доз", RW_CrimeCount(playerid, ITEM_GREEN));
    format(arg, sizeof arg, "скупщик в Гантоне: 700-1400 за дозу");
    RW_CrimeRowOut(playerid, 3, row, arg, "box");
    format(row, sizeof row, "Лаборатория: %s", (RW_CrimeLabNear(playerid) >= 0) ? ("вы на месте") : ("амбар или порт"));
    format(arg, sizeof arg, "варка 30 секунд");
    RW_CrimeRowOut(playerid, 4, row, arg, "gear");
    return 1;
}
stock RW_CrimeOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new subtitle[120], heat[24];
    RW_CrimeWantedText(gWantedStars[playerid], heat, sizeof heat);
    format(subtitle, sizeof subtitle, "Розыск: %s. Цель, варка и сбыт — по строкам ниже", heat);
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_CRIME, "Криминал", subtitle)) {
            RW_CrimeRows(playerid);
            RW_CefAction(playerid, 6, "Начать дело", "");
            if (RW_CrimeCount(playerid, ITEM_GREEN) > 0) RW_CefAction(playerid, 7, "Принять дозу", "ok");
            RW_CefAction(playerid, 8, "Закрыть", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_CRIME, "Криминал", subtitle);
    RW_CrimeRows(playerid);
    RW_UI_Row(playerid, 6, "Начать дело");
    RW_UI_Row(playerid, 7, "Закрыть");
    return 1;
}
stock RW_CrimeAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 0) {
        new kind, index;
        if (RW_CrimeNear(playerid, kind, index) < 0) return RW_Error(playerid, "Рядом нет цели: магазин 24/7, пакгауз в порту или банковский терминал.");
        return RW_CrimeHeist(playerid);
    }
    if (row == 6) return RW_CrimeHeist(playerid);
    if (row == 4) return RW_CraftCommand(playerid);
    if (row == 3) return RW_SellDrugsCommand(playerid);
    if (cef && row == 7) return RW_GreenCommand(playerid);
    if (row == 2) return RW_CraftCommand(playerid);
    if (row == 1) return RW_WantedCommand(playerid, "");
    return RW_CrimeOpen(playerid);
}
stock RW_CrimeClick(playerid, row) { return RW_CrimeAction(playerid, row, false); }
stock RW_CrimeCefClick(playerid, id) { return RW_CrimeAction(playerid, id, true); }
stock RW_CrimePanel(playerid)
{
    if (!RW_CefOn(playerid)) RW_Info(playerid, "Криминал: /heist — ограбление, /craft — варка, /green — доза, /selldrugs — сбыт, /wanted — розыск.");
    return RW_CrimeOpen(playerid);
}
