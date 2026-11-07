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
    RW_Info(playerid, "Начало: /job во дворе Эль-Корона, затем /work - задание. Деньги приходят на /payday.");
    RW_Info(playerid, "Донат-меню: /donate. Экранный интерфейс: /hud, /set и /key.");
    if (gMoney[playerid] <= 0 && gBank[playerid] <= 0) {
        RW_AddMoney(playerid, CITY_START_CASH);
        RW_BankAdd(playerid, CITY_START_BANK);
        RW_Ok(playerid, "Подъёмные новичку: 3 000 наличными и 2 000 на счёт. Удачи в городе.");
    }
    return 1;
}
stock RW_CityReset(playerid) { gCityWelcomed[playerid] = false; return 1; }
