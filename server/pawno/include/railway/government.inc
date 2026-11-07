/*  ============================================================================
 *  railway/government.inc
 *  Мэрия: лицензия адвоката, справка о несудимости и договор с адвокатом.
 *
 *  Раньше лицензию адвоката выдавал только админ. Теперь её покупают
 *  в мэрии на реальной точке карты за деньги и с проверкой: судимость
 *  за последние три дня — отказ. Там же берут справку о несудимости.
 *
 *  Договор — это Y-действие: лицензированный адвокат нажимает Y на игроке,
 *  тот платит назначенную цену, и если клиента посадят, адвоката зовут
 *  автоматически. Сделка лежит в SQLite и переживает рестарт.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_government_included
    #endinput
#endif
#define _railway_government_included

#include <railway/core>
#include <railway/ui>
#include <railway/police>

/* Мэрия: Downtown, у City Hall (реальная точка из docs/КООРДИНАТЫ.md). */
#define GOV_X                   1352.4
#define GOV_Y                   -1759.4
#define GOV_Z                   13.5
#define GOV_RANGE               8.0
#define GOV_LICENCE_PRICE       25000
#define GOV_LICENCE_DAYS        7
#define GOV_CLEAN_DAYS          3       /* без судимости столько дней */
#define GOV_RECORD_PRICE        500
#define GOV_CONTRACT_HOURS      24
#define GOV_CONTRACT_MIN        1000

forward RW_GovClick(playerid, row);
forward RW_GovCefClick(playerid, id);
forward RW_ContractCall(client);

/*  --------------------------------- схема --------------------------------- */

stock RW_GovSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS lawyer_contracts (client INTEGER PRIMARY KEY, lawyer INTEGER NOT NULL, price INTEGER NOT NULL, stamp INTEGER NOT NULL)")) return 0;
    return 1;
}
stock RW_GovReset(playerid)
{
    #pragma unused playerid
    return 1;
}
stock bool:RW_GovNear(playerid)
{
    return IsPlayerInRangeOfPoint(playerid, GOV_RANGE, GOV_X, GOV_Y, GOV_Z) ? true : false;
}

/*  --------------------------------- судимость ------------------------------ */

/* Сколько приводов было за последние дни. */
stock RW_GovArrests(playerid, days)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[220], total;
    format(query, sizeof query, "SELECT COUNT(*) AS n FROM arrests WHERE player_id=%d AND stamp>%d", gAccountId[playerid], gettime() - days * 86400);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) total = db_get_field_assoc_int(r, "n");
    db_free_result(r);
    return total;
}

/*  ------------------------------ лицензия адвоката ------------------------ */

stock RW_GovLicence(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!RW_GovNear(playerid)) return RW_Error(playerid, "Это делается в мэрии: Downtown, у City Hall (1352, -1759).");
    if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0)
        return RW_Error(playerid, "Полицейский на службе не получает адвокатскую лицензию.");
    if (RW_GovArrests(playerid, GOV_CLEAN_DAYS) > 0)
        return RW_Error(playerid, "Отказ: привод за последние три дня. Приходите с чистой историей.");
    if (gMoney[playerid] < GOV_LICENCE_PRICE) return RW_Error(playerid, "Лицензия стоит 25 000 наличными.");
    if (!RW_TakeMoney(playerid, GOV_LICENCE_PRICE)) return RW_Error(playerid, "Не хватает наличных.");
    new expire = gettime() + GOV_LICENCE_DAYS * 86400;
    if (RW_IsLawyer(playerid) && gLawyerExpire[playerid] > gettime()) expire = gLawyerExpire[playerid] + GOV_LICENCE_DAYS * 86400;
    gLawyerLicense[playerid] = 1;
    gLawyerExpire[playerid] = expire;
    if (gLawyerPrice[playerid] <= 0) gLawyerPrice[playerid] = GOV_CONTRACT_MIN;
    new query[260];
    format(query, sizeof query, "INSERT OR REPLACE INTO lawyers(player_id,license,expire,price,rating) VALUES(%d,1,%d,%d,%d)", gAccountId[playerid], expire, gLawyerPrice[playerid], gLawyerRating[playerid]);
    RW_DB_Exec(query);
    new days = (expire - gettime()) / 86400, message[180], money[32];
    RW_FormatMoney(GOV_LICENCE_PRICE, money, sizeof money);
    format(message, sizeof message, "Лицензия адвоката оформлена на %d дн. за %s. Цена за дело: /lawprice <сумма>.", days, money);
    return RW_Ok(playerid, message);
}
/* Справка о несудимости: то, что видит полиция в своём архиве. */
stock RW_GovRecord(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!RW_GovNear(playerid)) return RW_Error(playerid, "Справку выдают в мэрии: Downtown, у City Hall (1352, -1759).");
    if (gMoney[playerid] < GOV_RECORD_PRICE) return RW_Error(playerid, "Справка стоит 500 наличными.");
    if (!RW_TakeMoney(playerid, GOV_RECORD_PRICE)) return RW_Error(playerid, "Не хватает наличных.");
    new total = RW_GovArrests(playerid, 30), message[180];
    if (!total) format(message, sizeof message, "Справка: за 30 дней приводов нет, история чистая.");
    else format(message, sizeof message, "Справка: за 30 дней приводов — %d. С такой историей лицензию не дадут три дня.", total);
    return RW_Ok(playerid, message);
}

