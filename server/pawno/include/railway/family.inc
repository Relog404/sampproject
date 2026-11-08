/*  ============================================================================
 *  railway/family.inc
 *  Семьи: общий гараж, склад старших и территории.
 *
 *  Семья — это не чат, а хозяйство: у неё своя касса, свой склад предметов
 *  (ключи от общих машин и всё, что решили сдать) и свои территории, которые
 *  приносят деньги каждые десять минут. Всё лежит в SQLite, поэтому семья
 *  живёт и после рестарта, и когда лидер офлайн.
 *
 *  Машина передаётся ключом: ключ кладётся на склад семьи, а любой член семьи
 *  берёт его оттуда себе в инвентарь. Так гараж, замки и угон остаются теми
 *  же, что у личной машины.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_family_included
    #endinput
#endif
#define _railway_family_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>

/* FAMILY_NONE и gFamily живут в core.inc: экономика считает долю семьи. */
#define FAMILY_RANK_MEMBER  0
#define FAMILY_RANK_SENIOR  1
#define FAMILY_RANK_LEADER  2
#define FAMILY_MAX_NAME     24
#define FAMILY_CREATE_COST  50000
#define FAMILY_SLOTS        40      /* предметов на складе */
#define FAMILY_ZONES        4
#define FAMILY_ZONE_RANGE   10.0
#define FAMILY_CAPTURE      120     /* секунд на захват */
#define FAMILY_INCOME       150     /* с территории за раз */
#define FAMILY_PAY_PERIOD   600     /* раз в десять минут */
#define FAMILY_ROWS         6
#define FAMILY_MODE_MAIN    0       /* главная: семья, касса, склад     */
#define FAMILY_MODE_STORAGE 1
#define FAMILY_MODE_MEMBERS 2
#define FAMILY_MODE_MEMBER  3
#define FAMILY_MEMBER_ROWS  5
       /* склад: вещи и «взять»            */

/* Реальные точки: районы Лос-Сантоса, за которые идут войны. */
static const Float:gFamilyZone[FAMILY_ZONES][3] = {
    {  2244.4, -1665.4, 13.5 },     /* Гантон             */
    {  2503.4, -1722.4, 13.4 },     /* Лас-Колинас        */
    {  1595.5, -2235.9, 13.5 },     /* Оушен-Докс         */
    {  1352.4, -1759.4, 13.5 }      /* Даунтаун           */
};
static const gFamilyZoneName[FAMILY_ZONES][] = {
    "Гантон", "Лас-Колинас", "Оушен-Докс", "Даунтаун"
};
static const gFamilyRankName[3][] = { "боец", "старший", "лидер" };

new gFamilyRank[MAX_PLAYERS];
new gFamilyInvite[MAX_PLAYERS];
new gFamilyCash[MAX_PLAYERS];
new gFamilyCapture[MAX_PLAYERS];
new gFamilyZoneStamp;
new gFamilyTick[MAX_PLAYERS];
new gFamilyMode[MAX_PLAYERS];        /* главная или склад                 */
new gFamilyPage[MAX_PLAYERS];
new gFamilyPick[MAX_PLAYERS];
        /* страница склада                   */
new gFamilyIds[MAX_PLAYERS][FAMILY_ROWS]; /* вещи на складе: что лежит на строке */

forward RW_FamilyPulse();
forward RW_FamilyClick(playerid, row);
forward RW_FamilyCefClick(playerid, id);

/*  --------------------------------- схема --------------------------------- */

stock RW_FamilySchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS families (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, leader INTEGER NOT NULL, cash INTEGER NOT NULL DEFAULT 0, stamp INTEGER NOT NULL)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS family_members (family INTEGER NOT NULL, player_id INTEGER NOT NULL, rank INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(family,player_id))")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS family_items (id INTEGER PRIMARY KEY AUTOINCREMENT, family INTEGER NOT NULL, type INTEGER NOT NULL, data INTEGER NOT NULL DEFAULT 0, amount INTEGER NOT NULL DEFAULT 1)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS family_zones (zone INTEGER PRIMARY KEY, family INTEGER NOT NULL DEFAULT 0, stamp INTEGER NOT NULL)")) return 0;
    return 1;
}
stock RW_FamilyReset(playerid)
{
    gFamily[playerid] = FAMILY_NONE;
    gFamilyRank[playerid] = FAMILY_RANK_MEMBER;
    gFamilyInvite[playerid] = FAMILY_NONE;
    gFamilyCash[playerid] = 0;
    gFamilyCapture[playerid] = 0;
    gFamilyTick[playerid] = 0;
    gFamilyMode[playerid] = FAMILY_MODE_MAIN;
    gFamilyPage[playerid] = 0;
    gFamilyPick[playerid] = 0;
    return 1;
}
stock RW_FamilyLoad(playerid)
{
    gFamily[playerid] = FAMILY_NONE;
    gFamilyRank[playerid] = FAMILY_RANK_MEMBER;
    if (gAccountId[playerid] <= 0) return 0;
    new query[200];
    format(query, sizeof query, "SELECT family,rank FROM family_members WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) {
        gFamily[playerid] = db_get_field_assoc_int(r, "family");
        gFamilyRank[playerid] = db_get_field_assoc_int(r, "rank");
    }
    db_free_result(r);
    if (gFamily[playerid] > 0) gFamilyCash[playerid] = RW_FamilyMoney(gFamily[playerid]);
    return 1;
}
stock RW_FamilyField(family, const field[])
{
    if (family <= 0) return -1;
    new query[200], value;
    format(query, sizeof query, "SELECT %s AS v FROM families WHERE id=%d", field, family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return -1;
    if (db_num_rows(r) == 1) value = db_get_field_assoc_int(r, "v");
    db_free_result(r);
    return value;
}
stock RW_FamilyMoney(family)
{
    new money = RW_FamilyField(family, "cash");
    return money > 0 ? money : 0;
}
stock RW_FamilyName(family, dest[], size = sizeof dest)
{
    dest[0] = EOS;
    if (family <= 0) return 0;
    new query[200];
    format(query, sizeof query, "SELECT name FROM families WHERE id=%d", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) db_get_field_assoc(r, "name", dest, size);
    db_free_result(r);
    return 1;
}
stock RW_FamilyMembers(family)
{
    if (family <= 0) return 0;
    new query[200], total;
    format(query, sizeof query, "SELECT COUNT(*) AS n FROM family_members WHERE family=%d", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) total = db_get_field_assoc_int(r, "n");
    db_free_result(r);
    return total;
}
stock RW_FamilyZones(family)
{
    if (family <= 0) return 0;
    new query[200], total;
    format(query, sizeof query, "SELECT COUNT(*) AS n FROM family_zones WHERE family=%d", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) total = db_get_field_assoc_int(r, "n");
    db_free_result(r);
    return total;
}
stock RW_FamilyAddMoney(family, amount)
{
    if (family <= 0 || amount == 0) return 0;
    new query[220];
    format(query, sizeof query, "UPDATE families SET cash=cash+%d WHERE id=%d", amount, family);
    RW_DB_Exec(query);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gFamily[p] == family) gFamilyCash[p] = RW_FamilyMoney(family);
    return 1;
}
stock bool:RW_FamilyTakeMoney(family, amount)
{
    if (family <= 0 || amount <= 0) return false;
    if (RW_FamilyMoney(family) < amount) return false;
    new query[220];
    format(query, sizeof query, "UPDATE families SET cash=cash-%d WHERE id=%d", amount, family);
    RW_DB_Exec(query);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gFamily[p] == family) gFamilyCash[p] = RW_FamilyMoney(family);
    return true;
}

