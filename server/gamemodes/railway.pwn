/*  ============================================================================
 *  railway.pwn Ч Ђ∆елезнодорожный вокзалї
 *  SA-MP 0.3.7 (R5-2) Х Windows server Х native TextDraw UI
 *
 *  ¬озможности:
 *    Х спавн на железнодорожном вокзале (Los Santos, Unity Station);
 *    Х регистраци€ и вход через красивый анимированный TextDraw-интерфейс;
 *    Х /spcar <id> Ч спавн стандартной машины, владелец Ч вызвавший;
 *    Х ездить может любой водитель со вставленным ключом и включЄнным мотором;
 *    Х двери открываютс€/закрываютс€ командой /door;
 *    Х при посадке за руль всегда показываетс€ TextDraw-спидометр с передачей.
 *
 *   одировка исходников: Windows-1251.
 *  ========================================================================= */

#pragma dynamic 32768

#include <railway/core>
#include <railway/ui>
#include <railway/accounts>
#include <railway/vehicles>
#include <railway/speedo>
#include <railway/interaction>
#include <railway/commands>

/* ------------------------------------------------------------------ запуск */

/* ------------------------------------------------------------------ точка входа */

/*  Ѕез main() сервер пишет "Run time error 20: Invalid index parameter
 *  (bad entry point)": SA-MP ищет точку входа в таблице публичных функций. */
main()
{
    print("[railway] √ейммод загружен, ждЄм OnGameModeInit.");
}

public OnGameModeInit()
{
    print(" ");
    print("  ============================================");
    print("   RAILWAY Ч Ђ∆елезнодорожный вокзалї v" RW_VERSION);
    print("   SA-MP 0.3.7 R5-2 Х Native TextDraw interface");
    print("  ============================================");
    print(" ");

    SetGameModeText("∆елезнодорожный вокзал");
    ShowPlayerMarkers(PLAYER_MARKERS_MODE_GLOBAL);
    ShowNameTags(1);
    AllowInteriorWeapons(1);
    SetWorldTime(10);
    SetWeather(0);
    EnableStuntBonusForAll(0);
    UsePlayerPedAnims();
    ManualVehicleEngineAndLights();

    for (new i = 0; i < MAX_VEHICLES; i++) {
        gVehSlotOf[i] = -1;
    }
    for (new i = 0; i < MAX_PLAYERS; i++) {
        gAccountId[i] = -1;
        gPlayerVehSlot[i] = -1;
        gSpeedoVehicle[i] = INVALID_VEHICLE_ID;
        gSkinId[i] = DEFAULT_SKIN;
        gMoney[i] = DEFAULT_MONEY;
    }
    gVehSlotCount = 0;

    if (!RW_DB_Init()) {
        print("[railway] ¬Ќ»ћјЌ»≈: база данных недоступна, регистраци€ работать не будет.");
    }

    RW_UI_Init();
    RW_Speedo_Init();

    print("[railway] √отово. ¬окзал: Los Santos Unity Station.");
    return 1;
}

public OnGameModeExit()
{
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p)) {
        RW_SaveAccount(p);
        RW_UI_Reset(p);
    }
    for (new i = 0; i < VEHICLE_SLOTS; i++) {
        if (gVeh[i][vsUsed]) {
            RW_Veh_DestroySlot(i, false);
        }
    }

    if (gDB != DB:0) {
        db_close(gDB);
        gDB = DB:0;
    }

    print("[railway] —ервер остановлен.");
    return 1;
}

/* ------------------------------------------------------------------ подключение */

public OnPlayerConnect(playerid)
{
    new string[256];
    new nick[MAX_PLAYER_NAME];

    gLogged[playerid] = false;
    gAccountId[playerid] = -1;
    gAdminLevel[playerid] = 0;
    gAuthBusy[playerid] = false;
    gLoginAttempts[playerid] = 0;
    gLastAuthTick[playerid] = 0;
    gLastSpcarTick[playerid] = 0;
    gEnforceLastTick[playerid] = 0;
    gPlayerVehSlot[playerid] = -1;
    gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    gSpeedoShown[playerid] = false;
    gNoDrive[playerid] = false;
    gSkinId[playerid] = DEFAULT_SKIN;
    gMoney[playerid] = DEFAULT_MONEY;
    RW_Speedo_ResetPlayer(playerid);
    RW_InteractionReset(playerid);
    gMapPending[playerid] = false;

    RW_GetNick(playerid, nick);

    SetPlayerColor(playerid, 0x8FD3FFFF);
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    TogglePlayerSpectating(playerid, true);
    SetPlayerPos(playerid, STATION_X, STATION_Y, STATION_Z);

    RW_StartAuthCamera(playerid);
    RW_UI_Create(playerid);
    RW_UI_OpenAuth(playerid);

    format(string, sizeof string, "%s {F5B04C}%s{FFFFFF}, добро пожаловать на вокзал!", RW_TAG, nick);
    SendClientMessage(playerid, COLOR_WHITE, string);

    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    RW_SaveAccount(playerid);
    RW_Veh_OnPlayerDisconnect(playerid);

    if (gAuthTimer[playerid] != 0) {
        KillTimer(gAuthTimer[playerid]);
        gAuthTimer[playerid] = 0;
    }
    gLogged[playerid] = false;
    gAccountId[playerid] = -1;
    gAdminLevel[playerid] = 0;
    gAuthBusy[playerid] = false;
    gSpeedoShown[playerid] = false;
    gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    gNoDrive[playerid] = false;
    gSkinId[playerid] = DEFAULT_SKIN;
    gMoney[playerid] = DEFAULT_MONEY;
    RW_UI_Reset(playerid);

    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    if (!gLogged[playerid] && !gUIInput[playerid]) RW_UI_OpenAuth(playerid);
    /*  лассический выбор класса отключЄн: игрок находитс€ в режиме камеры,
       пока не войдЄт в аккаунт через TextDraw. */
    return 1;
}

