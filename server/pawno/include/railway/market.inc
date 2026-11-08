/*  ============================================================================
 *  railway/market.inc
 *  Маркетплейс: объявления игроков, склад на 24 часа и авто-аукцион.
 *
 *  Логика целиком серверная и хранится в SQLite: предмет уходит со склада
 *  продавца в базу, деньги за него приходят на счёт в банке, поэтому сделка
 *  доживает и до перезапуска, и до выхода продавца из игры. Интерфейс один и
 *  тот же: браузер (CEF), а если плагина нет — обычные диалоги; в чате всё то
 *  же самое доступно командами.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_market_included
    #endinput
#endif
#define _railway_market_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/economy>

#define MARKET_FEE              500     /* комиссия склада за час          */
#define MARKET_MIN_HOURS        5       /* минимум: 5 часов = 2 500        */
#define MARKET_MAX_HOURS        24      /* склад держит лот сутки          */
#define MARKET_AUCTION_SECONDS  (24 * 3600)
#define MARKET_ROWS             6       /* лотов на странице               */
#define MARKET_BID_STEP         5       /* шаг ставки, процентов           */
#define MARKET_MAX_PRICE        100000000

/* status: 0 склад, 1 аукцион, 2 ждёт получения, 3 закрыт */
#define MARKET_OPEN             0
#define MARKET_AUCTION          1
#define MARKET_CLAIM            2
#define MARKET_DONE             3

new gMarketPage[MAX_PLAYERS];
new gMarketIds[MAX_PLAYERS][MARKET_ROWS];
new gMarketPick[MAX_PLAYERS];
new gMarketTick[MAX_PLAYERS];

/* Три слова из параметров команды: как /sell 4 15000 12. */
stock RW_MarketArgs(const params[], a[], asize, b[], bsize, c[], csize)
{
    new rest[144];
    RW_ParseCommand(params, a, asize, rest, sizeof rest);
    RW_ParseCommand(rest, b, bsize, rest, sizeof rest);
    RW_ParseCommand(rest, c, csize, rest, sizeof rest);
    return 1;
}

forward RW_MarketPulse();

/*  --------------------------------- схема --------------------------------- */

stock RW_MarketSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS market_lots (id INTEGER PRIMARY KEY AUTOINCREMENT, seller INT NOT NULL, seller_name TEXT NOT NULL, type INT NOT NULL, data INT NOT NULL DEFAULT 0, amount INT NOT NULL DEFAULT 1, price INT NOT NULL, bid INT NOT NULL DEFAULT 0, bidder INT NOT NULL DEFAULT 0, bidder_name TEXT NOT NULL DEFAULT '', status INT NOT NULL DEFAULT 0, claim INT NOT NULL DEFAULT 0, ends INT NOT NULL, stamp INT NOT NULL)")) return 0;
    return 1;
}
stock RW_MarketReset(playerid)
{
    gMarketPage[playerid] = 0;
    gMarketPick[playerid] = 0;
    gMarketTick[playerid] = 0;
    for (new i; i < MARKET_ROWS; i++) gMarketIds[playerid][i] = 0;
    return 1;
}

/*  ------------------------------- названия -------------------------------- */