/*  -------------------------------- создание ------------------------------- */

/* Зарегистрировать семью: и /fcreate <название>, и поле ввода в панели. */
stock RW_FamilyCreateNamed(playerid, const name[])
{
    if (gFamily[playerid] != FAMILY_NONE) {
        if (RW_CefOn(playerid)) return RW_Error(playerid, "Вы уже в семье: выйти можно строкой в панели семьи.");
        return RW_Error(playerid, "Вы уже в семье: сначала /fleave.");
    }
    if (strlen(name) < 3) return RW_Error(playerid, "Слишком короткое название: хотя бы три буквы.");
    if (!RW_SafeSqlText(name)) return RW_Error(playerid, "В названии только буквы, цифры и пробелы.");
    if (gMoney[playerid] < FAMILY_CREATE_COST) return RW_Error(playerid, "Нужно 50 000 наличными на регистрацию семьи.");
    new query[320], safe[FAMILY_MAX_NAME];
    RW_PoliceSqlName(name, safe, sizeof safe);
    format(query, sizeof query, "SELECT id FROM families WHERE name='%s'", safe);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) { db_free_result(r); return RW_Error(playerid, "Такое название уже занято."); }
        db_free_result(r);
    }
    if (!RW_TakeMoney(playerid, FAMILY_CREATE_COST)) return RW_Error(playerid, "Не хватает наличных.");
    format(query, sizeof query, "INSERT INTO families(name,leader,cash,stamp) VALUES('%s',%d,0,%d)", safe, gAccountId[playerid], gettime());
    RW_DB_Exec(query);
    new family;
    r = db_query(gDB, "SELECT last_insert_rowid() AS id");
    if (r != DBResult:0) { if (db_num_rows(r) == 1) family = db_get_field_assoc_int(r, "id"); db_free_result(r); }
    if (family <= 0) return RW_Error(playerid, "Не удалось зарегистрировать семью.");
    format(query, sizeof query, "INSERT OR REPLACE INTO family_members(family,player_id,rank) VALUES(%d,%d,%d)", family, gAccountId[playerid], FAMILY_RANK_LEADER);
    RW_DB_Exec(query);
    gFamily[playerid] = family;
    gFamilyRank[playerid] = FAMILY_RANK_LEADER;
    gFamilyCash[playerid] = 0;
    new message[180];
    format(message, sizeof message, "Семья «%s» зарегистрирована. Вы — лидер. Зовите людей: /finvite <id>.", name);
    return RW_Ok(playerid, message);

}

stock RW_FamilyCreate(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!params[0]) return RW_Info(playerid, "Использование: /fcreate <название>. Стоит 50 000 наличными.");
    return RW_FamilyCreateNamed(playerid, params);
}

/*  --------------------------------- состав -------------------------------- */

stock RW_FamilyInvite(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error(playerid, "Звать в семью могут лидер и старшие.");
    if (!params[0]) return RW_Info(playerid, "Использование: /finvite <id игрока>.");
    return RW_FamilyInviteId(playerid, strval(params));
}
stock RW_FamilyJoin(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] != FAMILY_NONE) return RW_Error(playerid, "Вы уже в семье.");
    if (gFamilyInvite[playerid] == FAMILY_NONE) {
        if (RW_CefOn(playerid)) return RW_Error(playerid, "Вас никто не звал: пусть лидер или старший пригласит строкой в панели семьи.");
        return RW_Error(playerid, "Вас никто не звал: пусть лидер или старший напишет /finvite <id>.");
    }
    new family = gFamilyInvite[playerid];
    new query[200];
    format(query, sizeof query, "INSERT OR REPLACE INTO family_members(family,player_id,rank) VALUES(%d,%d,%d)", family, gAccountId[playerid], FAMILY_RANK_MEMBER);
    RW_DB_Exec(query);
    gFamily[playerid] = family;
    gFamilyRank[playerid] = FAMILY_RANK_MEMBER;
    gFamilyInvite[playerid] = FAMILY_NONE;
    gFamilyCash[playerid] = RW_FamilyMoney(family);
    new name[FAMILY_MAX_NAME], nick[MAX_PLAYER_NAME], message[180];
    RW_FamilyName(family, name, sizeof name);
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s вступил в семью «%s».", nick, name);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gFamily[p] == family && gLogged[p]) RW_Info(p, message);
    format(message, sizeof message, "Добро пожаловать в семью «%s». Гараж: /fgarage, склад: /fstore.", name);
    return RW_Ok(playerid, message);
}
stock RW_FamilyLeave(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new family = gFamily[playerid];
    if (family == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (gFamilyRank[playerid] == FAMILY_RANK_LEADER && RW_FamilyMembers(family) > 1)
        return RW_Error(playerid, "Лидер не уходит первым: передайте пост через /frank <id> 2.");
    new query[200];
    format(query, sizeof query, "DELETE FROM family_members WHERE family=%d AND player_id=%d", family, gAccountId[playerid]);
    RW_DB_Exec(query);
    gFamily[playerid] = FAMILY_NONE;
    gFamilyRank[playerid] = FAMILY_RANK_MEMBER;
    gFamilyCash[playerid] = 0;
    return RW_Ok(playerid, "Вы вышли из семьи.");
}
stock RW_FamilyKick(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error(playerid, "Выгонять могут лидер и старшие.");
    if (!params[0]) return RW_Info(playerid, "Использование: /fkick <id игрока>.");
    new target = strval(params);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return RW_Error(playerid, "Нет такого игрока.");
    if (gFamily[target] != gFamily[playerid]) return RW_Error(playerid, "Этот игрок не в вашей семье.");
    if (gFamilyRank[target] >= gFamilyRank[playerid]) return RW_Error(playerid, "Равного или старше себя не выгоняют.");
    new query[200];
    format(query, sizeof query, "DELETE FROM family_members WHERE family=%d AND player_id=%d", gFamily[playerid], gAccountId[target]);
    RW_DB_Exec(query);
    gFamily[target] = FAMILY_NONE;
    gFamilyRank[target] = FAMILY_RANK_MEMBER;
    RW_Info(target, "Вас выгнали из семьи.");
    return RW_Ok(playerid, "Игрок исключён из семьи.");
}
stock RW_FamilyRank(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamilyRank[playerid] != FAMILY_RANK_LEADER) return RW_Error(playerid, "Ранги меняет только лидер.");
    new id[16], rank[16];
    RW_MarketArgs(params, id, sizeof id, rank, sizeof rank, rank, sizeof rank);
    if (!id[0] || !rank[0]) return RW_Info(playerid, "Использование: /frank <id игрока> <0 боец, 1 старший, 2 лидер>.");
    new target = strval(id), value = strval(rank);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return RW_Error(playerid, "Нет такого игрока.");
    if (gFamily[target] != gFamily[playerid]) return RW_Error(playerid, "Этот игрок не в вашей семье.");
    if (value < 0 || value > FAMILY_RANK_LEADER) return RW_Error(playerid, "Ранг: 0 боец, 1 старший, 2 лидер.");
    if (value == FAMILY_RANK_LEADER) {
        new query[200];
        format(query, sizeof query, "UPDATE family_members SET rank=%d WHERE family=%d AND rank=%d", FAMILY_RANK_SENIOR, gFamily[playerid], FAMILY_RANK_LEADER);
        RW_DB_Exec(query);
        gFamilyRank[playerid] = FAMILY_RANK_SENIOR;
        for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gFamily[p] == gFamily[playerid] && gFamilyRank[p] == FAMILY_RANK_LEADER) gFamilyRank[p] = FAMILY_RANK_SENIOR;
        RW_DB_Exec(query);
    }
    new query[200];
    format(query, sizeof query, "UPDATE family_members SET rank=%d WHERE family=%d AND player_id=%d", value, gFamily[playerid], gAccountId[target]);
    RW_DB_Exec(query);
    gFamilyRank[target] = value;
    new nick[MAX_PLAYER_NAME], message[180];
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s теперь %s семьи.", nick, gFamilyRankName[value]);
    return RW_Ok(playerid, message);
}

