/*  ============================================================================
 *  railway/commands.inc
 *  “екстовые команды.
 *  ========================================================================= */

#if defined _railway_commands_included
    #endinput
#endif
#define _railway_commands_included

#include <railway/core>
#include <railway/ui>
#include <railway/accounts>
#include <railway/vehicles>

stock RW_ParseCommand(const cmdtext[], cmd[], cmdSize, params[], paramsSize)
{
    new i = 0;
    new j = 0;

    if (cmdtext[0] == '/' || cmdtext[0] == '!') {
        i = 1;
    }

    while (cmdtext[i] != EOS && cmdtext[i] != ' ' && j < cmdSize - 1) {
        cmd[j++] = cmdtext[i++];
    }
    cmd[j] = EOS;

    while (cmdtext[i] == ' ') {
        i++;
    }

    j = 0;
    while (cmdtext[i] != EOS && cmdtext[i] != '\r' && cmdtext[i] != '\n' && j < paramsSize - 1) {
        params[j++] = cmdtext[i++];
    }
    params[j] = EOS;

    /* убираем хвостовые пробелы: "/spcar 411 " == "/spcar 411" */
    while (j > 0 && (params[j - 1] == ' ' || params[j - 1] == '\t')) {
        params[--j] = EOS;
    }
    return 1;
}

stock RW_CommandHelp(playerid)
{
    RW_Accent(playerid, "--- {F5B04C}Unity Station{FFFFFF} ---");
    SendClientMessage(playerid, COLOR_GREY, "/inv - inventory. Click an item for its description and actions.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}Y{FFFFFF} - car menu /vmenu; {F5B04C}N{FFFFFF} - engine /engine; {F5B04C}H{FFFFFF} - doors. /settings");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/spcar <id>{FFFFFF} - temporary car (400-611). /garage - station garage.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/door{FFFFFF} - unlock/lock the aimed car; its key must be in /inv.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/key{FFFFFF} - insert/remove key; N - engine.");
    SendClientMessage(playerid, COLOR_GREY, "Map: ESC > Map > right click. /tpm - confirm teleport; /delmark - remove marker.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/mycar{FFFFFF} - temporary car status.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/stats{FFFFFF} - account /admins - online administrators");
    SendClientMessage(playerid, COLOR_DARK_GREY, "Doors must be open to enter. With a key left in the ignition, any driver can start the engine.");
    return 1;
}

stock RW_CommandAdminHelp(playerid)
{
    if (gAdminLevel[playerid] < 1) {
        RW_Error(playerid, "You are not an administrator.");
        return 0;
    }

    RW_Accent(playerid, "--- {F5B04C}Admin commands{FFFFFF} ---");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/kick <id> [reason]{FFFFFF} - kick a player.");
    SendClientMessage(playerid, COLOR_GREY, "/giveitem <id> <1=backpack,2=key> [vehicleid]");
    SendClientMessage(playerid, COLOR_DARK_GREY, "/givecar <id|name> <model> /flycar /speed <km/h> /usk <km/h/s>");
    return 1;
}

stock RW_CommandStats(playerid)
{
    new string[192];
    new nick[MAX_PLAYER_NAME];

    RW_GetNick(playerid, nick);

    format(string, sizeof string, "Account {F5B04C}%s{FFFFFF} / ID %d / Admin %d",
        nick, gAccountId[playerid], gAdminLevel[playerid]);
    SendClientMessage(playerid, COLOR_WHITE, string);

    format(string, sizeof string, "Money: {7BD88F}%d{FFFFFF} / Skin: %d / Active cars: %d",
        GetPlayerMoney(playerid), gSkinId[playerid], gVehSlotCount);
    SendClientMessage(playerid, COLOR_GREY, string);
    return 1;
}

stock RW_CommandAdmins(playerid)
{
    new string[128];
    new count = 0;

    RW_Accent(playerid, "Online administrators:");
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || gAdminLevel[i] < 1) {
            continue;
        }
        new nick[MAX_PLAYER_NAME];
        RW_GetNick(i, nick);
        format(string, sizeof string, "  {F5B04C}%s{FFFFFF} - level %d", nick, gAdminLevel[i]);
        SendClientMessage(playerid, COLOR_GREY, string);
        count++;
    }

    if (count == 0) {
        RW_Info(playerid, "No administrators online.");
    }
    return 1;
}

stock RW_AdminSetLevel(const nick[], level)
{
    new query[256];

    if (!RW_ValidNick(nick) || !RW_SafeSqlText(nick)) {
        return 0;
    }

    format(query, sizeof query, "UPDATE `accounts` SET `admin`=%d WHERE `name`='%s'", level, nick);
    if (!RW_DB_AccountExists(nick) || !RW_DB_Exec(query)) return 0;

    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i]) {
            continue;
        }
        new other[MAX_PLAYER_NAME];
        RW_GetNick(i, other);
        if (!strcmp(other, nick, true)) {
            gAdminLevel[i] = level;
            new string[128];
            format(string, sizeof string, "Your admin level has been changed to %d.", level);
            RW_Accent(i, string);
        }
    }
    return 1;
}

