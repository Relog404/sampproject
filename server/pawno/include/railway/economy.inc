/*  ============================================================================

 *  railway/economy.inc

 *  Экономика: работа (грузчик, курьер, дальнобойщик), магазин 24/7 и банк.

 *

 *  Деньги в кармане (gMoney) и деньги на счету (bank_accounts) — разные вещи:

 *  зарплата капает на счёт, товары в 24/7 покупаются наличными. Как и везде,

 *  всё считает сервер: браузерное меню и TextDraw-меню вызывают одни и те же

 *  функции, а состояние лежит в SQLite и переживает перезапуск.

 *

 *  Кодировка исходников: Windows-1251.

 *  ========================================================================= */



#if defined _railway_economy_included

    #endinput

#endif

#define _railway_economy_included



#include <railway/core>

#include <railway/ui>

#include <railway/inventory>

#include <railway/transport>        /* RW_TrunkName — подписи товаров      */



#define ECON_TASK_ICON          95          /* свободный id иконки на карте  */



#define JOB_NONE                0

#define JOB_LOADER              1

#define JOB_COURIER             2

#define JOB_TRUCKER             3

#define JOB_COUNT               4



#define JOB_MAX_LEVEL           5

#define JOB_EXP_PER_LEVEL       10          /* заданий до следующего уровня  */

#define JOB_TASK_SECONDS        240         /* срок на один этап задания     */

#define JOB_TASK_RANGE          8.0

#define JOB_HIRE_RANGE          12.0

#define JOB_COOLDOWN_MS         2000        /* пауза между заданиями         */



#define SHOP_RANGE              10.0

#define BANK_ATM_RANGE          6.0

#define PAYDAY_SECONDS          900         /* зарплата раз в 15 минут       */

#define BANK_INTEREST_PERCENT   1

#define BANK_INTEREST_MAX       2000

#define BANK_MIN_TRANSFER       50



/* Товары 24/7: тип, data, amount, цена. */

static const gShopItems[][4] = {

    { ITEM_PHONE,    0, 1,  900 },

    { ITEM_SIM,      0, 1,  200 },

    { ITEM_BACKPACK, 0, 1, 1500 },

    { ITEM_BANDAGE,  0, 1,  150 },

    { ITEM_SYRINGE,  0, 1,  300 },

    { ITEM_LOCKPICK, 0, 1,  450 },

    { ITEM_ARMOUR,   1, 1, 2500 }

};



/* Магазины 24/7. */

static const Float:gShops[][3] = {

    {  1833.0, -1842.0, 13.5 },     /* вокзал        */

    {  1352.0, -1759.0, 13.5 },     /* центр         */

    {  2244.0, -1665.0, 13.5 },     /* восток        */

    {  2503.0, -1722.0, 13.4 }      /* Джефферсон    */

};



/* Банкоматы. */

static const Float:gAtms[][3] = {

    {  1729.3, -1943.7, 13.5 },     /* вокзал        */

    {  1175.8, -1324.7, 15.4 },     /* больница      */

    {  1004.0,  -939.0, 42.2 },     /* Temple        */

    {  1635.0, -1700.0, 13.6 }      /* штрафстоянка  */

};



/* Биржа труда: работа, x, y, z. */

static const Float:gJobHire[][4] = {

    { 1.0, 1660.0, -2260.0, 13.6 }, /* порт: грузчик */

    { 2.0, 1745.0, -1900.0, 13.5 }, /* депо: курьер  */

    { 3.0, 2200.0, -2280.0, 13.5 }  /* база: трейлер */

};



/* Ящики и склад грузчика. */

static const Float:gLoaderCrates[][3] = {

    { 1620.0, -2260.0, 13.6 },

    { 1660.0, -2260.0, 13.6 },

    { 1700.0, -2260.0, 13.6 }

};

static const Float:gLoaderDrop[3] = { 1660.0, -2200.0, 13.6 };



/* Адреса курьера. */

static const Float:gCourierPoints[][3] = {

    { 1729.3, -1943.7, 13.5 },      /* вокзал        */

    { 1175.8, -1324.7, 15.4 },      /* больница      */

    { 1481.0, -1740.0, 13.5 },      /* центр         */

    { 2020.0, -1420.0, 13.5 },      /* Глен Парк     */

    { 2244.0, -1665.0, 13.5 },      /* восток        */

    { 1352.0, -1759.0, 13.5 }       /* Сити Холл     */

};



/* Города дальнобойщика. */

static const Float:gTruckerPoints[][3] = {

    { -1610.0,  730.0, 32.0 },      /* San Fierro    */

    {  2180.0, 2350.0, 12.0 },      /* Las Venturas  */

    { -2430.0, 2320.0,  5.0 }       /* Bayside       */

};