/*  ---------------------------------- склад -------------------------------- */

stock RW_FamilyItemCount(family)
{
    if (family <= 0) return 0;
    new query[200], total;
    format(query, sizeof query, "SELECT COUNT(*) AS n FROM family_items WHERE family=%d", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) total = db_get_field_assoc_int(r, "n");
    db_free_result(r);
    return total;
}
/* Кладёт предмет на склад напрямую: так семья получает долю закладчика. */
stock RW_FamilyStash(family, type, data, amount)
{
    if (family <= 0 || type <= 0 || amount < 1) return 0;
    if (RW_FamilyItemCount(family) >= FAMILY_SLOTS) return 0;
    new query[260];
    format(query, sizeof query, "INSERT INTO family_items(family,type,data,amount) VALUES(%d,%d,%d,%d)", family, type, data, amount);
    return RW_DB_Exec(query);
}
/* Кладёт предмет со слота инвентаря на склад семьи. */
/* Положить вещь на склад семьи: и /fstore <слот>, и кнопка в карточке вещи. */
stock RW_FamilyStoreSlot(playerid, slot)
{
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error(playerid, "На склад сдают лидер и старшие.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    if (slot < 0 || slot >= INV_SLOTS) return RW_Error(playerid, "Нет такого слота: слоты с 0 до 79.");
    new type = gInv[playerid][slot][iiType], data = gInv[playerid][slot][iiData], amount = gInv[playerid][slot][iiAmount];
    if (type <= 0 || amount < 1) return RW_Error(playerid, "В этом слоте пусто.");
    if (gInvEquipped[playerid] == slot) return RW_Error(playerid, "Сначала снимите предмет.");
    if (!RW_InvCanRemove(playerid, slot)) return 1;
    if (RW_FamilyItemCount(gFamily[playerid]) >= FAMILY_SLOTS) return RW_Error(playerid, "Склад заполнен: 40 предметов.");
    new query[260];
    format(query, sizeof query, "INSERT INTO family_items(family,type,data,amount) VALUES(%d,%d,%d,%d)", gFamily[playerid], type, data, amount);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось положить предмет.");
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось снять предмет с инвентаря.");
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    if (RW_CefOn(playerid)) return RW_Ok(playerid, "Предмет ушёл на склад семьи: забрать его — нажатием на строку склада.");
    return RW_Ok(playerid, "Предмет ушёл на склад семьи. Забрать: /ftake <номер>, список: /fstore list.");

}

stock RW_FamilyStore(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!params[0]) return RW_Info(playerid, "Использование: /fstore <слот инвентаря>. Забрать: /ftake <номер>.");
    return RW_FamilyStoreSlot(playerid, strval(params));
}
/* Взять вещь со склада: клик передаёт номер строки из базы, команда —
   порядковый номер из списка, но делает это одно и то же. */
