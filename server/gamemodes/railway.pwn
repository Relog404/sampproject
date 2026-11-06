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

/* Browser interface on the omp-cef plugin. Set to 0 to build a gamemode that
   needs no plugin at all - the TextDraw interface is then the only one. */
#define RAILWAY_CEF             (1)

#include <railway/core>
#include <railway/ui>
#include <railway/accounts>
#include <railway/inventory>
#include <railway/phone>
#include <railway/police>
#include <railway/ground>
#include <railway/weapons>
#include <railway/medical>
#include <railway/ems>
#include <railway/vehicles>
#include <railway/speedo>
#include <railway/interaction>
#include <railway/garage>
#include <railway/transport>
#include <railway/economy>
#include <railway/motion>
#include <railway/cabin>
#include <railway/skins>
#include <railway/armour>
#if RAILWAY_CEF
    #include <railway/cefui>
#else
    #include <railway/cefstub>
#endif
#include <railway/commands>

/* ------------------------------------------------------------------ запуск */

/* ------------------------------------------------------------------ точка входа */

/*  Ѕез main() сервер пишет "Run time error 20: Invalid index parameter
 *  (bad entry point)": SA-MP ищет точку входа в таблице публичных функций. */
main()
{
    print("[railway] Gamemode loaded; waiting for OnGameModeInit.");
}

public OnGameModeInit()
{
    print(" ");
    print("  ============================================");
    print("   RAILWAY - Unity Station v" RW_VERSION);
    print("   SA-MP 0.3.7 R5-2 / Native TextDraw interface");
    print("  ============================================");
    print(" ");

    SetGameModeText("Unity Station");
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
        gKeyTarget[i] = -1;
        gSpeedoVehicle[i] = INVALID_VEHICLE_ID;
        gSkinId[i] = DEFAULT_SKIN;
        gMoney[i] = DEFAULT_MONEY;
    }
    gVehSlotCount = 0;

    if (!RW_DB_Init()) {
        print("[railway] WARNING: database unavailable; registration disabled.");
    }

    if (!RW_GarageSchema()) print("[garage] Persistent garage unavailable.");
    if (!RW_InvInit()) print("[inventory] Inventory unavailable; existing data preserved.");
    if (!RW_PhoneSchema()) print("[phone] Phone tables unavailable; calls and SMS disabled.");
    if (!RW_PoliceSchema()) print("[police] Police tables unavailable; arrests and fines disabled.");
    RW_PoliceInit();
    if (!RW_EmsSchema()) print("[ems] Emergency tables unavailable; 911 calls disabled.");
    if (!RW_TransportSchema()) print("[transport] Transport tables unavailable; trunks and tuning disabled.");
    if (!RW_EconomySchema()) print("[economy] Economy tables unavailable; jobs, shops and bank accounts disabled.");
    RW_EconomyInit();
    RW_GarageInit();
    RW_MotionInit();
    SetTimer("RW_PhonePulse", 1000, true);
    SetTimer("RW_PolicePulse", 1000, true);
    SetTimer("RW_EmsPulse", 1000, true);
    SetTimer("RW_TransportPulse", 1000, true);
    SetTimer("RW_EconomyPulse", 1000, true);
    RW_CefInit();
    SetTimer("RW_CabinPulse", 100, true);
    RW_UI_Init();
    RW_Speedo_Init();

    print("[railway] Ready. Los Santos Unity Station.");
    return 1;
}

public OnGameModeExit()
{
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p)) {
        RW_ClearRegisterPassword(p);
        RW_SaveAccount(p);
        RW_UI_Reset(p);
    }
    for (new i = 0; i < VEHICLE_SLOTS; i++) {
        if (gVeh[i][vsUsed]) {
            RW_Veh_DestroySlot(i, false);
        }
    }

    RW_GarageExit();
    RW_InvExit();
    if (gDB != DB:0) {
        db_close(gDB);
        gDB = DB:0;
    }

    print("[railway] Server stopped.");
    return 1;
}

/* ------------------------------------------------------------------ подключение */