stock RW_MarketItemName(type, data, dest[], size = sizeof dest)
{
    if (type == ITEM_BACKPACK) format(dest, size, "Рюкзак");
    else if (type == ITEM_CAR_KEY) format(dest, size, "Ключи от авто");
    else if (type == ITEM_WEAPON) RW_WeaponName(data, dest, size);
    else if (type == ITEM_AMMO) { new cname[16]; RW_CaliberName(data, cname, sizeof cname); format(dest, size, "Патроны %s", cname); }
    else if (type == ITEM_BANDAGE) format(dest, size, "Бинт");
    else if (type == ITEM_SYRINGE) format(dest, size, "Адреналин");
    else if (type == ITEM_ARMOUR) RW_ArmourName(data, dest, size);
    else if (type == ITEM_SIM) format(dest, size, "SIM-карта");
    else if (type == ITEM_PHONE) format(dest, size, "Телефон");
    else if (type == ITEM_CUFFS) format(dest, size, "Наручники");
    else if (type == ITEM_TASER) format(dest, size, "Тайзер");
    else if (type == ITEM_BATON) format(dest, size, "Дубинка");
    else if (type == ITEM_BADGE) format(dest, size, "Жетон");
    else if (type == ITEM_DEFIB) format(dest, size, "Дефибриллятор");
    else if (type == ITEM_MEDKIT) format(dest, size, "Аптечка");
    else if (type == ITEM_LOCKPICK) format(dest, size, "Отмычка");
    else if (type == ITEM_TUNING) { new pname[24]; RW_TuneName(data, pname, sizeof pname); format(dest, size, "%s (%d)", pname, data); }
    else if (type == ITEM_PHOTO) format(dest, size, "Фото");
    else if (type == ITEM_CHEM) format(dest, size, "Прекурсор");
    else if (type == ITEM_GREEN) format(dest, size, "Green");
    else format(dest, size, "Предмет");
    return 1;
}
/* Сколько часов и минут осталось лоту. */
stock RW_MarketLeft(ends, dest[], size = sizeof dest)
{
    new left = ends - gettime();
    if (left < 0) left = 0;
    new hours = left / 3600, minutes = (left % 3600) / 60;
    if (hours > 0) format(dest, size, "%d ч %02d мин", hours, minutes);
    else format(dest, size, "%d мин", minutes);
    return 1;
}
stock RW_MarketFee(hours) { return hours * MARKET_FEE; }
/* Минимальная ставка: стартовая цена или предыдущая ставка плюс шаг. */
stock RW_MarketMinBid(price, bid)
{
    new min = (bid > 0) ? (bid + (bid * MARKET_BID_STEP) / 100 + 1) : (price);
    if (min < 1) min = 1;
    return min;
}

/*  --------------------------------- деньги -------------------------------- */
/* Продавец может быть офлайн, поэтому деньги кладутся прямо на счёт в банке. */
stock RW_MarketCredit(account, amount)
{
    if (account <= 0 || amount <= 0) return 0;
    new query[220];
    format(query, sizeof query, "INSERT OR REPLACE INTO bank_accounts(player_id,balance) VALUES(%d,COALESCE((SELECT balance FROM bank_accounts WHERE player_id=%d),0)+%d)", account, account, amount);
    if (!RW_DB_Exec(query)) return 0;
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gAccountId[p] == account) gBank[p] += amount;
    return 1;
}

/*  ------------------------------- предметы -------------------------------- */

/* Снимает предмет со слота инвентаря и возвращает его uid (0 — нельзя). */
stock RW_MarketTakeItem(playerid, slot)
{
    if (slot < 0 || slot >= INV_SLOTS) return 0;
    new uid = gInv[playerid][slot][iiUid];
    if (uid <= 0) return 0;
    if (gInvEquipped[playerid] == slot) { RW_Error(playerid, "Снимите предмет, прежде чем продавать."); return 0; }
    if (!RW_InvCanRemove(playerid, slot)) return 0;
    new query[160];
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", uid, gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return 0;
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return uid;
}
/* Выдаёт предмет покупателю; для патронов возвращает и количество. */
stock RW_MarketGive(playerid, type, data, amount)
{
    new uid = RW_InvGrant(playerid, type, data, amount);
    if (uid <= 0) return 0;
    if (amount > 1) {
        new query[160];
        format(query, sizeof query, "UPDATE inventory_items SET amount=%d WHERE uid=%d", amount, uid);
        if (!RW_InvWrite(query)) return 0;
        new slot = RW_InvFind(playerid, uid);
        if (slot >= 0) gInv[playerid][slot][iiAmount] = amount;
    }
    return uid;
}

/*  -------------------------------- продажа -------------------------------- */

