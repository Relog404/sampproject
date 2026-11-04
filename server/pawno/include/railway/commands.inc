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
    RW_Accent(playerid, "Ч Ч Ч {F5B04C}∆елезнодорожный вокзал{FFFFFF} Ч Ч Ч");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/spcar <id>{FFFFFF} Ч создать машину (400Ц611). ¬ладелец Ч вы.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/door{FFFFFF} Ч открыть/закрыть двери своей машины.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/key{FFFFFF} Ч вставить/вынуть ключ; E Ч зажигание.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/mycar{FFFFFF} Ч состо€ние своей машины.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/stats{FFFFFF} Ч ваш аккаунт   Х   {F5B04C}/admins{FFFFFF} Ч администраци€");
    SendClientMessage(playerid, COLOR_DARK_GREY, "¬ машину сад€тс€ только с открытой дверью. — оставленным ключом завести может любой водитель.");
    return 1;
}

stock RW_CommandAdminHelp(playerid)
{
    if (gAdminLevel[playerid] < 1) {
        RW_Error(playerid, "” вас нет прав администратора.");
        return 0;
    }

    RW_Accent(playerid, "Ч Ч Ч {F5B04C}јдмин-команды{FFFFFF} Ч Ч Ч");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/kick <id> [причина]{FFFFFF} Ч kick игрока.");
    if (gAdminLevel[playerid] >= 2) {
        SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/setadmin <ник> <0-3>{FFFFFF} Ч выдать уровень администратора.");
    }
    SendClientMessage(playerid, COLOR_DARK_GREY, "ћашины, созданные админом через /spcar, принадлежат админу.");
    return 1;
}

stock RW_CommandStats(playerid)
{
    new string[192];
    new nick[MAX_PLAYER_NAME];

    RW_GetNick(playerid, nick);

    format(string, sizeof string, "јккаунт {F5B04C}%s{FFFFFF}   Х   ID %d   Х   јдмин: %d",
        nick, gAccountId[playerid], gAdminLevel[playerid]);
    SendClientMessage(playerid, COLOR_WHITE, string);

    format(string, sizeof string, "ƒеньги: {7BD88F}%d{FFFFFF}   Х   —кин: %d   Х   ћашин в системе: %d",
        GetPlayerMoney(playerid), gSkinId[playerid], gVehSlotCount);
    SendClientMessage(playerid, COLOR_GREY, string);
    return 1;
}

stock RW_CommandAdmins(playerid)
{
    new string[128];
    new count = 0;

    RW_Accent(playerid, "јдминистраци€ онлайн:");
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || gAdminLevel[i] < 1) {
            continue;
        }
        new nick[MAX_PLAYER_NAME];
        RW_GetNick(i, nick);
        format(string, sizeof string, "  {F5B04C}%s{FFFFFF} Ч уровень %d", nick, gAdminLevel[i]);
        SendClientMessage(playerid, COLOR_GREY, string);
        count++;
    }

    if (count == 0) {
        RW_Info(playerid, "—ейчас администраторов нет в сети.");
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
    RW_DB_Exec(query);

    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i]) {
            continue;
        }
        new other[MAX_PLAYER_NAME];
        RW_GetNick(i, other);
        if (!strcmp(other, nick, true)) {
            gAdminLevel[i] = level;
            new string[128];
            format(string, sizeof string, "¬аш уровень администратора изменЄн на %d.", level);
            RW_Accent(i, string);
        }
    }
    return 1;
}

stock RW_CommandKick(playerid, const params[])
{
    if (gAdminLevel[playerid] < 1) {
        RW_Error(playerid, "” вас нет прав администратора.");
        return 1;
    }

    if (params[0] == EOS) {
        RW_Info(playerid, "»спользование: /kick <id> [причина]");
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
        RW_Error(playerid, "ID должен быть числом.");
        return 1;
    }

    new target = strval(targetText);
    if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) {
        RW_Error(playerid, "»грок не найден.");
        return 1;
    }

    new string[192];
    new adminNick[MAX_PLAYER_NAME];
    new targetNick[MAX_PLAYER_NAME];

    RW_GetNick(playerid, adminNick);
    RW_GetNick(target, targetNick);

    if (reason[0] == EOS) {
        format(reason, sizeof reason, "–ешение администрации");
    }

    format(string, sizeof string, "{FF6B6B}%s{FFFFFF} кикнут администратором {F5B04C}%s{FFFFFF}. ѕричина: %s",
        targetNick, adminNick, reason);
    SendClientMessageToAll(COLOR_WHITE, string);
    Kick(target);
    return 1;
}

stock RW_CommandProcess(playerid, cmdtext[])
{
    new cmd[32];
    new params[144];
    new string[192];

    RW_ParseCommand(cmdtext, cmd, sizeof cmd, params, sizeof params);

    if (!strcmp(cmd, "help", true) || !strcmp(cmd, "commands", true)) {
        return RW_CommandHelp(playerid);
    }

    if (!gLogged[playerid]) {
        RW_Error(playerid, "—начала войдите в аккаунт.");
        return 1;
    }

    if (!strcmp(cmd, "spcar", true) || !strcmp(cmd, "veh", true) || !strcmp(cmd, "car", true)) {
        return RW_Veh_CommandSpcar(playerid, params);
    }
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
    if (!strcmp(cmd, "setadmin", true)) {
        if (gAdminLevel[playerid] < 2) {
            RW_Error(playerid, "Ќужен уровень администратора 2.");
            return 1;
        }

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
            RW_Info(playerid, "»спользование: /setadmin <ник> <0-3>");
            return 1;
        }

        new level = strval(levelText);
        if (level < 0) level = 0;
        if (level > 3) level = 3;

        RW_AdminSetLevel(nick, level);

        format(string, sizeof string, "”ровень %d выдан аккаунту {F5B04C}%s{FFFFFF}.", level, nick);
        RW_Ok(playerid, string);
        return 1;
    }

    RW_Error(playerid, "Ќеизвестна€ команда. {FFFFFF}/help Ч список команд.");
    return 1;
}
