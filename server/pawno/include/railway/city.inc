/*  railway/city.inc
 *
 *  Город вокруг станции: неписи-актёры, приветствие новичка и короткое
 *  вступление с камеры при первом появлении.
 *
 *  Актёры — не боты: это статичные персонажи клиента (CreateActor), поэтому
 *  плагины не нужны, а сервер держит только их координаты и реплики.
 *  Подписи над ними печатает движок своим шрифтом, поэтому они латиницей;
 *  по-русски игрок видит HUD, меню и телефон в браузере.
 */

#if defined _railway_city_included
    #endinput
#endif
#define _railway_city_included

#define CITY_ACTORS         6           /* неписей в городе                */
#define CITY_TALK_RANGE     4.0         /* с какого расстояния слышна реплика */
#define CITY_INTRO_MS       2600        /* длительность вступления         */
#define CITY_START_CASH     3000        /* подъёмные новичку наличными     */
#define CITY_START_BANK     2000        /* и на счёт                       */

new gCityActor[CITY_ACTORS];
new Text3D:gCityLabel[CITY_ACTORS];
new bool:gCityWelcomed[MAX_PLAYERS];

static const Float:gCityActorPos[CITY_ACTORS][4] = {
    {1745.6, -1860.4, 13.6, 180.0},     /* станция Unity: диспетчер       */
    {1352.4, -1757.2, 13.6, 180.0},     /* 24/7 Downtown: продавец        */
    {2244.4, -1663.2, 13.6, 180.0},     /* 24/7 Ganton: продавец          */
    {1811.0, -1886.2, 13.5, 90.0},      /* двор Эль-Корона: бригадир      */
    {1177.0, -1321.5, 15.5, 0.0},       /* больница All Saints: фельдшер  */
    {1555.0, -1673.4, 16.3, 180.0}      /* участок LSPD: дежурный         */
};
static const gCityActorSkin[CITY_ACTORS] = { 20, 26, 29, 141, 147, 170 };
static const gCityActorName[CITY_ACTORS][] = {
    "Dispatcher Mills", "Clerk Harris", "Clerk Ruiz", "Foreman Boyd", "Medic Reed", "Officer Diaz"
};
static const gCityActorLine[CITY_ACTORS][] = {
    "Jobs are at the El Corona yard and at LS International: /job. The task appears with /work.",
    "Every shop pays its owner: /till shows the till of your business.",
    "Robberies only from 12:00 to 01:00, and the alarm wakes the whole police shift.",
    "Weather pays: rain, fog and night raise the rate. /work for the next task.",
    "Down? Call 911 from the phone: an ambulance crew will take the call.",
    "Wanted list: /wanted. A fine is paid at the station, a lawyer takes the case: /lawyer."
};

/*  --------------------------------- город ------------------------------- */
stock RW_CityInit()
{
    for (new i; i < CITY_ACTORS; i++) {
        gCityActor[i] = CreateActor(gCityActorSkin[i], gCityActorPos[i][0], gCityActorPos[i][1], gCityActorPos[i][2], gCityActorPos[i][3]);
        SetActorInvulnerable(gCityActor[i], true);
        new text[96];
        format(text, sizeof text, "%s\n/talk - заговорить", gCityActorName[i]);
        gCityLabel[i] = Create3DTextLabel(text, 0xF5B04CFF, gCityActorPos[i][0], gCityActorPos[i][1], gCityActorPos[i][2] + 1.1, 12.0, 0, 0);
    }
    return 1;
}
stock RW_CityNear(playerid)
{
    new Float:x, Float:y, Float:z, best = -1;
    new Float:dist = CITY_TALK_RANGE * CITY_TALK_RANGE;
    GetPlayerPos(playerid, x, y, z);
    #pragma unused z
    for (new i; i < CITY_ACTORS; i++) {
        new Float:dx = gCityActorPos[i][0] - x, Float:dy = gCityActorPos[i][1] - y;
        new Float:d = dx * dx + dy * dy;
        if (d < dist) { dist = d; best = i; }
    }
    return best;
}
stock RW_CityTalk(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new id = RW_CityNear(playerid);
    if (id < 0) return RW_Error(playerid, "There is nobody to talk to here. Look for a person with a name above.");
    new message[220];
    format(message, sizeof message, "%s: %s", gCityActorName[id], gCityActorLine[id]);
    return RW_Info(playerid, message);
}