public OnPlayerRequestSpawn(playerid)
{
    return gLogged[playerid] ? 1 : 0;
}

public OnPlayerSpawn(playerid)
{
    new string[192];

    if (!gLogged[playerid]) {
        return 0;
    }

    SetPlayerPos(playerid, STATION_X, STATION_Y, STATION_Z);
    SetPlayerFacingAngle(playerid, STATION_ANGLE);
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    SetPlayerSkin(playerid, gSkinId[playerid]);
    SetPlayerHealth(playerid, 100.0);
    SetPlayerArmour(playerid, 0.0);
    SetPlayerWantedLevel(playerid, 0);
    SetCameraBehindPlayer(playerid);

    ResetPlayerMoney(playerid);
    GivePlayerMoney(playerid, gMoney[playerid]);

    SetPlayerColor(playerid, (gAdminLevel[playerid] > 0) ? 0xF5B04CFF : 0x8FD3FFFF);

    RW_Veh_OnPlayerSpawn(playerid);

    format(string, sizeof string, "%s ¬ы на перроне. {F5B04C}/spcar 411{FFFFFF} Ч получить машину.", RW_TAG);
    SendClientMessage(playerid, COLOR_WHITE, string);

    return 1;
}

public OnPlayerDeath(playerid, killerid, reason)
{
    RW_UI_Close(playerid);
    RW_UI_SpeedoHide(playerid);
    gMapPending[playerid] = false;
    gNoDrive[playerid] = false;
    return 1;
}

/* ------------------------------------------------------------------ транспорт */

public OnPlayerStateChange(playerid, newstate, oldstate)
{
    if (newstate == PLAYER_STATE_PASSENGER) {
        new v = GetPlayerVehicleID(playerid);
        new slot = gVehSlotOf[v];
        if (slot >= 0 && gVeh[slot][vsUsed] && !gVeh[slot][vsDoors]) RemovePlayerFromVehicle(playerid);
    }
    if (newstate == PLAYER_STATE_DRIVER) {
        new vehicleid = GetPlayerVehicleID(playerid);

        if (vehicleid > 0 && vehicleid < MAX_VEHICLES) {
            new slot = gVehSlotOf[vehicleid];

            /* ƒвери закрыты Ч в машину нельз€, даже если клиент пропустил замок. */
            if (slot >= 0 && gVeh[slot][vsUsed] && !gVeh[slot][vsDoors]) {
                RW_Error(playerid, "ƒверь закрыта. ¬ладелец может открыть еЄ командой /door.");
                RemovePlayerFromVehicle(playerid);
                return 1;
            }

            RW_Veh_OnEnterDriver(playerid, vehicleid);
        }
        return 1;
    }

    if (oldstate == PLAYER_STATE_DRIVER && newstate != PLAYER_STATE_DRIVER) {
        RW_Veh_OnExitDriver(playerid);
        return 1;
    }

    return 1;
}

public OnPlayerEnterVehicle(playerid, vehicleid, ispassenger)
{
    if (ispassenger) {
        return 1;
    }

    new slot = gVehSlotOf[vehicleid];
    if (slot >= 0 && gVeh[slot][vsUsed] && !gVeh[slot][vsDoors]) {
        RW_Info(playerid, "ƒверь закрыта Ч машина заперта. ѕопросите владельца открыть /door.");
    }
    return 1;
}

public OnPlayerExitVehicle(playerid, vehicleid)
{
    return 1;
}

public OnVehicleDeath(vehicleid, killerid)
{
    new slot = gVehSlotOf[vehicleid];
    if (slot >= 0 && gVeh[slot][vsUsed]) {
        SetTimerEx("RW_Veh_RespawnLater", 6000, false, "i", vehicleid);
    }
    return 1;
}

/* ------------------------------------------------------------------ каждый тик */

public OnPlayerUpdate(playerid)
{
    if (!gLogged[playerid]) {
        return 1;
    }

    if (GetTickCount() - gEnforceLastTick[playerid] >= ENFORCE_INTERVAL) {
        gEnforceLastTick[playerid] = GetTickCount();
        RW_Veh_Enforce(playerid);
    }

    return 1;
}

