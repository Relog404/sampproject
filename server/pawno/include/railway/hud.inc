/*  railway/hud.inc
 *
 *  Постоянный HUD в браузере: прозрачный радар, подсказки клавиш и карточка
 *  бонуса с кнопкой «скрыть». Без CEF-плагина игрок остаётся на текстовом
 *  интерфейсе, и это нормально: HUD — надстройка, а не часть логики.
 *
 *  Текст для игрока здесь английскими ключами (Y:interaction, starter), а по-
 *  русски их печатает сама страница hud.html: сервер не зависит от шрифтов
 *  клиента, а игрок видит родной язык.
 */

#if defined _railway_hud_included
    #endinput
#endif
#define _railway_hud_included

#define HUD_RADAR_RANGE     200.0       /* дальность радара, метров        */
#define HUD_MAX_BLIPS       14          /* сколько точек уходит за раз     */
#define HUD_ONLINE_SECONDS  1800        /* бонус за игру: раз в 30 минут   */
#define HUD_ONLINE_BONUS    500         /* сумма бонуса за игру            */
#define HUD_DAILY_SECONDS   86400       /* стартовый бонус: раз в сутки    */
#define HUD_DAILY_BONUS     5000        /* множитель растёт по дню         */
#define HUD_DAILY_LIMIT     3           /* дней подряд                     */

new bool:gHudOn[MAX_PLAYERS];           /* HUD включён                     */
new bool:gHudHidden[MAX_PLAYERS];       /* карточку бонуса закрыли         */
new gHudDay[MAX_PLAYERS];               /* взятых дневных бонусов          */
new gHudDaily[MAX_PLAYERS];             /* время последнего дневного       */
new gHudOnline[MAX_PLAYERS];            /* время последнего бонуса за игру */
new gHudStamp[MAX_PLAYERS];             /* когда вошёл: отсчёт получаса    */
new gHudTick[MAX_PLAYERS];              /* счётчик пакетов радара          */

/* Точки интереса на радаре: работа, магазины, больница, участок, станция. */
#define HUD_POI_WORK        (1)
#define HUD_POI_SHOP        (2)
#define HUD_POI_PLAYER      (3)
#define HUD_POI_HELP        (4)
#define HUD_POI_STOP        (5)

static const Float:gHudPoi[][3] = {
    {1352.4, -1759.4, 13.5},    /* 24/7 Downtown                           */
    {2244.4, -1665.4, 13.5},    /* 24/7 Ganton                             */
    {2503.4, -1722.4, 13.4},    /* 24/7 Las Colinas                        */
    {1833.4, -1842.4, 13.5},    /* 24/7 Unity                              */
    {1811.0, -1888.0, 13.4},    /* работа: двор Эль-Корона                 */
    {1962.0, -2184.0, 13.5},    /* работа: ЛС-Интернешнл                   */
    {1177.0, -1323.5, 15.4},    /* больница All Saints                     */
    {1555.0, -1675.6, 16.2},    /* участок LSPD                            */
    {1744.0, -1862.0, 13.5}     /* станция Unity: точка появления          */
};
static const gHudPoiKind[] = { HUD_POI_SHOP, HUD_POI_SHOP, HUD_POI_SHOP, HUD_POI_SHOP,
                               HUD_POI_WORK, HUD_POI_WORK, HUD_POI_HELP, HUD_POI_HELP, HUD_POI_STOP };

/* Районы: название под радаром. Ключи переводит страница. */
new Float:gHudZone[][3] = {
    {2200.0, -1650.0, 0.0}, {2050.0, -1450.0, 0.0}, {2170.0, -1990.0, 0.0},
    {1780.0, -1900.0, 0.0}, {1900.0, -1730.0, 0.0}, {2300.0, -1450.0, 0.0},
    {2450.0, -1700.0, 0.0}, {2050.0, -1700.0, 0.0}, {1450.0, -1700.0, 0.0},
    {1450.0, -1500.0, 0.0}, {1150.0, -1400.0, 0.0}, {1500.0, -1650.0, 0.0},
    {2300.0, -2400.0, 0.0}, {2750.0, -1900.0, 0.0}, {930.0, -1850.0, 0.0},
    {1250.0, -800.0, 0.0}, {370.0, -1800.0, 0.0}, {550.0, -1250.0, 0.0}
};
new gHudZoneName[][] = {
    "ganton", "jefferson", "willowfield", "elcorona", "idlewood", "eastlosSantos",
    "lascolinas", "glenpark", "downtown", "commerce", "market", "pershingsquare",
    "oceandocks", "playadelseville", "veronabeach", "vinewood", "santamariabeach", "rodeo"
};