stock RW_CommandKick(playerid, const params[])
{
    if (gAdminLevel[playerid] < 1) {
        RW_Error(playerid, "You are not an administrator.");
        return 1;
    }

    if (params[0] == EOS) {
        RW_Info(playerid, "Usage: /kick <id> [reason]");
        return 1;
    }

    new targetText[16];
    new reason[96];
    new i = 0;
    new j = 0;

    while (params[i] == ' ') {
        i++;
    }
    while (params[i] != EOS && params[i] != ' ' && j < sizeof targetText - 1) {
        targetText[j++] = params[i++];
    }
    targetText[j] = EOS;

    while (params[i] == ' ') {
        i++;
    }
    strmid(reason, params, i, strlen(params), sizeof reason);

    if (!RW_IsNumeric(targetText)) {
        RW_Error(playerid, "ID must be a number.");
        return 1;
    }

    new target = strval(targetText);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) {
        RW_Error(playerid, "Player not found.");
        return 1;
    }

    new string[192];
    new adminNick[MAX_PLAYER_NAME];
    new targetNick[MAX_PLAYER_NAME];

    RW_GetNick(playerid, adminNick);
    RW_GetNick(target, targetNick);

    if (reason[0] == EOS) {
        format(reason, sizeof reason, "Administrative decision");
    }

    format(string, sizeof string, "{FF6B6B}%s{FFFFFF} kicked by {F5B04C}%s{FFFFFF}. Reason: %s",
        targetNick, adminNick, reason);
    SendClientMessageToAll(COLOR_WHITE, string);
    Kick(target);
    return 1;
}

stock RW_CommandProcess(playerid, cmdtext[])
{
    new cmd[32];
    new params[144];
    RW_ParseCommand(cmdtext, cmd, sizeof cmd, params, sizeof params);

    if (!strcmp(cmd, "help", true) || !strcmp(cmd, "commands", true)) {
        return RW_CommandHelp(playerid);
    }

    if ((!strcmp(cmd, "tdlatin", true) || !strcmp(cmd, "uitd", true))) return RW_UI_Language(playerid, true);
    if ((!strcmp(cmd, "uinative", true) || !strcmp(cmd, "tdrus", true) || !strcmp(cmd, "uiru", true))) return RW_UI_Language(playerid, false);
    if (!gLogged[playerid]) {
        RW_Error(playerid, "Log in first.");
        return 1;
    }

    if (!strcmp(cmd, "inv", true) || !strcmp(cmd, "inventory", true)) return RW_InvOpen(playerid);
    if (!strcmp(cmd, "givecar", true)) return RW_GarageGive(playerid, params);
    if (!strcmp(cmd, "flycar", true) || !strcmp(cmd, "speed", true) || !strcmp(cmd, "usk", true)) return RW_MotionCommand(playerid, cmd, params);
    if (!strcmp(cmd, "garage", true)) return RW_GarageOpen(playerid);
    if (!strcmp(cmd, "giveitem", true)) return RW_InvGiveCommand(playerid, params);
    if (!strcmp(cmd, "spcar", true) || !strcmp(cmd, "veh", true) || !strcmp(cmd, "car", true)) {
        return RW_Veh_CommandSpcar(playerid, params);
    }
    if (!strcmp(cmd, "tpm", true)) return RW_MapPrompt(playerid);
    if (!strcmp(cmd, "delmark", true)) {
        gMapHasPoint[playerid] = false;
        gMapPending[playerid] = false;
        RemovePlayerMapIcon(playerid, RW_MAP_ICON);
        if (gUIMode[playerid] == UI_MAP) RW_UI_Close(playerid);
        return RW_ControlNotify(playerid, "Marker removed.");
    }
    if (!strcmp(cmd, "engine", true)) return RW_EngineCommand(playerid);
    if (!strcmp(cmd, "vmenu", true)) return RW_ControlOpen(playerid, false);
    if (!strcmp(cmd, "settings", true)) {
        return RW_ControlOpen(playerid, true);
    }
    if (!strcmp(cmd, "key", true)) {
        return RW_Veh_CommandKey(playerid);
    }
    if (!strcmp(cmd, "door", true) || !strcmp(cmd, "doors", true)) {
        return RW_Veh_CommandDoor(playerid);
    }
    if (!strcmp(cmd, "mycar", true)) {
        return RW_Veh_CommandMyCar(playerid);
    }
    if (!strcmp(cmd, "stats", true)) {
        return RW_CommandStats(playerid);
    }
    if (!strcmp(cmd, "admins", true)) {
        return RW_CommandAdmins(playerid);
    }
    if (!strcmp(cmd, "ahelp", true)) {
        return RW_CommandAdminHelp(playerid);
    }
    if (!strcmp(cmd, "kick", true)) {
        return RW_CommandKick(playerid, params);
    }
    if (!strcmp(cmd, "setadmin", true)) return RW_Error(playerid, "Admin assignment is console-only: setadmin <name> <0-3>.");

    RW_Error(playerid, "Unknown command. {FFFFFF}/help - commands.");
    return 1;
}