/* Продажа со склада: одна функция и на команду, и на клик по кнопке. */
stock RW_MarketSell(playerid, slot, price, hours)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    if (slot < 0 || slot >= INV_SLOTS) return RW_Error(playerid, "Нет такого слота: слоты с 0 до 79.");
    if (price < 1 || price > MARKET_MAX_PRICE) return RW_Error(playerid, "Цена — от 1 до 100 000 000.");
    if (hours < MARKET_MIN_HOURS || hours > MARKET_MAX_HOURS) return RW_Error(playerid, "Срок — от 5 до 24 часов, то есть от 2 500 до 12 000 комиссии.");
    new fee = RW_MarketFee(hours);
    if (gMoney[playerid] < fee) return RW_Error(playerid, "Не хватает на комиссию склада: нужно 500 за час, минимум 2 500.");
    new type = gInv[playerid][slot][iiType], data = gInv[playerid][slot][iiData], amount = gInv[playerid][slot][iiAmount];
    if (type <= 0 || amount < 1) return RW_Error(playerid, "В этом слоте пусто.");
    if (RW_MarketTakeItem(playerid, slot) <= 0) return RW_Error(playerid, "Этот предмет продать нельзя.");
    if (!RW_TakeMoney(playerid, fee)) return RW_Error(playerid, "Не хватает наличных на комиссию.");
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], query[420], item[40];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    RW_MarketItemName(type, data, item, sizeof item);
    format(query, sizeof query, "INSERT INTO market_lots(seller,seller_name,type,data,amount,price,status,ends,stamp) VALUES(%d,'%s',%d,%d,%d,%d,0,%d,%d)",
        gAccountId[playerid], safe, type, data, amount, price, gettime() + hours * 3600, gettime());
    RW_DB_Exec(query);
    new DBResult:r = db_query(gDB, "SELECT last_insert_rowid() AS id");
    new lot;
    if (r != DBResult:0) { if (db_num_rows(r) == 1) lot = db_get_field_assoc_int(r, "id"); db_free_result(r); }
    new money[32], message[200];
    RW_FormatMoney(fee, money, sizeof money);
    format(message, sizeof message, "Лот №%d: %s за %d. Комиссия %s, на складе %d ч. Деньги придут на счёт в банке.", lot, item, price, money, hours);
    return RW_Ok(playerid, message);
}
stock RW_SellCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    new slot[16], price[16], hours[16];
    RW_MarketArgs(params, slot, sizeof slot, price, sizeof price, hours, sizeof hours);
    if (!slot[0] || !price[0]) return RW_Info(playerid, "Использование: /sell <слот инвентаря> <цена> [часы 5-24]. Комиссия 500 за час.");
    return RW_MarketSell(playerid, strval(slot), strval(price), hours[0] ? strval(hours) : MARKET_MIN_HOURS);
}

/*  -------------------------------- витрина -------------------------------- */