/*  --------------------------------- база -------------------------------- */
stock RW_HudSchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS bonuses (player_id INTEGER PRIMARY KEY, day INT, daily INT, online INT, hidden INT)");
}
stock RW_HudLoad(playerid)
{
    gHudDay[playerid] = 0; gHudDaily[playerid] = 0; gHudOnline[playerid] = 0;
    gHudHidden[playerid] = false;
    if (gAccountId[playerid] <= 0) return 0;
    new query[128];
    format(query, sizeof query, "SELECT day,daily,online,hidden FROM bonuses WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r)) {
        gHudDay[playerid] = db_get_field_assoc_int(r, "day");
        gHudDaily[playerid] = db_get_field_assoc_int(r, "daily");
        gHudOnline[playerid] = db_get_field_assoc_int(r, "online");
        gHudHidden[playerid] = db_get_field_assoc_int(r, "hidden") ? true : false;
    }
    db_free_result(r);
    gHudStamp[playerid] = gettime();
    gHudOn[playerid] = true;
    return 1;
}
stock RW_HudSave(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[200];
    format(query, sizeof query, "INSERT OR REPLACE INTO bonuses (player_id,day,daily,online,hidden) VALUES (%d,%d,%d,%d,%d)",
        gAccountId[playerid], gHudDay[playerid], gHudDaily[playerid], gHudOnline[playerid], gHudHidden[playerid] ? 1 : 0);
    return RW_DB_Exec(query);
}
stock RW_HudReset(playerid)
{
    gHudOn[playerid] = false; gHudHidden[playerid] = false;
    gHudDay[playerid] = 0; gHudDaily[playerid] = 0; gHudOnline[playerid] = 0;
    return 1;
}

/*  --------------------------------- бонусы ------------------------------ */
stock RW_HudOnlineReady(playerid)
{
    return (gettime() - gHudOnline[playerid] >= HUD_ONLINE_SECONDS) ? 1 : 0;
}
stock RW_HudDailyReady(playerid)
{
    return (gettime() - gHudDaily[playerid] >= HUD_DAILY_SECONDS && gHudDay[playerid] < HUD_DAILY_LIMIT) ? 1 : 0;
}
stock RW_HudBonusAmount(playerid)
{
    return RW_HudDailyBonus(gHudDay[playerid] + 1);
}
stock RW_HudDailyBonus(day)
{
    if (day < 1) day = 1;
    if (day > HUD_DAILY_LIMIT) day = HUD_DAILY_LIMIT;
    return HUD_DAILY_BONUS * day;
}
/* Карточка: ключ, сумма и день. Пустой ключ прячет карточку. */
stock RW_HudBonusCard(playerid)
{
    if (!RW_CefOn(playerid) || !gHudOn[playerid]) return 0;
    if (gHudHidden[playerid]) return RW_HudBonusEmit(playerid, "", 0, 0);
    if (RW_HudOnlineReady(playerid)) return RW_HudBonusEmit(playerid, "online", HUD_ONLINE_BONUS, gHudDay[playerid]);
    if (RW_HudDailyReady(playerid)) return RW_HudBonusEmit(playerid, "starter", RW_HudBonusAmount(playerid), gHudDay[playerid] + 1);
    return RW_HudBonusEmit(playerid, "", 0, 0);
}
stock RW_HudBonusEmit(playerid, const key[], amount, day)
{
    return RW_CefHudBonus(playerid, key, amount, day);
    return 1;
}
stock RW_HudBonusClaim(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new sum[32], message[160];
    if (RW_HudOnlineReady(playerid)) {
        RW_AddMoney(playerid, HUD_ONLINE_BONUS);
        gHudOnline[playerid] = gettime();
        RW_FormatMoney(HUD_ONLINE_BONUS, sum, sizeof sum);
        format(message, sizeof message, "Bonus: %s in cash. Next one in 30 minutes.", sum);
    } else if (RW_HudDailyReady(playerid)) {
        new amount = RW_HudBonusAmount(playerid);
        RW_BankAdd(playerid, amount);
        gHudDay[playerid]++;
        gHudDaily[playerid] = gettime();
        gHudHidden[playerid] = false;
        RW_FormatMoney(amount, sum, sizeof sum);
        format(message, sizeof message, "Starter bonus, day %d of %d: %s on the account.", gHudDay[playerid], HUD_DAILY_LIMIT, sum);
    } else {
        return RW_Error(playerid, "Nothing to claim yet: the next bonus comes later.");
    }
    RW_HudSave(playerid);
    RW_HudBonusCard(playerid);
    return RW_Ok(playerid, message);
}
stock RW_HudBonusHide(playerid)
{
    gHudHidden[playerid] = true;
    RW_HudSave(playerid);
    RW_HudBonusCard(playerid);
    if (!RW_CefOn(playerid)) return RW_Info(playerid, "The bonus card is hidden for you. /bonus brings it back.");
    return 1;
}

