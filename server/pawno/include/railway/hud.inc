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

new bool:gHudOn[MAX_PLAYERS];           /* HUD включён                     */
new bool:gHudMouse[MAX_PLAYERS];        /* курсор включён командой /mouse  */
new gHudTick[MAX_PLAYERS];              /* счётчик пакетов радара          */

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
    if (GetPlayerVehicleID(playerid) > 0) strcat(hints, "N:engine;H:doors;");
    else strcat(hints, "Y:interaction;");
    strcat(hints, "AltY:mouse;");
    strcat(hints, "F2:donate;");
    return RW_CefHudHint(playerid, hints);
    return 1;
}
stock RW_HudShow(playerid, bool:on)
{
    gHudOn[playerid] = on;
    if (!RW_CefOn(playerid)) return 0;
    RW_CefHudShow(playerid, on ? 1 : 0);
    if (on) RW_HudHints(playerid);
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
        gHudTick[playerid]++;
        if (gHudTick[playerid] % 4 == 0) RW_HudHints(playerid);
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
public RW_HudMenuEvent(playerid, browserid, arg)
{
    #pragma unused browserid, arg
    if (!gLogged[playerid]) return 0;
    return RW_MenuOpen(playerid);
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
