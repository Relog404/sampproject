/*  ============================================================================
 *  railway/property.inc
 *  Недвижимость: дома и бизнесы, двери, вход и налоги.
 *
 *  Как и везде, всё считает сервер: браузерное меню и нативный диалог вызывают
 *  одни и те же функции, состояние лежит в SQLite и переживает перезапуск.
 *  Деньги за покупку и налоги идут со счёта в банке, а не из кармана: клиент
 *  не может «заплатить» тем, чего у него нет.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */
#if defined _railway_property_included
    #endinput
#endif
#define _railway_property_included

#include <railway/core>
#include <railway/ui>
#include <railway/economy>

#define PROP_KIND_HOUSE         0
#define PROP_KIND_BIZ           1
#define PROP_MAX                8
#define PROP_RANGE              6.0         /* радиус двери                  */
#define PROP_SELL_PERCENT       75          /* выкуп государством, % от цены  */
#define PROP_TAX_LIMIT          3           /* просрочек до аукциона         */
#define PROP_AUCTION_SECONDS    600         /* сколько идёт аукцион          */
#define PROP_BID_STEP           1000        /* минимальный шаг ставки        */
#define BIZ_TILL_SHARE          10          /* доля выручки в кассу бизнеса  */
#define ROB_START_HOUR          12          /* окно ограбления: с 12:00      */
#define ROB_END_HOUR            1           /* до 01:00                      */
#define ROB_SHARE               30          /* сколько уносит грабитель, %   */
#define ROB_COOLDOWN_SECONDS    900         /* пауза после ограбления        */
#define ROB_FAIL_CHANCE         30          /* шанс поднять тревогу и уйти ни с чем */

/* Вид, цена, налог за payday, интерьер. */
static const gPropInfo[][4] = {
    { PROP_KIND_HOUSE,  25000,  500, 1 },
    { PROP_KIND_HOUSE,  35000,  650, 2 },
    { PROP_KIND_HOUSE,  30000,  600, 1 },
    { PROP_KIND_HOUSE,  28000,  550, 2 },
    { PROP_KIND_BIZ,   120000, 2500, 17 },
    { PROP_KIND_BIZ,   110000, 2300, 17 },
    { PROP_KIND_BIZ,   100000, 2100, 17 },
    { PROP_KIND_BIZ,   130000, 2700, 17 }
};
/* Настоящие адреса Los Santos. */
static const gPropNames[][] = {
    "Ganton, house 14",
    "Jefferson, house 7",
    "Willowfield, house 3",
    "El Corona, house 22",
    "24/7 Downtown",
    "24/7 Ganton",
    "24/7 Las Colinas",
    "24/7 Unity"
};
/* Двери: точки на улице у настоящих домов и магазинов. */
static const Float:gPropDoor[][3] = {
    { 2270.0, -1680.0, 13.5 },      /* Ganton       */
    { 2050.0, -1400.0, 13.5 },      /* Jefferson    */
    { 2170.0, -1990.0, 13.6 },      /* Willowfield  */
    { 1780.0, -1900.0, 13.5 },      /* El Corona    */
    { 1352.4, -1759.4, 13.5 },      /* Downtown     */
    { 2244.4, -1665.4, 13.5 },      /* Ganton       */
    { 2503.4, -1722.4, 13.4 },      /* Las Colinas  */
    { 1833.4, -1842.4, 13.5 }       /* Unity        */
};
/* Точка внутри: квартиры (интерьеры 1 и 2) и торговый зал 24/7 (интерьер 17). */
static const Float:gPropIn[][3] = {
    {  223.0439, 1287.2197, 1082.1296 },
    {  225.6307, 1021.4432, 1084.0695 },
    {  223.0439, 1287.2197, 1082.1296 },
    {  225.6307, 1021.4432, 1084.0695 },
    {  -25.8849, -185.8689, 1003.5499 },
    {  -25.8849, -185.8689, 1003.5499 },
    {  -25.8849, -185.8689, 1003.5499 },
    {  -25.8849, -185.8689, 1003.5499 }
};

new gPropOwner[PROP_MAX];               /* account id владельца, 0 — город   */
new gPropDoorOpen[PROP_MAX];            /* 1 — двери открыты                 */
new gPropTaxDue[PROP_MAX];              /* накопленный налог                 */
new gPropMissed[PROP_MAX];              /* сколько payday просрочено         */
new gPropCash[PROP_MAX];                /* касса бизнеса                     */
new gPropOwnerName[PROP_MAX][MAX_PLAYER_NAME+1];
new gPropAt[MAX_PLAYERS];               /* -1 снаружи, иначе id недвижимости */
new gPropOpen[MAX_PLAYERS];             /* id в открытом меню                */
/* Аукцион: ставка, её автор, залог и время окончания. */
new gPropBid[PROP_MAX];
new gPropBidder[PROP_MAX];
new gPropEscrow[PROP_MAX];
new gPropAucEnd[PROP_MAX];
new gPropRobEnd[PROP_MAX];              /* когда бизнес снова можно грабить  */