/* На чём можно работать дальнобойщиком. */

static const gTruckModels[] = { 414, 455, 403, 443, 515, 514, 499, 498, 609, 524 };



new gBank[MAX_PLAYERS];               /* деньги на счету                   */

new gJob[MAX_PLAYERS];                /* JOB_NONE…JOB_TRUCKER              */

new gJobLevel[MAX_PLAYERS];

new gJobExp[MAX_PLAYERS];

new gJobTasks[MAX_PLAYERS];

new gJobEarned[MAX_PLAYERS];

new gJobStage[MAX_PLAYERS];           /* 0 нет задания, 1 взять, 2 сдать   */

new Float:gJobX[MAX_PLAYERS];

new Float:gJobY[MAX_PLAYERS];

new Float:gJobZ[MAX_PLAYERS];

new gJobEnd[MAX_PLAYERS];             /* когда задание сгорит (gettime)    */

new gJobPay[MAX_PLAYERS];

new gJobTick[MAX_PLAYERS];            /* защита от спама                   */

new gPayday[MAX_PLAYERS];             /* секунд с прошлой зарплаты         */

new bool:gJobIcon[MAX_PLAYERS];       /* иконка задания на карте включена  */



forward RW_EconomyPulse();



/*  ----------------------------------- база -------------------------------- */



stock RW_EconomySchema()

{

    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS bank_accounts (player_id INT PRIMARY KEY, balance INT)")) return 0;

    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS job_players (player_id INT PRIMARY KEY, job INT, level INT, exp INT, tasks INT, earned INT, stamp INT)")) return 0;

    return 1;

}

stock RW_EconomyReset(playerid)

{

    gJobStage[playerid] = 0;

    gJobEnd[playerid] = 0;

    gJobPay[playerid] = 0;

    gJobTick[playerid] = 0;

    gPayday[playerid] = 0;

    if (gJobIcon[playerid]) { RemovePlayerMapIcon(playerid, ECON_TASK_ICON); gJobIcon[playerid] = false; }

    return 1;

}

// Счёт и работа читаются при входе, чтобы всё пережило перезапуск сервера.

stock RW_EconomyLoad(playerid)

{

    gBank[playerid] = 0;

    gJob[playerid] = JOB_NONE;

    gJobLevel[playerid] = 1;

    gJobExp[playerid] = 0;

    gJobTasks[playerid] = 0;

    gJobEarned[playerid] = 0;

    RW_EconomyReset(playerid);

    if (!gLogged[playerid] || gAccountId[playerid] <= 0) return 0;



    new query[220];

    format(query, sizeof query, "SELECT balance FROM bank_accounts WHERE player_id=%d", gAccountId[playerid]);

    new DBResult:r = db_query(gDB, query);

    if (r != DBResult:0) {

        if (db_num_rows(r) == 1) gBank[playerid] = db_get_field_assoc_int(r, "balance");

        db_free_result(r);

    }

    format(query, sizeof query, "SELECT job,level,exp,tasks,earned FROM job_players WHERE player_id=%d", gAccountId[playerid]);

    r = db_query(gDB, query);

    if (r != DBResult:0) {

        if (db_num_rows(r) == 1) {

            gJob[playerid] = db_get_field_assoc_int(r, "job");

            gJobLevel[playerid] = db_get_field_assoc_int(r, "level");

            gJobExp[playerid] = db_get_field_assoc_int(r, "exp");

            gJobTasks[playerid] = db_get_field_assoc_int(r, "tasks");

            gJobEarned[playerid] = db_get_field_assoc_int(r, "earned");

        }

        db_free_result(r);

    }

    if (gBank[playerid] < 0) gBank[playerid] = 0;

    if (gJob[playerid] < 0 || gJob[playerid] >= JOB_COUNT) gJob[playerid] = JOB_NONE;

    if (gJobLevel[playerid] < 1) gJobLevel[playerid] = 1;

    if (gJobLevel[playerid] > JOB_MAX_LEVEL) gJobLevel[playerid] = JOB_MAX_LEVEL;

    if (gJobExp[playerid] < 0) gJobExp[playerid] = 0;

    return 1;

}

stock RW_EconomySave(playerid)

{

    if (!gLogged[playerid] || gAccountId[playerid] <= 0) return 0;

    new query[320];

    format(query, sizeof query, "INSERT OR REPLACE INTO bank_accounts(player_id,balance) VALUES(%d,%d)", gAccountId[playerid], gBank[playerid]);

    if (!RW_DB_Exec(query)) return 0;

    format(query, sizeof query, "INSERT OR REPLACE INTO job_players(player_id,job,level,exp,tasks,earned,stamp) VALUES(%d,%d,%d,%d,%d,%d,%d)",

        gAccountId[playerid], gJob[playerid], gJobLevel[playerid], gJobExp[playerid], gJobTasks[playerid], gJobEarned[playerid], gettime());

    return RW_DB_Exec(query);

}



