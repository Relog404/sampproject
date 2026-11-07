/*  ============================================================================
 *  railway/commands.inc
 *  Текстовые команды.
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
    SendClientMessage(playerid, COLOR_GREY, "/inv - инвентарь. Нажмите предмет, чтобы увидеть описание и действия.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}Y{FFFFFF} - машина под прицелом, игрок рядом (позвать), иначе телефон /vmenu; {F5B04C}N{FFFFFF} - двигатель; {F5B04C}H{FFFFFF} - двери. /settings");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/phone{FFFFFF} - телефон; /call, /sms, /contacts, /addcontact, /911, /topup, /tariff, /sim.");
    SendClientMessage(playerid, COLOR_GREY, "Полиция: /duty, /mdt, /cuff, /escort, /wall, /wanted, /arrest, /release, /fine, /track; Y на игроке.");
    SendClientMessage(playerid, COLOR_GREY, "Адвокатура: /lawyer, /bailrequest, /endcase, /lawprice; Y на задержанном, чтобы взять дело.");
    SendClientMessage(playerid, COLOR_GREY, "Скорая: /revive, /heal, /carry, /load, /admit, /ems, /calls, /take <id>; Y на упавшем игроке.");
    SendClientMessage(playerid, COLOR_GREY, "Транспорт: /fuel, /refuel, /trunk, /tune, /repair, /tow, /impound, /unimpound; Y на машине.");
    SendClientMessage(playerid, COLOR_GREY, "Деньги: /shop, /bank, /balance, /deposit, /withdraw, /transfer <id> <сумма>, /job, /work, /quitjob, /payday.");
    SendClientMessage(playerid, COLOR_GREY, "Недвижимость: /property, /doors, /enter, /exit, /paytax, /till put|take <сумма>, /bid <сумма>, /rob (12:00-01:00).");
    SendClientMessage(playerid, COLOR_GREY, "Сюжет: /story - восемь глав с опытом и деньгами; /profile - уровень, опыт и вся статистика.");
    SendClientMessage(playerid, COLOR_GREY, "Интерфейс: /hud - экранный интерфейс, /bonus - бесплатный бонус, F2 или /donate - донат, /set <hud|hints|bonus|speedo|names|markers|welcome>, /key <0|1|2>, Alt + Y - курсор мыши, C - донат, Alt + H - настройки, F3 (Alt + N) - меню, / - телефон, /talk - непись.  Маркетплейс: /market, /sell <слот> <цена> [часы], /buy <лот>, /stavka <лот> <сумма>, /unsell <лот>, /claim Криминал: /crime, /heist, /craft, /green, /selldrugs, /wanted. Семья: /family, /fcreate, /finvite, /fjoin, /fleave, /fkick, /frank, /fgive, /fstore, /ftake, /fcap, /fzones, /fcash..");
    SendClientMessage(playerid, COLOR_GREY, "Приложения: /apps, /weather, /group, /g <текст>, /ginvite, /taxi, /uber, /news, /ad, /auto, /deliver, /snap, /photo, /lawcall, /radio.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/spcar <id>{FFFFFF} - временная машина (400-611). /garage - гараж вокзала.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/door{FFFFFF} - открыть/закрыть машину под прицелом; ключ должен быть в /inv.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/key{FFFFFF} - вставить/вынуть ключ; N - двигатель.");
    SendClientMessage(playerid, COLOR_GREY, "Карта: ESC > Карта > правая кнопка. /tpm - подтвердить телепорт; /delmark - убрать метку.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/armfit{FFFFFF} - администратор в бронежилете: подогнать жилет под скин.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/skin [id txd texture]{FFFFFF} - текстуры из папки arm/ клиента.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/board{FFFFFF} - сесть в самолёт под прицелом (до 20 человек); {F5B04C}/unboard{FFFFFF} - выйти.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/mycar{FFFFFF} - состояние временной машины.");
    SendClientMessage(playerid, COLOR_GREY, "{F5B04C}/stats{FFFFFF} - аккаунт; /admins - администраторы в сети");
    SendClientMessage(playerid, COLOR_DARK_GREY, "Чтобы сесть, двери должны быть открыты. С ключом в замке зажигания мотор заведёт любой водитель.");
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
    /* Один слеш в чате — телефон: SA-MP не отдаёт серверу клавиши P и [. */
    if (!strcmp(cmdtext, "/", true)) return RW_PhoneOpen(playerid);
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
    if (!strcmp(cmd, "duty", true)) return RW_PoliceDutyCommand(playerid);
    if (!strcmp(cmd, "faction", true)) return RW_PoliceFactionCommand(playerid, params);
    if (!strcmp(cmd, "factions", true)) return RW_PoliceFactionsCommand(playerid);
    if (!strcmp(cmd, "mdt", true)) return RW_MdtOpen(playerid);
    if (!strcmp(cmd, "cuff", true)) return RW_PoliceCuffCommand(playerid, params);
    if (!strcmp(cmd, "uncuff", true)) return RW_PoliceUncuffCommand(playerid, params);
    if (!strcmp(cmd, "escort", true)) return RW_PoliceEscortCommand(playerid, params);
    if (!strcmp(cmd, "wall", true)) return RW_PoliceWallCommand(playerid, params);
    if (!strcmp(cmd, "wanted", true)) return RW_PoliceWantedCommand(playerid, params);
    if (!strcmp(cmd, "arrest", true)) return RW_PoliceArrestCommand(playerid, params);
    if (!strcmp(cmd, "release", true)) return RW_PoliceReleaseCommand(playerid, params);
    if (!strcmp(cmd, "bail", true)) return RW_PoliceBail(playerid);
    if (!strcmp(cmd, "fine", true)) return RW_PoliceFineCommand(playerid, params);
    if (!strcmp(cmd, "fines", true)) return RW_PolicePayFines(playerid);
    if (!strcmp(cmd, "track", true)) return RW_PoliceTrackCommand(playerid, params);
    if (!strcmp(cmd, "untrack", true)) return RW_PoliceUntrack(playerid);
    if (!strcmp(cmd, "lawyer", true)) return RW_LawyerCommand(playerid);
    if (!strcmp(cmd, "setlawyer", true)) return RW_LawyerLicenseCommand(playerid, params);
    if (!strcmp(cmd, "lawprice", true)) return RW_LawyerPriceCommand(playerid, params);
    if (!strcmp(cmd, "bailrequest", true)) return RW_PoliceBailRequest(playerid);
    if (!strcmp(cmd, "endcase", true)) return RW_PoliceCaseEnd(playerid);
    if (!strcmp(cmd, "heal", true)) return RW_EmsHealCommand(playerid, params);
    if (!strcmp(cmd, "revive", true)) return RW_EmsReviveCommand(playerid, params);
    if (!strcmp(cmd, "carry", true)) return RW_EmsCarryCommand(playerid, params);
    if (!strcmp(cmd, "drop", true)) return RW_EmsDrop(playerid);
    if (!strcmp(cmd, "load", true)) return RW_EmsLoadIn(playerid);
    if (!strcmp(cmd, "admit", true)) return RW_EmsAdmit(playerid);
    if (!strcmp(cmd, "ems", true)) return RW_EmsListCommand(playerid);
    if (!strcmp(cmd, "calls", true)) return RW_EmsCallsCommand(playerid);
    if (!strcmp(cmd, "take", true)) return RW_EmsTakeCommand(playerid, params);
    if (!strcmp(cmd, "fuel", true)) return RW_FuelCommand(playerid);
    if (!strcmp(cmd, "refuel", true)) return RW_RefuelCommand(playerid);
    if (!strcmp(cmd, "trunk", true)) return RW_TrunkCommand(playerid);
    if (!strcmp(cmd, "repair", true)) return RW_RepairCommand(playerid);
    if (!strcmp(cmd, "tow", true)) return RW_TowCommand(playerid);
    if (!strcmp(cmd, "impound", true)) return RW_ImpoundList(playerid);
    if (!strcmp(cmd, "unimpound", true)) return RW_UnimpoundCommand(playerid, params);
    if (!strcmp(cmd, "tune", true)) return RW_TuneCommand(playerid);
    if (!strcmp(cmd, "shop", true)) return RW_ShopCommand(playerid);
    if (!strcmp(cmd, "bank", true)) return RW_BankCommand(playerid);
    if (!strcmp(cmd, "balance", true)) return RW_BalanceCommand(playerid);
    if (!strcmp(cmd, "deposit", true)) return RW_DepositCommand(playerid, params);
    if (!strcmp(cmd, "withdraw", true)) return RW_WithdrawCommand(playerid, params);
    if (!strcmp(cmd, "transfer", true)) return RW_TransferCommand(playerid, params);
    if (!strcmp(cmd, "job", true)) return RW_JobCommand(playerid);
    if (!strcmp(cmd, "work", true)) return RW_WorkCommand(playerid);
    if (!strcmp(cmd, "quitjob", true)) return RW_QuitJobCommand(playerid);
    if (!strcmp(cmd, "payday", true)) return RW_PaydayCommand(playerid);
    if (!strcmp(cmd, "apps", true)) return RW_PhoneAppList(playerid);
    if (!strcmp(cmd, "weather", true)) return RW_WeatherCommand(playerid);
    if (!strcmp(cmd, "group", true)) return RW_GroupInfo(playerid);
    if (!strcmp(cmd, "ginvite", true)) return RW_GroupInviteCommand(playerid, params);
    if (!strcmp(cmd, "gaccept", true)) return RW_GroupAccept(playerid);
    if (!strcmp(cmd, "gleave", true)) return RW_GroupLeave(playerid);
    if (!strcmp(cmd, "g", true)) return RW_GroupSayCommand(playerid, params);
    if (!strcmp(cmd, "taxi", true)) return RW_UberOrder(playerid);
    if (!strcmp(cmd, "uber", true)) return RW_PhoneAppOpen(playerid, PHONE_APP_UBER);
    if (!strcmp(cmd, "news", true)) return RW_NewsList(playerid);
    if (!strcmp(cmd, "ad", true)) return RW_NewsPostCommand(playerid, params);
    if (!strcmp(cmd, "auto", true)) return RW_PhoneAppOpen(playerid, PHONE_APP_AUTO);
    if (!strcmp(cmd, "deliver", true)) return RW_AutoDeliverCommand(playerid, params);
    if (!strcmp(cmd, "snap", true)) return RW_PhotoTake(playerid);
    if (!strcmp(cmd, "photo", true)) return RW_PhotoCommand(playerid);
    if (!strcmp(cmd, "radio", true)) return RW_RadioCommand(playerid);
    if (!strcmp(cmd, "property", true) || !strcmp(cmd, "realty", true)) return RW_PropertyCommand(playerid);
    if (!strcmp(cmd, "enter", true)) return RW_EnterCommand(playerid);
    if (!strcmp(cmd, "exit", true)) return RW_ExitCommand(playerid);
    if (!strcmp(cmd, "doors", true)) return RW_DoorsCommand(playerid);
    if (!strcmp(cmd, "paytax", true)) return RW_PayTaxCommand(playerid);
    if (!strcmp(cmd, "till", true)) return RW_PropTillCommand(playerid, params);
    if (!strcmp(cmd, "bid", true)) return RW_PropBidCommand(playerid, params);
    if (!strcmp(cmd, "rob", true)) return RW_PropRobCommand(playerid);
    if (!strcmp(cmd, "hud", true)) return RW_HudCommand(playerid);
    if (!strcmp(cmd, "bonus", true)) return RW_BonusCommand(playerid);
    if (!strcmp(cmd, "mouse", true)) return RW_HudMouseCommand(playerid);
    if (!strcmp(cmd, "menu", true)) return RW_MenuCommand(playerid);
    if (!strcmp(cmd, "settings", true)) return RW_SettingsCommand(playerid);
    if (!strcmp(cmd, "set", true)) return RW_SetCommand(playerid, params);
    if (!strcmp(cmd, "key", true)) return RW_KeyCommand(playerid, params);
    if (!strcmp(cmd, "market", true) || !strcmp(cmd, "mp", true)) return RW_MarketCommand(playerid, params);
    if (!strcmp(cmd, "sell", true)) return RW_SellCommand(playerid, params);
    if (!strcmp(cmd, "buy", true)) return RW_BuyCommand(playerid, params);
    if (!strcmp(cmd, "stavka", true)) return RW_BidCommand(playerid, params);
    if (!strcmp(cmd, "unsell", true)) return RW_CancelCommand(playerid, params);
    if (!strcmp(cmd, "claim", true)) return RW_TakeCommand(playerid);
    if (!strcmp(cmd, "crime", true)) return RW_CrimePanel(playerid);
    if (!strcmp(cmd, "heist", true)) return RW_CrimeHeist(playerid);
    if (!strcmp(cmd, "craft", true)) return RW_CraftCommand(playerid);
    if (!strcmp(cmd, "green", true)) return RW_GreenCommand(playerid);
    if (!strcmp(cmd, "selldrugs", true)) return RW_SellDrugsCommand(playerid);
    if (!strcmp(cmd, "wanted", true)) return RW_WantedCommand(playerid, params);
    if (!strcmp(cmd, "family", true)) return RW_FamilyPanel(playerid);
    if (!strcmp(cmd, "fcreate", true)) return RW_FamilyCreate(playerid, params);
    if (!strcmp(cmd, "finvite", true)) return RW_FamilyInvite(playerid, params);
    if (!strcmp(cmd, "fjoin", true)) return RW_FamilyJoin(playerid);
    if (!strcmp(cmd, "fleave", true)) return RW_FamilyLeave(playerid);
    if (!strcmp(cmd, "fkick", true)) return RW_FamilyKick(playerid, params);
    if (!strcmp(cmd, "frank", true)) return RW_FamilyRank(playerid, params);
    if (!strcmp(cmd, "fgive", true)) return RW_FamilyGiveCar(playerid);
    if (!strcmp(cmd, "fstore", true)) return RW_FamilyStore(playerid, params);
    if (!strcmp(cmd, "ftake", true)) return RW_FamilyTakeItem(playerid, params);
    if (!strcmp(cmd, "fcap", true)) return RW_FamilyCapture(playerid);
    if (!strcmp(cmd, "fzones", true)) return RW_FamilyZonesCommand(playerid);
    if (!strcmp(cmd, "fcash", true)) return RW_FamilyCashCommand(playerid, params);
    if (!strcmp(cmd, "donate", true)) return RW_DonateCommand(playerid);
    if (!strcmp(cmd, "points", true)) return RW_PointsCommand(playerid);
    if (!strcmp(cmd, "givedonate", true)) return RW_GiveDonateCommand(playerid, params);
    if (!strcmp(cmd, "talk", true)) return RW_CityTalk(playerid);
    if (!strcmp(cmd, "story", true)) return RW_StoryCommand(playerid);
    if (!strcmp(cmd, "quest", true)) return RW_StoryCommand(playerid);
    if (!strcmp(cmd, "profile", true)) return RW_ProfileCommand(playerid);
    if (!strcmp(cmd, "lawcall", true)) return RW_AppLawyerCall(playerid);
    if (!strcmp(cmd, "payfines", true)) return RW_AppPayFines(playerid);
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
