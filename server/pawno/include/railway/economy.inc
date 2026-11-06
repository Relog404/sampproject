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
#define JOB_GARBAGE             4
#define JOB_FARMER              5
#define JOB_MUSHROOM            6
#define JOB_QUARRY              7
#define JOB_LUMBER              8
#define JOB_POST                9
#define JOB_BUS                 10
#define JOB_TAXI                11
#define JOB_COUNT               12
#define JOB_MAX_LEVEL           5
#define JOB_EXP_PER_LEVEL       10          /* заданий до следующего уровня  */
#define JOB_TASK_SECONDS        240         /* срок на один этап задания     */
#define JOB_TASK_RANGE          8.0
#define JOB_HIRE_RANGE          12.0
#define JOB_COOLDOWN_MS         2000        /* пауза между заданиями         */
#define TRUCKER_WAIT_SECONDS    20          /* пока освободится рампа        */
#define TRUCKER_URGENT_BONUS    25          /* срочный заказ получателя      */
#define TAXI_JOB_BONUS          15          /* % к счётчику за работу такси  */
#define TAXI_JOB_LEVEL_BONUS    5           /* и ещё столько за каждый уровень */
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

/* Магазины 24/7 Los Santos: реальные точки карты. */
static const Float:gShops[][3] = {
    {  1352.4, -1759.4, 13.5 },     /* Downtown, у City Hall   */
    {  2244.4, -1665.4, 13.5 },     /* Ganton                  */
    {  2503.4, -1722.4, 13.4 },     /* Las Colinas             */
    {  1833.4, -1842.4, 13.5 }      /* Unity                   */
};

/* Банкоматы: вокзал, больница, заправка в Temple и участок LSPD. */
static const Float:gAtms[][3] = {
    {  1742.9, -1862.6, 13.6 },     /* Unity Station           */
    {  1175.8, -1324.7, 15.4 },     /* All Saints General      */
    {  1004.0,  -939.0, 42.2 },     /* Temple, заправка        */
    {  1544.6, -1675.5, 16.2 }      /* LSPD, Pershing Square   */
};

/* Биржа труда: работа, x, y, z. Реальные районы и города San Andreas. */
static const Float:gJobHire[][4] = {
    { 1.0,  2745.0, -2372.0, 13.6 }, /* грузчик: Ocean Docks, пакгаузы        */
    { 2.0,  1742.9, -1862.6, 13.6 }, /* курьер: Unity Station                 */
    { 3.0,  2670.0, -2330.0, 13.6 }, /* дальнобойщик: контейнерный терминал   */
    { 4.0,  2125.0, -1975.0, 13.5 }, /* мусорщик: Willowfield, депо           */
    { 5.0,   250.0,  -180.0,  6.0 }, /* фермер: Blueberry Acres, Red County   */
    { 6.0,  2300.0,   100.0, 26.0 }, /* грибник: Palomino Creek               */
    { 7.0,   620.0,   860.0, 25.0 }, /* карьерщик: Hunter Quarry, Bone County */
    { 8.0, -2170.0, -2385.0, 30.0 }, /* лесоруб: Angel Pine, Whetstone        */
    { 9.0,  1950.0, -1750.0, 13.5 }, /* почтальон: Idlewood, почта            */
    { 10.0, 1850.0, -1880.0, 13.5 }, /* водитель: El Corona, автобусный парк  */
    { 11.0, 1900.0, -2280.0, 13.5 }  /* таксист: LS International, стоянка    */
};

/* Ящики у причалов Ocean Docks и склад. */
static const Float:gLoaderCrates[][3] = {
    { 2720.0, -2400.0, 13.6 },
    { 2750.0, -2400.0, 13.6 },
    { 2780.0, -2400.0, 13.6 }
};
static const Float:gLoaderDrop[3] = { 2750.0, -2330.0, 13.6 };

/* Адреса курьера: настоящие заведения Los Santos. */
static const Float:gCourierPoints[][3] = {
    { 1742.9, -1862.6, 13.6 },      /* Unity Station      */
    { 1175.8, -1324.7, 15.4 },      /* All Saints General */
    { 1544.6, -1675.5, 16.2 },      /* LSPD HQ            */
    { 1352.4, -1759.4, 13.5 },      /* Downtown 24/7      */
    { 2244.4, -1665.4, 13.5 },      /* Ganton 24/7        */
    { 2503.4, -1722.4, 13.4 },      /* Las Colinas 24/7   */
    { 1944.3, -1772.5, 13.4 }       /* Idlewood, заправка */
};