/*  ---------------------------------- банк --------------------------------- */

/*  Счёт в банке: зарплата падает туда, проценты капают раз в зарплату.       */



stock RW_BankBalance(playerid) { return gBank[playerid]; }

// Пополнение и снятие пишутся в базу сразу: деньги не должны теряться.

stock RW_BankAdd(playerid, amount)

{

    if (amount <= 0) return 0;

    gBank[playerid] += amount;

    RW_EconomySave(playerid);

    return 1;

}

stock bool:RW_BankTake(playerid, amount)

{

    if (amount <= 0 || gBank[playerid] < amount) return false;

    gBank[playerid] -= amount;

    RW_EconomySave(playerid);

    return true;

}

stock bool:RW_BankNear(playerid)

{

    for (new i; i < sizeof gAtms; i++)

        if (IsPlayerInRangeOfPoint(playerid, BANK_ATM_RANGE, gAtms[i][0], gAtms[i][1], gAtms[i][2])) return true;

    return false;

}

stock RW_BankOpen(playerid)

{

    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");

    if (!RW_BankNear(playerid)) return RW_Error(playerid, "No ATM nearby. Try the station, the hospital or a gas station.");

    return RW_BankRender(playerid);

}

stock RW_BankRender(playerid)

{

    #if RAILWAY_CEF

        if (RW_CefOn(playerid)) return RW_CefBank(playerid);

    #endif

    return RW_BankPanel(playerid);

}

stock RW_BankSubtitle(playerid, dest[], size = sizeof dest)

{

    new cash[32], acc[32];

    RW_FormatMoney(gMoney[playerid], cash, sizeof cash);

    RW_FormatMoney(gBank[playerid], acc, sizeof acc);

    format(dest, size, "Cash %s / Account %s", cash, acc);

}

stock RW_CefBank(playerid)

{

    new subtitle[80];

    RW_BankSubtitle(playerid, subtitle, sizeof subtitle);

    if (!RW_CefOpen(playerid, CEF_MODE_BANK, "Bank", subtitle)) return RW_BankPanel(playerid);

    RW_CefRow(playerid, 0, "Put all cash on the account", "all", "wallet");

    RW_CefRow(playerid, 1, "Put 1 000 on the account", "1000", "wallet");

    RW_CefRow(playerid, 2, "Take 1 000 in cash", "1000", "cash");

    RW_CefRow(playerid, 3, "Transfer: /transfer <id> <sum>", "chat", "users");

    RW_CefAction(playerid, 8, "Close", "danger");

    return RW_CefEnd(playerid);

}

stock RW_BankPanel(playerid)

{

    new subtitle[80];

    RW_BankSubtitle(playerid, subtitle, sizeof subtitle);

    RW_UI_Panel(playerid, UI_BANK, "Bank", subtitle);

    RW_UI_Row(playerid, 0, "Put all cash on the account");

    RW_UI_Row(playerid, 1, "Put 1 000 on the account");

    RW_UI_Row(playerid, 2, "Take 1 000 in cash");

    RW_UI_Row(playerid, 3, "Transfer: /transfer <id> <sum>");

    RW_UI_Row(playerid, 7, "Close");

    return 1;

}

stock RW_BankAction(playerid, row, bool:cef)

{

    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);

    if (!RW_BankNear(playerid)) return RW_Error(playerid, "Step away from the ATM: the session is closed.");

    if (row == 0) return RW_BankDeposit(playerid, gMoney[playerid]);

    if (row == 1) return RW_BankDeposit(playerid, 1000);

    if (row == 2) return RW_BankWithdraw(playerid, 1000);

    if (row == 3) return RW_Info(playerid, "Use: /transfer <player id> <sum>. The money leaves your account.");

    return 1;

}

stock RW_BankCefClick(playerid, row) { return RW_BankAction(playerid, row, true); }

stock RW_BankClick(playerid, row) { return RW_BankAction(playerid, row, false); }

stock RW_BankDeposit(playerid, amount)

{

    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");

    if (!RW_BankNear(playerid)) return RW_Error(playerid, "No ATM nearby.");

    if (amount <= 0) return RW_Error(playerid, "Nothing to put on the account.");

    if (!RW_TakeMoney(playerid, amount)) return RW_Error(playerid, "Not enough cash.");

    gBank[playerid] += amount;

    RW_EconomySave(playerid);

    new sum[32], message[128];

    RW_FormatMoney(amount, sum, sizeof sum);

    format(message, sizeof message, "%s is on the account. Balance: %d.", sum, gBank[playerid]);

    return RW_Ok(playerid, message);

}

