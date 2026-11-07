/*  ============================================================================
 *  railway/core.inc
 *  Базовые константы, состояние игроков и общие утилиты.
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_core_included
    #endinput
#endif
#define _railway_core_included

#pragma tabsize 0

#include <a_samp>
#include <a_sampdb>


/* ------------------------------------------------------------------ версия */
#define RW_VERSION                  "2.0.0"
#define RW_TAG                      "{F5B04C}[Railway]{FFFFFF}"

/* ------------------------------------------------------------------ цвета  */
#define COLOR_WHITE                 0xFFFFFFFF
#define COLOR_GREY                  0xA9B4C2FF
#define COLOR_DARK_GREY             0x6E7A88FF
#define COLOR_ACCENT                0xF5B04CFF
#define COLOR_ACCENT_SOFT           0xD9A253FF
#define COLOR_OK                    0x7BD88FFF
#define COLOR_ERROR                 0xFF6B6BFF
#define COLOR_INFO                  0x8FD3FFFF

/* Вызываются из других срезов, описаны позже: работа, такси, адвокат, налог. */
forward RW_PropTaxTick(playerid);
forward RW_PropPulse();
forward RW_PropSale(playerid, price);
forward RW_HudPulse();
forward RW_HudBonusClick(playerid, browserid, arg);
forward RW_HudBonusHideEvent(playerid, browserid, arg);
forward RW_HudDonateEvent(playerid, browserid, arg);
forward RW_HudMouseToggle(playerid);
forward RW_DonateOpen(playerid);
forward RW_DonateClick(playerid, row);
forward RW_DonateCefClick(playerid, id);
forward RW_DonateBonus(playerid, amount);
forward RW_CityOpen(playerid);
forward RW_StoryEvent(playerid, event);
forward RW_StoryTask(playerid);
forward RW_StoryClick(playerid, row);
forward RW_StoryCefClick(playerid, id);
forward RW_PropClick(playerid, row);
forward RW_JobHandOver(playerid);
forward RW_UberOrder(playerid);
forward RW_TaxiRankCall(playerid);
forward RW_LawyerCallFor(playerid, target);

/* ------------------------------------------------------------------ вокзал */
#define STATION_NAME                "Unity Station"
/* Настоящий Unity Station в Los Santos: El Corona/Unity, восточнее путей. */
#define STATION_X                   (1742.9)
#define STATION_Y                   (-1862.6)
#define STATION_Z                   (13.6)
#define STATION_ANGLE               (0.0)
#define CAMERA_ORBIT_RADIUS         (8.5)
#define CAMERA_ORBIT_HEIGHT         (6.2)
#define CAMERA_ORBIT_SPEED          (0.9)   /* градусов за тик таймера */

/* Номера действий в браузерном меню: не должны пересекаться со строками. */
#define CEF_JOB_QUIT                20
#define CEF_JOB_CLOSE               21