/* Дальние города: San Fierro, Bayside и Dillimore. */
static const Float:gTruckerPoints[][3] = {
    { -1935.6,  239.8, 34.0 },      /* San Fierro, Doherty     */
    { -2350.0, 2345.0,  5.0 },      /* Bayside, Tierra Robada  */
    {   660.5, -565.0, 16.3 }       /* Dillimore, Red County   */
};

/* На чём можно работать дальнобойщиком. */
static const gTruckModels[] = { 414, 455, 403, 443, 515, 514, 499, 498, 609, 524 };

/* Мусорщик: баки по городу и свалка в Willowfield. */
static const Float:gGarbageBins[][3] = {
    { 1352.4, -1759.4, 13.5 },      /* Downtown    */
    { 2244.4, -1665.4, 13.5 },      /* Ganton      */
    { 1944.3, -1772.5, 13.4 },      /* Idlewood    */
    { 2503.4, -1722.4, 13.4 }       /* Las Colinas */
};
static const Float:gGarbageDump[3] = { 2125.0, -1975.0, 13.5 };

/* Фермер: грядки Blueberry Acres и амбар. */
static const Float:gFarmRows[][3] = {
    {  220.0, -200.0, 5.0 },
    {  260.0, -200.0, 5.0 },
    {  300.0, -200.0, 5.0 }
};
static const Float:gFarmBarn[3] = { 250.0, -150.0, 6.0 };

/* Грибник: лес у Palomino Creek и скупщик в посёлке. */
static const Float:gMushroomSpots[][3] = {
    { 2250.0,  60.0, 26.0 },
    { 2290.0,  90.0, 26.0 },
    { 2320.0,  50.0, 26.0 }
};
static const Float:gMushroomBuyer[3] = { 2270.0, 70.0, 26.0 };

/* Карьерщик: жилы Hunter Quarry и дробилка. */
static const Float:gQuarryVeins[][3] = {
    { 580.0, 830.0, 20.0 },
    { 620.0, 850.0, 20.0 },
    { 660.0, 820.0, 20.0 }
};
static const Float:gQuarryCrusher[3] = { 620.0, 880.0, 25.0 };

/* Лесоруб: делянка у Angel Pine и лесопилка. */
static const Float:gTreeSpots[][3] = {
    { -2150.0, -2400.0, 30.0 },
    { -2180.0, -2360.0, 30.0 },
    { -2210.0, -2420.0, 30.0 }
};
static const Float:gSawmill[3] = { -2170.0, -2385.0, 30.0 };

/* Почтальон: ящики по городу и почта в Idlewood. */
static const Float:gPostBoxes[][3] = {
    { 1400.0, -1720.0, 13.5 },      /* Downtown    */
    { 2260.0, -1690.0, 13.5 },      /* Ganton      */
    { 2490.0, -1690.0, 13.4 }       /* Las Colinas */
};
static const Float:gPostOffice[3] = { 1950.0, -1750.0, 13.5 };

/* Автобусный маршрут: кольцо по настоящим районам Лос-Сантоса. */
static const Float:gBusStops[][3] = {
    { 2202.2, -1965.0, 13.6 },      /* Willowfield   */
    { 2503.4, -1722.4, 13.4 },      /* Las Colinas   */
    { 2244.4, -1665.4, 13.5 },      /* Ganton        */
    { 1742.9, -1862.6, 13.6 },      /* Unity Station */
    { 1352.4, -1759.4, 13.5 },      /* Downtown      */
    { 1175.8, -1324.7, 15.4 }       /* All Saints    */
};

/* Получатели груза в других городах: встреча на месте разгрузки. */
static const gTruckerNames[][] = {
    "Mr. Whitaker", "Ms. Alvarez", "Mr. Okafor", "Ms. Kowalski", "Mr. Delgado"
};