stock RW_BankWithdraw(playerid, amount)

{

    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");

    if (!RW_BankNear(playerid)) return RW_Error(playerid, "No ATM nearby.");

    if (amount <= 0) return RW_Error(playerid, "Nothing to take.");

    if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account.");

    gBank[playerid] -= amount;

    RW_AddMoney(playerid, amount);

    RW_EconomySave(playerid);

    new sum[32], message[128];

    RW_FormatMoney(amount, sum, sizeof sum);

    format(message, sizeof message, "%s in cash. Balance: %d.", sum, gBank[playerid]);

    return RW_Ok(playerid, message);

}

// Перевод со счёта на счёт: минимальная сумма отсекает мусор.

stock RW_BankTransfer(playerid, target, amount)

{

    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");

    if (target < 0 || target >= MAX_PLAYERS || target == playerid || !IsPlayerConnected(target) || !gLogged[target])

        return RW_Error(playerid, "Player with that id is not online.");

    if (amount < BANK_MIN_TRANSFER) return RW_Error(playerid, "At least 50 on a transfer.");

    if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account.");

    gBank[playerid] -= amount;

    gBank[target] += amount;

    RW_EconomySave(playerid);

    RW_EconomySave(target);

    new sum[32], message[144];

    RW_FormatMoney(amount, sum, sizeof sum);

    format(message, sizeof message, "Sent %s from the account. Balance: %d.", sum, gBank[playerid]);

    RW_Ok(playerid, message);

    format(message, sizeof message, "Account: +%s. Balance: %d.", sum, gBank[target]);

    return RW_Ok(target, message);

}

stock RW_BalanceCommand(playerid)

{

    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");

    new cash[32], acc[32], message[160];

    RW_FormatMoney(gMoney[playerid], cash, sizeof cash);

    RW_FormatMoney(gBank[playerid], acc, sizeof acc);

    format(message, sizeof message, "Cash: %s rub. Account: %s rub.%s", cash, acc,

        RW_BankNear(playerid) ? (" Use /bank for the terminal.") : (""));

    return RW_Info(playerid, message);

}

stock RW_BankCommand(playerid) { return RW_BankOpen(playerid); }

stock RW_DepositCommand(playerid, const params[])

{

    new word[32];

    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /deposit <sum>.");

    return RW_BankDeposit(playerid, strval(word));

}

stock RW_WithdrawCommand(playerid, const params[])

{

    new word[32];

    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /withdraw <sum>.");

    return RW_BankWithdraw(playerid, strval(word));

}

stock RW_TransferCommand(playerid, const params[])

{

    new a[32], b[32], rest[144];

    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);

    RW_ParseCommand(rest, b, sizeof b, rest, sizeof rest);

    if (!a[0] || !RW_IsNumeric(a) || !b[0] || !RW_IsNumeric(b)) return RW_Error(playerid, "Use: /transfer <player id> <sum>.");

    return RW_BankTransfer(playerid, strval(a), strval(b));

}


/*  ------------------------------ магазин 24/7 ----------------------------- */
/*  Покупка наличными: цена, свободное место в карманах и только потом выдача. */