stock RW_FamilyTakeId(playerid, id)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    if (id <= 0) return RW_Error(playerid, "Нет такого предмета.");
    new query[260];
    format(query, sizeof query, "SELECT type,data,amount FROM family_items WHERE id=%d AND family=%d", id, gFamily[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Error(playerid, "Этого предмета уже нет на складе.");
    if (!db_num_rows(r)) { db_free_result(r); return RW_Error(playerid, "Этого предмета уже нет на складе."); }
    new type = db_get_field_assoc_int(r, "type"), data = db_get_field_assoc_int(r, "data");
    new amount = db_get_field_assoc_int(r, "amount");
    db_free_result(r);
    if (type <= 0 || amount < 1) return RW_Error(playerid, "Этого предмета уже нет на складе.");
    if (RW_FamilyGrant(playerid, type, data, amount) <= 0) return RW_Error(playerid, "В инвентаре нет места.");
    format(query, sizeof query, "DELETE FROM family_items WHERE id=%d AND family=%d", id, gFamily[playerid]);
    RW_DB_Exec(query);
    return RW_Ok(playerid, "Предмет взят со склада семьи.");
}

stock RW_FamilyTakeItem(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (!params[0]) return RW_FamilyStorageList(playerid);
    new row = strval(params);
    new query[260];
    format(query, sizeof query, "SELECT id FROM family_items WHERE family=%d ORDER BY id", gFamily[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Error(playerid, "Склад пуст.");
    new rows = db_num_rows(r), id;
    for (new i; i < rows; i++) {
        id = db_get_field_assoc_int(r, "id");
        if (i == row - 1) break;
        db_next_row(r);
    }
    db_free_result(r);
    if (row < 1 || row > rows || id <= 0) return RW_Error(playerid, "Нет такого предмета.");
    return RW_FamilyTakeId(playerid, id);
}
/* Выдача со склада: пачки патронов и прекурсоров не теряют количество. */
stock RW_FamilyGrant(playerid, type, data, amount)
{
    new uid = RW_InvGrant(playerid, type, data, amount);
    if (uid <= 0) return 0;
    if (amount > 1) {
        new query[160];
        format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d", amount, uid);
        if (!RW_InvWrite(query)) return uid;
        new slot = RW_InvFind(playerid, uid);
        if (slot >= 0) gInv[playerid][slot][iiAmount] = amount;
    }
    return uid;
}
stock RW_FamilyStorageList(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new family = gFamily[playerid];
    if (family == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    new query[260];
    format(query, sizeof query, "SELECT id,type,data,amount FROM family_items WHERE family=%d ORDER BY id", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "Склад семьи пуст.");
    new rows = db_num_rows(r);
    if (!rows) { db_free_result(r); return RW_Info(playerid, "Склад семьи пуст."); }
    if (RW_CefOn(playerid)) RW_Accent(playerid, "Склад семьи (взять: нажмите на вещь в панели):");
    else RW_Accent(playerid, "Склад семьи (взять: /ftake <номер>):");
    for (new i; i < rows; i++) {
        new id = db_get_field_assoc_int(r, "id"), type = db_get_field_assoc_int(r, "type");
        new data = db_get_field_assoc_int(r, "data"), amount = db_get_field_assoc_int(r, "amount");
        new item[40], line[120];
        RW_MarketItemName(type, data, item, sizeof item);
        format(line, sizeof line, "%d. %s x%d", i + 1, item, amount);
        SendClientMessage(playerid, COLOR_GREY, line);
        #pragma unused id
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}

/*  ------------------------------- общий гараж ----------------------------- */

/* Машина передаётся ключом: ключ от текущей машины уходит на склад семьи. */
stock RW_FamilyGiveCar(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    new slot = RW_FamilyKeySlot(playerid);
    if (slot < 0) return RW_Error(playerid, "Возьмите ключи от своей машины в руки: ключ лежит в инвентаре.");
    new type = gInv[playerid][slot][iiType], data = gInv[playerid][slot][iiData];
    if (RW_FamilyItemCount(gFamily[playerid]) >= FAMILY_SLOTS) return RW_Error(playerid, "Склад заполнен: 40 предметов.");
    new query[260];
    format(query, sizeof query, "INSERT INTO family_items(family,type,data,amount) VALUES(%d,%d,%d,1)", gFamily[playerid], type, data);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось передать ключи.");
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return RW_Error(playerid, "Не удалось снять ключи с инвентаря.");
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    if (RW_CefOn(playerid)) return RW_Ok(playerid, "Машина передана семье: ключи лежат на складе, забрать — нажатием.");
    return RW_Ok(playerid, "Машина передана семье: ключи лежат на складе. Взять: /ftake <номер>.");
}
/* Слот с ключом от машины, в которой сидит или которую держит игрок. */
stock RW_FamilyKeySlot(playerid)
{
    new vehicleid = GetPlayerVehicleID(playerid);
    new slot = RW_IsOwnedSlot(vehicleid);
    new token = 0;
    if (slot >= 0 && gVeh[slot][vsUsed]) token = gVehKeyUid[slot];
    for (new i; i < INV_SLOTS; i++) {
        if (gInv[playerid][i][iiType] != ITEM_CAR_KEY) continue;
        if (token > 0 && gInv[playerid][i][iiData] == token) return i;
        if (token == 0) return i;
    }
    return -1;
}

/*  -------------------------------- территории ----------------------------- */

stock RW_FamilyZoneNear(playerid)
{
    for (new i; i < FAMILY_ZONES; i++)
        if (IsPlayerInRangeOfPoint(playerid, FAMILY_ZONE_RANGE, gFamilyZone[i][0], gFamilyZone[i][1], gFamilyZone[i][2])) return i;
    return -1;
}
stock RW_FamilyZoneOwner(zone)
{
    if (zone < 0 || zone >= FAMILY_ZONES) return 0;
    new query[200], owner;
    format(query, sizeof query, "SELECT family FROM family_zones WHERE zone=%d", zone);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) owner = db_get_field_assoc_int(r, "family");
    db_free_result(r);
    return owner;
}
stock RW_FamilyCapture(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    new zone = RW_FamilyZoneNear(playerid);
    if (zone < 0) return RW_Error(playerid, "Встаньте на территорию: Гантон, Лас-Колинас, Оушен-Докс или Даунтаун.");
    new owner = RW_FamilyZoneOwner(zone);
    if (owner == gFamily[playerid]) return RW_Error(playerid, "Эта территория уже ваша.");
    if (gFamilyCapture[playerid] > 0) return RW_Error(playerid, "Вы уже держите территорию.");
    gFamilyCapture[playerid] = zone + 1;
    new name[FAMILY_MAX_NAME], message[180];
    RW_FamilyName(gFamily[playerid], name, sizeof name);
    format(message, sizeof message, "Захват «%s»: держите точку %d секунд. Владелец получит предупреждение.", gFamilyZoneName[zone], FAMILY_CAPTURE);
    RW_Ok(playerid, message);
    if (owner > 0) {
        new ownerName[FAMILY_MAX_NAME];
        RW_FamilyName(owner, ownerName, sizeof ownerName);
        format(message, sizeof message, "Тревога: семья «%s» забирает «%s» (%d, %d)!", name, gFamilyZoneName[zone], floatround(gFamilyZone[zone][0]), floatround(gFamilyZone[zone][1]));
        for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gLogged[p] && gFamily[p] == owner) RW_Error(p, message);
    }
    return 1;
}
/* Каждые десять минут территории приносят деньги в кассу семьи. */
public RW_FamilyPulse()
{
    /* Захват: держишь точку две минуты — территория твоя, отошёл — сорвалось. */
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || gFamilyCapture[p] <= 0) continue;
        if (RW_FamilyTick(p)) continue;
        gFamilyCapture[p] = 0;
        gFamilyTick[p] = 0;
        RW_Error(p, "Вы отошли от точки: захват сорван.");
    }
    if (gDB == DB:0 || !gDBWritable) return 1;
    new now = gettime();
    if (now - gFamilyZoneStamp < FAMILY_PAY_PERIOD) return 1;
    gFamilyZoneStamp = now;
    new DBResult:r = db_query(gDB, "SELECT zone,family FROM family_zones WHERE family>0");
    if (r == DBResult:0) return 1;
    new rows = db_num_rows(r);
    for (new i; i < rows; i++) {
        RW_FamilyAddMoney(db_get_field_assoc_int(r, "family"), FAMILY_INCOME);
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
stock RW_FamilyZonesCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    RW_Accent(playerid, "Территории Лос-Сантоса (доход 150 каждые 10 минут):");
    for (new i; i < FAMILY_ZONES; i++) {
        new owner = RW_FamilyZoneOwner(i), name[FAMILY_MAX_NAME], line[160];
        if (owner > 0) RW_FamilyName(owner, name, sizeof name);
        else format(name, sizeof name, "ничья");
        format(line, sizeof line, "%s (%d, %d) — %s", gFamilyZoneName[i], floatround(gFamilyZone[i][0]), floatround(gFamilyZone[i][1]), name);
        SendClientMessage(playerid, COLOR_GREY, line);
    }
    if (RW_CefOn(playerid)) return RW_Info(playerid, "Захват: кнопкой в панели семьи, стоя на точке 120 секунд.");
    return RW_Info(playerid, "Захват: /fcap, стоя на точке 120 секунд.");
}
/* Захват идёт в общем таймере криминала: держим точку, пока игрок рядом. */
stock RW_FamilyTick(playerid)
{
    if (gFamilyCapture[playerid] <= 0) return 1;
    new zone = gFamilyCapture[playerid] - 1;
    if (zone < 0 || zone >= FAMILY_ZONES) { gFamilyCapture[playerid] = 0; return 1; }
    if (!IsPlayerInRangeOfPoint(playerid, FAMILY_ZONE_RANGE, gFamilyZone[zone][0], gFamilyZone[zone][1], gFamilyZone[zone][2])) return 0;
    if (gFamilyTick[playerid] == 0) { gFamilyTick[playerid] = gettime() + FAMILY_CAPTURE; return 1; }
    if (gettime() < gFamilyTick[playerid]) return 1;
    gFamilyTick[playerid] = 0;
    gFamilyCapture[playerid] = 0;
    new query[220], name[FAMILY_MAX_NAME], message[180];
    format(query, sizeof query, "INSERT OR REPLACE INTO family_zones(zone,family,stamp) VALUES(%d,%d,%d)", zone, gFamily[playerid], gettime());
    RW_DB_Exec(query);
    RW_FamilyName(gFamily[playerid], name, sizeof name);
    format(message, sizeof message, "Территория «%s» теперь за семьёй «%s».", gFamilyZoneName[zone], name);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gLogged[p] && gFamily[p] == gFamily[playerid]) RW_Ok(p, message);
    return 1;
}

/*  ---------------------------------- касса -------------------------------- */

/* Снять деньги из кассы семьи: и /fcash <сумма>, и кнопка в панели. */
stock RW_FamilyCashTake(playerid, amount)
{
    if (gFamilyRank[playerid] != FAMILY_RANK_LEADER) return RW_Error(playerid, "Кассой распоряжается лидер.");
    if (amount <= 0) return RW_Error(playerid, "Сумма должна быть больше нуля.");
    if (!RW_FamilyTakeMoney(gFamily[playerid], amount)) return RW_Error(playerid, "В кассе столько нет.");
    RW_AddMoney(playerid, amount);
    new money[32], message[160];
    RW_FormatMoney(amount, money, sizeof money);
    format(message, sizeof message, "Снято из кассы: %s.", money);
    return RW_Ok(playerid, message);

}

stock RW_FamilyCashCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    new sum[16], rest[32];
    RW_MarketArgs(params, sum, sizeof sum, rest, sizeof rest, rest, sizeof rest);
    if (!sum[0]) {
        new name[FAMILY_MAX_NAME], message[200], money[32];
        RW_FamilyName(gFamily[playerid], name, sizeof name);
        RW_FormatMoney(gFamilyCash[playerid], money, sizeof money);
        format(message, sizeof message, "Касса семьи \"%s\": %s. Территорий: %d, бойцов: %d. Снять: /fcash <сумма>.", name, money, RW_FamilyZones(gFamily[playerid]), RW_FamilyMembers(gFamily[playerid]));
        return RW_Info(playerid, message);
    }
    return RW_FamilyCashTake(playerid, strval(sum));
}