/* Автобусы и такси: на чём работают водитель и таксист. */
static const gBusModels[] = { 431, 437 };
static const gTaxiModels[] = { 420, 438 };
new gBank[MAX_PLAYERS];               /* деньги на счету                   */
new gJob[MAX_PLAYERS];                /* JOB_NONE…JOB_POST                 */
new gJobLevel[MAX_PLAYERS];
new gJobExp[MAX_PLAYERS];
new gJobTasks[MAX_PLAYERS];
new gJobEarned[MAX_PLAYERS];
new gJobPage[MAX_PLAYERS];            /* страница списка профессий        */
new gJobBusStop[MAX_PLAYERS];         /* следующая остановка маршрута     */
new gJobMeet[MAX_PLAYERS];            /* 0 нет, 1 ждём рампу, 2 сдал, 3 срочный */
new gJobMeetEnd[MAX_PLAYERS];         /* когда рампа освободится            */
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
    gJobPage[playerid] = 0;
    gJobBusStop[playerid] = 0;
    gJobMeet[playerid] = 0;
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
    else if (job == JOB_GARBAGE) format(dest, size, "Garbage collector");
    else if (job == JOB_FARMER) format(dest, size, "Farmer");
    else if (job == JOB_MUSHROOM) format(dest, size, "Mushroom picker");
    else if (job == JOB_QUARRY) format(dest, size, "Quarry worker");
    else if (job == JOB_LUMBER) format(dest, size, "Lumberjack");
    else if (job == JOB_POST) format(dest, size, "Postman");
    else if (job == JOB_BUS) format(dest, size, "Bus driver");
    else if (job == JOB_TAXI) format(dest, size, "Taxi driver");
    else format(dest, size, "Unemployed");
    return 1;
}
/* Подпись профессии в меню биржи труда. */
stock RW_JobTitle(job, dest[], size = sizeof dest)
{
    if (job == JOB_LOADER) format(dest, size, "Loader: crates in the docks");
    else if (job == JOB_COURIER) format(dest, size, "Courier: parcels across the city");
    else if (job == JOB_TRUCKER) format(dest, size, "Trucker: runs to other cities");
    else if (job == JOB_GARBAGE) format(dest, size, "Garbage: bins around the city");
    else if (job == JOB_FARMER) format(dest, size, "Farmer: rows in Blueberry Acres");
    else if (job == JOB_MUSHROOM) format(dest, size, "Mushroom picker: woods by the creek");
    else if (job == JOB_QUARRY) format(dest, size, "Quarry worker: Hunter Quarry");
    else if (job == JOB_LUMBER) format(dest, size, "Lumberjack: Angel Pine forest");
    else if (job == JOB_POST) format(dest, size, "Postman: boxes across the city");
    else if (job == JOB_BUS) format(dest, size, "Bus driver: the city ring route");
    else if (job == JOB_TAXI) format(dest, size, "Taxi driver: fares from Uber Drive");
    else format(dest, size, "Unemployed");
    return 1;
}
/* Подсказка: когда профессия выгоднее. */
stock RW_JobHint(job, dest[], size = sizeof dest)
{
    if (job == JOB_GARBAGE) format(dest, size, "best 06:00-13:00");
    else if (job == JOB_BUS) format(dest, size, "best at 07:00-09:30 and 17:00-19:30");
    else if (job == JOB_TAXI) format(dest, size, "the meter pays, no tasks");
    else if (job == JOB_FARMER) format(dest, size, "best in extra sunny");
    else if (job == JOB_MUSHROOM) format(dest, size, "best in rain");
    else if (job == JOB_QUARRY) format(dest, size, "best in fog");
    else if (job == JOB_LUMBER) format(dest, size, "best in clear weather");
    else if (job == JOB_TRUCKER || job == JOB_COURIER) format(dest, size, "best in clear weather");
    else format(dest, size, "any way");
    return 1;
}
// Начальная ставка задания: дальнобойщик возит далеко, грузчик — тяжело.
stock RW_JobBasePay(job)
{
    if (job == JOB_LOADER) return 200;
    if (job == JOB_COURIER) return 120;
    if (job == JOB_TRUCKER) return 300;
    if (job == JOB_GARBAGE) return 180;
    if (job == JOB_FARMER) return 170;
    if (job == JOB_MUSHROOM) return 160;
    if (job == JOB_QUARRY) return 220;
    if (job == JOB_LUMBER) return 230;
    if (job == JOB_POST) return 140;
    if (job == JOB_BUS) return 260;
    return 0;                                   /* таксист получает по счётчику */
}
// Зарплата раз в PAYDAY_SECONDS, помноженная на уровень.
stock RW_JobSalary(job)
{
    if (job == JOB_LOADER) return 300;
    if (job == JOB_COURIER) return 350;
    if (job == JOB_TRUCKER) return 400;
    if (job == JOB_GARBAGE) return 320;
    if (job == JOB_FARMER) return 300;
    if (job == JOB_MUSHROOM) return 290;
    if (job == JOB_QUARRY) return 400;
    if (job == JOB_LUMBER) return 380;
    if (job == JOB_POST) return 330;
    if (job == JOB_BUS) return 420;
    if (job == JOB_TAXI) return 300;
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
        return RW_Error(playerid, "No labour exchange nearby. /job works at hiring points across the state.");
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
    for (new job = 1; job < JOB_COUNT; job++) {
        new title[72], hint[32];
        RW_JobTitle(job, title, sizeof title);
        RW_JobHint(job, hint, sizeof hint);
        RW_CefRow(playerid, job - 1, title, hint, "bag");
    }
    if (gJob[playerid] != JOB_NONE) RW_CefAction(playerid, CEF_JOB_QUIT, "Quit the job", "danger");
    RW_CefAction(playerid, CEF_JOB_CLOSE, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_JobPanel(playerid)
{
    new name[24], subtitle[80];
    RW_JobName(gJob[playerid], name, sizeof name);
    format(subtitle, sizeof subtitle, "%s - level %d", name, gJobLevel[playerid]);
    RW_UI_Panel(playerid, UI_JOB, "Labour exchange", subtitle);
    /* В текстовом меню восемь строк: пять профессий, перелистывание, увольнение и выход. */
    new first = gJobPage[playerid] * 5 + 1;
    for (new row = 0; row < 5; row++) {
        new title[72];
        if (first + row < JOB_COUNT) RW_JobTitle(first + row, title, sizeof title);
        else format(title, sizeof title, "-");
        RW_UI_Row(playerid, row, title);
    }
    if (first + 5 < JOB_COUNT) RW_UI_Row(playerid, 5, "Next jobs");
    else RW_UI_Row(playerid, 5, "First jobs");
    if (gJob[playerid] != JOB_NONE) RW_UI_Row(playerid, 6, "Quit the job");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_JobAction(playerid, row, bool:cef)
{
    if (row == (cef ? CEF_JOB_CLOSE : 7)) return RW_UI_Close(playerid);
    if (cef) {
        if (row == CEF_JOB_QUIT) return RW_JobQuit(playerid);
        if (row < 0 || row >= JOB_COUNT - 1) return 1;
        return RW_JobHire(playerid, row + 1);
    }
    if (row == 6) return RW_JobQuit(playerid);
    if (row == 5) { gJobPage[playerid] = (gJobPage[playerid] + 1) % ((JOB_COUNT - 2) / 5 + 1); return RW_JobPanel(playerid); }
    if (row < 0 || row > 4) return 1;
    new job = gJobPage[playerid] * 5 + 1 + row;
    if (job < 1 || job >= JOB_COUNT) return 1;
    return RW_JobHire(playerid, job);
}
stock RW_JobCefClick(playerid, row) { return RW_JobAction(playerid, row, true); }
stock RW_JobClick(playerid, row) { return RW_JobAction(playerid, row, false); }
stock RW_JobHire(playerid, job)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (job <= JOB_NONE || job >= JOB_COUNT) return RW_Error(playerid, "No such job.");
    if (RW_JobHireIndex(playerid) < 0) return RW_Error(playerid, "Come to a hiring point: the docks, the station, the depots and the farms.");
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
stock RW_JobPoint(playerid, job, stage, &Float:x, &Float:y, &Float:z)
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
    if (job == JOB_GARBAGE) {
        if (stage == 1) { index = random(sizeof gGarbageBins); x = gGarbageBins[index][0]; y = gGarbageBins[index][1]; z = gGarbageBins[index][2]; return 1; }
        x = gGarbageDump[0]; y = gGarbageDump[1]; z = gGarbageDump[2];
        return 1;
    }
    if (job == JOB_FARMER) {
        if (stage == 1) { index = random(sizeof gFarmRows); x = gFarmRows[index][0]; y = gFarmRows[index][1]; z = gFarmRows[index][2]; return 1; }
        x = gFarmBarn[0]; y = gFarmBarn[1]; z = gFarmBarn[2];
        return 1;
    }
    if (job == JOB_MUSHROOM) {
        if (stage == 1) { index = random(sizeof gMushroomSpots); x = gMushroomSpots[index][0]; y = gMushroomSpots[index][1]; z = gMushroomSpots[index][2]; return 1; }
        x = gMushroomBuyer[0]; y = gMushroomBuyer[1]; z = gMushroomBuyer[2];
        return 1;
    }
    if (job == JOB_QUARRY) {
        if (stage == 1) { index = random(sizeof gQuarryVeins); x = gQuarryVeins[index][0]; y = gQuarryVeins[index][1]; z = gQuarryVeins[index][2]; return 1; }
        x = gQuarryCrusher[0]; y = gQuarryCrusher[1]; z = gQuarryCrusher[2];
        return 1;
    }
    if (job == JOB_LUMBER) {
        if (stage == 1) { index = random(sizeof gTreeSpots); x = gTreeSpots[index][0]; y = gTreeSpots[index][1]; z = gTreeSpots[index][2]; return 1; }
        x = gSawmill[0]; y = gSawmill[1]; z = gSawmill[2];
        return 1;
    }
    if (job == JOB_BUS) {
        new stop = gJobBusStop[playerid] % sizeof gBusStops;
        if (stage == 2) stop = (stop + 1) % sizeof gBusStops;
        x = gBusStops[stop][0]; y = gBusStops[stop][1]; z = gBusStops[stop][2];
        return 1;
    }
    if (job == JOB_TAXI) return 0;          /* таксист работает по вызовам   */
    if (job == JOB_POST) {
        if (stage == 1) { index = random(sizeof gPostBoxes); x = gPostBoxes[index][0]; y = gPostBoxes[index][1]; z = gPostBoxes[index][2]; return 1; }
        x = gPostOffice[0]; y = gPostOffice[1]; z = gPostOffice[2];
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
    if (gJob[playerid] == JOB_TAXI)
        return RW_Error(playerid, "A taxi driver takes orders in the Uber Drive app: /uber or /taxi.");
    if (gJob[playerid] == JOB_BUS && !RW_InModelList(RW_JobVehicleModel(playerid), gBusModels, sizeof gBusModels))
        return RW_Error(playerid, "The route runs on a coach: take a bus from the depot in El Corona.");
    gJobMeet[playerid] = 0;

    new Float:x, Float:y, Float:z;
    RW_JobPoint(playerid, gJob[playerid], 1, x, y, z);
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
    gJobPage[playerid] = 0;
    gJobBusStop[playerid] = 0;
    gJobMeet[playerid] = 0;
    gJobEnd[playerid] = 0;
    gJobPay[playerid] = 0;
    gJobTick[playerid] = GetTickCount();
    if (!silent) return RW_Error(playerid, "Out of time. The task is gone, take another one.");
    return 1;
}
// Сдать задание: деньги в карман, опыт в карточку, уровень растёт.
/* Модель машины под игроком: 0, если он не за рулём. */
stock RW_JobVehicleModel(playerid)
{
    if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return 0;
    new vehicleid = GetPlayerVehicleID(playerid);
    if (vehicleid <= 0 || vehicleid >= MAX_VEHICLES) return 0;
    return GetVehicleModel(vehicleid);
}
/* Есть ли модель в списке допустимых. */
stock bool:RW_InModelList(model, const list[], size)
{
    if (model <= 0) return false;
    for (new i; i < size; i++) if (list[i] == model) return true;
    return false;
}
/* Таксист: надбавка к счётчику Uber Drive за работу на такси. */
stock RW_TaxiJobBonus(playerid)
{
    if (gJob[playerid] != JOB_TAXI) return 0;
    if (!RW_InModelList(RW_JobVehicleModel(playerid), gTaxiModels, sizeof gTaxiModels)) return 0;
    return TAXI_JOB_BONUS + (gJobLevel[playerid] - 1) * TAXI_JOB_LEVEL_BONUS;
}
/* Y на точке сдачи: сдать задание, не открывая меню. */
public RW_JobHandOver(playerid)
{
    if (gJob[playerid] == JOB_NONE || gJobStage[playerid] != 2) return 0;
    if (!IsPlayerInRangeOfPoint(playerid, JOB_TASK_RANGE, gJobX[playerid], gJobY[playerid], gJobZ[playerid])) return 0;
    RW_WorkCommand(playerid);
    return 1;
}
/* Бонус времени суток: у каждой профессии своё удачное окно. */
stock RW_JobTimeBonus(job)
{
    new hour, minute, second;
    gettime(hour, minute, second);
    #pragma unused second
    if (job == JOB_GARBAGE && hour >= 6 && hour < 13) return 15;   /* утренний вывоз   */
    if (job == JOB_POST && hour >= 9 && hour < 18) return 10;      /* рабочий день     */
    if (job == JOB_BUS) {
        /* Часы пик: 07:00-09:30 и 17:00-19:30. */
        new minutes = hour * 60 + minute;
        if (minutes >= 420 && minutes < 570) return 25;
        if (minutes >= 1020 && minutes < 1170) return 25;
        return 0;
    }
    return 0;
}
stock RW_JobTaskDone(playerid)
{
    new pay = RW_JobBasePay(gJob[playerid]) + gJobPay[playerid];
    pay = pay + floatround(float(pay) * 0.15 * float(gJobLevel[playerid] - 1));
    new weatherBonus = RW_WeatherBonus(gJob[playerid]);
    new timeBonus = RW_JobTimeBonus(gJob[playerid]);
    new bonus = weatherBonus + timeBonus;
    if (gJobMeet[playerid] == 3) bonus = bonus + TRUCKER_URGENT_BONUS;
    if (bonus != 0) pay = pay + floatround(float(pay) * float(bonus) / 100.0);
    RW_AddMoney(playerid, pay);
    gJobTasks[playerid]++;
    gJobEarned[playerid] += pay;
    gJobExp[playerid]++;
    RW_JobTaskCancel(playerid, true);
    RW_EconomySave(playerid);
    new sum[32], message[160];
    RW_FormatMoney(pay, sum, sizeof sum);
    if (bonus != 0)
        format(message, sizeof message, "Done: %s in cash (%+d%% for weather and time). Cash: %d.", sum, bonus, gMoney[playerid]);
    else
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

    /* Дальнобойщик: на месте разгрузки сервер разыгрывает встречу с получателем. */
    if (gJobStage[playerid] == 2 && gJob[playerid] == JOB_TRUCKER) {
        new name[24], message[160];
        if (gJobMeet[playerid] == 1) {
            if (gettime() < gJobMeetEnd[playerid]) {
                format(message, sizeof message, "The ramp is still busy: %d s to wait.", gJobMeetEnd[playerid] - gettime());
                return RW_Info(playerid, message);
            }
            gJobMeet[playerid] = 2;
            return RW_JobTaskDone(playerid);
        }
        if (gJobMeet[playerid] == 0) {
            format(name, sizeof name, "%s", gTruckerNames[random(sizeof gTruckerNames)]);
            new roll = random(100);
            if (roll < 30) {
                /* Рампа занята: груз примут через паузу, задание висит. */
                gJobMeet[playerid] = 1;
                gJobMeetEnd[playerid] = gettime() + TRUCKER_WAIT_SECONDS;
                format(message, sizeof message, "%s: the ramp is busy, wait %d seconds and press /work again.", name, TRUCKER_WAIT_SECONDS);
                return RW_Info(playerid, message);
            }
            if (roll < 50) {
                /* Срочный заказ: получатель доплачивает. */
                gJobMeet[playerid] = 3;
                format(message, sizeof message, "%s: urgent order, the cargo is taken at once.", name);
                RW_Info(playerid, message);
                return RW_JobTaskDone(playerid);
            }
            gJobMeet[playerid] = 2;
            format(message, sizeof message, "%s signed the papers for the cargo.", name);
            RW_Info(playerid, message);
            return RW_JobTaskDone(playerid);
        }
    }
    if (gJob[playerid] == JOB_BUS && gJobStage[playerid] == 2)
        gJobBusStop[playerid] = (gJobBusStop[playerid] + 1) % sizeof gBusStops;

    if (gJobStage[playerid] == 1) {
        new Float:x, Float:y, Float:z;
        RW_JobPoint(playerid, gJob[playerid], 2, x, y, z);
        gJobStage[playerid] = 2;
        gJobX[playerid] = x; gJobY[playerid] = y; gJobZ[playerid] = z;
        gJobPay[playerid] = floatround(RW_EconomyDistance(playerid, x, y, z) / 6.0);
        gJobEnd[playerid] = gettime() + JOB_TASK_SECONDS * 2;
        RemovePlayerMapIcon(playerid, ECON_TASK_ICON);
        SetPlayerMapIcon(playerid, ECON_TASK_ICON, x, y, z, 0, 0x7BD88FFF, MAPICON_GLOBAL);
        gJobIcon[playerid] = true;
        new message[160];
        format(message, sizeof message, "Got it. Carry it %d m from here, you have %d seconds.", floatround(RW_EconomyDistance(playerid, x, y, z)), JOB_TASK_SECONDS * 2);
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