/* ------------------------------------------------------------------ настройки */
#define SPCAR_MIN_ADMIN_LEVEL       (0)      /* 0 — /spcar доступна всем игрокам */
#define SPCAR_COOLDOWN_MS           (1200)
#define VEHICLE_SLOTS               (100)
#define CABIN_MAX                   (20)
#define CEF_MODE_NONE               (0)
#define CEF_MODE_VEHICLE            (1)
#define CEF_MODE_PASSENGERS         (2)
#define CEF_MODE_POLICE             (3)
#define CEF_MODE_MDT                (4)
#define CEF_MODE_EMS                (5)
#define CEF_MODE_TRUNK              (6)
#define CEF_MODE_LOCKPICK           (7)
#define CEF_MODE_SERVICE            (8)
#define CEF_MODE_SHOP               (9)
#define CEF_MODE_BANK               (10)
#define CEF_MODE_JOB                (11)
#define CEF_MODE_APP                (12)
#define CEF_MODE_PROPERTY           (13)
#define CEF_MODE_DONATE             (14)
#define CEF_MODE_STORY              (15)
/* События сюжетной линии: их шлют все модули, поэтому они лежат здесь. */
#define STORY_EV_JOB        (1)         /* устроился на работу              */
#define STORY_EV_TASK       (2)         /* закрыл рабочее задание           */
#define STORY_EV_TAXI       (3)         /* вызвал такси                     */
#define STORY_EV_PROPERTY   (4)         /* купил дом                        */
#define STORY_EV_BUSINESS   (5)         /* купил бизнес                     */
#define STORY_EV_ROB        (6)         /* ограбил кассу                    */
#define STORY_EV_PAYDAY     (7)         /* получил зарплату                 */
#define STORY_EV_NIGHT      (8)         /* закрыл задание ночью             */
/* вкладки планшета служб: полиция видит 0-3 и 5, скорая — вызовы */
#define MDT_CALLS                   (4)
/* топливо */
#define FUEL_MAX                    100.0
#define FUEL_IDLE                   0.008      /* литров в секунду на холостых */
#define FUEL_SPEED                  0.0006     /* литров в секунду за км/ч     */
#define FUEL_PRICE                  3
#define FUEL_STATION_RANGE          12.0
/* тюнинг */
#define TUNE_PARTS                  5
#define TUNE_MAX_LEVEL              5
#define TUNE_SPEED_STEP             5.0        /* км/ч за уровень двигателя   */
#define TUNE_INSTALL_MS             10000
/* Loco Low Co, Willowfield (реальный тюнинг-гараж LS) */
#define TUNING_X                    2645.6
#define TUNING_Y                    -2020.3
#define TUNING_Z                    13.5
#define TUNING_RANGE                15.0
/* эвакуация и взлом */
/* Штрафстоянка LSPD за зданием участка, Pershing Square */
#define IMPOUND_X                   1635.0
#define IMPOUND_Y                   -1700.0
#define IMPOUND_Z                   13.6
#define IMPOUND_FEE                 500
#define PICK_STEPS                  3
#define PICK_MS                     8000
#define TRUNK_SLOTS                 10
#define PHONE_TARIFFS               (3)
/* приложения телефона и их настройки */
#define RADIO_ROWS              5           /* станций на экране радио      */
#define TAXI_RANK_RANGE         8.0         /* радиус стоянки такси         */
#define PHONE_APP_BANK              2
#define PHONE_APP_WEATHER           3
#define PHONE_APP_GROUP             4
#define PHONE_APP_LAWYER            5
#define PHONE_APP_UBER              6
#define PHONE_APP_NEWS              7
#define PHONE_APP_AUTO              8
#define PHONE_APP_CAMERA            9
#define PHONE_APP_RADIO            12
#define WEATHER_COUNT               6
#define WEATHER_CHANGE_SECONDS      1800
#define NEWS_PRICE                  100
#define NEWS_MAX_LEN                96
#define AUTO_DELIVER_FEE            200
#define AUTO_DELIVER_DISTANCE       50.0
#define UBER_BASE_FARE              50
#define UBER_PER_METRE              2
#define UBER_RANGE                  15.0
#define PHOTO_PLACES                6
#define ITEM_SIM                    8
#define ITEM_PHONE                  9
#define ITEM_CUFFS                  10
#define ITEM_TASER                  11
#define ITEM_BATON                  12
#define ITEM_BADGE                  13
#define ITEM_DEFIB                  14
#define ITEM_MEDKIT                 15
#define ITEM_LOCKPICK               16
#define ITEM_TUNING                 17
#define ITEM_PHOTO                  18
#define FACTION_NONE                0
#define FACTION_LSPD                1
#define FACTION_SHERIFF             2
#define FACTION_EMS                 3
#define FACTION_MECHANIC            4
#define EMS_DEFIB_CHARGES           3
/* All Saints General: куда скорая сдаёт пациентов. */
/* All Saints General Hospital, Los Santos */
#define HOSPITAL_X                  1175.8
#define HOSPITAL_Y                  -1324.7
#define HOSPITAL_Z                  15.4
#define HOSPITAL_RANGE              40.0
/* LSPD: entrance and the cells behind the station. */
#define POLICE_STATION_X            1544.6
#define POLICE_STATION_Y            -1675.5
#define POLICE_STATION_Z            13.6
#define POLICE_STATION_RANGE        40.0
#define POLICE_CELL_X               1568.6
#define POLICE_CELL_Y               -1694.3
#define POLICE_CELL_Z               5.9
#define CAL_9MM                     1
#define CAL_44                      2
#define CAL_12GA                    3
#define CAL_556                     4
#define CAL_762                     5
#define CAL_308                     6
#define CAL_COUNT                   6    /* максимум персональных машин  */
#define KEY_RANGE                   (6.0)    /* радиус взаимодействия с авто */
#define SPEEDO_INTERVAL             (150)    /* мс, частота обновления HUD   */
#define ENFORCE_INTERVAL            (400)    /* мс, проверка права на езду   */
#define DEFAULT_SKIN                (0)
#define DEFAULT_MONEY               (2500)
#define MAX_LOGIN_ATTEMPTS          (5)

#define DIALOG_LOGIN                 (9100)
#define DIALOG_REGISTER              (9101)
#define DIALOG_REGISTER_CONFIRM      (9102)
#define DIALOG_NATIVE_MENU           (9103)
#define RW_MAP_ICON                  (99)

