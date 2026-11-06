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

/* ------------------------------------------------------------------ вокзал */
#define STATION_NAME                "Unity Station"
#define STATION_X                   (1729.3188)
#define STATION_Y                   (-1943.7242)
#define STATION_Z                   (13.5451)
#define STATION_ANGLE               (0.0)
#define CAMERA_ORBIT_RADIUS         (8.5)
#define CAMERA_ORBIT_HEIGHT         (6.2)
#define CAMERA_ORBIT_SPEED          (0.9)   /* градусов за тик таймера */

/* ------------------------------------------------------------------ настройки */
#define SPCAR_MIN_ADMIN_LEVEL       (0)      /* 0 — /spcar доступна всем игрокам */
#define SPCAR_COOLDOWN_MS           (1200)
#define VEHICLE_SLOTS               (100)
#define CABIN_MAX                   (20)
#define CEF_MODE_NONE               (0)
#define CEF_MODE_VEHICLE            (1)
#define CEF_MODE_PASSENGERS         (2)
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