/*  --------------------------------- договор -------------------------------- */

stock RW_ContractOf(client)
{
    if (gAccountId[client] <= 0) return 0;
    new query[220], lawyer;
    format(query, sizeof query, "SELECT lawyer FROM lawyer_contracts WHERE client=%d AND stamp>%d", gAccountId[client], gettime() - GOV_CONTRACT_HOURS * 3600);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r) == 1) lawyer = db_get_field_assoc_int(r, "lawyer");
    db_free_result(r);
    return lawyer;
}
/* Y на игроке: адвокат предлагает защиту, клиент платит сразу. */
stock RW_ContractOffer(playerid, target)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!RW_IsLawyer(playerid)) return RW_Error(playerid, "Нужна лицензия адвоката: её выдают в мэрии (1352, -1759).");
    if (target < 0 || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Встаньте рядом с игроком.");
    if (target == playerid) return RW_Error(playerid, "С собой договор не заключают.");
    if (RW_PoliceOnDuty(target) && RW_PoliceBadge(target) > 0) return RW_Error(playerid, "Полицейскому на службе адвокат не нужен.");
    new price = gLawyerPrice[playerid];
    if (price < GOV_CONTRACT_MIN) price = GOV_CONTRACT_MIN;
    if (gMoney[target] < price) {
        new nick[MAX_PLAYER_NAME], message[160], money[32];
        RW_GetNick(target, nick);
        RW_FormatMoney(price, money, sizeof money);
        format(message, sizeof message, "У %s нет %s наличными на договор.", nick, money);
        return RW_Error(playerid, message);
    }
    if (!RW_TakeMoney(target, price)) return RW_Error(playerid, "Не удалось списать деньги за договор.");
    RW_AddMoney(playerid, price);
    RW_StoryEarned(playerid, price);
    new query[260];
    format(query, sizeof query, "INSERT OR REPLACE INTO lawyer_contracts(client,lawyer,price,stamp) VALUES(%d,%d,%d,%d)", gAccountId[target], gAccountId[playerid], price, gettime());
    RW_DB_Exec(query);
    new nick[MAX_PLAYER_NAME], tnick[MAX_PLAYER_NAME], message[180], money[32];
    RW_GetNick(playerid, nick);
    RW_GetNick(target, tnick);
    RW_FormatMoney(price, money, sizeof money);
    format(message, sizeof message, "Договор на %d ч: если %s сядет, вас позовут. Гонорар %s получен.", GOV_CONTRACT_HOURS, tnick, money);
    RW_Ok(playerid, message);
    format(message, sizeof message, "Ваш адвокат — %s. Если вас посадят, он будет вызван сам (%d ч).", nick, GOV_CONTRACT_HOURS);
    return RW_Ok(target, message);
}
/* Клиента посадили — зовём его адвоката, если тот в сети. */
stock RW_ContractCall(client)
{
    if (!gLogged[client]) return 0;
    new lawyer = RW_ContractOf(client);
    if (lawyer <= 0) return 0;
    new nick[MAX_PLAYER_NAME], message[180];
    RW_GetNick(client, nick);
    format(message, sizeof message, "Ваш клиент %s в камере. Дело за вами: подойдите и нажмите на него Y.", nick);
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gLogged[p] && gAccountId[p] == lawyer) RW_Error(p, message);
    format(message, sizeof message, "Адвокат по договору уже в курсе.");
    return RW_Info(client, message);
}
stock RW_ContractCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new Float:x, Float:y, Float:z, best = -1;
    new Float:bestDist = 3.5;
    GetPlayerPos(playerid, x, y, z);
    for (new i; i < MAX_PLAYERS; i++) {
        if (i == playerid || !IsPlayerConnected(i) || !gLogged[i]) continue;
        new Float:dist = GetPlayerDistanceFromPoint(i, x, y, z);
        if (dist > bestDist) continue;
        bestDist = dist; best = i;
    }
    return RW_ContractOffer(playerid, best);
}