/* ------------------------------------------------------------------ слоты авто */
enum E_VEHICLE_SLOT
{
    vsUsed,
    vsVehicleId,
    vsOwnerAcc,
    vsOwnerPlayer,
    vsModel,
    bool:vsKey,
    bool:vsDoors,
    bool:vsEngine,
    bool:vsLights,
    vsOwnerName[MAX_PLAYER_NAME]
};

/* ------------------------------------------------------------------ состояние */
new bool:gMapPending[MAX_PLAYERS], bool:gMapHasPoint[MAX_PLAYERS];
new bool:gRegPending[MAX_PLAYERS];
new gRegPassword[MAX_PLAYERS][65];
new Float:gMapPoint[MAX_PLAYERS][3];
new gControlKeys[MAX_PLAYERS][3], gControlVehicle[MAX_PLAYERS], gControlMode[MAX_PLAYERS], gControlTick[MAX_PLAYERS];
new DB:gDB;
new bool:gDBWritable;

new gVeh[VEHICLE_SLOTS][E_VEHICLE_SLOT];
new gVehSlotOf[MAX_VEHICLES];          /* vehicleid -> слот (или -1)         */
new gVehSlotCount;
new gVehKeyUid[VEHICLE_SLOTS], gVehGarageId[VEHICLE_SLOTS], gVehSpawnStamp[VEHICLE_SLOTS], gVehStampCounter;

/* Weapon state lives here because the inventory include needs it earlier. */
new gWpActive[MAX_PLAYERS], gWpWeapon[MAX_PLAYERS], gWpAmmo[MAX_PLAYERS];
new bool:gWpDirty[MAX_PLAYERS];

/* Medical state: bandages, adrenaline and the downed state after death. */
new bool:gDowned[MAX_PLAYERS];
new gDownEnd[MAX_PLAYERS], gDownStamp[MAX_PLAYERS];
new Float:gDownX[MAX_PLAYERS], Float:gDownY[MAX_PLAYERS], Float:gDownZ[MAX_PLAYERS];
new bool:gMedBusy[MAX_PLAYERS], gMedCooldown[MAX_PLAYERS], gMedTick[MAX_PLAYERS];
new PlayerText:gMedTD[MAX_PLAYERS][9];
new bool:gMedCreated[MAX_PLAYERS];

new bool:gLogged[MAX_PLAYERS];         /* игрок авторизован                  */
new gAccountId[MAX_PLAYERS];           /* id аккаунта в БД (-1 — гость)      */
new gAdminLevel[MAX_PLAYERS];
new gSkinId[MAX_PLAYERS];
new gMoney[MAX_PLAYERS];

new bool:gAuthBusy[MAX_PLAYERS];

new bool:gSpeedoShown[MAX_PLAYERS];
new gSpeedoVehicle[MAX_PLAYERS];

new bool:gNoDrive[MAX_PLAYERS];        /* за рулём, но ехать нельзя          */
new gEnforceLastTick[MAX_PLAYERS];
new gLastAuthTick[MAX_PLAYERS];
new gLoginAttempts[MAX_PLAYERS];
new gLastSpcarTick[MAX_PLAYERS];

new gAuthTimer[MAX_PLAYERS];           /* таймер камеры на экране входа      */
#define SKIN_MAX (16)
new gSkinTxd[SKIN_MAX][32];         /* client-side TXD names (arm/*.txd)      */
new gSkinTexture[SKIN_MAX][32];     /* texture names inside those TXDs        */
new gInvWorn[MAX_PLAYERS];          /* worn body armour item uid (0 = none)   */
new gArmourData[MAX_PLAYERS];       /* variant of the worn armour             */
new gArmourHp[MAX_PLAYERS];         /* armour points left                     */
new gArmourObject[MAX_PLAYERS];     /* server object used for a custom TXD    */
new bool:gArmourFitting[MAX_PLAYERS];
new gArmourFitSkin[MAX_PLAYERS], gArmourFitUid[MAX_PLAYERS];
new bool:gCefEnabled;                /* the CEF plugin answered the probe    */
new bool:gCefReady[MAX_PLAYERS];     /* browsers created for this player     */
new gCefMode[MAX_PLAYERS];           /* which CEF screen is open             */
new gCabinVehicle[MAX_PLAYERS];
new gCabinSeat[MAX_PLAYERS];
new gPlayerVehSlot[MAX_PLAYERS];       /* слот персональной машины (-1)      */

/* ------------------------------------------------------------------ утилиты */

stock RW_Send(playerid, const text[])
{
    SendClientMessage(playerid, COLOR_WHITE, text);
    return 1;
}