public OnPlayerConnect(playerid)
{
    new string[256];
    new nick[MAX_PLAYER_NAME];

    RW_InvReset(playerid);
    RW_WpReset(playerid);
    RW_MedReset(playerid);
    RW_CefReset(playerid);
    RW_PhoneReset(playerid);
    RW_PoliceReset(playerid);
    RW_EmsReset(playerid);
    RW_TransportReset(playerid);
    RW_EconomySave(playerid);
    RW_EconomyReset(playerid);
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
    gMapHasPoint[playerid] = false;
    gUIRussianDialogs[playerid] = true;
    RW_ClearRegisterPassword(playerid);
    RemovePlayerMapIcon(playerid, RW_MAP_ICON);

    RW_GetNick(playerid, nick);

    SetPlayerColor(playerid, 0x8FD3FFFF);
    SetPlayerInterior(playerid, 0);
    SetPlayerVirtualWorld(playerid, 0);
    TogglePlayerSpectating(playerid, true);
    SetPlayerPos(playerid, STATION_X, STATION_Y, STATION_Z);

    RW_StartAuthCamera(playerid);
    RW_UI_Create(playerid);
    RW_UI_OpenAuth(playerid);

    format(string, sizeof string, "%s {F5B04C}%s{FFFFFF}, welcome to Unity Station!", RW_TAG, nick);
    SendClientMessage(playerid, COLOR_WHITE, string);
    SendClientMessage(playerid, COLOR_GREY, "Interface: /uiru - native dialogs; /uitd - compact TextDraw panels. All text is English.");

    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    RW_WpFlush(playerid);
    RW_CabinReset(playerid);
    RW_ArmourReset(playerid);
    RW_PhoneSave(playerid);
    RW_PoliceSave(playerid);
    RW_EmsCallClose(playerid);
    RW_EmsReset(playerid);
    RW_CefReset(playerid);
    RW_PhoneReset(playerid);
    RW_PoliceReset(playerid);
    RW_ClearRegisterPassword(playerid);
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

    if (RW_MedSpawn(playerid)) return 1;

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
    RW_InvAttach(playerid,true);

    format(string, sizeof string, "%s Welcome to the station. {F5B04C}/spcar 411{FFFFFF} - temporary car; /inv - inventory.", RW_TAG);
    SendClientMessage(playerid, COLOR_WHITE, string);

    return 1;
}