/* ------------------------------------------------------------------ команды */

public OnPlayerCommandText(playerid, cmdtext[])
{
    return RW_CommandProcess(playerid, cmdtext);
}

public OnRconCommand(cmd[])
{
    new cmdName[32];
    new params[144];

    RW_ParseCommand(cmd, cmdName, sizeof cmdName, params, sizeof params);

    if (!strcmp(cmdName, "setadmin", true)) {
        new nick[MAX_PLAYER_NAME];
        new levelText[8];
        new i = 0;
        new j = 0;

        while (params[i] == ' ') {
            i++;
        }
        while (params[i] != EOS && params[i] != ' ' && j < sizeof nick - 1) {
            nick[j++] = params[i++];
        }
        nick[j] = EOS;

        while (params[i] == ' ') {
            i++;
        }
        j = 0;
        while (params[i] != EOS && j < sizeof levelText - 1) {
            levelText[j++] = params[i++];
        }
        levelText[j] = EOS;

        if (nick[0] == EOS || !RW_IsNumeric(levelText)) {
            print("»спользование: setadmin <ник> <0-3>");
            return 1;
        }

        new level = strval(levelText);
        if (level < 0) level = 0;
        if (level > 3) level = 3;

        RW_AdminSetLevel(nick, level);
        printf("”ровень администратора %d выдан: %s", level, nick);
        return 1;
    }

    if (!strcmp(cmdName, "dbcheck", true)) {
        printf("Ѕаза: handle=%d, аккаунтов=%d", _:gDB, RW_DB_AccountCount());

        if (gDB == DB:0) {
            print("Ѕаза: соединение Ќ≈ открыто (смотрите сообщени€ при старте сервера).");
        } else if (!RW_DB_TableExists("accounts")) {
            print("Ѕаза: нет таблицы accounts.");
        } else if (RW_DB_Probe()) {
            print("Ѕаза: запись и чтение работают.");
        } else {
            print("Ѕаза: таблица есть, но запись не сохран€етс€.");
            print("”далите scriptfiles/railway.db и перезапустите сервер.");
        }
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ диалоги (резерв) */

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (gLogged[playerid]) return 0;
    if (dialogid != DIALOG_LOGIN && dialogid != DIALOG_REGISTER) return 0;
    gUIInput[playerid] = false;
    if (!response) { Kick(playerid); return 1; }
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);
    new bool:exists = RW_DB_AccountExists(nick);
    if ((dialogid == DIALOG_REGISTER && exists) || (dialogid == DIALOG_LOGIN && !exists)) {
        RW_ShowLoginDialog(playerid);
        return 1;
    }
    RW_AuthSubmit(playerid, !exists, inputtext, inputtext);
    return 1;
}

public OnPlayerClickMap(playerid, Float:fX, Float:fY, Float:fZ)
{
    if (!gLogged[playerid] || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return 1;
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return 1;
    if (gControlMode[playerid] != 0) return 1;
    if (!(fX >= -3000.0 && fX <= 3000.0 && fY >= -3000.0 && fY <= 3000.0 && fZ >= -100.0 && fZ <= 1500.0)) return 1;
    gMapPoint[playerid][0] = fX; gMapPoint[playerid][1] = fY; gMapPoint[playerid][2] = fZ;
    gMapPending[playerid] = true;
    RW_UI_Panel(playerid, UI_MAP, "TELEPORT", "K METKE NA KARTE?");
    RW_UI_Row(playerid, 0, "DA / TELEPORT");
    RW_UI_Row(playerid, 1, "NET / OTMENA");
    return 1;
}


public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
    if (!gUICreated[playerid]) return 0;
    if (playertextid == gUI[playerid][11]) {
        if (!gLogged[playerid]) Kick(playerid);
        else RW_UI_Close(playerid);
        return 1;
    }
    for (new i; i < 8; i++) if (playertextid == gUI[playerid][3+i]) return RW_UI_Click(playerid, i);
    return 0;
}
public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
    if (clickedid != Text:INVALID_TEXT_DRAW || gUIInput[playerid]) return 0;
    if (gUIMode[playerid] == UI_AUTH && !gLogged[playerid]) SelectTextDraw(playerid, 0xD8C39BFF);
    else if (gUIMode[playerid] != 0) RW_UI_Close(playerid);
    return 1;
}
public OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
    if (!gLogged[playerid] || gUIMode[playerid] != 0 || gUIInput[playerid]) return 1;
    if (GetTickCount()-gControlTick[playerid] < 250) return 1;
    for (new i; i < 3; i++) {
        new key = gControlKeys[playerid][i];
        if (!(newkeys & key) || (oldkeys & key)) continue;
        gControlTick[playerid] = GetTickCount();
        if (i == 2) RW_ControlOpen(playerid, false);
        else if (i == 1) RW_EngineCommand(playerid);
        else RW_ControlExecute(playerid, RW_ControlTarget(playerid), 0);
        break;
    }
    return 1;
}