stock RW_MarketRows(playerid)
{
    new query[320];
    new offset = gMarketPage[playerid] * MARKET_ROWS;
    for (new i; i < MARKET_ROWS; i++) gMarketIds[playerid][i] = 0;
    format(query, sizeof query, "SELECT id,type,data,amount,price,bid,status,ends,seller_name FROM market_lots WHERE status IN (0,1) ORDER BY status,id DESC LIMIT %d OFFSET %d", MARKET_ROWS, offset);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r), index;
    for (new i; i < rows && index < MARKET_ROWS; i++) {
        new id = db_get_field_assoc_int(r, "id");
        if (id <= 0) { db_next_row(r); continue; }
        new type = db_get_field_assoc_int(r, "type"), data = db_get_field_assoc_int(r, "data");
        new amount = db_get_field_assoc_int(r, "amount"), price = db_get_field_assoc_int(r, "price");
        new bid = db_get_field_assoc_int(r, "bid"), status = db_get_field_assoc_int(r, "status");
        new ends = db_get_field_assoc_int(r, "ends");
        new seller[MAX_PLAYER_NAME]; db_get_field_assoc(r, "seller_name", seller, sizeof seller);
        gMarketIds[playerid][index] = id;
        new item[40], left[24], money[32], label[96], note[96];
        RW_MarketItemName(type, data, item, sizeof item);
        RW_MarketLeft(ends, left, sizeof left);
        if (status == MARKET_AUCTION) {
            RW_FormatMoney(bid > 0 ? bid : price, money, sizeof money);
            if (amount > 1) format(label, sizeof label, "№%d %s x%d", id, item, amount);
            else format(label, sizeof label, "№%d %s", id, item);
            format(note, sizeof note, "Аукцион: %s, осталось %s", money, left);
        } else {
            RW_FormatMoney(price, money, sizeof money);
            if (amount > 1) format(label, sizeof label, "№%d %s x%d", id, item, amount);
            else format(label, sizeof label, "№%d %s", id, item);
            format(note, sizeof note, "%s, продавец %s, %s", money, seller, left);
        }
        RW_MarketRowOut(playerid, index, label, note, "box");
        index++;
        db_next_row(r);
    }
    db_free_result(r);
    if (!index) {
        /* В браузере вещь выставляют из инвентаря, без него — командой. */
        if (RW_CefOn(playerid)) RW_MarketRowOut(playerid, 0, "Лотов нет",
            "Вернитесь позже или выставите вещь из инвентаря", "info");
        else RW_MarketRowOut(playerid, 0, "Лотов нет", "Вернитесь позже или выставите свой: /sell", "info");
    }
    /* Строка 7 только в браузере: в диалоге под этим номером «Закрыть».
       Она же — действие над выбранным лотом, поэтому второго нажатия
       на строку лота не нужно. */
    if (RW_CefOn(playerid) && gCefMode[playerid] == CEF_MODE_MARKET) {
        new pick = gMarketPick[playerid], label[64];
        if (pick <= 0) RW_MarketRowOut(playerid, 7, "Выберите лот: нажмите на строку выше",
            "Здесь появится покупка, ставка или снятие", "info");
        else if (RW_MarketLotField(pick, "status") == MARKET_OPEN
                 && RW_MarketLotField(pick, "seller") == gAccountId[playerid]) {
            format(label, sizeof label, "Снять лот №%d с витрины", pick);
            RW_MarketRowOut(playerid, 7, label, "Комиссия склада не возвращается", "wallet");
        } else if (RW_MarketLotField(pick, "status") == MARKET_OPEN) {
            format(label, sizeof label, "Купить лот №%d", pick);
            RW_MarketRowOut(playerid, 7, label, "Наличными, деньги уйдут продавцу на счёт", "ok");
        } else if (RW_MarketLotField(pick, "status") == MARKET_AUCTION) {
            format(label, sizeof label, "Поднять ставку: лот №%d", pick);
            RW_MarketRowOut(playerid, 7, label, "Минимум — на 5 процентов выше текущей", "wallet");
        } else RW_MarketRowOut(playerid, 7, "Лот закрыт: выберите другой", "", "info");
    }
    return index;
}
stock RW_MarketRowOut(playerid, row, const text[], const arg[], const icon[])
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
stock RW_MarketOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new subtitle[120];
    new count = RW_MarketCount();
    format(subtitle, sizeof subtitle, "Лотов на витрине: %d. Страница %d. Выберите лот — действие появится ниже", count, gMarketPage[playerid] + 1);
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_MARKET, "Маркетплейс", subtitle)) {
            RW_MarketRows(playerid);
            RW_CefAction(playerid, 6, "Дальше", "");
            RW_CefAction(playerid, 9, "Забрать со склада", "wallet");
            RW_CefAction(playerid, 8, "Закрыть", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_MARKET, "Маркетплейс", subtitle);
    RW_MarketRows(playerid);
    RW_UI_Row(playerid, 6, "Дальше");
    RW_UI_Row(playerid, 7, "Закрыть");
    return 1;
}
/* Что вообще можно выставить на витрину: остальное продать нельзя. */
stock bool:RW_MarketSellable(type)
{
    if (type <= 0) return false;
    if (type == ITEM_BACKPACK || type == ITEM_PHONE || type == ITEM_SIM || type == ITEM_CAR_KEY) return false;
    return true;
}
stock RW_MarketCount()
{
    if (gDB == DB:0) return 0;
    new DBResult:r = db_query(gDB, "SELECT COUNT(*) AS n FROM market_lots WHERE status IN (0,1)");
    if (r == DBResult:0) return 0;
    new total;
    if (db_num_rows(r) == 1) total = db_get_field_assoc_int(r, "n");
    db_free_result(r);
    return total;
}
stock RW_MarketRefresh(playerid)
{
    if (!RW_CefOn(playerid)) return 1;
    if (gCefMode[playerid] != CEF_MODE_MARKET) return 1;
    return RW_MarketOpen(playerid);
}
stock RW_MarketAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 6) {
        new total = RW_MarketCount();
        new pages = (total + MARKET_ROWS - 1) / MARKET_ROWS;
        if (pages < 1) pages = 1;
        gMarketPage[playerid] = (gMarketPage[playerid] + 1) % pages;
        gMarketPick[playerid] = 0;
        return RW_MarketOpen(playerid);
    }
    if (row == 7 && cef) {
        new pick = gMarketPick[playerid];
        if (pick <= 0) return RW_Error(playerid, "Сначала выберите лот: нажмите на него.");
        new status = RW_MarketLotField(pick, "status");
        if (status == MARKET_OPEN && RW_MarketLotField(pick, "seller") == gAccountId[playerid])
            return RW_MarketUnsell(playerid, pick);
        if (status == MARKET_OPEN) return RW_MarketBuy(playerid, pick);
        if (status == MARKET_AUCTION)
            return RW_InputOpen(playerid, INPUT_BID, pick, "Аукцион: лот №" , "Ваша ставка наличными",
                "Минимум — на 5 процентов выше текущей");
        return RW_Error(playerid, "Этот лот уже закрыт: выберите другой.");
    }
    if (row == 9 && cef) return RW_MarketClaim(playerid);
    new lot = gMarketIds[playerid][row];
    if (lot <= 0) return 1;
    if (GetTickCount() - gMarketTick[playerid] < 250) return 1;
    gMarketTick[playerid] = GetTickCount();
    if (gMarketPick[playerid] != lot) {
        gMarketPick[playerid] = lot;
        /* В браузере второе нажатие не нужно: ниже появится кнопка. */
        if (cef) { RW_MarketRefresh(playerid); return RW_Info(playerid, "Лот выбран. Действие — кнопкой под списком."); }
        return RW_Info(playerid, "Нажмите на эту строку ещё раз, чтобы купить.");
    }
    gMarketPick[playerid] = 0;
    RW_MarketBuy(playerid, lot);
    return RW_MarketRefresh(playerid);
}
stock RW_MarketClick(playerid, row) { return RW_MarketAction(playerid, row, false); }
stock RW_MarketCefClick(playerid, id) { return RW_MarketAction(playerid, id, true); }