/*  ---------------------------------- база -------------------------------- */

stock RW_PropSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS properties (id INTEGER PRIMARY KEY, owner_id INT, doors INT, tax_due INT, missed INT, cash INT)")) return 0;
    for (new i; i < PROP_MAX; i++) {
        new query[160];
        format(query, sizeof query, "INSERT OR IGNORE INTO properties (id,owner_id,doors,tax_due,missed,cash) VALUES (%d,0,0,0,0,0)", i);
        if (!RW_DB_Exec(query)) return 0;
    }
    return 1;
}
/* Состояние держим в памяти, в базу пишем при каждом изменении. */
stock RW_PropLoad()
{
    new DBResult:r = db_query(gDB, "SELECT p.id,p.owner_id,p.doors,p.tax_due,p.missed,p.cash,a.name FROM properties p LEFT JOIN accounts a ON a.id=p.owner_id ORDER BY p.id");
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    for (new i; i < rows && i < PROP_MAX; i++) {
        gPropOwner[i] = db_get_field_assoc_int(r, "owner_id");
        gPropDoorOpen[i] = db_get_field_assoc_int(r, "doors");
        gPropTaxDue[i] = db_get_field_assoc_int(r, "tax_due");
        gPropMissed[i] = db_get_field_assoc_int(r, "missed");
        gPropCash[i] = db_get_field_assoc_int(r, "cash");
        new name[MAX_PLAYER_NAME+1];
        db_get_field_assoc(r, "name", name, sizeof name);
        format(gPropOwnerName[i], MAX_PLAYER_NAME+1, "%s", name);
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
/* Аукцион живёт в отдельной таблице: её можно добавить к уже рабочей базе. */
stock RW_PropAucSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS property_auctions (id INTEGER PRIMARY KEY, bid INT, bidder INT, escrow INT, ends INT)")) return 0;
    for (new i; i < PROP_MAX; i++) {
        new query[180];
        format(query, sizeof query, "INSERT OR IGNORE INTO property_auctions (id,bid,bidder,escrow,ends) VALUES (%d,0,0,0,0)", i);
        if (!RW_DB_Exec(query)) return 0;
    }
    return 1;
}
stock RW_PropAucLoad()
{
    new DBResult:r = db_query(gDB, "SELECT id,bid,bidder,escrow,ends FROM property_auctions ORDER BY id");
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    for (new i; i < rows && i < PROP_MAX; i++) {
        gPropBid[i] = db_get_field_assoc_int(r, "bid");
        gPropBidder[i] = db_get_field_assoc_int(r, "bidder");
        gPropEscrow[i] = db_get_field_assoc_int(r, "escrow");
        gPropAucEnd[i] = db_get_field_assoc_int(r, "ends");
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
stock RW_PropAucSave(id)
{
    new query[200];
    format(query, sizeof query, "UPDATE property_auctions SET bid=%d,bidder=%d,escrow=%d,ends=%d WHERE id=%d",
        gPropBid[id], gPropBidder[id], gPropEscrow[id], gPropAucEnd[id], id);
    return RW_DB_Exec(query);
}
/* Зачислить деньги на счёт аккаунта: владельца может не быть в сети. */
stock RW_PropCredit(account, amount)
{
    if (account <= 0 || amount <= 0) return 0;
    new query[160];
    format(query, sizeof query, "UPDATE bank_accounts SET balance=balance+%d WHERE player_id=%d", amount, account);
    return RW_DB_Exec(query);
}
stock RW_PropIsBiz(id)
{
    if (id < 0 || id >= PROP_MAX) return 0;
    return (gPropInfo[id][0] == PROP_KIND_BIZ) ? 1 : 0;
}
stock RW_PropSave(id)
{
    new query[200];
    format(query, sizeof query, "UPDATE properties SET owner_id=%d,doors=%d,tax_due=%d,missed=%d,cash=%d WHERE id=%d",
        gPropOwner[id], gPropDoorOpen[id], gPropTaxDue[id], gPropMissed[id], gPropCash[id], id);
    return RW_DB_Exec(query);
}

/*  -------------------------------- помощники ----------------------------- */

stock RW_PropNear(playerid)
{
    for (new i; i < PROP_MAX; i++)
        if (IsPlayerInRangeOfPoint(playerid, PROP_RANGE, gPropDoor[i][0], gPropDoor[i][1], gPropDoor[i][2])) return i;
    return -1;
}
stock RW_PropMine(playerid, id)
{
    return (id >= 0 && id < PROP_MAX && gPropOwner[id] > 0 && gPropOwner[id] == gAccountId[playerid]) ? 1 : 0;
}
stock RW_PropKindName(id, dest[], size = sizeof dest)
{
    format(dest, size, "%s", gPropInfo[id][0] == PROP_KIND_BIZ ? ("Business") : ("House"));
    return 1;
}
/* Подпись объекта: адрес, вид, цена и хозяин. */
stock RW_PropLabel(id, dest[], size = sizeof dest)
{
    new kind[16], price[32];
    RW_PropKindName(id, kind, sizeof kind);
    RW_FormatMoney(gPropInfo[id][1], price, sizeof price);
    if (gPropOwner[id] > 0)
        format(dest, size, "%s (%s) - %s, owner %s", gPropNames[id], kind, price, gPropOwnerName[id]);
    else
        format(dest, size, "%s (%s) - %s, sold by the city", gPropNames[id], kind, price);
    return 1;
}

/*  ---------------------------------- меню -------------------------------- */

stock RW_PropOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = gPropAt[playerid] >= 0 ? (gPropAt[playerid]) : (RW_PropNear(playerid));
    if (id < 0) return RW_Error(playerid, "No house or business door nearby. Stand at the door.");
    gPropOpen[playerid] = id;
    return RW_PropRender(playerid);
}
stock RW_PropRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefProp(playerid);
    #endif
    return RW_PropPanel(playerid);
}
stock RW_PropSubtitle(playerid, dest[], size = sizeof dest)
{
    new id = gPropOpen[playerid], acc[32];
    RW_FormatMoney(gBank[playerid], acc, sizeof acc);
    if (id < 0) { format(dest, size, "Account %s", acc); return 1; }
    if (RW_PropMine(playerid, id)) {
        if (gPropTaxDue[id] > 0) format(dest, size, "Yours, tax due %d (%d paydays late)", gPropTaxDue[id], gPropMissed[id]);
        else format(dest, size, "Yours, tax is paid. Account %s", acc);
        return 1;
    }
    if (gPropOwner[id] > 0) { format(dest, size, "Owned by %s. Account %s", gPropOwnerName[id], acc); return 1; }
    format(dest, size, "For sale. Account %s", acc);
    return 1;
}
/* Строки меню: общие для браузера и нативного диалога. */
stock RW_PropRows(playerid)
{
    new id = gPropOpen[playerid], row[96], tax[32];
    RW_FormatMoney(gPropInfo[id][2], tax, sizeof tax);
    RW_PropLabel(id, row, sizeof row);
    RW_AppRowProp(playerid, 0, row, "", "home");
    if (gPropOwner[id] <= 0) {
        new price[32];
        RW_FormatMoney(gPropInfo[id][1], price, sizeof price);
        format(row, sizeof row, "Buy it for %s (from the account)", price);
        RW_AppRowProp(playerid, 1, row, "buy", "wallet");
    } else if (RW_PropMine(playerid, id)) {
        new back[32];
        RW_FormatMoney(gPropInfo[id][1] * PROP_SELL_PERCENT / 100, back, sizeof back);
        format(row, sizeof row, "Sell it to the city for %s", back);
        RW_AppRowProp(playerid, 1, row, "sell", "wallet");
    } else {
        RW_AppRowProp(playerid, 1, "Not yours: the owner only", "", "info");
    }
    if (RW_PropMine(playerid, id)) {
        format(row, sizeof row, "Doors: %s", gPropDoorOpen[id] ? ("open") : ("closed"));
        RW_AppRowProp(playerid, 2, row, "doors", "key");
        format(row, sizeof row, "Tax per payday: %s", tax);
        RW_AppRowProp(playerid, 3, row, "tax", "wallet");
    } else {
        RW_AppRowProp(playerid, 2, "The owner opens the doors", "", "info");
        format(row, sizeof row, "Tax per payday: %s (the owner pays)", tax);
        RW_AppRowProp(playerid, 3, row, "", "info");
    }
    if (gPropAt[playerid] >= 0) RW_AppRowProp(playerid, 4, "Go outside", "exit", "door");
    else RW_AppRowProp(playerid, 4, "Go inside", "enter", "door");
    if (RW_PropAucOn(id)) {
        new bid[32];
        RW_FormatMoney(gPropBid[id], bid, sizeof bid);
        format(row, sizeof row, "Auction: bid %s, %d s left", bid, gPropAucEnd[id] - gettime());
        RW_AppRowProp(playerid, 5, row, "bid", "wallet");
        RW_AppRowProp(playerid, 7, "The auction is running: wait for it to end", "", "info");
    } else if (gPropInfo[id][0] == PROP_KIND_BIZ && RW_PropMine(playerid, id)) {
        new till[32];
        RW_FormatMoney(gPropCash[id], till, sizeof till);
        format(row, sizeof row, "Till: %s. Put money in", till);
        RW_AppRowProp(playerid, 5, row, "till", "wallet");
        RW_AppRowProp(playerid, 7, "Take money out of the till", "take", "wallet");
    } else if (gPropInfo[id][0] == PROP_KIND_BIZ) {
        new till[32];
        RW_FormatMoney(gPropCash[id], till, sizeof till);
        format(row, sizeof row, "Till: %s (the owner only)", till);
        RW_AppRowProp(playerid, 5, row, "", "info");
        RW_AppRowProp(playerid, 7, "Only the owner takes the cash", "", "info");
    } else if (RW_PropMine(playerid, id)) {
        format(row, sizeof row, "Pay the tax: %s", tax);
        RW_AppRowProp(playerid, 5, row, "tax", "wallet");
        RW_AppRowProp(playerid, 7, "One owner, one property", "", "info");
    } else {
        format(row, sizeof row, "Tax per payday: %s (the owner pays)", tax);
        RW_AppRowProp(playerid, 5, row, "", "info");
        RW_AppRowProp(playerid, 7, "One owner, one property", "", "info");
    }
    if (gPropInfo[id][0] == PROP_KIND_BIZ) RW_AppRowProp(playerid, 6, "Rob the till: from 12:00 to 01:00", "rob", "danger");
    else RW_AppRowProp(playerid, 6, "One owner, one property", "", "info");
    return 1;
}
/* Обёртка строки: в браузер с иконкой, в диалог без неё. */
stock RW_AppRowProp(playerid, row, const text[], const arg[], const icon[])
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefRow(playerid, row, text, arg, icon); return 1; }
    #else
        #pragma unused arg, icon
    #endif
    RW_UI_Row(playerid, row, text);
    return 1;
}
stock RW_CefProp(playerid)
{
    new subtitle[96];
    RW_PropSubtitle(playerid, subtitle, sizeof subtitle);
    if (!RW_CefOpen(playerid, CEF_MODE_PROPERTY, "Property", subtitle)) return RW_PropPanel(playerid);
    RW_PropRows(playerid);
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_PropPanel(playerid)
{
    new subtitle[96];
    RW_PropSubtitle(playerid, subtitle, sizeof subtitle);
    RW_UI_Panel(playerid, UI_PROPERTY, "Property", subtitle);
    RW_PropRows(playerid);
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_PropAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    new id = gPropOpen[playerid];
    if (id < 0) return 1;
    if (GetTickCount() - gAppTick[playerid] < 250 && row != 4) return 1;
    gAppTick[playerid] = GetTickCount();
    if (row == 1) {
        if (RW_PropMine(playerid, id)) return RW_PropSell(playerid, id);
        return RW_PropBuy(playerid, id);
    }
    if (row == 2) return RW_PropToggleDoors(playerid, id);
    if (row == 3) {
        if (!RW_PropMine(playerid, id)) return RW_Error(playerid, "Only the owner pays the tax.");
        return RW_PropPayTax(playerid, id);
    }
    if (row == 4) {
        if (gPropAt[playerid] >= 0) return RW_PropExit(playerid);
        return RW_PropEnter(playerid, id);
    }
    if (row == 5 && RW_PropAucOn(id))
        return RW_InputOpen(playerid, INPUT_PROP_BID, id, "Auction", "Your bid: at least the current one plus 1 000", "For example: 50000");
    if (row == 5 && RW_PropMine(playerid, id) && gPropInfo[id][0] != PROP_KIND_BIZ) return RW_PropPayTax(playerid, id);
    if (row == 5 && RW_PropMine(playerid, id) && gPropInfo[id][0] == PROP_KIND_BIZ && !RW_PropAucOn(id))
        return RW_InputOpen(playerid, INPUT_TILL_PUT, id, "Till", "How much to put in: from the bank account", "For example: 5000");
    if (row == 6 && gPropInfo[id][0] == PROP_KIND_BIZ) return RW_PropRob(playerid, id);
    if (cef && row == 7 && RW_PropMine(playerid, id) && gPropInfo[id][0] == PROP_KIND_BIZ && !RW_PropAucOn(id))
        return RW_InputOpen(playerid, INPUT_TILL_TAKE, id, "Till", "How much to take out: cash into your pocket", "For example: 5000");
    return 1;
}
stock RW_PropCefClick(playerid, row) { return RW_PropAction(playerid, row, true); }
stock RW_PropClick(playerid, row) { return RW_PropAction(playerid, row, false); }

/*  --------------------------------- действия ----------------------------- */

stock RW_PropBuy(playerid, id)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (id < 0 || id >= PROP_MAX) return RW_Error(playerid, "No such property.");
    if (gPropOwner[id] > 0) return RW_Error(playerid, "It is already owned.");
    if (RW_PropOwns(playerid) >= 0) return RW_Error(playerid, "You already own property. Sell it first.");
    new price = gPropInfo[id][1];
    if (gBank[playerid] < price) return RW_Error(playerid, "Not enough money on the account.");
    RW_BankTake(playerid, price);
    gPropOwner[id] = gAccountId[playerid];
    gPropDoorOpen[id] = 0;
    gPropTaxDue[id] = 0;
    gPropMissed[id] = 0;
    RW_GetNick(playerid, gPropOwnerName[id], MAX_PLAYER_NAME+1);
    RW_PropSave(id);
    RW_StoryEvent(playerid, (gPropInfo[id][0] == PROP_KIND_BIZ) ? STORY_EV_BUSINESS : STORY_EV_PROPERTY);
    RW_UI_Close(playerid);
    new sum[32], message[160];
    RW_FormatMoney(price, sum, sizeof sum);
    format(message, sizeof message, "Bought %s for %s. Doors are closed: /doors opens them.", gPropNames[id], sum);
    return RW_Ok(playerid, message);
}
stock RW_PropSell(playerid, id)
{
    if (!RW_PropMine(playerid, id)) return RW_Error(playerid, "This is not yours.");
    new back = gPropInfo[id][1] * PROP_SELL_PERCENT / 100;
    back = back + gPropCash[id];
    RW_BankAdd(playerid, back);
    gPropOwner[id] = 0;
    gPropOwnerName[id][0] = 0;
    gPropDoorOpen[id] = 0;
    gPropTaxDue[id] = 0;
    gPropMissed[id] = 0;
    gPropCash[id] = 0;
    RW_PropSave(id);
    RW_UI_Close(playerid);
    new sum[32], message[160];
    RW_FormatMoney(back, sum, sizeof sum);
    format(message, sizeof message, "Sold %s: %s back on the account, the cash register with it.", gPropNames[id], sum);
    return RW_Ok(playerid, message);
}
stock RW_PropToggleDoors(playerid, id)
{
    if (!RW_PropMine(playerid, id)) return RW_Error(playerid, "Only the owner opens these doors.");
    if (gPropMissed[id] >= PROP_TAX_LIMIT) return RW_Error(playerid, RW_CefOn(playerid)
        ? ("The city sealed the doors: press the tax row in the property panel.")
        : ("The city sealed the doors: pay the tax with /paytax."));
    gPropDoorOpen[id] = gPropDoorOpen[id] ? (0) : (1);
    RW_PropSave(id);
    new message[128];
    format(message, sizeof message, "Doors are now %s.", gPropDoorOpen[id] ? ("open") : ("closed"));
    return RW_Ok(playerid, message);
}
stock RW_PropEnter(playerid, id)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (id < 0 || id >= PROP_MAX) return RW_Error(playerid, "No such property.");
    if (gPropAt[playerid] >= 0) return RW_Info(playerid, "You are already inside. /exit gets you out.");
    if (!RW_PropMine(playerid, id) && !gPropDoorOpen[id])
        return RW_Error(playerid, "The door is locked. Only the owner can open it.");
    if (!RW_PropNear(playerid)) return RW_Error(playerid, "Stand at the door.");
    SetPlayerInterior(playerid, gPropInfo[id][3]);
    SetPlayerPos(playerid, gPropIn[id][0], gPropIn[id][1], gPropIn[id][2]);
    SetPlayerFacingAngle(playerid, 0.0);
    gPropAt[playerid] = id;
    new message[128];
    format(message, sizeof message, "%s: you are inside. /exit - back to the street.", gPropNames[id]);
    return RW_Ok(playerid, message);
}
stock RW_PropExit(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = gPropAt[playerid];
    if (id < 0) return RW_Info(playerid, "You are not inside.");
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    SetPlayerPos(playerid, gPropDoor[id][0], gPropDoor[id][1], gPropDoor[id][2]);
    gPropAt[playerid] = -1;
    new message[128];
    format(message, sizeof message, "%s: you are on the street again.", gPropNames[id]);
    return RW_Ok(playerid, message);
}
/* Есть ли у игрока недвижимость: индекс или -1. */
stock RW_PropOwns(playerid)
{
    for (new i; i < PROP_MAX; i++) if (RW_PropMine(playerid, i)) return i;
    return -1;
}
stock RW_PropPayTax(playerid, id)
{
    if (!RW_PropMine(playerid, id)) return RW_Error(playerid, "This is not yours.");
    if (gPropTaxDue[id] <= 0) return RW_Info(playerid, "The tax is already paid.");
    if (gBank[playerid] < gPropTaxDue[id]) return RW_Error(playerid, "Not enough money on the account.");
    RW_BankTake(playerid, gPropTaxDue[id]);
    gPropTaxDue[id] = 0;
    gPropMissed[id] = 0;
    RW_PropSave(id);
    new sum[32], message[144];
    RW_FormatMoney(gPropInfo[id][2], sum, sizeof sum);
    format(message, sizeof message, "Tax paid for %s (%s per payday). The doors are yours again.", gPropNames[id], sum);
    return RW_Ok(playerid, message);
}
/* Налог списывается на payday: со счёта, иначе копится долг. */
public RW_PropTaxTick(playerid)
{
    if (!gLogged[playerid]) return 0;
    for (new i; i < PROP_MAX; i++) {
        if (!RW_PropMine(playerid, i)) continue;
        new tax = gPropInfo[i][2];
        if (gBank[playerid] >= tax) {
            RW_BankTake(playerid, tax);
            gPropMissed[i] = 0;
        } else {
            gPropTaxDue[i] = gPropTaxDue[i] + tax;
            gPropMissed[i]++;
            if (gPropMissed[i] >= PROP_TAX_LIMIT) {
                gPropDoorOpen[i] = 0;
                if (gPropAucEnd[i] <= 0) RW_PropAucStart(i);
            }
        }
        RW_PropSave(i);
    }
    return 1;
}