stock RW_Info(playerid, const text[])
{
    new buf[256];
    format(buf, sizeof buf, "%s %s", RW_TAG, text);
    SendClientMessage(playerid, COLOR_GREY, buf);
    RW_UI_Notify(playerid, text, 0);
    return 1;
}

stock RW_Ok(playerid, const text[])
{
    new buf[256];
    format(buf, sizeof buf, "%s {7BD88F}/{FFFFFF} %s", RW_TAG, text);
    SendClientMessage(playerid, COLOR_WHITE, buf);
    RW_UI_Notify(playerid, text, 1);
    return 1;
}

stock RW_Error(playerid, const text[])
{
    new buf[256];
    format(buf, sizeof buf, "%s {FF6B6B}/{FFFFFF} %s", RW_TAG, text);
    SendClientMessage(playerid, COLOR_WHITE, buf);
    RW_UI_Notify(playerid, text, 2);
    return 1;
}

stock RW_Accent(playerid, const text[])
{
    new buf[256];
    format(buf, sizeof buf, "%s %s", RW_TAG, text);
    SendClientMessage(playerid, COLOR_ACCENT, buf);
    return 1;
}

stock RW_GetNick(playerid, dest[], size = sizeof dest)
{
    GetPlayerName(playerid, dest, size);
    return 1;
}

/* Ник в SA-MP: 3..20 символов, A-Z a-z 0-9 _ $ = ( ) [ ] { } . @ */
stock bool:RW_ValidNick(const nick[])
{
    new len = strlen(nick);
    if (len < 3 || len > MAX_PLAYER_NAME - 1) {
        return false;
    }

    for (new i = 0; i < len; i++) {
        new c = nick[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            continue;
        }
        switch (c) {
            case '_', '$', '=', '(', ')', '[', ']', '{', '}', '.', '@': continue;
        }
        return false;
    }
    return true;
}

/* Только безопасные для SQL символы (ник уже провалидирован, но лишним не будет) */
stock bool:RW_SafeSqlText(const text[])
{
    new len = strlen(text);
    for (new i = 0; i < len; i++) {
        if (text[i] == '\'' || text[i] == '"' || text[i] == '\\') {
            return false;
        }
    }
    return true;
}

stock RW_FormatMoney(amount, dest[], size = sizeof dest)
{
    new buf[32];
    format(buf, sizeof buf, "%d", amount);

    new out[32];
    new len = strlen(buf);
    new pos = 0;

    for (new i = 0; i < len; i++) {
        if (i > 0 && ((len - i) % 3) == 0) {
            out[pos++] = ' ';
        }
        out[pos++] = buf[i];
    }
    out[pos] = EOS;

    format(dest, size, "%s", out);
    return 1;
}

// Cash is mirrored in gMoney so it survives relogs; the HUD follows the native.
stock RW_SyncMoney(playerid)
{
    ResetPlayerMoney(playerid);
    GivePlayerMoney(playerid, gMoney[playerid]);
    return 1;
}
stock RW_SetMoney(playerid, amount)
{
    if (amount < 0) amount = 0;
    gMoney[playerid] = amount;
    return RW_SyncMoney(playerid);
}
stock bool:RW_TakeMoney(playerid, amount)
{
    if (amount < 0 || gMoney[playerid] < amount) return false;
    gMoney[playerid] -= amount;
    RW_SyncMoney(playerid);
    return true;
}
stock RW_AddMoney(playerid, amount)
{
    if (amount <= 0) return 0;
    gMoney[playerid] += amount;
    RW_SyncMoney(playerid);
    return 1;
}
stock RW_TrimLeft(const source[], dest[], size = sizeof dest)
{
    new start = 0;
    while (source[start] == ' ' || source[start] == '\t') {
        start++;
    }
    strmid(dest, source, start, strlen(source), size);
    return 1;
}

