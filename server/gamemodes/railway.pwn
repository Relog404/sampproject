/*  ============================================================================
 *  railway.pwn — «Железнодорожный вокзал»
 *  SA-MP 0.3.7 (R5-2) • Windows server • CEF UI (aurora-mp/omp-cef)
 *
 *  Возможности:
 *    • спавн на железнодорожном вокзале (Los Santos, Unity Station);
 *    • регистрация и вход через красивый анимированный CEF-интерфейс;
 *    • /spcar <id> — спавн стандартной машины, владелец — вызвавший;
 *    • ездить может только владелец с вставленным ключом (/key);
 *    • двери открываются/закрываются командой /door;
 *    • при посадке за руль всегда показывается CEF-спидометр с передачей.
 *
 *  Кодировка исходников: Windows-1251.
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

/*  Без main() сервер пишет "Run time error 20: Invalid index parameter
 *  (bad entry point)": SA-MP ищет точку входа в таблице публичных функций. */
main()
{
    print("[railway] Гейммод загружен, ждём OnGameModeInit.");
}

public OnGameModeInit()
{
    print(" ");
    print("  ============================================");
    print("   RAILWAY — «Железнодорожный вокзал» v" RW_VERSION);
    print("   SA-MP 0.3.7 R5-2 • CEF interface");
    print("  ============================================");
    print(" ");

    SetGameModeText("Железнодорожный вокзал");
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
        print("[railway] ВНИМАНИЕ: база данных недоступна, регистрация работать не будет.");
    }

    RW_UI_Init();
    RW_Speedo_Init();

    print("[railway] Готово. Вокзал: Los Santos Unity Station.");
    return 1;
}

public OnGameModeExit()
{
    for (new i = 0; i < VEHICLE_SLOTS; i++) {
        if (gVeh[i][vsUsed]) {
            RW_Veh_DestroySlot(i, false);
        }
    }

    if (gDB != DB:0) {
        db_close(gDB);
        gDB = DB:0;
    }

    print("[railway] Сервер остановлен.");
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
    gAuthFallbackSent[playerid] = false;
    gLoginAttempts[playerid] = 0;
    gLastAuthTick[playerid] = 0;
    gLastSpcarTick[playerid] = 0;
    gEnforceLastTick[playerid] = 0;
    gPlayerVehSlot[playerid] = -1;
    gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    gSpeedoShown[playerid] = false;
    gAuthBrowserShown[playerid] = false;
    gNoDrive[playerid] = false;
    gCefReady[playerid] = false;
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
    RW_ArmFallback(playerid);

    if (gCefReady[playerid] && !gLogged[playerid]) {
        RW_UI_OpenAuth(playerid);
    }

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
    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
        gFallbackTimer[playerid] = 0;
    }

    gLogged[playerid] = false;
    gAccountId[playerid] = -1;
    gAdminLevel[playerid] = 0;
    gCefReady[playerid] = false;
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
    /* Классический выбор класса отключён: игрок находится в режиме камеры,
       пока не войдёт в аккаунт через CEF. */
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

    format(string, sizeof string, "%s Вы на перроне. {F5B04C}/spcar 411{FFFFFF} — получить машину.", RW_TAG);
    SendClientMessage(playerid, COLOR_WHITE, string);

    return 1;
}