/*  --------------------------------- аукцион ------------------------------ */
/*  Три просрочки — и город выставляет объект на торги. Прежнему владельцу   */
/*  возвращается 75 % цены, победитель получает объект.                      */

stock RW_PropAucStart(id)
{
    gPropBid[id] = gPropInfo[id][1] / 2;
    gPropBidder[id] = 0;
    gPropEscrow[id] = 0;
    gPropAucEnd[id] = gettime() + PROP_AUCTION_SECONDS;
    RW_PropAucSave(id);
    return 1;
}
stock RW_PropAucOn(id)
{
    return (gPropAucEnd[id] > 0 && gPropOwner[id] > 0) ? 1 : 0;
}
stock RW_PropBid(playerid, id, amount)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (id < 0 || id >= PROP_MAX) return RW_Error(playerid, "No such property.");
    if (!RW_PropAucOn(id)) return RW_Error(playerid, "It is not up for auction.");
    if (RW_PropMine(playerid, id)) return RW_Error(playerid, "You cannot buy back your own property: pay the tax.");
    if (amount < gPropBid[id] + PROP_BID_STEP) return RW_Error(playerid, "Bid higher: at least the current bid plus 1 000.");
    if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account for such a bid.");
    /* Залог: деньги снимаются сразу, прошлую ставку возвращаем её автору. */
    if (gPropEscrow[id] > 0) RW_PropCredit(gPropBidder[id], gPropEscrow[id]);
    RW_BankTake(playerid, amount);
    gPropBid[id] = amount;
    gPropBidder[id] = gAccountId[playerid];
    gPropEscrow[id] = amount;
    RW_PropAucSave(id);
    new sum[32], message[160];
    RW_FormatMoney(amount, sum, sizeof sum);
    format(message, sizeof message, "Bid on %s: %s. If somebody bids higher, the money comes back.", gPropNames[id], sum);
    return RW_Ok(playerid, message);
}
/* Закрытие торгов: вызывается таймером, считает сервер. */
public RW_PropPulse()
{
    for (new id; id < PROP_MAX; id++) {
        if (gPropAucEnd[id] <= 0 || gettime() < gPropAucEnd[id]) continue;
        RW_PropAucClose(id);
    }
    return 1;
}
stock RW_PropAucClose(id)
{
    new price = gPropInfo[id][1];
    new back = price * PROP_SELL_PERCENT / 100;
    new old = gPropOwner[id];
    if (old > 0) RW_PropCredit(old, back);
    if (gPropBidder[id] > 0) {
        /* Победитель: залог уже снят, объект переходит к нему. */
        gPropOwner[id] = gPropBidder[id];
        new DBResult:r = db_query(gDB, "SELECT name FROM accounts ORDER BY id");
        if (r != DBResult:0) {
            new rows = db_num_rows(r);
            for (new i; i < rows && i < MAX_PLAYERS; i++) {
                new name[MAX_PLAYER_NAME+1];
                db_get_field_assoc(r, "name", name, sizeof name);
                if (i + 1 == gPropBidder[id]) format(gPropOwnerName[id], MAX_PLAYER_NAME+1, "%s", name);
                db_next_row(r);
            }
            db_free_result(r);
        }
    } else {
        gPropOwner[id] = 0;
        gPropOwnerName[id][0] = 0;
    }
    gPropBid[id] = 0;
    gPropBidder[id] = 0;
    gPropEscrow[id] = 0;
    gPropAucEnd[id] = 0;
    gPropDoorOpen[id] = 0;
    gPropTaxDue[id] = 0;
    gPropMissed[id] = 0;
    RW_PropSave(id);
    RW_PropAucSave(id);
    return 1;
}

