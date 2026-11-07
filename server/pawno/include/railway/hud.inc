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
#define HUD_MSK_HOURS       3           /* Москва — это UTC+3          */
#define HUD_CLOCK_UNKNOWN   (-99)       /* сдвиг ещё не посчитан       */

new bool:gHudOn[MAX_PLAYERS];           /* HUD включён                     */
new gHudTick[MAX_PLAYERS];              /* счётчик пакетов радара          */
new gClockShift = HUD_CLOCK_UNKNOWN;    /* сдвиг локального времени к UTC  */
new gHudBank[MAX_PLAYERS];              /* что уже показано: деньги        */
new gHudCash[MAX_PLAYERS];
new gHudMinute[MAX_PLAYERS] = {-1, ...};

/* Стартовых бонусов больше нет: HUD отвечает за подсказки, спидометр и деньги. */
stock RW_HudLoad(playerid)
{
    gHudOn[playerid] = true;
    return 1;
}
stock RW_HudReset(playerid)
{
    gHudOn[playerid] = false; gHudMouse[playerid] = false;
    return 1;
}


stock RW_HudHints(playerid)
{
    new hints[96];
    hints[0] = 0;
    if (!RW_SettingsOn(playerid, SET_HINTS)) return RW_CefHudHint(playerid, hints);
    if (GetPlayerVehicleID(playerid) > 0) strcat(hints, "N:engine;");
    else strcat(hints, "Y:interaction;");
    strcat(hints, "AltBracket:phone;AltU:inventory;AltY:mouse;");
    return RW_CefHudHint(playerid, hints);
}
/*  --------------------------------- кошелёк ---------------------------- */
/*  Справа от штатного радара: деньги в банке, наличные в кармане и
    московское время. Страница только рисует цифры, считает всё сервер. */

/*  Москву считаем от UTC: SQLite знает UTC всегда, а gettime() отдаёт
    локальное время машины, и на сервере это может быть что угодно. Один
    запрос при старте, дальше — чистая арифметика. */
stock RW_HudClock()
{
    if (gDB == DB:0) { gClockShift = 0; return 0; }
    new DBResult:res = db_query(gDB, "SELECT CAST(strftime('%H','now') AS INTEGER) AS `utc`");
    if (res == DBResult:0) { gClockShift = 0; return 0; }
    if (db_num_rows(res) <= 0) { db_free_result(res); gClockShift = 0; return 0; }
    new text[8], hour, minute, second;
    db_get_field_assoc(res, "utc", text, sizeof text);
    db_free_result(res);
    gettime(hour, minute, second);
    #pragma unused minute, second
    gClockShift = (strval(text) - hour + 24) % 24;
    return 1;
}
stock RW_HudMsk(&hour, &minute)
{
    new second;
    gettime(hour, minute, second);
    #pragma unused second
    if (gClockShift == HUD_CLOCK_UNKNOWN) RW_HudClock();
    hour = (hour + gClockShift + HUD_MSK_HOURS) % 24;
    return 1;
}
stock RW_HudWallet(playerid)
{
    if (!RW_CefOn(playerid) || !gHudOn[playerid]) return 0;
    new hour, minute;
    RW_HudMsk(hour, minute);
    if (gBank[playerid] == gHudBank[playerid] && gMoney[playerid] == gHudCash[playerid]
        && minute == gHudMinute[playerid]) return 1;
    gHudBank[playerid] = gBank[playerid];
    gHudCash[playerid] = gMoney[playerid];
    gHudMinute[playerid] = minute;
    return RW_CefHudWallet(playerid, gBank[playerid], gMoney[playerid], hour, minute);
}

stock RW_HudShow(playerid, bool:on)
{
    gHudOn[playerid] = on;
    if (!RW_CefOn(playerid)) return 0;
    RW_CefHudShow(playerid, on ? 1 : 0);
    if (on) { RW_HudHints(playerid); RW_HudWallet(playerid); }
    return 1;
}
stock RW_HudToggle(playerid)
{
    if (!RW_CefOn(playerid)) return RW_Info(playerid, "The head-up display needs the browser interface: the plugin is off for you.");
    RW_HudShow(playerid, !gHudOn[playerid]);
    return RW_Info(playerid, gHudOn[playerid] ? "HUD is on." : "HUD is off. /hud brings it back.");
}

/* Раз в полсекунды: подсказки по ситуации. реже — подсказки и карточка бонуса. */
public RW_HudPulse()
{
    for (new playerid; playerid < MAX_PLAYERS; playerid++) {
        if (!IsPlayerConnected(playerid) || !gLogged[playerid]) continue;
        RW_GpsPulse(playerid);
        if (!RW_CefOn(playerid) || !gHudOn[playerid]) continue;
        gHudTick[playerid]++;
        if (gHudTick[playerid] % 4 == 0) RW_HudHints(playerid);
        RW_HudWallet(playerid);   /* деньги и время меняются редко: страница
                                     получает их только когда есть что менять */
    }
    return 1;
}

/* Курсор включают и выключают и клавишей, и страница: когда браузер в
   фокусе, SA-MP может не отдать нажатие серверу. */
public RW_HudMouseEvent(playerid, browserid, on)
{
    #pragma unused browserid
    if (!gLogged[playerid]) return 0;
    if (on && !gHudMouse[playerid]) return RW_HudMouseToggle(playerid);
    if (!on && gHudMouse[playerid]) return RW_HudMouseToggle(playerid);
    return 1;
}

/*  --------------------------------- команда ---------------------------- */
stock RW_HudCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    return RW_HudToggle(playerid);
}
/* Курсор: по умолчанию его нет, чтобы не мешал. Включает его Alt + Y или /mouse. */
stock RW_HudMouseToggle(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!RW_CefOn(playerid)) return RW_Info(playerid, "Браузерного интерфейса нет: мышь не нужна, используйте /donate.");
    gHudMouse[playerid] = !gHudMouse[playerid];
    RW_CefHudMouse(playerid, gHudMouse[playerid]);
    /* Пока курсор на экране, персонаж стоит: иначе клик уходит в игру. */
    if (!IsPlayerInAnyVehicle(playerid)) TogglePlayerControllable(playerid, !gHudMouse[playerid]);
    return RW_Info(playerid, gHudMouse[playerid] ? "Мышь включена: нажмите карточку бонуса. Alt + Y снова спрячет курсор." : "Мышь скрыта.");
}
stock RW_HudMouseCommand(playerid) { return RW_HudMouseToggle(playerid); }