/*  -------------------------------- покупка -------------------------------- */

stock RW_MarketLotField(lot, const field[])
{
    new query[200], value;
    format(query, sizeof query, "SELECT %s AS v FROM market_lots WHERE id=%d", field, lot);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return -1;
    if (db_num_rows(r) == 1) value = db_get_field_assoc_int(r, "v");
    db_free_result(r);
    return value;
}
stock RW_MarketBuy(playerid, lot)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (lot <= 0) return RW_Error(playerid, "Нет такого лота.");
    if (RW_MarketLotField(lot, "status") != MARKET_OPEN) return RW_Error(playerid, "Этот лот уже на аукционе: ставка — /bid <лот> <сумма>.");
    new type = RW_MarketLotField(lot, "type"), data = RW_MarketLotField(lot, "data");
    new amount = RW_MarketLotField(lot, "amount"), price = RW_MarketLotField(lot, "price");
    new seller = RW_MarketLotField(lot, "seller");
    if (type <= 0 || amount < 1 || price <= 0 || seller <= 0) return RW_Error(playerid, "Лот уже закрыт.");
    if (seller == gAccountId[playerid]) return RW_Error(playerid, "Свой лот покупать нельзя: снимите его командой /cancel <лот>.");
    if (gMoney[playerid] < price) return RW_Error(playerid, "Не хватает наличных.");
    new uid = RW_MarketGive(playerid, type, data, amount);
    if (uid <= 0) return RW_Error(playerid, "В инвентаре нет места: освободите слот и повторите.");
    RW_TakeMoney(playerid, price);
    RW_MarketCredit(seller, price);
    new query[200];
    format(query, sizeof query, "UPDATE market_lots SET status=%d,claim=0 WHERE id=%d AND status=%d", MARKET_DONE, lot, MARKET_OPEN);
    RW_DB_Exec(query);
    new item[40], money[32], message[200];
    RW_MarketItemName(type, data, item, sizeof item);
    RW_FormatMoney(price, money, sizeof money);
    format(message, sizeof message, "Куплено: %s за %s. Деньги ушли продавцу на счёт, забрать покупку можно в инвентаре.", item, money);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gAccountId[p] == seller && p != playerid)
        RW_Info(p, "Ваш лот купили. Деньги уже на счёте в банке.");
    return RW_Ok(playerid, message);
}
stock RW_BuyCommand(playerid, const params[])
{
    if (!params[0]) return RW_Info(playerid, "Использование: /buy <номер лота>. Витрина — /market.");
    return RW_MarketBuy(playerid, strval(params));
}