stock bool:RW_ShopNear(playerid)
{
    for (new i; i < sizeof gShops; i++)
        if (IsPlayerInRangeOfPoint(playerid, SHOP_RANGE, gShops[i][0], gShops[i][1], gShops[i][2])) return true;
    return false;
}
stock RW_ShopOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_ShopNear(playerid)) return RW_Error(playerid, "No 24/7 shop nearby.");
    return RW_ShopRender(playerid);
}
stock RW_ShopRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefShop(playerid);
    #endif
    return RW_ShopPanel(playerid);
}
// Подпись строки: название товара и его цена.
stock RW_ShopRowLabel(row, dest[], size = sizeof dest)
{
    new name[32], price[32];
    RW_TrunkName(gShopItems[row][0], gShopItems[row][1], name, sizeof name);
    RW_FormatMoney(gShopItems[row][3], price, sizeof price);
    format(dest, size, "%s - %s", name, price);
}
stock RW_CefShop(playerid)
{
    new subtitle[64];
    RW_FormatMoney(gMoney[playerid], subtitle, sizeof subtitle);
    if (!RW_CefOpen(playerid, CEF_MODE_SHOP, "24/7 shop", subtitle)) return RW_ShopPanel(playerid);
    for (new i; i < sizeof gShopItems; i++) {
        new label[64];
        RW_ShopRowLabel(i, label, sizeof label);
        RW_CefRow(playerid, i, label, "buy", "bag");
    }
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_ShopPanel(playerid)
{
    new subtitle[64];
    RW_FormatMoney(gMoney[playerid], subtitle, sizeof subtitle);
    RW_UI_Panel(playerid, UI_SHOP, "24/7 shop", subtitle);
    for (new i; i < sizeof gShopItems; i++) {
        new label[64];
        RW_ShopRowLabel(i, label, sizeof label);
        RW_UI_Row(playerid, i, label);
    }
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_ShopAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row < 0 || row >= sizeof gShopItems) return 1;
    if (!RW_ShopNear(playerid)) return RW_Error(playerid, "Step away from the counter: the shop is closed.");
    return RW_ShopBuy(playerid, row);
}
stock RW_ShopCefClick(playerid, row) { return RW_ShopAction(playerid, row, true); }
stock RW_ShopClick(playerid, row) { return RW_ShopAction(playerid, row, false); }
stock RW_ShopBuy(playerid, row)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return RW_Error(playerid, "Log in first.");
    new type = gShopItems[row][0], data = gShopItems[row][1], amount = gShopItems[row][2], price = gShopItems[row][3];
    if (gMoney[playerid] < price) return RW_Error(playerid, "Not enough cash for it.");
    if (RW_InvFree(playerid, 0, type, data, amount) < 0) return RW_Error(playerid, "No room for it in your pockets.");
    new uid = RW_InvGrant(playerid, type, data, amount);
    if (!uid) return RW_Error(playerid, "Could not hand the goods over.");
    RW_TakeMoney(playerid, price);
    new name[32], sum[32], message[144];
    RW_TrunkName(type, data, name, sizeof name);
    RW_FormatMoney(price, sum, sizeof sum);
    format(message, sizeof message, "Bought %s for %s. Cash: %d.", name, sum, gMoney[playerid]);
    return RW_Ok(playerid, message);
}
stock RW_ShopCommand(playerid) { return RW_ShopOpen(playerid); }

/*  --------------------------------- работа -------------------------------- */
/*  Три работы: грузчик в порту пешком, курьер по городу, дальнобойщик между  *
 *  городами. Задание — две точки: взять и сдать. Оплата зависит от уровня.   */