/*  ---------------------------------- касса ------------------------------- */
/*  Выручка 24/7 падает в кассу владельца, забрать её можно командой.        */

public RW_PropSale(playerid, price)
{
    new id = RW_PropNear(playerid);
    if (id < 0 || gPropOwner[id] <= 0 || gPropInfo[id][0] != PROP_KIND_BIZ) return 0;
    if (price <= 0) return 0;
    gPropCash[id] = gPropCash[id] + price * BIZ_TILL_SHARE / 100;
    RW_PropSave(id);
    return 1;
}
stock RW_PropTill(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = RW_PropNear(playerid);
    if (id < 0 || !RW_PropMine(playerid, id)) {
        new own = RW_PropOwns(playerid);
        if (own < 0 || gPropInfo[own][0] != PROP_KIND_BIZ) return RW_Error(playerid, "Stand at the door of your business.");
        id = own;
    }
    if (gPropInfo[id][0] != PROP_KIND_BIZ) return RW_Error(playerid, "Only a business has a till.");
    new sum[32], message[160];
    RW_FormatMoney(gPropCash[id], sum, sizeof sum);
    format(message, sizeof message, "%s till: %s. /till put <sum> or /till take <sum>.", gPropNames[id], sum);
    return RW_Ok(playerid, message);
}
/* Касса: одна функция и на команду, и на клик по кнопке в панели. */
stock RW_PropTillAmount(playerid, id, amount, bool:put)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (id < 0 || !RW_PropMine(playerid, id) || gPropInfo[id][0] != PROP_KIND_BIZ)
        return RW_Error(playerid, "Stand at the door of your business.");
    if (amount <= 0) return RW_Error(playerid, put ? ("Tell how much: /till put <sum>.") : ("Tell how much: /till take <sum>."));
    new sum[32], message[160];
    if (put) {
        if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account.");
        RW_BankTake(playerid, amount);
        gPropCash[id] = gPropCash[id] + amount;
        RW_PropSave(id);
        RW_FormatMoney(amount, sum, sizeof sum);
        format(message, sizeof message, "Put %s into the till. Now: %d.", sum, gPropCash[id]);
        return RW_Ok(playerid, message);
    }
    if (amount > gPropCash[id]) return RW_Error(playerid, "The till does not have that much.");
    gPropCash[id] = gPropCash[id] - amount;
    RW_BankAdd(playerid, amount);
    RW_PropSave(id);
    RW_FormatMoney(amount, sum, sizeof sum);
    format(message, sizeof message, "Took %s from the till. Left: %d.", sum, gPropCash[id]);
    return RW_Ok(playerid, message);
}
stock RW_PropTillCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new word[24], i, j;
    while (params[i] && params[i] != ' ' && j < sizeof word - 1) { word[j] = params[i]; j++; i++; }
    word[j] = 0;
    while (params[i] == ' ') i++;
    new amount = strval(params[i]);
    new id = RW_PropNear(playerid);
    if (id < 0 || !RW_PropMine(playerid, id)) id = RW_PropOwns(playerid);
    if (id < 0 || !RW_PropMine(playerid, id) || gPropInfo[id][0] != PROP_KIND_BIZ)
        return RW_Error(playerid, "Stand at the door of your business.");
    if (!word[0]) return RW_PropTill(playerid);
    if (!strcmp(word, "take", true)) return RW_PropTillAmount(playerid, id, amount, false);
    if (!strcmp(word, "put", true)) return RW_PropTillAmount(playerid, id, amount, true);
    return RW_Error(playerid, "Use: /till put <sum> or /till take <sum>.");
}