/*  --------------------------------- радар ------------------------------- */
stock RW_HudZone(playerid, dest[], size = sizeof dest)
{
    new Float:x, Float:y, Float:z, best = -1;
    new Float:dist = 99999999.0;
    GetPlayerPos(playerid, x, y, z);
    #pragma unused z
    for (new i; i < sizeof gHudZone; i++) {
        new Float:dx = gHudZone[i][0] - x, Float:dy = gHudZone[i][1] - y;
        new Float:d = dx * dx + dy * dy;
        if (d < dist) { dist = d; best = i; }
    }
    if (best < 0) return 0;
    return format(dest, size, "%s", gHudZoneName[best]);
}
stock RW_HudRadar(playerid)
{
    new Float:x, Float:y, Float:z, Float:angle;
    GetPlayerPos(playerid, x, y, z);
    GetPlayerFacingAngle(playerid, angle);
    #pragma unused z
    new blips[192];
    blips[0] = 0;
    new count;
    for (new i; i < sizeof gHudPoi && count < HUD_MAX_BLIPS; i++) {
        new Float:dx = gHudPoi[i][0] - x, Float:dy = gHudPoi[i][1] - y;
        if (dx * dx + dy * dy > HUD_RADAR_RANGE * HUD_RADAR_RANGE) continue;
        new part[24];
        format(part, sizeof part, "%d:%d:%d;", gHudPoiKind[i], floatround(dx), floatround(dy));
        strcat(blips, part, sizeof blips);
        count++;
    }
    for (new i; i < MAX_PLAYERS && count < HUD_MAX_BLIPS; i++) {
        if (i == playerid || !IsPlayerConnected(i) || !gLogged[i]) continue;
        if (GetPlayerInterior(i) != GetPlayerInterior(playerid)) continue;
        if (GetPlayerVirtualWorld(i) != GetPlayerVirtualWorld(playerid)) continue;
        new Float:px, Float:py, Float:pz;
        GetPlayerPos(i, px, py, pz);
        new Float:dx = px - x, Float:dy = py - y;
        #pragma unused pz
        if (dx * dx + dy * dy > HUD_RADAR_RANGE * HUD_RADAR_RANGE) continue;
        new part[24];
        format(part, sizeof part, "%d:%d:%d;", HUD_POI_PLAYER, floatround(dx), floatround(dy));
        strcat(blips, part, sizeof blips);
        count++;
    }
    new zone[32];
    RW_HudZone(playerid, zone, sizeof zone);
    return RW_CefHudRadar(playerid, floatround(angle), blips, zone);
    return 1;
}
stock RW_HudHints(playerid)
{
    new hints[96];
    hints[0] = 0;
    if (GetPlayerVehicleID(playerid) > 0) strcat(hints, "N:engine;H:doors;");
    else strcat(hints, "Y:interaction;");
    strcat(hints, "F2:donate;");
    return RW_CefHudHint(playerid, hints);
    return 1;
}
stock RW_HudShow(playerid, bool:on)
{
    gHudOn[playerid] = on;
    if (!RW_CefOn(playerid)) return 0;
    RW_CefHudShow(playerid, on ? 1 : 0);
    if (on) { RW_HudRadar(playerid); RW_HudHints(playerid); RW_HudBonusCard(playerid); }
    return 1;
}
stock RW_HudToggle(playerid)
{
    if (!RW_CefOn(playerid)) return RW_Info(playerid, "The head-up display needs the browser interface: the plugin is off for you.");
    RW_HudShow(playerid, !gHudOn[playerid]);
    return RW_Info(playerid, gHudOn[playerid] ? "HUD is on." : "HUD is off. /hud brings it back.");
}

/* Раз в полсекунды: радар; реже — подсказки и карточка бонуса. */
public RW_HudPulse()
{
    for (new playerid; playerid < MAX_PLAYERS; playerid++) {
        if (!IsPlayerConnected(playerid) || !gLogged[playerid]) continue;
        if (!RW_CefOn(playerid) || !gHudOn[playerid]) continue;
        RW_HudRadar(playerid);
        gHudTick[playerid]++;
        if (gHudTick[playerid] % 4 == 0) RW_HudHints(playerid);
        if (gHudTick[playerid] % 8 == 0) RW_HudBonusCard(playerid);
    }
    return 1;
}

/*  ------------------------- события со страницы ------------------------- */
public RW_HudBonusClick(playerid, browserid, arg)
{
    #pragma unused browserid, arg
    if (!gLogged[playerid]) return 0;
    return RW_HudBonusClaim(playerid);
}
public RW_HudBonusHideEvent(playerid, browserid, arg)
{
    #pragma unused browserid, arg
    if (!gLogged[playerid]) return 0;
    return RW_HudBonusHide(playerid);
}
public RW_HudDonateEvent(playerid, browserid, arg)
{
    #pragma unused browserid, arg
    if (!gLogged[playerid]) return 0;
    return RW_DonateOpen(playerid);
}

/*  --------------------------------- команда ---------------------------- */
stock RW_HudCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gHudHidden[playerid]) {
        gHudHidden[playerid] = false;
        RW_HudSave(playerid);
        RW_HudBonusCard(playerid);
        return RW_Info(playerid, "The bonus card is back.");
    }
    return RW_HudToggle(playerid);
}
stock RW_BonusCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    return RW_HudBonusClaim(playerid);
}