stock RW_JobName(job, dest[], size = sizeof dest)
{
    if (job == JOB_LOADER) format(dest, size, "Loader");
    else if (job == JOB_COURIER) format(dest, size, "Courier");
    else if (job == JOB_TRUCKER) format(dest, size, "Trucker");
    else format(dest, size, "Unemployed");
    return 1;
}
// Начальная ставка задания: дальнобойщик возит далеко, грузчик — тяжело.
stock RW_JobBasePay(job)
{
    if (job == JOB_LOADER) return 200;
    if (job == JOB_COURIER) return 120;
    if (job == JOB_TRUCKER) return 300;
    return 0;
}
// Зарплата раз в PAYDAY_SECONDS, помноженная на уровень.
stock RW_JobSalary(job)
{
    if (job == JOB_LOADER) return 300;
    if (job == JOB_COURIER) return 350;
    if (job == JOB_TRUCKER) return 400;
    return 0;
}
// Индекс биржи, у которой стоит игрок: -1, если рядом нет ни одной.
stock RW_JobHireIndex(playerid)
{
    for (new i; i < sizeof gJobHire; i++)
        if (IsPlayerInRangeOfPoint(playerid, JOB_HIRE_RANGE, gJobHire[i][1], gJobHire[i][2], gJobHire[i][3])) return i;
    return -1;
}
stock RW_JobOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_JobHireIndex(playerid) < 0 && gJob[playerid] == JOB_NONE)
        return RW_Error(playerid, "No labour exchange nearby. /job works at the docks, the depot and the truck base.");
    return RW_JobRender(playerid);
}
stock RW_JobRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefJob(playerid);
    #endif
    return RW_JobPanel(playerid);
}
stock RW_CefJob(playerid)
{
    new name[24], subtitle[80];
    RW_JobName(gJob[playerid], name, sizeof name);
    format(subtitle, sizeof subtitle, "%s - level %d", name, gJobLevel[playerid]);
    if (!RW_CefOpen(playerid, CEF_MODE_JOB, "Labour exchange", subtitle)) return RW_JobPanel(playerid);
    RW_CefRow(playerid, 0, "Loader: crates in the docks", "on foot", "bag");
    RW_CefRow(playerid, 1, "Courier: parcels across the city", "any way", "map");
    RW_CefRow(playerid, 2, "Trucker: runs to other cities", "van", "eject");
    if (gJob[playerid] != JOB_NONE) RW_CefAction(playerid, 7, "Quit the job", "danger");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_JobPanel(playerid)
{
    new name[24], subtitle[80];
    RW_JobName(gJob[playerid], name, sizeof name);
    format(subtitle, sizeof subtitle, "%s - level %d", name, gJobLevel[playerid]);
    RW_UI_Panel(playerid, UI_JOB, "Labour exchange", subtitle);
    RW_UI_Row(playerid, 0, "Loader: crates in the docks");
    RW_UI_Row(playerid, 1, "Courier: parcels across the city");
    RW_UI_Row(playerid, 2, "Trucker: runs to other cities");
    RW_UI_Row(playerid, 3, "Quit the job");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_JobAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 3 && !cef) return RW_JobQuit(playerid);
    if (row == 7 && cef) return RW_JobQuit(playerid);
    if (row < 0 || row > 2) return 1;
    return RW_JobHire(playerid, row + 1);
}
stock RW_JobCefClick(playerid, row) { return RW_JobAction(playerid, row, true); }
stock RW_JobClick(playerid, row) { return RW_JobAction(playerid, row, false); }
stock RW_JobHire(playerid, job)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (job <= JOB_NONE || job >= JOB_COUNT) return RW_Error(playerid, "No such job.");
    if (RW_JobHireIndex(playerid) < 0) return RW_Error(playerid, "Come to the labour exchange: the docks, the depot or the truck base.");
    if (gJob[playerid] == job) return RW_Info(playerid, "You already work here.");
    if (gJobStage[playerid] > 0) RW_JobTaskCancel(playerid, true);
    gJob[playerid] = job;
    if (gJobLevel[playerid] < 1) gJobLevel[playerid] = 1;
    RW_EconomySave(playerid);
    RW_UI_Close(playerid);
    new name[24], message[160];
    RW_JobName(job, name, sizeof name);
    format(message, sizeof message, "Hired as %s, level %d. /work takes a task, /quitjob leaves.", name, gJobLevel[playerid]);
    return RW_Ok(playerid, message);
}
stock RW_JobQuit(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gJob[playerid] == JOB_NONE) return RW_Info(playerid, "You are not employed.");
    if (gJobStage[playerid] > 0) RW_JobTaskCancel(playerid, true);
    gJob[playerid] = JOB_NONE;
    RW_EconomySave(playerid);
    RW_UI_Close(playerid);
    return RW_Info(playerid, "You left the job. The level and the record stay on the card.");
}
// Точка задания: у каждой работы свои адреса.
stock RW_JobPoint(job, stage, &Float:x, &Float:y, &Float:z)
{
    new index = random(sizeof gCourierPoints);
    if (job == JOB_LOADER) {
        if (stage == 1) { index = random(sizeof gLoaderCrates); x = gLoaderCrates[index][0]; y = gLoaderCrates[index][1]; z = gLoaderCrates[index][2]; return 1; }
        x = gLoaderDrop[0]; y = gLoaderDrop[1]; z = gLoaderDrop[2];
        return 1;
    }
    if (job == JOB_TRUCKER) {
        if (stage == 1) { x = gJobHire[2][1]; y = gJobHire[2][2]; z = gJobHire[2][3]; return 1; }
        index = random(sizeof gTruckerPoints);
        x = gTruckerPoints[index][0]; y = gTruckerPoints[index][1]; z = gTruckerPoints[index][2];
        return 1;
    }
    if (stage == 1) { x = gJobHire[1][1]; y = gJobHire[1][2]; z = gJobHire[1][3]; return 1; }
    x = gCourierPoints[index][0]; y = gCourierPoints[index][1]; z = gCourierPoints[index][2];
    return 1;
}
stock bool:RW_JobVehicleOk(playerid)
{
    new vehicleid = GetPlayerVehicleID(playerid);
    if (!vehicleid) return false;
    new model = GetVehicleModel(vehicleid);
    for (new i; i < sizeof gTruckModels; i++) if (gTruckModels[i] == model) return true;
    return false;
}
// Взять задание: точка, срок, иконка на карте и обещанная оплата.
stock RW_JobTaskTake(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gJob[playerid] == JOB_NONE) return RW_Error(playerid, "Get a job first: /job at the labour exchange.");
    if (gJobStage[playerid] > 0) return RW_Info(playerid, "Finish the task you have taken.");
    if (GetTickCount() - gJobTick[playerid] < JOB_COOLDOWN_MS) return RW_Info(playerid, "Catch your breath for a second.");
    if (gJob[playerid] == JOB_TRUCKER && !RW_JobVehicleOk(playerid))
        return RW_Error(playerid, "A trucker works from the cab: take a van or a truck from the base.");
    if (gJob[playerid] == JOB_LOADER && GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
        return RW_Error(playerid, "A loader works on foot: step out of the car.");

    new Float:x, Float:y, Float:z;
    RW_JobPoint(gJob[playerid], 1, x, y, z);
    gJobStage[playerid] = 1;
    gJobX[playerid] = x; gJobY[playerid] = y; gJobZ[playerid] = z;
    gJobEnd[playerid] = gettime() + JOB_TASK_SECONDS;
    SetPlayerMapIcon(playerid, ECON_TASK_ICON, x, y, z, 0, 0xF5B04CFF, MAPICON_GLOBAL);
    gJobIcon[playerid] = true;
    gJobTick[playerid] = GetTickCount();
    new message[160];
    format(message, sizeof message, "Task: the mark on the map, %d m away. %d seconds to do it.",
        floatround(RW_EconomyDistance(playerid, x, y, z)), JOB_TASK_SECONDS);
    return RW_Info(playerid, message);
}
stock RW_JobTaskCancel(playerid, bool:silent = false)
{
    if (gJobIcon[playerid]) { RemovePlayerMapIcon(playerid, ECON_TASK_ICON); gJobIcon[playerid] = false; }
    gJobStage[playerid] = 0;
    gJobEnd[playerid] = 0;
    gJobPay[playerid] = 0;
    gJobTick[playerid] = GetTickCount();
    if (!silent) return RW_Error(playerid, "Out of time. The task is gone, take another one.");
    return 1;
}
// Сдать задание: деньги в карман, опыт в карточку, уровень растёт.
stock RW_JobTaskDone(playerid)
{
    new pay = RW_JobBasePay(gJob[playerid]) + gJobPay[playerid];
    pay = pay + floatround(float(pay) * 0.15 * float(gJobLevel[playerid] - 1));
    RW_AddMoney(playerid, pay);
    gJobTasks[playerid]++;
    gJobEarned[playerid] += pay;
    gJobExp[playerid]++;
    RW_JobTaskCancel(playerid, true);
    RW_EconomySave(playerid);
    new sum[32], message[160];
    RW_FormatMoney(pay, sum, sizeof sum);
    format(message, sizeof message, "Done: %s in cash. Cash: %d.", sum, gMoney[playerid]);
    RW_Ok(playerid, message);
    if (gJobExp[playerid] >= gJobLevel[playerid] * JOB_EXP_PER_LEVEL && gJobLevel[playerid] < JOB_MAX_LEVEL) {
        gJobLevel[playerid]++;
        RW_EconomySave(playerid);
        format(message, sizeof message, "Level %d. Tasks now pay better and the salary is higher.", gJobLevel[playerid]);
        return RW_Accent(playerid, message);
    }
    return 1;
}
// /work: взять задание, сдать его на точке или узнать, сколько ещё идти.
stock RW_WorkCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gJob[playerid] == JOB_NONE) return RW_Error(playerid, "Get a job first: /job at the labour exchange.");
    if (gJobStage[playerid] == 0) return RW_JobTaskTake(playerid);
    if (GetTickCount() - gJobTick[playerid] < 1000) return 1;
    gJobTick[playerid] = GetTickCount();
    if (!IsPlayerInRangeOfPoint(playerid, JOB_TASK_RANGE, gJobX[playerid], gJobY[playerid], gJobZ[playerid])) {
        new message[128];
        format(message, sizeof message, "Not there yet: %d m to the mark.", floatround(RW_EconomyDistance(playerid, gJobX[playerid], gJobY[playerid], gJobZ[playerid])));
        return RW_Info(playerid, message);
    }
    if (gJob[playerid] == JOB_TRUCKER && gJobStage[playerid] == 2 && GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
        return RW_Error(playerid, "Deliver it in the van: sit behind the wheel.");
    if (gJob[playerid] == JOB_LOADER && GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
        return RW_Error(playerid, "A loader works on foot: step out of the car.");

    if (gJobStage[playerid] == 1) {
        new Float:x, Float:y, Float:z;
        RW_JobPoint(gJob[playerid], 2, x, y, z);
        gJobStage[playerid] = 2;
        gJobX[playerid] = x; gJobY[playerid] = y; gJobZ[playerid] = z;
        gJobPay[playerid] = floatround(RW_EconomyDistance(playerid, x, y, z) / 6.0);
        gJobEnd[playerid] = gettime() + JOB_TASK_SECONDS * 2;
        RemovePlayerMapIcon(playerid, ECON_TASK_ICON);
        SetPlayerMapIcon(playerid, ECON_TASK_ICON, x, y, z, 0, 0x7BD88FFF, MAPICON_GLOBAL);
        gJobIcon[playerid] = true;
        new message[160];
        format(message, sizeof message, "Loaded. Take it %d m from here, you have %d seconds.", floatround(RW_EconomyDistance(playerid, x, y, z)), JOB_TASK_SECONDS * 2);
        return RW_Info(playerid, message);
    }
    return RW_JobTaskDone(playerid);
}
stock RW_JobCommand(playerid) { return RW_JobOpen(playerid); }
stock RW_QuitJobCommand(playerid) { return RW_JobQuit(playerid); }

/*  -------------------------------- помощники ------------------------------ */

stock Float:RW_EconomyDistance(playerid, Float:x, Float:y, Float:z)
{
    new Float:px, Float:py, Float:pz;
    GetPlayerPos(playerid, px, py, pz);
    return VectorSize(px - x, py - y, pz - z);
}

/*  -------------------------------- зарплата ------------------------------- */
/*  Раз в PAYDAY_SECONDS: оклад по работе и процент на остаток счёта.         */

stock RW_EconomyPayday(playerid)
{
    new salary = RW_JobSalary(gJob[playerid]) * gJobLevel[playerid];
    new interest = gBank[playerid] * BANK_INTEREST_PERCENT / 100;
    if (interest > BANK_INTEREST_MAX) interest = BANK_INTEREST_MAX;
    if (salary <= 0 && interest <= 0) return RW_Info(playerid, "Payday: nothing to pay. Get a job: /job at the labour exchange.");
    gBank[playerid] += salary + interest;
    RW_EconomySave(playerid);
    new message[192];
    if (salary > 0 && interest > 0)
        format(message, sizeof message, "Payday: salary %d and interest %d are on the account. Balance: %d.", salary, interest, gBank[playerid]);
    else if (salary > 0)
        format(message, sizeof message, "Payday: salary %d is on the account. Balance: %d.", salary, gBank[playerid]);
    else
        format(message, sizeof message, "Payday: interest %d is on the account. Balance: %d.", interest, gBank[playerid]);
    return RW_Ok(playerid, message);
}
stock RW_PaydayCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new left = PAYDAY_SECONDS - gPayday[playerid];
    if (left < 0) left = 0;
    new name[24], message[192];
    RW_JobName(gJob[playerid], name, sizeof name);
    format(message, sizeof message, "%s level %d: salary %d per payday. Next in %d s. Account: %d. Tasks: %d, earned: %d.",
        name, gJobLevel[playerid], RW_JobSalary(gJob[playerid]) * gJobLevel[playerid], left, gBank[playerid],
        gJobTasks[playerid], gJobEarned[playerid]);
    return RW_Info(playerid, message);
}