/*  --------------------------------- панель -------------------------------- */

/*  ---------------------------- склад семьи -------------------------------- */
/*  Список вместо сообщений в чат: строка — вещь, нажатие забирает её. */
stock RW_FamilyStorageRows(playerid)
{
    new query[260], family = gFamily[playerid];
    format(query, sizeof query, "SELECT id,type,data,amount FROM family_items WHERE family=%d ORDER BY id LIMIT %d OFFSET %d",
        family, FAMILY_ROWS, gFamilyPage[playerid] * FAMILY_ROWS);
    /* Сначала чистим: иначе на новой странице в нижних строках останутся
       вещи с прошлой и нажатие заберёт не то. */
    for (new i; i < FAMILY_ROWS; i++) gFamilyIds[playerid][i] = 0;
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) { RW_FamilyRowOut(playerid, 0, "Склад пуст", "сдать вещь: кнопка в карточке", "info"); return 0; }
    new rows = db_num_rows(r), index, item[40], text[96], arg[32];
    for (; index < rows; index++) {
        new id = db_get_field_assoc_int(r, "id"), type = db_get_field_assoc_int(r, "type");
        new data = db_get_field_assoc_int(r, "data"), amount = db_get_field_assoc_int(r, "amount");
        gFamilyIds[playerid][index] = id;
        RW_MarketItemName(type, data, item, sizeof item);
        format(text, sizeof text, "%s x%d", item, amount);
        format(arg, sizeof arg, "взять");
        RW_FamilyRowOut(playerid, index, text, arg, "box");
        db_next_row(r);
    }
    db_free_result(r);
    if (!index) RW_FamilyRowOut(playerid, 0, "На этой странице пусто", "вернитесь: кнопка «Назад»", "info");
    return index;
}

/* Сдавать на склад могут лидер и старшие: остальным кнопку не показываем. */
stock bool:RW_FamilyCanStore(playerid)
{
    if (!gLogged[playerid]) return false;
    if (gFamily[playerid] == FAMILY_NONE) return false;
    return gFamilyRank[playerid] >= FAMILY_RANK_SENIOR;
}