public OnPlayerDeath(playerid, killerid, reason)
{
    if (gControlMode[playerid] != 0) RW_ControlClose(playerid, CONTROL_BROWSER);
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

            /* Двери закрыты — в машину нельзя, даже если клиент пропустил замок. */
            if (slot >= 0 && gVeh[slot][vsUsed] && !gVeh[slot][vsDoors]) {
                RW_Error(playerid, "Дверь закрыта. Владелец может открыть её командой /door.");
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
        RW_Info(playerid, "Дверь закрыта — машина заперта. Попросите владельца открыть /door.");
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
            print("Использование: setadmin <ник> <0-3>");
            return 1;
        }

        new level = strval(levelText);
        if (level < 0) level = 0;
        if (level > 3) level = 3;

        RW_AdminSetLevel(nick, level);
        printf("Уровень администратора %d выдан: %s", level, nick);
        return 1;
    }

    if (!strcmp(cmdName, "dbcheck", true)) {
        printf("База: handle=%d, аккаунтов=%d", _:gDB, RW_DB_AccountCount());

        if (gDB == DB:0) {
            print("База: соединение НЕ открыто (смотрите сообщения при старте сервера).");
        } else if (!RW_DB_TableExists("accounts")) {
            print("База: нет таблицы accounts.");
        } else if (RW_DB_Probe()) {
            print("База: запись и чтение работают.");
        } else {
            print("База: таблица есть, но запись не сохраняется.");
            print("Удалите scriptfiles/railway.db и перезапустите сервер.");
        }
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ диалоги (резерв) */

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (dialogid == DIALOG_MAP_TELEPORT) {
        if (!gMapPending[playerid]) return 1;
        gMapPending[playerid] = false;
        if (!response || !gLogged[playerid] || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return 1;
        if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return 1;
        SetPlayerPosFindZ(playerid, gMapPoint[playerid][0], gMapPoint[playerid][1], gMapPoint[playerid][2] + 1.0);
        SetCameraBehindPlayer(playerid);
        RW_Ok(playerid, "Вы телепортировались к метке.");
        return 1;
    }
    if (gLogged[playerid]) return 0;
    if (dialogid != DIALOG_LOGIN && dialogid != DIALOG_REGISTER) return 0;
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
    ShowPlayerDialog(playerid, DIALOG_MAP_TELEPORT, DIALOG_STYLE_MSGBOX, "Телепорт по метке", "Телепортироваться к выбранной метке на карте?", "Да", "Нет");
    return 1;
}

/* ------------------------------------------------------------------ CEF */

public OnCefInitialize(playerid, bool:success, E_CEF_INIT_REASON:reason, const message[])
{
    gCefStage[playerid] = success ? 1 : -1;
    gCefReason[playerid] = _:reason;
    printf("[railway:cef] initialize player=%d success=%d reason=%d elapsed=%dms message=%s", playerid, _:success, _:reason, GetTickCount()-gCefStarted[playerid], message);
    if (!success) {
        printf("[railway] CEF не инициализирован у игрока %d (reason=%d): %s", playerid, _:reason, message);
        gCefReady[playerid] = false;
        /* Не ждём таймер: сразу даём текстовый вход. */
        RW_AuthFallbackNow(playerid, "CEF не инициализирован");
        return 1;
    }
    return 1;
}

public OnCefDownloadStart(playerid)
{
    gCefStage[playerid] = 2;
    printf("[railway:cef] download start player=%d elapsed=%dms", playerid, GetTickCount()-gCefStarted[playerid]);
    return 1;
}

public OnCefDownloadProgress(playerid, const fileName[], filePercent, totalPercent,
    fileDownloadedKb, fileTotalKb, totalDownloadedKb, totalKb)
{
    #pragma unused filePercent
    #pragma unused fileDownloadedKb
    #pragma unused fileTotalKb
    if (gCefStage[playerid] != 2 || totalPercent/10 != gCefPercent[playerid]/10) {
        printf("[railway:cef] download player=%d elapsed=%dms total=%d%% kb=%d/%d file=%s", playerid, GetTickCount()-gCefStarted[playerid], totalPercent, totalDownloadedKb, totalKb, fileName);
    }
    gCefStage[playerid] = 2;
    gCefPercent[playerid] = totalPercent;
    // Do not postpone fallback forever while the transfer trickles in.
    return 1;
}

public OnCefDownloadFinish(playerid)
{
    gCefStage[playerid] = 3;
    gCefPercent[playerid] = 100;
    printf("[railway:cef] download finish player=%d elapsed=%dms", playerid, GetTickCount()-gCefStarted[playerid]);
    return 1;
}

public OnCefReady(playerid)
{
    gCefStage[playerid] = 4;
    printf("[railway:cef] ready player=%d elapsed=%dms", playerid, GetTickCount()-gCefStarted[playerid]);
    gCefReady[playerid] = true;
    RW_InteractionInit(playerid);

    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
        gFallbackTimer[playerid] = 0;
    }

    if (!gLogged[playerid]) {
        ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
        gAuthFallbackSent[playerid] = false;
        RW_UI_OpenAuth(playerid);
    } else if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER) {
        RW_UI_SpeedoShow(playerid, GetPlayerVehicleID(playerid));
    }
    return 1;
}

public OnCefBrowserCreated(playerid, browserid, bool:success, E_CEF_CREATE_STATUS:code, const reason[])
{
    printf("[railway:cef] browser player=%d browser=%d success=%d code=%d reason=%s", playerid, browserid, _:success, _:code, reason);
    RW_UI_OnBrowserCreated(playerid, browserid, success);
    if (browserid == CEF_BROWSER_SPEEDO && !success) gSpeedoShown[playerid] = false;
    if (browserid == CONTROL_BROWSER && !success) {
        gControlCreated[playerid] = false;
        gControlReady[playerid] = false;
    }

    if (!success) {
        printf("[railway] Браузер %d не создан у игрока %d: %s", browserid, playerid, reason);

        if (browserid == CEF_BROWSER_AUTH) {
            gAuthBrowserCreated[playerid] = false;
            RW_AuthFallbackNow(playerid, "браузер входа не создан");
        }
    }
    return 1;
}

public OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
    // Default GTA vehicle Look Right is E. Physical rebindings apply once CEF is ready.
    if (gCefReady[playerid] || !gLogged[playerid] || gControlMode[playerid] != 0) return 1;
    if ((newkeys & KEY_LOOK_RIGHT) && !(oldkeys & KEY_LOOK_RIGHT) && GetPlayerState(playerid) == PLAYER_STATE_DRIVER) {
        if (GetTickCount() - gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        RW_EngineCommand(playerid);
    }
    return 1;
}