/*  ---------------------------------- таймер ------------------------------- */

public RW_EconomyPulse()
{
    for (new playerid; playerid < MAX_PLAYERS; playerid++) {
        if (!IsPlayerConnected(playerid) || !gLogged[playerid]) continue;
        if (gJobStage[playerid] > 0 && gettime() > gJobEnd[playerid]) RW_JobTaskCancel(playerid);
        gPayday[playerid]++;
        if (gPayday[playerid] >= PAYDAY_SECONDS) {
            gPayday[playerid] = 0;
            RW_EconomyPayday(playerid);
        }
    }
    return 1;
}

/*  ---------------------------------- точки -------------------------------- */
/*  Пикапы и подписи, чтобы точки было видно на карте и в игре.               */

stock RW_EconomyInit()
{
    for (new i; i < sizeof gShops; i++) {
        CreatePickup(1239, 1, gShops[i][0], gShops[i][1], gShops[i][2], 0);
        Create3DTextLabel("24/7 SHOP\n/shop - goods for cash", 0x7BD88FFF, gShops[i][0], gShops[i][1], gShops[i][2] + 1.1, 25.0, 0, 0);
    }
    for (new i; i < sizeof gAtms; i++) {
        CreatePickup(1559, 1, gAtms[i][0], gAtms[i][1], gAtms[i][2], 0);
        Create3DTextLabel("ATM\n/bank - account and cash", 0xF5B04CFF, gAtms[i][0], gAtms[i][1], gAtms[i][2] + 1.1, 25.0, 0, 0);
    }
    for (new i; i < sizeof gJobHire; i++) {
        new name[24], label[64];
        RW_JobName(floatround(gJobHire[i][0]), name, sizeof name);
        format(label, sizeof label, "LABOUR EXCHANGE\n%s: /job", name);
        CreatePickup(1239, 1, gJobHire[i][1], gJobHire[i][2], gJobHire[i][3], 0);
        Create3DTextLabel(label, 0xD8C39BFF, gJobHire[i][1], gJobHire[i][2], gJobHire[i][3] + 1.1, 25.0, 0, 0);
    }
    return 1;
}