/*  ------------------------------- аукцион --------------------------------- */

/* Ставка на аукционе: та же функция, что и по команде, но по клику. */
stock RW_MarketBid(playerid, lot, amount)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (lot <= 0 || amount <= 0) return RW_Error(playerid, "Номер лота и сумма должны быть числами.");
    if (RW_MarketLotField(lot, "status") != MARKET_AUCTION) return RW_Error(playerid, "Ставки принимаются только на аукционе: лот уходит туда, когда выйдет срок склада.");
    new seller = RW_MarketLotField(lot, "seller");
    if (seller == gAccountId[playerid]) return RW_Error(playerid, "На свой лот ставку делать нельзя.");
    new bid = RW_MarketLotField(lot, "bid"), price = RW_MarketLotField(lot, "price");
    new min = RW_MarketMinBid(price, bid);
    if (amount < min) return RW_Error(playerid, "Ставка слишком маленькая: нужна хотя бы %d.", min);
    if (gMoney[playerid] < amount) return RW_Error(playerid, "Не хватает наличных.");
    if (!RW_TakeMoney(playerid, amount)) return RW_Error(playerid, "Не хватает наличных.");
    new old = RW_MarketLotField(lot, "bidder");
    if (old > 0 && old != gAccountId[playerid]) RW_MarketCredit(old, bid);
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], query[320];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "UPDATE market_lots SET bid=%d,bidder=%d,bidder_name='%s' WHERE id=%d AND status=%d", amount, gAccountId[playerid], safe, lot, MARKET_AUCTION);
    RW_DB_Exec(query);
    new money[32], message[180], left[24];
    RW_FormatMoney(amount, money, sizeof money);
    RW_MarketLeft(RW_MarketLotField(lot, "ends"), left, sizeof left);
    format(message, sizeof message, "Ставка %s принята. До конца аукциона %s. Если перебьют, деньги вернутся на счёт.", money, left);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gAccountId[p] == old && p != playerid)
        RW_Info(p, "Вашу ставку перебили. Деньги вернулись на счёт в банке.");
    return RW_Ok(playerid, message);
}
stock RW_BidCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new id[16], sum[16];
    RW_MarketArgs(params, id, sizeof id, sum, sizeof sum, sum, sizeof sum);
    if (!id[0] || !sum[0]) return RW_Info(playerid, "Использование: /bid <номер лота> <сумма>.");
    return RW_MarketBid(playerid, strval(id), strval(sum));
}

/*  --------------------------- снятие и получение -------------------------- */

/* Снять свой лот с витрины: и команда /cancel, и кнопка в браузере. */
stock RW_MarketUnsell(playerid, lot)
{
    if (lot <= 0) return RW_Error(playerid, "Нет такого лота.");
    if (RW_MarketLotField(lot, "seller") != gAccountId[playerid]) return RW_Error(playerid, "Это не ваш лот.");
    if (RW_MarketLotField(lot, "status") != MARKET_OPEN) return RW_Error(playerid, "Лот уже на аукционе или закрыт.");
    new query[200];
    format(query, sizeof query, "UPDATE market_lots SET status=%d,claim=%d WHERE id=%d AND status=%d", MARKET_CLAIM, gAccountId[playerid], lot, MARKET_OPEN);
    RW_DB_Exec(query);
    return RW_Ok(playerid, "Лот снят с витрины. Заберите предмет: кнопка «Забрать со склада» или /claim.");
}