/*  --------------------------------- панель -------------------------------- */

stock RW_GovRowOut(playerid, row, const text[], const arg[], const icon[])
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
stock RW_GovRows(playerid)
{
    new row[96], arg[96], days;
    if (RW_IsLawyer(playerid)) days = (gLawyerExpire[playerid] - gettime()) / 86400 + 1;
    if (RW_IsLawyer(playerid)) format(row, sizeof row, "Лицензия адвоката: ещё %d дн.", days > 0 ? days : 0);
    else format(row, sizeof row, "Лицензия адвоката");
    format(arg, sizeof arg, "25 000, на 7 дней");
    RW_GovRowOut(playerid, 0, row, arg, "law");
    format(row, sizeof row, "Справка о несудимости");
    format(arg, sizeof arg, "500, приводы за 30 дней");
    RW_GovRowOut(playerid, 1, row, arg, "info");
    format(row, sizeof row, "Судимость за 3 дня: %d", RW_GovArrests(playerid, GOV_CLEAN_DAYS));
    format(arg, sizeof arg, "с ней лицензию не дадут");
    RW_GovRowOut(playerid, 2, row, arg, "alert");
    format(row, sizeof row, "Цена за дело: %d", gLawyerPrice[playerid]);
    if (RW_IsLawyer(playerid)) format(arg, sizeof arg, "нажмите: сумму спросит поле ввода");
    else format(arg, sizeof arg, "сначала лицензия адвоката");
    RW_GovRowOut(playerid, 3, row, arg, "wallet");
    format(row, sizeof row, "Договор с адвокатом");
    format(arg, sizeof arg, "Y на адвокате или /contract");
    RW_GovRowOut(playerid, 4, row, arg, "users");
    return 1;
}
stock RW_GovOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    if (!RW_GovNear(playerid)) return RW_Error(playerid, "Мэрия в Даунтауне, у City Hall: 1352, -1759.");
    new subtitle[120];
    format(subtitle, sizeof subtitle, "Город Лос-Сантос. Приводов за 30 дней: %d", RW_GovArrests(playerid, 30));
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_GOV, "Мэрия", subtitle)) {
            RW_GovRows(playerid);
            RW_CefAction(playerid, 6, "Оформить лицензию", "");
            RW_CefAction(playerid, 8, "Выйти", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_GOV, "Мэрия", subtitle);
    RW_GovRows(playerid);
    RW_UI_Row(playerid, 6, "Оформить лицензию");
    RW_UI_Row(playerid, 7, "Выйти");
    return 1;
}
stock RW_GovAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    if (row == 0 || row == 6) return RW_GovLicence(playerid);
    if (row == 1) return RW_GovRecord(playerid);
    if (row == 4) return RW_ContractCommand(playerid);
    if (row == 3) {
        if (!RW_IsLawyer(playerid)) return RW_Error(playerid, "Сначала лицензия адвоката: строка выше.");
        return RW_InputOpen(playerid, INPUT_LAW_PRICE, 0, "Цена за дело",
            "Сколько берёте за защиту: от 0 до 50 000", "Например: 3000");
    }
    return RW_GovOpen(playerid);
}
stock RW_GovClick(playerid, row) { return RW_GovAction(playerid, row, false); }
stock RW_GovCefClick(playerid, id) { return RW_GovAction(playerid, id, true); }
stock RW_GovPanel(playerid)
{
    if (!RW_CefOn(playerid)) RW_Info(playerid, "Мэрия: /licence — лицензия адвоката, /record — справка о несудимости, /contract — договор с адвокатом.");
    return RW_GovOpen(playerid);
}