/*  -------------------------------- ограбление ---------------------------- */
/*  С 12:00 до 01:00: унести часть кассы бизнеса, рискуя поднять тревогу.    */

stock RW_PropRobWindow()
{
    new hour, minute, second;
    gettime(hour, minute, second);
    #pragma unused minute, second
    return (hour >= ROB_START_HOUR || hour < ROB_END_HOUR) ? 1 : 0;
}
stock RW_PropRob(playerid, id)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (id < 0 || id >= PROP_MAX) return RW_Error(playerid, "Stand at the door of a business.");
    if (gPropInfo[id][0] != PROP_KIND_BIZ) return RW_Error(playerid, "Only a business can be robbed.");
    if (!RW_PropRobWindow()) return RW_Error(playerid, "Too early: the tills are taken to the bank at night. Come from 12:00 to 01:00.");
    if (GetTickCount() - gAppTick[playerid] < 2000) return 1;
    gAppTick[playerid] = GetTickCount();
    if (gettime() < gPropRobEnd[id]) return RW_Error(playerid, "This place was hit recently. Try another one.");
    gPropRobEnd[id] = gettime() + ROB_COOLDOWN_SECONDS;
    if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0)
        return RW_Error(playerid, "An officer on duty does not rob shops.");
    new Float:x, Float:y, Float:z, nick[MAX_PLAYER_NAME], message[180];
    GetPlayerPos(playerid, x, y, z);
    RW_GetNick(playerid, nick);
    if (random(100) < ROB_FAIL_CHANCE || gPropCash[id] <= 0) {
        /* Тревога: полиция на службе узнаёт адрес. */
        format(message, sizeof message, "Alarm at %s (%d, %d): %s tried the till.", gPropNames[id], floatround(x), floatround(y), nick);
        for (new i; i < MAX_PLAYERS; i++)
            if (IsPlayerConnected(i) && gLogged[i] && RW_PoliceOnDuty(i) && RW_PoliceBadge(i) > 0) RW_Error(i, message);
        return RW_Error(playerid, "The alarm went off and the till is out of reach. Run!");
    }
    new take = gPropCash[id] * ROB_SHARE / 100;
    gPropCash[id] = gPropCash[id] - take;
    RW_AddMoney(playerid, take);
    RW_PropSave(id);
    RW_StoryEvent(playerid, STORY_EV_ROB);
    RW_StoryEarned(playerid, take);
    format(message, sizeof message, "Alarm at %s (%d, %d): %s is robbing the till.", gPropNames[id], floatround(x), floatround(y), nick);
    for (new i; i < MAX_PLAYERS; i++)
        if (IsPlayerConnected(i) && gLogged[i] && RW_PoliceOnDuty(i) && RW_PoliceBadge(i) > 0) RW_Error(i, message);
    new sum[32];
    RW_FormatMoney(take, sum, sizeof sum);
    format(message, sizeof message, "Clean job: %s in cash. The police heard the alarm.", sum);
    return RW_Ok(playerid, message);
}
stock RW_PropRobCommand(playerid) { return RW_PropRob(playerid, RW_PropNear(playerid)); }