/*  ------------------------------ появление ------------------------------ */
/* Первый вход в сессии: камера над площадью, приветствие и подъёмные. */
stock RW_CitySpawn(playerid)
{
    if (gCityWelcomed[playerid]) return 0;
    gCityWelcomed[playerid] = true;
    RW_CityIntro(playerid);
    if (RW_SettingsOn(playerid, SET_WELCOME)) RW_CityWelcome(playerid);
    RW_HudShow(playerid, true);
    return 1;
}
stock RW_CityIntro(playerid)
{
    TogglePlayerControllable(playerid, false);
    SetPlayerCameraPos(playerid, STATION_X + 6.0, STATION_Y + 9.0, STATION_Z + 7.0);
    SetPlayerCameraLookAt(playerid, STATION_X, STATION_Y, STATION_Z + 1.0);
    SetTimerEx("RW_CityIntroEnd", CITY_INTRO_MS, false, "i", playerid);
    return 1;
}
public RW_CityIntroEnd(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;
    TogglePlayerControllable(playerid, true);
    SetCameraBehindPlayer(playerid);
    return 1;
}
stock RW_CityWelcome(playerid)
{
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);
    RW_Accent(playerid, "--- Добро пожаловать на Unity Station ---");
    RW_Info(playerid, "Это ролевой сервер: играйте роль, говорите от лица персонажа, а /b - для разговора вне роли.");
    if (RW_CefOn(playerid)) {
        RW_Info(playerid, "Начало: устройтесь на точке найма во дворе Эль-Корона, подойдя и нажав Y. Задание — строка в панели работы, деньги — в приложении Bank.");
        RW_Info(playerid, "Донат-меню: клавиша C. Экранный интерфейс и клавиши: Alt + H.");
    } else {
        RW_Info(playerid, "Начало: /job во дворе Эль-Корона, затем /work - задание. Деньги приходят на /payday.");
        RW_Info(playerid, "Донат-меню: /donate. Экранный интерфейс: /hud, /set и /key.");
    }
    if (gMoney[playerid] <= 0 && gBank[playerid] <= 0) {
        RW_AddMoney(playerid, CITY_START_CASH);
        RW_BankAdd(playerid, CITY_START_BANK);
        RW_Ok(playerid, "Подъёмные новичку: 3 000 наличными и 2 000 на счёт. Удачи в городе.");
    }
    return 1;
}
stock RW_CityReset(playerid) { gCityWelcomed[playerid] = false; return 1; }

/*  -------------------------------- навигатор ----------------------------- */
/*  /gps ставит метку на карте и ведёт до неё: метка видна на радаре, а HUD
    показывает, сколько осталось. Ехать надо самому — это не телепорт.       */
#define GPS_ARRIVE              25.0    /* приехали                        */
#define GPS_MAX_PLACES          10

static const Float:gGpsPlace[GPS_MAX_PLACES][3] = {
    { 1742.9, -1862.6, 13.6 },      /* вокзал Unity Station               */
    { 1175.8, -1324.7, 15.4 },      /* больница All Saints                */
    { 1544.6, -1675.5, 13.6 },      /* полиция, Pershing Square           */
    { 1352.4, -1759.4, 13.5 },      /* мэрия, Даунтаун                    */
    { 2244.4, -1665.4, 13.5 },      /* магазин 24/7, Гантон               */
    { 1004.0, -939.0, 42.2 },       /* банкомат и заправка, Темпл         */
    { 1595.5, -2235.9, 13.5 },      /* заправка, Оушен-Докс               */
    { 2745.0, -2372.0, 13.6 },      /* биржа труда, Оушен-Докс            */
    { 1635.0, -1700.0, 13.6 },      /* штрафстоянка                       */
    { 1833.4, -1842.4, 13.5 }       /* магазин 24/7, Юнити                */
};
static const gGpsPlaceName[GPS_MAX_PLACES][] = {
    "Вокзал", "Больница", "Полиция", "Мэрия", "Магазин Гантон",
    "Банк Темпл", "Заправка Оушен-Докс", "Биржа труда", "Штрафстоянка", "Магазин Юнити"
};

new bool:gGpsOn[MAX_PLAYERS];
new Float:gGpsX[MAX_PLAYERS], Float:gGpsY[MAX_PLAYERS], Float:gGpsZ[MAX_PLAYERS];
new gGpsLabel[MAX_PLAYERS][32];
new gGpsLeft[MAX_PLAYERS];

stock RW_GpsCount() { return GPS_MAX_PLACES; }

stock RW_GpsClear(playerid)
{
    if (!gGpsOn[playerid]) return 0;
    gGpsOn[playerid] = false;
    gGpsLeft[playerid] = 0;
    gGpsLabel[playerid][0] = 0;
    RemovePlayerMapIcon(playerid, GPS_ICON);
    RW_CefHudGps(playerid, "");
    return 1;
}