/* Состав семьи в чат: строка «Бойцов» отвечает списком, а не молчанием. */
stock RW_FamilyList(playerid)
{
    new family = gFamily[playerid];
    if (family == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    new query[220];
    format(query, sizeof query,
        "SELECT m.player_id, m.rank, a.name FROM family_members m LEFT JOIN accounts a ON a.id=m.player_id WHERE m.family=%d ORDER BY m.rank DESC, a.name", family);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "Состав семьи пуст.");
    new rows = db_num_rows(r), name[MAX_PLAYER_NAME], line[120], rank[16];
    if (!rows) { db_free_result(r); return RW_Info(playerid, "Состав семьи пуст."); }
    RW_Accent(playerid, "Состав семьи:");
    for (new i; i < rows; i++) {
        db_get_field_assoc(r, "name", name, sizeof name);
        new value = db_get_field_assoc_int(r, "rank");
        if (value < 0) value = 0;
        if (value > FAMILY_RANK_LEADER) value = FAMILY_RANK_LEADER;
        format(rank, sizeof rank, "%s", gFamilyRankName[value]);
        format(line, sizeof line, "%s — %s%s", name, rank, RW_NameOnline(name, playerid) ? " (в игре)" : "");
        SendClientMessage(playerid, COLOR_GREY, line);
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}

stock RW_FamilyAskName(playerid)
{
    return RW_InputOpen(playerid, INPUT_FAMILY_NAME, 0, "Новая семья",
        "Название: буквы, цифры и пробелы, от трёх знаков", "Например: Северные");
}

stock RW_FamilyOpenStorage(playerid)
{
    gFamilyMode[playerid] = FAMILY_MODE_STORAGE;
    gFamilyPage[playerid] = 0;
    return RW_FamilyOpen(playerid);
}

/*  ------------------------------- состав -------------------------------- */
/*  Состав, ранги и исключение — строками: команды /flist, /frank и /fkick
    остались запасным путём для тех, у кого браузера нет. Решение лидера
    должно действовать и когда боец не в сети, поэтому всё считается по
    аккаунту из базы, а не по номеру игрока. */
stock RW_FamilyAccountRank(family, account)
{
    if (family <= 0 || account <= 0) return -1;
    new query[200], rank = -1;
    format(query, sizeof query, "SELECT rank FROM family_members WHERE family=%d AND player_id=%d", family, account);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) { if (db_num_rows(r) == 1) rank = db_get_field_assoc_int(r, "rank"); db_free_result(r); }
    return rank;
}
stock RW_FamilyAccountName(account, dest[], size = sizeof dest)
{
    dest[0] = EOS;
    if (account <= 0) return 0;
    new query[160];
    format(query, sizeof query, "SELECT name FROM accounts WHERE id=%d", account);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) db_get_field_assoc(r, "name", dest, size);
    db_free_result(r);
    return 1;
}
stock RW_FamilyOnline(account)
{
    if (account <= 0) return 0;
    for (new i; i < MAX_PLAYERS; i++)
        if (IsPlayerConnected(i) && gLogged[i] && gAccountId[i] == account) return 1;
    return 0;
}
stock RW_FamilyInviteId(playerid, target)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamily[playerid] == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error(playerid, "Звать в семью могут лидер и старшие.");
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Нет такого игрока.");
    if (gFamily[target] != FAMILY_NONE) return RW_Error(playerid, "Этот игрок уже в семье.");
    gFamilyInvite[target] = gFamily[playerid];
    new name[FAMILY_MAX_NAME], nick[MAX_PLAYER_NAME], message[180];
    RW_FamilyName(gFamily[playerid], name, sizeof name);
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s зовёт вас в семью «%s». Принять: /fjoin, или строка приглашения в панели семьи.", nick, name);
    RW_Info(target, message);
    format(message, sizeof message, "Приглашение отправлено. Ждите ответа: /fjoin.");
    return RW_Ok(playerid, message);
}
stock RW_FamilyRankSet(playerid, account, rank)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamilyRank[playerid] != FAMILY_RANK_LEADER) return RW_Error(playerid, "Ранги меняет только лидер.");
    new family = gFamily[playerid];
    if (family == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (account <= 0) return RW_Error(playerid, "Выберите бойца в списке.");
    if (account == gAccountId[playerid]) return RW_Error(playerid, "Свой ранг меняется передачей поста: /frank <id> 2.");
    if (rank != FAMILY_RANK_MEMBER && rank != FAMILY_RANK_SENIOR) return RW_Error(playerid, "Ранг: 0 боец, 1 старший.");
    new was = RW_FamilyAccountRank(family, account);
    if (was < 0) return RW_Error(playerid, "Этот игрок не в вашей семье.");
    if (was == FAMILY_RANK_LEADER) return RW_Error(playerid, "Лидер остаётся лидером: сначала передайте пост.");
    if (was == rank) return RW_Info(playerid, "У него уже этот ранг.");
    new query[200];
    format(query, sizeof query, "UPDATE family_members SET rank=%d WHERE family=%d AND player_id=%d", rank, family, account);
    RW_DB_Exec(query);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != account) continue;
        gFamilyRank[i] = rank;
        RW_Info(i, rank == FAMILY_RANK_SENIOR ? ("Вас назначили старшим.") : ("Вас перевели в бойцы."));
    }
    return RW_Ok(playerid, rank == FAMILY_RANK_SENIOR ? ("Боец стал старшим.") : ("Боец снова в бойцах."));
}
stock RW_FamilyKickAccount(playerid, account)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (gFamilyRank[playerid] < FAMILY_RANK_SENIOR) return RW_Error(playerid, "Выгонять могут лидер и старшие.");
    new family = gFamily[playerid];
    if (family == FAMILY_NONE) return RW_Error(playerid, "Вы не в семье.");
    if (account <= 0) return RW_Error(playerid, "Выберите бойца в списке.");
    if (account == gAccountId[playerid]) return RW_Error(playerid, "Себя выгоняют выходом из семьи.");
    new was = RW_FamilyAccountRank(family, account);
    if (was < 0) return RW_Error(playerid, "Этот игрок не в вашей семье.");
    if (was >= gFamilyRank[playerid]) return RW_Error(playerid, "Равного или старше себя не выгоняют.");
    new query[200];
    format(query, sizeof query, "DELETE FROM family_members WHERE family=%d AND player_id=%d", family, account);
    RW_DB_Exec(query);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != account) continue;
        gFamily[i] = FAMILY_NONE;
        gFamilyRank[i] = FAMILY_RANK_MEMBER;
        gFamilyCash[i] = 0;
        RW_Info(i, "Вас выгнали из семьи.");
    }
    return RW_Ok(playerid, "Боец исключён из семьи.");
}

/* Список состава: строка — боец, нажатие открывает его карточку. */
stock RW_FamilyMembersRows(playerid)
{
    new query[280], family = gFamily[playerid], index;
    for (new i; i < FAMILY_ROWS; i++) gFamilyIds[playerid][i] = 0;
    format(query, sizeof query,
        "SELECT m.player_id,m.rank,a.name FROM family_members m LEFT JOIN accounts a ON a.id=m.player_id WHERE m.family=%d ORDER BY m.rank DESC, a.name LIMIT %d OFFSET %d",
        family, FAMILY_MEMBER_ROWS, gFamilyPage[playerid] * FAMILY_MEMBER_ROWS);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows && index < FAMILY_MEMBER_ROWS; i++) {
            new name[MAX_PLAYER_NAME], label[96], arg[40], value = db_get_field_assoc_int(r, "rank");
            db_get_field_assoc(r, "name", name, sizeof name);
            if (value < 0) value = 0;
            if (value > FAMILY_RANK_LEADER) value = FAMILY_RANK_LEADER;
            gFamilyIds[playerid][index] = db_get_field_assoc_int(r, "player_id");
            format(label, sizeof label, "%s", name[0] ? name : ("Боец"));
            format(arg, sizeof arg, "%s%s", gFamilyRankName[value], RW_FamilyOnline(gFamilyIds[playerid][index]) ? (", в игре") : (""));
            RW_FamilyRowOut(playerid, index, label, arg, "users");
            index++;
            db_next_row(r);
        }
        db_free_result(r);
    }
    if (!index) RW_FamilyRowOut(playerid, 0, "В семье никого не нашлось", "проверьте состав позже", "info");
    /* Приглашать могут лидер и старшие: им и строка. */
    if (gFamilyRank[playerid] >= FAMILY_RANK_SENIOR) RW_FamilyRowOut(playerid, 5, "Пригласить бойца",
        "нажмите: номер игрока спросит поле ввода", "group");
    return index;
}