stock RW_CancelCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!params[0]) return RW_Info(playerid, "Использование: /cancel <номер лота>. Комиссия склада не возвращается.");
    return RW_MarketUnsell(playerid, strval(params));
}
/* Отдаёт всё, что ждёт игрока на складе выдачи. */
/* Забрать всё со склада выдачи: и команда /claim, и кнопка в браузере. */
stock RW_MarketClaim(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!gInvLoaded[playerid]) return RW_Error(playerid, "Инвентарь ещё загружается.");
    new query[220];
    format(query, sizeof query, "SELECT id,type,data,amount FROM market_lots WHERE status=%d AND claim=%d ORDER BY id", MARKET_CLAIM, gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "На складе выдачи пусто.");
    new rows = db_num_rows(r);
    if (!rows) { db_free_result(r); return RW_Info(playerid, "На складе выдачи пусто."); }
    new taken, item[40], names[256], joined[320];
    for (new i; i < rows; i++) {
        new id = db_get_field_assoc_int(r, "id");
        new type = db_get_field_assoc_int(r, "type"), data = db_get_field_assoc_int(r, "data");
        new amount = db_get_field_assoc_int(r, "amount");
        if (RW_MarketGive(playerid, type, data, amount) <= 0) {
            RW_MarketItemName(type, data, item, sizeof item);
            format(names, sizeof names, "В инвентаре нет места: %s остался на складе выдачи.", item);
            RW_Error(playerid, names);
            break;
        }
        new done[160];
        format(done, sizeof done, "UPDATE market_lots SET status=%d,claim=0 WHERE id=%d", MARKET_DONE, id);
        RW_DB_Exec(done);
        RW_MarketItemName(type, data, item, sizeof item);
        if (taken) { format(joined, sizeof joined, "%s, %s", names, item); format(names, sizeof names, "%s", joined); }
        else format(names, sizeof names, "%s", item);
        taken++;
        db_next_row(r);
    }
    db_free_result(r);
    if (!taken) return 1;
    new message[300];
    format(message, sizeof message, "Получено со склада (%d): %s.", taken, names);
    return RW_Ok(playerid, message);
}

stock RW_TakeCommand(playerid) { return RW_MarketClaim(playerid); }

/*  --------------------------------- таймер -------------------------------- */
/* Склад живёт своё время, потом лот сам уходит на аукцион. */
public RW_MarketPulse()
{
    if (gDB == DB:0 || !gDBWritable) return 1;
    new now = gettime(), query[220];
    format(query, sizeof query, "UPDATE market_lots SET status=%d,bid=0,bidder=0,bidder_name='',ends=%d WHERE status=%d AND ends<=%d", MARKET_AUCTION, now + MARKET_AUCTION_SECONDS, MARKET_OPEN, now);
    RW_DB_Exec(query);
    new DBResult:r;
    format(query, sizeof query, "SELECT id,seller,bid,bidder FROM market_lots WHERE status=%d AND ends<=%d", MARKET_AUCTION, now);
    r = db_query(gDB, query);
    if (r == DBResult:0) return 1;
    new rows = db_num_rows(r);
    for (new i; i < rows; i++) {
        new id = db_get_field_assoc_int(r, "id");
        new seller = db_get_field_assoc_int(r, "seller");
        new bid = db_get_field_assoc_int(r, "bid");
        new bidder = db_get_field_assoc_int(r, "bidder");
        new winner = (bidder > 0) ? bidder : seller;
        new money = (bidder > 0) ? bid : 0;
        if (money > 0) RW_MarketCredit(seller, money);
        new q[220];
        format(q, sizeof q, "UPDATE market_lots SET status=%d,claim=%d WHERE id=%d", MARKET_CLAIM, winner, id);
        RW_DB_Exec(q);
        for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gAccountId[p] == winner)
            RW_Info(p, "Аукцион закончился. Заберите предмет на складе выдачи: /take.");
        for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gAccountId[p] == seller && money > 0)
            RW_Info(p, "Лот продан с аукциона. Деньги на счёте в банке.");
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}

/*  -------------------------------- команды -------------------------------- */

stock RW_MarketCommand(playerid, const params[])
{
    if (params[0]) gMarketPage[playerid] = strval(params);
    gMarketPick[playerid] = 0;
    if (!RW_CefOn(playerid)) RW_Info(playerid, "Маркетплейс: /sell <слот> <цена> [часы], /buy <лот>, /bid <лот> <сумма>, /cancel <лот>, /take.");
    return RW_MarketOpen(playerid);
}