stock bool:RW_IsNumeric(const text[])
{
    if (text[0] == EOS) {
        return false;
    }
    new i = 0;
    if (text[0] == '-') {
        i = 1;
        if (text[1] == EOS) {
            return false;
        }
    }
    for (; text[i] != EOS; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ машины */

stock RW_IsOwnedSlot(vehicleid)
{
    if (vehicleid < 0 || vehicleid >= MAX_VEHICLES) {
        return -1;
    }
    return gVehSlotOf[vehicleid];
}

stock RW_VehicleOwnerAcc(vehicleid)
{
    new slot = RW_IsOwnedSlot(vehicleid);
    if (slot < 0) {
        return -1;
    }
    return gVeh[slot][vsOwnerAcc];
}

stock bool:RW_IsVehicleOwner(playerid, vehicleid)
{
    new slot = RW_IsOwnedSlot(vehicleid);
    if (slot < 0 || !gLogged[playerid]) {
        return false;
    }
    return (gVeh[slot][vsOwnerAcc] == gAccountId[playerid]);
}

stock RW_VehicleName(model, dest[], size = sizeof dest)
{
    switch (model) {
        case 400: format(dest, size, "Landstalker");
        case 401: format(dest, size, "Bravura");
        case 402: format(dest, size, "Buffalo");
        case 404: format(dest, size, "Perennial");
        case 405: format(dest, size, "Sentinel");
        case 406: format(dest, size, "Dumper");
        case 407: format(dest, size, "Firetruck");
        case 408: format(dest, size, "Trashmaster");
        case 410: format(dest, size, "Manana");
        case 411: format(dest, size, "Infernus");
        case 412: format(dest, size, "Voodoo");
        case 415: format(dest, size, "Cheetah");
        case 416: format(dest, size, "Ambulance");
        case 418: format(dest, size, "Moonbeam");
        case 420: format(dest, size, "Taxi");
        case 421: format(dest, size, "Washington");
        case 422: format(dest, size, "Bobcat");
        case 424: format(dest, size, "BF Injection");
        case 426: format(dest, size, "Premier");
        case 429: format(dest, size, "Banshee");
        case 436: format(dest, size, "Blade");
        case 438: format(dest, size, "Cabbie");
        case 439: format(dest, size, "Stallion");
        case 442: format(dest, size, "Romero");
        case 451: format(dest, size, "Turismo");
        case 458: format(dest, size, "Solair");
        case 466: format(dest, size, "Glendale");
        case 467: format(dest, size, "Oceanic");
        case 474: format(dest, size, "Hermes");
        case 475: format(dest, size, "Sabre");
        case 477: format(dest, size, "ZR-350");
        case 480: format(dest, size, "Comet");
        case 489: format(dest, size, "Rancher");
        case 496: format(dest, size, "Blista Compact");
        case 500: format(dest, size, "Mesa");
        case 506: format(dest, size, "Super GT");
        case 507: format(dest, size, "Elegant");
        case 516: format(dest, size, "Nebula");
        case 517: format(dest, size, "Majestic");
        case 518: format(dest, size, "Buccaneer");
        case 529: format(dest, size, "Willard");
        case 533: format(dest, size, "Feltzer");
        case 534: format(dest, size, "Remington");
        case 535: format(dest, size, "Slamvan");
        case 536: format(dest, size, "Blade");
        case 540: format(dest, size, "Vincent");
        case 541: format(dest, size, "Bullet");
        case 542: format(dest, size, "Clover");
        case 543: format(dest, size, "Sadler");
        case 545: format(dest, size, "Hustler");
        case 546: format(dest, size, "Intruder");
        case 547: format(dest, size, "Primo");
        case 550: format(dest, size, "Sunrise");
        case 551: format(dest, size, "Merit");
        case 558: format(dest, size, "Uranus");
        case 559: format(dest, size, "Jester");
        case 560: format(dest, size, "Sultan");
        case 561: format(dest, size, "Stratum");
        case 562: format(dest, size, "Elegy");
        case 565: format(dest, size, "Flash");
        case 566: format(dest, size, "Tahoma");
        case 567: format(dest, size, "Savanna");
        case 575: format(dest, size, "Broadway");
        case 576: format(dest, size, "Tornado");
        case 579: format(dest, size, "Huntley");
        case 580: format(dest, size, "Stafford");
        case 581: format(dest, size, "BF-400");
        case 585: format(dest, size, "Emperor");
        case 586: format(dest, size, "Wayfarer");
        case 587: format(dest, size, "Euros");
        case 589: format(dest, size, "Club");
        case 596: format(dest, size, "Police Car (LS)");
        case 597: format(dest, size, "Police Car (SF)");
        case 598: format(dest, size, "Police Car (LV)");
        case 599: format(dest, size, "Police Ranger");
        case 600: format(dest, size, "Picador");
        case 602: format(dest, size, "Alpha");
        case 603: format(dest, size, "Phoenix");
        case 604: format(dest, size, "Glendale");
        case 605: format(dest, size, "Sadler");
        case 609: format(dest, size, "Boxville");
        case 610: format(dest, size, "Farm Trailer");
        default:  format(dest, size, "Vehicle #%d", model);
    }
    return 1;
}

stock bool:RW_CabinIsPlane(model)
{
    switch (model) {
        case 460, 464, 476, 511, 512, 513, 519, 520, 553, 577, 592: return true;
    }
    return false;
}
