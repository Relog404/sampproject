/*  railway/donate.inc
 *
 *  Донат-меню: открывается по F2 из HUD или командой /donate. Очки хранятся
 *  в отдельной таблице, поэтому схему accounts трогать не нужно. Покупки
 *  применяет сервер: клиент передаёт только номер пакета.
 */

#if defined _railway_donate_included
    #endinput
#endif
#define _railway_donate_included

#define DONATE_MAX          6           /* пакетов в меню                  */
#define DONATE_KIND_VIP     (1)
#define DONATE_KIND_MONEY   (2)
#define DONATE_VIP_BONUS    {0, 10, 25, 50}  /* прибавка к доходу, %       */

new gDonate[MAX_PLAYERS];               /* донат-очки                      */
new gVip[MAX_PLAYERS];                  /* уровень VIP: 0..3               */
new gDonateRow[MAX_PLAYERS];            /* выбранная строка меню           */

static const gDonateName[DONATE_MAX][] = {
    "VIP Basic: +10 % to every income",
    "VIP Gold: +25 % to every income",
    "VIP Platinum: +50 % to every income",
    "50 000 on the account",
    "250 000 on the account",
    "1 000 000 on the account"
};
static const gDonatePrice[DONATE_MAX] = { 100, 250, 500, 50, 200, 600 };
static const gDonateKind[DONATE_MAX] = { DONATE_KIND_VIP, DONATE_KIND_VIP, DONATE_KIND_VIP,
                                         DONATE_KIND_MONEY, DONATE_KIND_MONEY, DONATE_KIND_MONEY };
static const gDonateArg[DONATE_MAX] = { 1, 2, 3, 50000, 250000, 1000000 };

/*  --------------------------------- база -------------------------------- */
stock RW_DonateSchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS donates (player_id INTEGER PRIMARY KEY, points INT, vip INT)");
}
stock RW_DonateLoad(playerid)
{
    gDonate[playerid] = 0; gVip[playerid] = 0;
    if (gAccountId[playerid] <= 0) return 0;
    new query[128];
    format(query, sizeof query, "SELECT points,vip FROM donates WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r)) {
        gDonate[playerid] = db_get_field_assoc_int(r, "points");
        gVip[playerid] = db_get_field_assoc_int(r, "vip");
    }
    db_free_result(r);
    return 1;
}
stock RW_DonateSave(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[160];
    format(query, sizeof query, "INSERT OR REPLACE INTO donates (player_id,points,vip) VALUES (%d,%d,%d)",
        gAccountId[playerid], gDonate[playerid], gVip[playerid]);
    return RW_DB_Exec(query);
}
stock RW_DonateReset(playerid) { gDonate[playerid] = 0; gVip[playerid] = 0; gDonateRow[playerid] = 0; return 1; }

/* Прибавка VIP к любому доходу: считает сервер, а не клиент. */
stock RW_DonateBonus(playerid, amount)
{
    new bonus = gVip[playerid];
    if (bonus < 0) bonus = 0;
    if (bonus > 3) bonus = 3;
    new const percent[] = DONATE_VIP_BONUS;
    return amount + amount * percent[bonus] / 100;
}
stock RW_DonateLevel(playerid) { return gVip[playerid]; }