public OnPlayerDeath(playerid, killerid, reason)
{
    RW_UI_Close(playerid);
    RW_UI_SpeedoHide(playerid);
    gMapPending[playerid] = false;
    gNoDrive[playerid] = false;
    new Float:dx, Float:dy, Float:dz;
    GetPlayerPos(playerid, dx, dy, dz);
    RW_MedDown(playerid, dx, dy, dz);
    RW_PoliceOnDeath(playerid);
    return 1;
}
public OnPlayerWeaponShot(playerid, weaponid, hittype, hitid, Float:fX, Float:fY, Float:fZ)
{
    RW_WpShot(playerid, weaponid);
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
                RW_Error(playerid, "Door locked. A key holder can unlock it with /door.");
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
        RW_Info(playerid, "Car locked. Ask a key holder to use /door.");
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
        SetTimerEx("RW_Veh_RespawnLater", 6000, false, "ii", vehicleid, gVehSpawnStamp[slot]);
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

public OnPlayerText(playerid, text[])
{
    if (gPhoneState[playerid] == PHONE_TALK) return RW_PhoneSay(playerid, text);
    if (gCaseId[playerid]) return RW_PoliceCaseSay(playerid, text);
    return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    return RW_CommandProcess(playerid, cmdtext);
}

// SA-MP 0.3.7 dispatches custom RCON commands to filterscripts. The bundled
// admin_console relay calls this unique public; OnRconCommand is a fallback.
public OnRconCommand(cmd[]) { return RW_ConsoleCommand(cmd); }
forward RW_ConsoleCommand(cmd[]);
public RW_ConsoleCommand(cmd[])
{
    new cmdName[32], params[144], clean[200];
    if (strlen(cmd) >= sizeof clean) { print("[console] Command too long."); return 0; }
    RW_TrimLeft(cmd, clean, sizeof clean);
    RW_ParseCommand(clean, cmdName, sizeof cmdName, params, sizeof params);
    if (!strcmp(cmdName, "setadmin", true)) return RW_AdminConsoleSet(params);

    if (!strcmp(cmdName, "dbcheck", true)) {
        printf("Database: handle=%d, accounts=%d", _:gDB, RW_DB_AccountCount());
        if (gDB != DB:0) RW_DB_LogPath();

        if (gDB == DB:0) {
            print("Database connection is NOT open. Check startup logs.");
        } else if (!RW_DB_TableExists("accounts")) {
            print("Database: accounts table missing.");
        } else if ((gDBWritable = RW_DB_Probe())) {
            print("Database: read/write check passed.");
        } else {
            print("Database: table exists but writes fail.");
            print("Do not delete railway.db. Check permissions, disk space, locks and schema.");
        }
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ диалоги (резерв) */

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (dialogid == DIALOG_NATIVE_MENU) return RW_UI_NativeResponse(playerid, response, listitem);
    if (gLogged[playerid]) return 0;
    if (dialogid != DIALOG_LOGIN && dialogid != DIALOG_REGISTER && dialogid != DIALOG_REGISTER_CONFIRM) return 0;
    gUIInput[playerid] = false;
    if (!response) {
        RW_ClearRegisterPassword(playerid);
        if (dialogid == DIALOG_REGISTER_CONFIRM) RW_ShowLoginDialog(playerid);
        else Kick(playerid);
        return 1;
    }
    new nick[MAX_PLAYER_NAME]; RW_GetNick(playerid, nick);
    new bool:exists = RW_DB_AccountExists(nick);
    if ((dialogid != DIALOG_LOGIN && exists) || (dialogid == DIALOG_LOGIN && !exists)) {
        RW_ClearRegisterPassword(playerid);
        RW_ShowLoginDialog(playerid);
        return 1;
    }
    if (dialogid == DIALOG_REGISTER) {
        if (strlen(inputtext) < 6 || strlen(inputtext) > 64 || !RW_SafeSqlText(inputtext)) {
            RW_AuthSubmit(playerid, true, inputtext, inputtext);
            return 1;
        }
        format(gRegPassword[playerid], 65, "%s", inputtext);
        gRegPending[playerid] = true;
        return RW_ShowRegisterConfirm(playerid);
    }
    if (dialogid == DIALOG_REGISTER_CONFIRM) {
        if (!gRegPending[playerid]) return RW_ShowLoginDialog(playerid);
        new password[65]; format(password, sizeof password, "%s", gRegPassword[playerid]);
        RW_ClearRegisterPassword(playerid);
        RW_AuthSubmit(playerid, true, password, inputtext);
        for (new i; i < sizeof password; i++) password[i] = 0;
        return 1;
    }
    RW_AuthSubmit(playerid, false, inputtext, "");
    return 1;
}

public OnPlayerClickMap(playerid, Float:fX, Float:fY, Float:fZ)
{
    if (!gLogged[playerid]) return 1;
    if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return 1;
    if (!(fX >= -3000.0 && fX <= 3000.0 && fY >= -3000.0 && fY <= 3000.0 && fZ >= -100.0 && fZ <= 1500.0)) return 1;
    gMapPoint[playerid][0] = fX; gMapPoint[playerid][1] = fY; gMapPoint[playerid][2] = fZ;
    gMapHasPoint[playerid] = true;
    SetPlayerMapIcon(playerid, RW_MAP_ICON, fX, fY, fZ, 0, 0xD8C39BFF, MAPICON_GLOBAL);
    return RW_MapPrompt(playerid);
}


public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
    if (gUIMode[playerid] == UI_GARAGE) return RW_GarageClick(playerid, playertextid);
    if (gUIMode[playerid] == UI_KEYMENU) return RW_KeyMenuClick(playerid, playertextid);
    if (gUIMode[playerid] == UI_DOWNED) return RW_MedClick(playerid, playertextid);
    if (gUIMode[playerid] == UI_INVENTORY) return RW_InvClick(playerid, playertextid);
    if (!gUICreated[playerid] || gUIRussianDialogs[playerid]) return 0;
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
    if (clickedid != Text:INVALID_TEXT_DRAW) return 0;
    // Consume cancellation echoes even if a new panel is already selecting.
    if (gUIExpectedCancels[playerid] > 0) { gUIExpectedCancels[playerid]--; return 1; }
    // Programmatic cancellation has already cleared this flag. A genuine
    // client Esc consumes the current selection once, without another cancel.
    if (!gUISelecting[playerid]) return 1;
    gUISelecting[playerid] = false;
    if (gUIInput[playerid]) return 1;
    if (gUIMode[playerid] == UI_INVENTORY || gUIMode[playerid] == UI_GARAGE || gUIMode[playerid] == UI_KEYMENU || gUIMode[playerid] == UI_DOWNED) return RW_UI_Close(playerid);
    if (gUIRussianDialogs[playerid]) return 1;
    if (gUIMode[playerid] == UI_AUTH && !gLogged[playerid]) RW_UI_SelectBegin(playerid);
    else if (gUIMode[playerid] != 0) RW_UI_Close(playerid);
    return 1;
}
public OnPlayerKeyStateChange(playerid, newkeys, oldkeys)
{
    if (!gLogged[playerid] || gUIMode[playerid] != 0 || gUIInput[playerid] || gInvFitting[playerid]) return 1;
    if ((newkeys & KEY_SECONDARY_ATTACK) && !(oldkeys & KEY_SECONDARY_ATTACK) && gGroundAim[playerid]>0) return RW_GroundTake(playerid);
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

public OnPlayerPickUpPickup(playerid, pickupid)
{
    return RW_GaragePickup(playerid, pickupid);
}

public OnPlayerEditAttachedObject(playerid,response,index,modelid,boneid,Float:fOffsetX,Float:fOffsetY,Float:fOffsetZ,Float:fRotX,Float:fRotY,Float:fRotZ,Float:fScaleX,Float:fScaleY,Float:fScaleZ)
{
    if (index == 8) return RW_ArmourFitSave(playerid,response,index,modelid,boneid,fOffsetX,fOffsetY,fOffsetZ,fRotX,fRotY,fRotZ,fScaleX,fScaleY,fScaleZ);
    return RW_InvFitSave(playerid,response,index,modelid,boneid,fOffsetX,fOffsetY,fOffsetZ,fRotX,fRotY,fRotZ,fScaleX,fScaleY,fScaleZ);
}
public OnPlayerTakeDamage(playerid, issuerid, Float:amount, weaponid, bodypart)
{
    RW_ArmourDamage(playerid, amount);
    return 1;
}