/* Карточка бойца: ранг и исключение по строке, а не командой с номером. */
stock RW_FamilyMemberRows(playerid)
{
    new account = gFamilyPick[playerid], name[MAX_PLAYER_NAME], label[96], arg[64];
    RW_FamilyAccountName(account, name, sizeof name);
    new value = RW_FamilyAccountRank(gFamily[playerid], account);
    if (value < 0) value = 0;
    if (value > FAMILY_RANK_LEADER) value = FAMILY_RANK_LEADER;
    format(label, sizeof label, "%s", name[0] ? name : ("Боец"));
    format(arg, sizeof arg, "%s%s", gFamilyRankName[value], RW_FamilyOnline(account) ? (", в игре") : (", не в сети"));
    RW_FamilyRowOut(playerid, 0, label, arg, "users");
    if (gFamilyRank[playerid] == FAMILY_RANK_LEADER) {
        RW_FamilyRowOut(playerid, 1, "Назначить старшим", "нажмите", "group");
        RW_FamilyRowOut(playerid, 2, "Сделать бойцом", "нажмите", "refresh");
    } else {
        RW_FamilyRowOut(playerid, 1, "Ранги меняет лидер", "", "info");
    }
    if (gFamilyRank[playerid] >= FAMILY_RANK_SENIOR) RW_FamilyRowOut(playerid, 3, "Выгнать из семьи", "нажмите", "alert");
    else RW_FamilyRowOut(playerid, 3, "Выгоняют лидер и старшие", "", "info");
    return 1;
}
stock RW_FamilyOpenMembers(playerid)
{
    gFamilyMode[playerid] = FAMILY_MODE_MEMBERS;
    gFamilyPage[playerid] = 0;
    return RW_FamilyOpen(playerid);
}