/*  --------------------------------- меню -------------------------------- */
stock RW_DonateSubtitle(playerid, dest[], size = sizeof dest)
{
    return format(dest, size, "Balance: %d points. VIP: %d. How to top up: see the project site.", gDonate[playerid], gVip[playerid]);
}
stock RW_DonateRowText(playerid, row, dest[], size = sizeof dest)
{
    if (row < 0 || row >= DONATE_MAX) return 0;
    return format(dest, size, "%s - %d points", gDonateName[row], gDonatePrice[row]);
}
stock RW_DonateRows(playerid)
{
    for (new row; row < DONATE_MAX; row++) {
        new text[96];
        RW_DonateRowText(playerid, row, text, sizeof text);
        RW_DonateRowOut(playerid, row, text, "buy", "star");
    }
    return 1;
}
stock RW_DonateRowOut(playerid, row, const text[], const arg[], const icon[])
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefRow(playerid, row, text, arg, icon); return 1; }
    #else
        #pragma unused arg, icon
    #endif
    RW_UI_Row(playerid, row, text);
    return 1;
}
stock RW_DonateOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new subtitle[120];
    RW_DonateSubtitle(playerid, subtitle, sizeof subtitle);
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_DONATE, "Donate", subtitle)) {
            RW_DonateRows(playerid);
            RW_CefAction(playerid, 8, "Close", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_DONATE, "Donate", subtitle);
    RW_DonateRows(playerid);
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_DonateAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    gDonateRow[playerid] = row;
    if (cef) return RW_DonateBuy(playerid, row);
    /* Текстовое меню: строка открывает покупку, повторный клик подтверждает. */
    return RW_Info(playerid, "Press again on the same line to confirm the purchase.");
}
stock RW_DonateClick(playerid, row)
{
    if (row < 0 || row >= DONATE_MAX) return RW_UI_Close(playerid);
    if (gDonateRow[playerid] != row) {
        gDonateRow[playerid] = row;
        return RW_Info(playerid, "Press again on the same line to confirm the purchase.");
    }
    RW_DonateBuy(playerid, row);
    return RW_UI_Close(playerid);
}
stock RW_DonateCefClick(playerid, id) { return RW_DonateAction(playerid, id, true); }
stock RW_DonateBuy(playerid, row)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (row < 0 || row >= DONATE_MAX) return RW_Error(playerid, "No such package.");
    if (gDonate[playerid] < gDonatePrice[row])
        return RW_Error(playerid, "Not enough points: top up the balance on the project site.");
    if (gDonateKind[row] == DONATE_KIND_VIP && gVip[playerid] >= gDonateArg[row])
        return RW_Error(playerid, "You already have this VIP level or higher.");
    gDonate[playerid] = gDonate[playerid] - gDonatePrice[row];
    if (gDonateKind[row] == DONATE_KIND_VIP) {
        gVip[playerid] = gDonateArg[row];
        RW_DonateSave(playerid);
        return RW_Ok(playerid, "VIP is active. The bonus is added to every income by the server.");
    }
    RW_BankAdd(playerid, gDonateArg[row]);
    RW_DonateSave(playerid);
    new sum[32], message[160];
    RW_FormatMoney(gDonateArg[row], sum, sizeof sum);
    format(message, sizeof message, "Purchased: %s landed on the account.", sum);
    return RW_Ok(playerid, message);
}

/*  --------------------------------- команды ----------------------------- */
stock RW_DonateCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_CefOn(playerid)) return RW_DonateOpen(playerid);
    RW_Info(playerid, "Donate: F2 or /donate in the browser interface. Points are topped up on the project site.");
    return RW_DonateOpen(playerid);
}
stock RW_PointsCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new message[160];
    format(message, sizeof message, "Donate points: %d. VIP: %d (income bonus %d %%).",
        gDonate[playerid], gVip[playerid], RW_DonateBonus(playerid, 100) - 100);
    return RW_Ok(playerid, message);
}
stock RW_GiveDonateCommand(playerid, const params[])
{
    if (gAdminLevel[playerid] < 5) return RW_Error(playerid, "Admins only.");
    new target = strval(params), rest[32], i;
    while (params[i] && params[i] != ' ') i++;
    while (params[i] == ' ') i++;
    strmid(rest, params, i, strlen(params), sizeof rest);
    new amount = strval(rest);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target])
        return RW_Error(playerid, "Player is not online.");
    if (amount <= 0) return RW_Error(playerid, "Tell how many points: /givedonate <id> <points>.");
    gDonate[target] = gDonate[target] + amount;
    RW_DonateSave(target);
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(target, nick);
    format(message, sizeof message, "Added %d points to %s (now %d).", amount, nick, gDonate[target]);
    return RW_Ok(playerid, message);
}