stock RW_GpsSet(playerid, index)
{
    if (index < 0 || index >= GPS_MAX_PLACES) return RW_Error(playerid, "Нет такого места: /gps без названия покажет список.");
    gGpsOn[playerid] = true;
    gGpsX[playerid] = gGpsPlace[index][0];
    gGpsY[playerid] = gGpsPlace[index][1];
    gGpsZ[playerid] = gGpsPlace[index][2];
    format(gGpsLabel[playerid], 32, "%s", gGpsPlaceName[index]);
    SetPlayerMapIcon(playerid, GPS_ICON, gGpsX[playerid], gGpsY[playerid], gGpsZ[playerid], 0, 0xF5B04CFF, MAPICON_GLOBAL);
    RW_GpsPulse(playerid);
    if (RW_CefOn(playerid)) return RW_Ok(playerid, "Метка стоит: езжайте за ней. Снять — кнопка в навигаторе.");
    return RW_Ok(playerid, "Метка стоит: езжайте за ней. Снять — /gps off.");
}

/* Ищем место по началу названия: «вокз» найдёт «Вокзал». */
stock RW_GpsFind(const text[])
{
    for (new i; i < GPS_MAX_PLACES; i++) if (strfind(gGpsPlaceName[i], text, true) == 0) return i;
    for (new i; i < GPS_MAX_PLACES; i++) if (strfind(gGpsPlaceName[i], text, true) >= 0) return i;
    return -1;
}

stock RW_GpsCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    new arg[32];
    RW_MarketArgs(params, arg, sizeof arg, arg, sizeof arg, arg, sizeof arg);
    if (!arg[0]) return RW_GpsOpen(playerid);
    if (!strcmp(arg, "off", true) || !strcmp(arg, "сброс", true) || !strcmp(arg, "выкл", true)) {
        if (!gGpsOn[playerid]) return RW_Info(playerid, "Метки и нет.");
        RW_GpsClear(playerid);
        return RW_Info(playerid, "Метка снята.");
    }
    new index = RW_GpsFind(arg);
    if (index < 0) return RW_Error(playerid, "Нет такого места: /gps без названия покажет список.");
    return RW_GpsSet(playerid, index);
}

stock RW_GpsRows(playerid)
{
    new row[64];
    for (new i; i < GPS_MAX_PLACES; i++) {
        format(row, sizeof row, "%s", gGpsPlaceName[i]);
        RW_GpsRowOut(playerid, i, row, gGpsOn[playerid] ? "стоит" : "ехать", "target");
    }
    return 1;
}
stock RW_GpsRowOut(playerid, row, const text[], const arg[], const icon[])
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

stock RW_GpsOpen(playerid)
{
    new subtitle[80];
    format(subtitle, sizeof subtitle, "Куда ехать: %d мест", GPS_MAX_PLACES);
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_GPS, "Навигатор", subtitle)) {
            RW_GpsRows(playerid);
            RW_CefAction(playerid, 9, "Снять метку", "");
            RW_CefAction(playerid, 8, "Закрыть", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_GPS, "Навигатор", subtitle);
    RW_GpsRows(playerid);
    RW_UI_Row(playerid, 6, "Снять метку", "close");
    RW_UI_Row(playerid, 7, "Закрыть");
    return 1;
}

stock RW_GpsAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    /* Снятие метки: раньше это умела только команда /gps off. */
    if (row == (cef ? 9 : 6)) {
        if (!gGpsOn[playerid]) return RW_Info(playerid, "Метки и нет.");
        RW_GpsClear(playerid);
        RW_Info(playerid, "Метка снята.");
        return RW_GpsOpen(playerid);
    }
    if (row < 0 || row >= GPS_MAX_PLACES) return 1;
    RW_GpsSet(playerid, row);
    return RW_UI_Close(playerid);
}

/* Раз в секунду: сколько осталось и не приехали ли. */
public RW_GpsPulse(playerid)
{
    if (!gGpsOn[playerid]) return 0;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    new left = floatround(floatsqroot((x - gGpsX[playerid]) * (x - gGpsX[playerid])
        + (y - gGpsY[playerid]) * (y - gGpsY[playerid])));
    if (left <= GPS_ARRIVE) {
        RW_GpsClear(playerid);
        return RW_Ok(playerid, "Вы на месте.");
    }
    if (left == gGpsLeft[playerid]) return 1;
    gGpsLeft[playerid] = left;
    new text[48];
    format(text, sizeof text, "%s: %d м", gGpsLabel[playerid], left);
    return RW_CefHudGps(playerid, text);
}