stock RW_FamilyRowOut(playerid, row, const text[], const arg[], const icon[])
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
stock RW_FamilyRows(playerid)
{
    new row[96], arg[96], name[FAMILY_MAX_NAME];
    if (gFamily[playerid] == FAMILY_NONE) {
        RW_FamilyRowOut(playerid, 0, "Создать семью", "нажмите: название спросит поле ввода", "group");
        RW_FamilyRowOut(playerid, 1, "Стоимость семьи", "50 000 наличными", "wallet");
        RW_FamilyRowOut(playerid, 2, "Зачем это", "гараж, склад и территории", "info");
        if (gFamilyInvite[playerid] != FAMILY_NONE) {
            RW_FamilyName(gFamilyInvite[playerid], name, sizeof name);
            format(arg, sizeof arg, "принять: семья «%s»", name);
            RW_FamilyRowOut(playerid, 3, "Вас позвали в семью", arg, "users");
        } else RW_FamilyRowOut(playerid, 3, "Приглашение", "никто не звал: ждите приглашения", "users");
        return 1;
    }
    RW_FamilyName(gFamily[playerid], name, sizeof name);
    format(row, sizeof row, "Семья «%s»", name);
    format(arg, sizeof arg, "вы — %s", gFamilyRankName[gFamilyRank[playerid]]);
    RW_FamilyRowOut(playerid, 0, row, arg, "group");
    format(row, sizeof row, "Касса: %d", gFamilyCash[playerid]);
    if (gFamilyRank[playerid] == FAMILY_RANK_LEADER) format(arg, sizeof arg, "снять: нажмите, сумму спросит поле ввода");
    else format(arg, sizeof arg, "кассой распоряжается лидер");
    RW_FamilyRowOut(playerid, 1, row, arg, "wallet");
    format(row, sizeof row, "Склад: %d из %d", RW_FamilyItemCount(gFamily[playerid]), FAMILY_SLOTS);
    format(arg, sizeof arg, "открыть: нажмите, чтобы взять вещь");
    RW_FamilyRowOut(playerid, 2, row, arg, "box");
    format(row, sizeof row, "Территорий: %d", RW_FamilyZones(gFamily[playerid]));
    format(arg, sizeof arg, "нажмите: список территорий, захват — кнопкой ниже");
    RW_FamilyRowOut(playerid, 3, row, arg, "target");
    format(row, sizeof row, "Бойцов: %d", RW_FamilyMembers(gFamily[playerid]));
    format(arg, sizeof arg, "нажмите: откроет состав, ранги и исключение");
    RW_FamilyRowOut(playerid, 4, row, arg, "users");
    format(row, sizeof row, "Выйти из семьи");
    format(arg, sizeof arg, "лидер сначала передаёт пост: /frank <id> 2");
    RW_FamilyRowOut(playerid, 5, row, arg, "alert");
    return 1;
}
stock RW_FamilyOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new subtitle[120], name[FAMILY_MAX_NAME];
    if (gFamily[playerid] != FAMILY_NONE) {
        RW_FamilyName(gFamily[playerid], name, sizeof name);
        format(subtitle, sizeof subtitle, "Семья «%s»: %d бойцов, территорий %d", name, RW_FamilyMembers(gFamily[playerid]), RW_FamilyZones(gFamily[playerid]));
    } else {
        format(subtitle, sizeof subtitle, "Семья даёт гараж, склад и территории");
    }
    if (gFamily[playerid] == FAMILY_NONE) gFamilyMode[playerid] = FAMILY_MODE_MAIN;
    if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE) format(subtitle, sizeof subtitle, "Склад семьи: страница %d", gFamilyPage[playerid] + 1);
    if (gFamilyMode[playerid] == FAMILY_MODE_MEMBERS) format(subtitle, sizeof subtitle, "Состав: страница %d", gFamilyPage[playerid] + 1);
    if (gFamilyMode[playerid] == FAMILY_MODE_MEMBER) {
        new who[MAX_PLAYER_NAME];
        RW_FamilyAccountName(gFamilyPick[playerid], who, sizeof who);
        format(subtitle, sizeof subtitle, "Боец: %s", who[0] ? who : ("не выбран"));
    }
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_FAMILY, "Семья", subtitle)) {
            if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE) {
                RW_FamilyStorageRows(playerid);
                RW_CefAction(playerid, 6, "К семье", "");
                RW_CefAction(playerid, 7, "Дальше", "");
            } else if (gFamilyMode[playerid] == FAMILY_MODE_MEMBERS) {
                RW_FamilyMembersRows(playerid);
                RW_CefAction(playerid, 6, "К семье", "");
                RW_CefAction(playerid, 7, "Дальше", "");
            } else if (gFamilyMode[playerid] == FAMILY_MODE_MEMBER) {
                RW_FamilyMemberRows(playerid);
                RW_CefAction(playerid, 6, "К составу", "");
            } else {
                RW_FamilyRows(playerid);
                RW_CefAction(playerid, 6, "Захват территории", "");
            }
            RW_CefAction(playerid, 8, "Закрыть", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_FAMILY, "Семья", subtitle);
    if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE) RW_FamilyStorageRows(playerid);
    else if (gFamilyMode[playerid] == FAMILY_MODE_MEMBERS) RW_FamilyMembersRows(playerid);
    else if (gFamilyMode[playerid] == FAMILY_MODE_MEMBER) RW_FamilyMemberRows(playerid);
    else RW_FamilyRows(playerid);
    if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE || gFamilyMode[playerid] == FAMILY_MODE_MEMBERS) RW_UI_Row(playerid, 6, "К семье");
    else if (gFamilyMode[playerid] == FAMILY_MODE_MEMBER) RW_UI_Row(playerid, 6, "К составу");
    else RW_UI_Row(playerid, 6, "Захват территории");
    RW_UI_Row(playerid, 7, "Закрыть");
    return 1;
}
stock RW_FamilyAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) { gFamilyMode[playerid] = FAMILY_MODE_MAIN; return RW_UI_Close(playerid); }
    if (gFamilyMode[playerid] == FAMILY_MODE_STORAGE) {
        if (row == 6) { gFamilyMode[playerid] = FAMILY_MODE_MAIN; return RW_FamilyOpen(playerid); }
        if (cef && row == 7) {
            new pages = (RW_FamilyItemCount(gFamily[playerid]) + FAMILY_ROWS - 1) / FAMILY_ROWS;
            if (pages < 1) pages = 1;
            gFamilyPage[playerid] = (gFamilyPage[playerid] + 1) % pages;
            return RW_FamilyOpen(playerid);
        }
        if (row < FAMILY_ROWS && gFamilyIds[playerid][row] > 0) {
            new id = gFamilyIds[playerid][row];
            gFamilyIds[playerid][row] = 0;
            RW_FamilyTakeId(playerid, id);
            return RW_FamilyOpen(playerid);
        }
        return 1;
    }
    if (gFamilyMode[playerid] == FAMILY_MODE_MEMBER) {
        if (row == 6) { gFamilyMode[playerid] = FAMILY_MODE_MEMBERS; return RW_FamilyOpen(playerid); }
        new account = gFamilyPick[playerid];
        if (account <= 0) return RW_FamilyOpen(playerid);
        if (gFamilyRank[playerid] == FAMILY_RANK_LEADER && row == 1) { RW_FamilyRankSet(playerid, account, FAMILY_RANK_SENIOR); return RW_FamilyOpen(playerid); }
        if (gFamilyRank[playerid] == FAMILY_RANK_LEADER && row == 2) { RW_FamilyRankSet(playerid, account, FAMILY_RANK_MEMBER); return RW_FamilyOpen(playerid); }
        if (row == 3) {
            RW_FamilyKickAccount(playerid, account);
            /* Выгнанного в карточке больше не показываем: возвращаемся к составу. */
            if (RW_FamilyAccountRank(gFamily[playerid], account) < 0) gFamilyMode[playerid] = FAMILY_MODE_MEMBERS;
            return RW_FamilyOpen(playerid);
        }
        return RW_FamilyOpen(playerid);
    }
    if (gFamilyMode[playerid] == FAMILY_MODE_MEMBERS) {
        if (row == 6) { gFamilyMode[playerid] = FAMILY_MODE_MAIN; return RW_FamilyOpen(playerid); }
        if (cef && row == 7) {
            new pages = (RW_FamilyMembers(gFamily[playerid]) + FAMILY_MEMBER_ROWS - 1) / FAMILY_MEMBER_ROWS;
            if (pages < 1) pages = 1;
            gFamilyPage[playerid] = (gFamilyPage[playerid] + 1) % pages;
            return RW_FamilyOpen(playerid);
        }
        if (row < FAMILY_MEMBER_ROWS && gFamilyIds[playerid][row] > 0) {
            gFamilyPick[playerid] = gFamilyIds[playerid][row];
            gFamilyMode[playerid] = FAMILY_MODE_MEMBER;
            return RW_FamilyOpen(playerid);
        }
        if (row == 5 && gFamilyRank[playerid] >= FAMILY_RANK_SENIOR)
            return RW_InputOpen(playerid, INPUT_FAMILY_ID, 0, "Пригласить в семью",
                "Номер игрока: кто стоит рядом", "Например: 3");
        return RW_FamilyOpen(playerid);
    }
    if (row == 6) return RW_FamilyCapture(playerid);
    if (row == 2 && gFamily[playerid] != FAMILY_NONE) return RW_FamilyOpenStorage(playerid);
    if (row == 3 && gFamily[playerid] != FAMILY_NONE) return RW_FamilyZonesCommand(playerid);
    if (row == 4 && gFamily[playerid] != FAMILY_NONE) return RW_FamilyOpenMembers(playerid);
    if (row == 0 && gFamily[playerid] == FAMILY_NONE) return RW_FamilyAskName(playerid);
    if (row == 1 && gFamily[playerid] != FAMILY_NONE && gFamilyRank[playerid] == FAMILY_RANK_LEADER)
        return RW_InputOpen(playerid, INPUT_FAMILY_CASH, 0, "Касса семьи",
            "Сколько снять: наличными в карман", "Например: 5000");
    if (row == 3 && gFamily[playerid] == FAMILY_NONE) return RW_FamilyJoin(playerid);
    if (cef && row == 5 && gFamily[playerid] != FAMILY_NONE) {
        RW_FamilyLeave(playerid);
        return RW_FamilyOpen(playerid);
    }
    return RW_FamilyOpen(playerid);
}
stock RW_FamilyClick(playerid, row) { return RW_FamilyAction(playerid, row, false); }
stock RW_FamilyCefClick(playerid, id) { return RW_FamilyAction(playerid, id, true); }
stock RW_FamilyPanel(playerid)
{
    if (!RW_CefOn(playerid)) RW_Info(playerid, "Семья: /fcreate, /finvite, /fjoin, /fleave, /fgive, /fstore, /ftake, /fcap, /fzones, /fcash.");
    return RW_FamilyOpen(playerid);
}