/*  --------------------------------- команды ------------------------------ */

stock RW_PropertyCommand(playerid) { return RW_PropOpen(playerid); }
stock RW_EnterCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    return RW_PropEnter(playerid, RW_PropNear(playerid));
}
stock RW_ExitCommand(playerid) { return RW_PropExit(playerid); }
stock RW_DoorsCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = RW_PropNear(playerid);
    if (id < 0) {
        new own = RW_PropOwns(playerid);
        if (own < 0) return RW_Error(playerid, "Stand at your door or buy a property first.");
        return RW_PropToggleDoors(playerid, own);
    }
    return RW_PropToggleDoors(playerid, id);
}
stock RW_PropBidCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = RW_PropNear(playerid);
    if (id < 0 || !RW_PropAucOn(id)) return RW_Error(playerid, "Stand at the door of a property up for auction.");
    new amount = strval(params);
    if (amount <= 0) {
        new sum[32], message[160];
        RW_FormatMoney(gPropBid[id], sum, sizeof sum);
        format(message, sizeof message, "Current bid on %s: %s. Bid: /bid <sum>. Left: %d s.",
            gPropNames[id], sum, gPropAucEnd[id] - gettime());
        return RW_Ok(playerid, message);
    }
    return RW_PropBid(playerid, id, amount);
}
stock RW_PayTaxCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = RW_PropNear(playerid);
    if (id < 0) id = RW_PropOwns(playerid);
    if (id < 0) return RW_Error(playerid, "You own no property.");
    return RW_PropPayTax(playerid, id);
}

/*  ---------------------------------- точки ------------------------------- */

stock RW_PropInit()
{
    RW_PropLoad();
    RW_PropAucLoad();
    for (new i; i < PROP_MAX; i++) {
        new label[96];
        RW_PropLabel(i, label, sizeof label);
        CreatePickup(1272, 1, gPropDoor[i][0], gPropDoor[i][1], gPropDoor[i][2], 0);
        Create3DTextLabel(label, 0xD8C39BFF, gPropDoor[i][0], gPropDoor[i][1], gPropDoor[i][2] + 1.1, 20.0, 0, 0);
    }
    return 1;
}
stock RW_PropReset(playerid)
{
    gPropAt[playerid] = -1;
    gPropOpen[playerid] = -1;
    return 1;
}
