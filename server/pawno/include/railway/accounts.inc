/*  ============================================================================
 *  railway/accounts.inc
 *  Регистрация, вход, хранение аккаунтов (SQLite внутри samp-server).
 *  Пароли: SHA256(password + salt), соль генерируется на аккаунт.
 *  ========================================================================= */

#if defined _railway_accounts_included
    #endinput
#endif
#define _railway_accounts_included

#include <railway/core>
#include <railway/ui>

new Float:gCamAngle[MAX_PLAYERS];

/* ------------------------------------------------------------------ прототипы */
forward RW_Respond(playerid, bool:ok, const code[]);
forward RW_AuthSubmit(playerid, bool:registerMode, const password[], const confirm[]);
forward RW_DoRegister(playerid, const nick[], const password[]);
forward RW_DoLogin(playerid, const nick[], const password[], bool:silent);
forward RW_LoginSuccess(playerid, accid, skin, money, admin);
forward RW_SpawnPlayer(playerid);
forward RW_ShowLoginDialog(playerid);

/* ------------------------------------------------------------------ база */

/*  Все запросы идут через эти обёртки: db_query() возвращает дескриптор
 *  результата, который обязательно нужно освобождать, иначе они копятся. */
stock bool:RW_DB_Exec(query[])
{
    if (gDB == DB:0) return false;
    new DBResult:res = db_query(gDB, query);
    if (res == DBResult:0) {
        // Log only the statement type, never account hashes, salts or passwords.
        new operation[20], i;
        while (query[i] && query[i] != ' ' && i < sizeof operation-1) { operation[i] = query[i]; i++; }
        printf("[railway:db] SQL failed: %s. Check path, permissions, locks and schema; dbcheck.", operation);
        return false;
    }
    db_free_result(res);
    return true;
}
stock RW_DB_LogPath()
{
    new DBResult:res = db_query(gDB, "PRAGMA database_list");
    if (res != DBResult:0) {
        if (db_num_rows(res) > 0) {
            new path[260]; db_get_field_assoc(res, "file", path, sizeof path);
            printf("[railway:db] SQLite file: %s", path);
        }
        db_free_result(res);
    }
    return 1;
}

stock bool:RW_DB_TableExists(const name[])
{
    new query[160];
    new DBResult:res;
    new bool:found = false;

    format(query, sizeof query,
        "SELECT `name` FROM `sqlite_master` WHERE `type`='table' AND `name`='%s'", name);

    res = db_query(gDB, query);
    if (res != DBResult:0) {
        found = (db_num_rows(res) > 0);
        db_free_result(res);
    }
    return found;
}

/* Write/read check inside a savepoint: never delete a player's row.
 * Probe name is longer than MAX_PLAYER_NAME and cannot collide with a game nick.
 * Rollback removes all changes even when the check fails. */
stock bool:RW_DB_Probe()
{
    if (!RW_DB_Exec("SAVEPOINT railway_write_probe")) return false;
    new query[384], name[80], bool:ok = false;
    format(name, sizeof name, "__railway_write_probe__%d_%d", GetTickCount(), random(2147483647));
    format(query, sizeof query,
        "INSERT INTO `accounts` (`name`,`hash`,`salt`,`skin`,`money`,`admin`,`created`,`last_login`,`last_ip`) VALUES ('%s','probe','probe',0,0,0,0,0,'127.0.0.1')", name);
    if (RW_DB_Exec(query)) {
        format(query, sizeof query, "SELECT `id` FROM `accounts` WHERE `name`='%s' LIMIT 1", name);
        new DBResult:res = db_query(gDB, query);
        if (res != DBResult:0) { ok = db_num_rows(res) > 0; db_free_result(res); }
    }
    new bool:rolledBack = RW_DB_Exec("ROLLBACK TO railway_write_probe");
    new bool:released = RW_DB_Exec("RELEASE railway_write_probe");
    return ok && rolledBack && released;
}

stock RW_DB_CreateTable()
{
    RW_DB_Exec("CREATE TABLE IF NOT EXISTS `accounts` (`id` INTEGER PRIMARY KEY AUTOINCREMENT,`name` TEXT NOT NULL UNIQUE COLLATE NOCASE,`hash` TEXT NOT NULL,`salt` TEXT NOT NULL,`skin` INTEGER NOT NULL DEFAULT 0,`money` INTEGER NOT NULL DEFAULT 2500,`admin` INTEGER NOT NULL DEFAULT 0,`created` INTEGER NOT NULL DEFAULT 0,`last_login` INTEGER NOT NULL DEFAULT 0,`last_ip` TEXT NOT NULL DEFAULT '')");
    return 1;
}

stock RW_DB_Init()
{
    gDBWritable = false;
    // SA-MP resolves this name inside scriptfiles/; do not duplicate the prefix.
    gDB = db_open("railway.db");
    if (gDB == DB:0) {
        print("[railway:db] Cannot open scriptfiles/railway.db.");
        print("[railway:db] Create scriptfiles next to samp-server.exe; allow write access.");
        print("[railway:db] Start server from its own directory. Never delete an existing DB.");
        return 0;
    }
    RW_DB_LogPath();
    RW_DB_Exec("PRAGMA busy_timeout = 3000");
    RW_DB_CreateTable();
    if (!RW_DB_TableExists("accounts")) {
        print("[railway:db] accounts table missing; check directory write access and file permissions.");
        return 0;
    }
    gDBWritable = RW_DB_Probe();
    if (gDBWritable) {
        print("[railway:db] Database ready: read/write verified.");
        return 1;
    }
    print("[railway:db] Write check failed: permission, lock, disk space or incompatible schema.");
    print("[railway:db] Existing tables were NOT renamed or replaced. Back up the DB; run dbcheck.");
    return 0;
}

stock RW_DB_AccountCount()
{
    new DBResult:res = db_query(gDB, "SELECT COUNT(*) AS `c` FROM `accounts`");
    new count = 0;
    if (res != DBResult:0) {
        if (db_num_rows(res) > 0) {
            count = db_get_field_assoc_int(res, "c");
        }
        db_free_result(res);
    }
    return count;
}

stock bool:RW_DB_AccountExists(const name[])
{
    new query[160];
    new DBResult:res;
    new bool:found = false;

    format(query, sizeof query, "SELECT `id` FROM `accounts` WHERE `name` = '%s' LIMIT 1", name);
    res = db_query(gDB, query);

    if (res != DBResult:0) {
        found = (db_num_rows(res) > 0);
        db_free_result(res);
    }
    return found;
}

stock RW_DB_AdminLevelByName(const name[])
{
    new query[160];
    new DBResult:res;
    new level = 0;

    format(query, sizeof query, "SELECT `admin` FROM `accounts` WHERE `name` = '%s' LIMIT 1", name);
    res = db_query(gDB, query);

    if (res != DBResult:0) {
        if (db_num_rows(res) > 0) {
            level = db_get_field_assoc_int(res, "admin");
        }
        db_free_result(res);
    }
    return level;
}

/* ------------------------------------------------------------------ хеши */

stock RW_GenerateSalt(dest[], size)
{
    new seed[48];
    new hash[65];

    new pepper[24];
    format(seed, sizeof seed, "%d|%d|%d", GetTickCount(), random(2147483647), random(2147483647));
    format(pepper, sizeof pepper, "railway-salt-v1");
    SHA256_PassHash(seed, pepper, hash, sizeof hash);

    strmid(dest, hash, 0, (size - 1 < 32) ? (size - 1) : 32, size);
    return 1;
}

stock RW_HashPassword(const password[], const salt[], dest[], size)
{
    new pw[65];
    new sl[65];

    format(pw, sizeof pw, "%s", password);
    format(sl, sizeof sl, "%s", salt);

    SHA256_PassHash(pw, sl, dest, (size > 65) ? 65 : size);
    return 1;
}

/* ------------------------------------------------------------------ проверки */

stock bool:RW_NameOnline(const name[], exceptid)
{
    new nick[MAX_PLAYER_NAME];
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (i == exceptid || !IsPlayerConnected(i)) {
            continue;
        }
        RW_GetNick(i, nick);
        if (!strcmp(nick, name, true)) {
            return true;
        }
    }
    return false;
}

stock bool:RW_AccountOnline(accountid, exceptid)
{
    for (new i = 0; i < MAX_PLAYERS; i++) {
        if (i == exceptid || !IsPlayerConnected(i) || !gLogged[i]) {
            continue;
        }
        if (gAccountId[i] == accountid) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ вход/регистрация */

stock RW_Respond(playerid, bool:ok, const code[])
{
    gAuthBusy[playerid] = false;
    return RW_AuthDialogRespond(playerid, ok, code);
}

stock RW_AuthSubmit(playerid, bool:registerMode, const password[], const confirm[])
{
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);

    if (!IsPlayerConnected(playerid) || gLogged[playerid]) {
        return 0;
    }

    if (gDB == DB:0 || !RW_DB_TableExists("accounts") || (registerMode && !gDBWritable)) {
        RW_Respond(playerid, false, "err:db");
        return 0;
    }

    if (gAuthBusy[playerid]) {
        return 0;
    }

    if (GetTickCount() - gLastAuthTick[playerid] < 400) {
        RW_Respond(playerid, false, "err:cooldown");
        return 0;
    }
    gLastAuthTick[playerid] = GetTickCount();
    gAuthBusy[playerid] = true;

    new len = strlen(password);
    if (len < 6) {
        RW_Respond(playerid, false, "err:short");
        return 0;
    }
    if (len > 64) {
        RW_Respond(playerid, false, "err:long");
        return 0;
    }
    if (!RW_SafeSqlText(password)) {
        RW_Respond(playerid, false, "err:chars");
        return 0;
    }

    if (registerMode) {
        if (strcmp(password, confirm, false) != 0) {
            RW_Respond(playerid, false, "err:mismatch");
            return 0;
        }
        return RW_DoRegister(playerid, nick, password);
    }
    return RW_DoLogin(playerid, nick, password, false);
}

stock RW_DoRegister(playerid, const nick[], const password[])
{
    if (!RW_ValidNick(nick)) {
        RW_Respond(playerid, false, "err:nick");
        return 0;
    }
    if (RW_DB_AccountExists(nick)) {
        RW_Respond(playerid, false, "err:exists");
        return 0;
    }

    new salt[33];
    new hash[65];
    new query[512];
    new ip[16];

    RW_GenerateSalt(salt, sizeof salt);
    RW_HashPassword(password, salt, hash, sizeof hash);

    GetPlayerIp(playerid, ip, sizeof ip);
    if (!RW_SafeSqlText(ip)) {
        ip[0] = EOS;
    }

    format(query, sizeof query,
        "INSERT INTO `accounts` (`name`,`hash`,`salt`,`skin`,`money`,`admin`,`created`,`last_login`,`last_ip`) VALUES ('%s','%s','%s',%d,%d,0,%d,0,'%s')",
        nick, hash, salt, DEFAULT_SKIN, DEFAULT_MONEY, gettime(), ip);

    RW_DB_Exec(query);

    if (!RW_DB_AccountExists(nick)) {
        RW_Respond(playerid, false, "err:db");
        printf("[railway] Registration: account '%s' was not saved (handle %d).", nick, _:gDB);

        if (gDB == DB:0) {
            print("[railway] Database not open; check startup logs.");
        } else if (!RW_DB_TableExists("accounts")) {
            print("[railway] Missing accounts table.");
        } else {
            print("[railway] Table exists but INSERT did not save the account.");
        }
        return 0;
    }

    RW_Respond(playerid, true, "ok:register");
    RW_DoLogin(playerid, nick, password, true);

    /* Диалоговый режим: молчаливый вход мог не сработать — показываем вход. */
    if (!gLogged[playerid]) {
        RW_ShowAuthDialog(playerid, false, "Account created. Enter your password to log in.");
    }
    return 1;
}

stock RW_DoLogin(playerid, const nick[], const password[], bool:silent)
{
    new query[256];
    new DBResult:res;
    new salt[65];
    new stored[65];
    new hash[65];
    new accid;
    new skin;
    new money;
    new admin;

    format(query, sizeof query,
        "SELECT `id`,`hash`,`salt`,`skin`,`money`,`admin` FROM `accounts` WHERE `name` = '%s' LIMIT 1", nick);

    res = db_query(gDB, query);
    if (res == DBResult:0) {
        if (!silent) {
            RW_Respond(playerid, false, "err:db");
        }
        return 0;
    }

    if (db_num_rows(res) == 0) {
        db_free_result(res);
        if (!silent) {
            RW_Respond(playerid, false, "err:noaccount");
        }
        return 0;
    }

    accid = db_get_field_assoc_int(res, "id");
    db_get_field_assoc(res, "salt", salt, sizeof salt);
    db_get_field_assoc(res, "hash", stored, sizeof stored);
    skin = db_get_field_assoc_int(res, "skin");
    money = db_get_field_assoc_int(res, "money");
    admin = db_get_field_assoc_int(res, "admin");
    db_free_result(res);

    RW_HashPassword(password, salt, hash, sizeof hash);

    if (strcmp(hash, stored, false) != 0) {
        gLoginAttempts[playerid]++;

        if (gLoginAttempts[playerid] >= MAX_LOGIN_ATTEMPTS) {
            SendClientMessage(playerid, COLOR_ERROR, "Too many failed login attempts. Connection closed.");
            Kick(playerid);
            return 0;
        }

        if (!silent) {
            new left = MAX_LOGIN_ATTEMPTS - gLoginAttempts[playerid];
            new code[32];
            format(code, sizeof code, "err:badpass:%d", left);
            RW_Respond(playerid, false, code);
        }
        return 0;
    }

    if (RW_AccountOnline(accid, playerid)) {
        if (!silent) {
            RW_Respond(playerid, false, "err:inuse");
        }
        return 0;
    }

    RW_LoginSuccess(playerid, accid, skin, money, admin);
    return 1;
}

stock RW_LoginSuccess(playerid, accid, skin, money, admin)
{
    new nick[MAX_PLAYER_NAME];
    new string[256];
    new moneyFmt[32];

    RW_GetNick(playerid, nick);

    gLogged[playerid] = true;
    gAccountId[playerid] = accid;
    gSkinId[playerid] = skin;
    gMoney[playerid] = money;
    gAdminLevel[playerid] = admin;
    gLoginAttempts[playerid] = 0;
    gAuthBusy[playerid] = false;

    if (!RW_InvLoad(playerid)) RW_Error(playerid, "Inventory unavailable. Contact an administrator.");
    RW_PhoneLoad(playerid);
    RW_UI_CloseAuth(playerid);

    RW_SpawnPlayer(playerid);

    format(string, sizeof string, "Welcome, {F5B04C}%s{FFFFFF}! You are at %s.", nick, STATION_NAME);
    SendClientMessage(playerid, COLOR_WHITE, string);

    RW_FormatMoney(money, moneyFmt, sizeof moneyFmt);
    format(string, sizeof string, "Balance: {7BD88F}%s{FFFFFF} rub. /spcar {AAAAAA}<id>{FFFFFF} - temporary car /help - commands", moneyFmt);
    SendClientMessage(playerid, COLOR_GREY, string);

    if (admin > 0) {
        format(string, sizeof string, "Logged in as administrator (level %d). /ahelp - admin commands.", admin);
        RW_Accent(playerid, string);
    }

    return 1;
}

stock RW_SpawnPlayer(playerid)
{
    SetSpawnInfo(playerid, 0, gSkinId[playerid], STATION_X, STATION_Y, STATION_Z, STATION_ANGLE, 0, 0, 0, 0, 0, 0);
    TogglePlayerSpectating(playerid, false);
    SpawnPlayer(playerid);
    return 1;
}

stock RW_SaveAccount(playerid)
{
    new query[256];
    new ip[16];

    if (!gLogged[playerid] || gAccountId[playerid] < 0) {
        return 0;
    }

    gMoney[playerid] = GetPlayerMoney(playerid);

    GetPlayerIp(playerid, ip, sizeof ip);
    if (!RW_SafeSqlText(ip)) {
        ip[0] = EOS;
    }

    format(query, sizeof query,
        "UPDATE `accounts` SET `skin`=%d,`money`=%d,`last_login`=%d,`last_ip`='%s' WHERE `id`=%d",
        gSkinId[playerid], gMoney[playerid], gettime(), ip, gAccountId[playerid]);

    RW_DB_Exec(query);
    return 1;
}

/* ------------------------------------------------------------------ камера на экране входа */

forward RW_AuthCamera(playerid);
public RW_AuthCamera(playerid)
{
    if (!IsPlayerConnected(playerid) || gLogged[playerid]) {
        if (gAuthTimer[playerid] != 0) {
            KillTimer(gAuthTimer[playerid]);
            gAuthTimer[playerid] = 0;
        }
        return 0;
    }

    gCamAngle[playerid] += CAMERA_ORBIT_SPEED + random(30) / 100.0;
    if (gCamAngle[playerid] > 360.0) {
        gCamAngle[playerid] -= 360.0;
    }

    new Float:angle = gCamAngle[playerid];
    new Float:x = STATION_X + CAMERA_ORBIT_RADIUS * floatsin(-angle, degrees);
    new Float:y = STATION_Y + CAMERA_ORBIT_RADIUS * floatcos(-angle, degrees);
    new Float:z = STATION_Z + CAMERA_ORBIT_HEIGHT;

    SetPlayerCameraPos(playerid, x, y, z);
    SetPlayerCameraLookAt(playerid, STATION_X, STATION_Y, STATION_Z + 1.1, CAMERA_CUT);
    return 1;
}

stock RW_StartAuthCamera(playerid)
{
    gCamAngle[playerid] = random(360);

    if (gAuthTimer[playerid] != 0) {
        KillTimer(gAuthTimer[playerid]);
    }
    gAuthTimer[playerid] = SetTimerEx("RW_AuthCamera", 60, true, "i", playerid);

    RW_AuthCamera(playerid);

    /* Первый кадр — интерполяция к стартовой точке, чтобы не было рывка */
    new Float:a = gCamAngle[playerid];
    new Float:x = STATION_X + (CAMERA_ORBIT_RADIUS + 6.0) * floatsin(-a, degrees);
    new Float:y = STATION_Y + (CAMERA_ORBIT_RADIUS + 6.0) * floatcos(-a, degrees);
    InterpolateCameraPos(playerid,
        x, y, STATION_Z + CAMERA_ORBIT_HEIGHT + 4.0,
        STATION_X + CAMERA_ORBIT_RADIUS * floatsin(-a, degrees),
        STATION_Y + CAMERA_ORBIT_RADIUS * floatcos(-a, degrees),
        STATION_Z + CAMERA_ORBIT_HEIGHT,
        2500, CAMERA_CUT);
    return 1;
}

/* ------------------------------------------------------------------ резервные диалоги */

/*  Текст для кодов, которыми обмениваются Pawn и диалог. */
stock RW_AuthCodeText(const code[], dest[], size)
{
    if (!strcmp(code, "ok:register", true)) {
        format(dest, size, "Account created.");
    } else if (!strcmp(code, "ok:login", true)) {
        format(dest, size, "Logged in.");
    } else if (!strcmp(code, "err:short", true)) {
        format(dest, size, "Password too short (minimum 6 characters).");
    } else if (!strcmp(code, "err:long", true)) {
        format(dest, size, "Password too long (maximum 64 characters).");
    } else if (!strcmp(code, "err:chars", true)) {
        format(dest, size, "Password contains unsupported characters.");
    } else if (!strcmp(code, "err:mismatch", true)) {
        format(dest, size, "Passwords do not match.");
    } else if (!strcmp(code, "err:exists", true)) {
        format(dest, size, "This account name is already registered.");
    } else if (!strcmp(code, "err:noaccount", true)) {
        format(dest, size, "Account not found.");
    } else if (!strcmp(code, "err:inuse", true)) {
        format(dest, size, "This account is already online.");
    } else if (!strcmp(code, "err:nick", true)) {
        format(dest, size, "Invalid account name.");
    } else if (!strcmp(code, "err:db", true)) {
        format(dest, size, "Database error. Try again later.");
    } else if (!strcmp(code, "err:badpass", true, 11)) {
        format(dest, size, "Wrong password. Check your keyboard layout and Caps Lock.");
    } else if (!strcmp(code, "err:cooldown", true)) {
        format(dest, size, "Too fast. Wait a second.");
    } else if (!strcmp(code, "err:attempts", true)) {
        format(dest, size, "Too many login attempts.");
    } else {
        format(dest, size, "%s", code);
    }
    return 1;
}

/*  Показывает диалог входа или регистрации. note — строка-пояснение сверху
 *  (например, текст ошибки или «Аккаунт создан, войдите»). */
stock RW_ShowAuthDialog(playerid, bool:registerMode, const note[])
{
    new nick[MAX_PLAYER_NAME];
    new info[320];
    new head[160];

    RW_GetNick(playerid, nick);

    if (note[0] == EOS) {
        head[0] = EOS;
    } else {
        format(head, sizeof head, "{FF8080}%s{FFFFFF}\n\n", note);
    }

    RW_ClearRegisterPassword(playerid);
    RW_UI_PasswordBegin(playerid);
    registerMode = !RW_DB_AccountExists(nick);
    if (registerMode) {
        format(info, sizeof info,
            "%s{D8C39B}RAILWAY / CREATE ACCOUNT{FFFFFF}\n\nName: {D8C39B}%s{FFFFFF}\nChoose a password of 6 to 64 characters.\nRepeat it on the next step.\n\n{939BA6}Do not reuse a password from another service.",
            head, nick);
        ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD, "Railway | Register 1/2", info, "Next", "Quit");
    } else {
        format(info, sizeof info,
            "%s{D8C39B}RAILWAY / WELCOME BACK{FFFFFF}\n\nName: {D8C39B}%s{FFFFFF}\nEnter your password to continue.\n\n{939BA6}Input is hidden. Check your keyboard layout and Caps Lock.", head, nick);
        ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Railway | Login", info, "Login", "Quit");
    }
    return 1;
}

stock RW_ShowLoginDialog(playerid)
{
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);
    return RW_ShowAuthDialog(playerid, !RW_DB_AccountExists(nick), "");
}

/*  Ответ на попытку входа/регистрации в диалоговом режиме. */
stock RW_AuthDialogRespond(playerid, bool:ok, const code[])
{
    new text[192];

    RW_AuthCodeText(code, text, sizeof text);

    if (ok) {
        if (!strcmp(code, "ok:register", true)) {
            SendClientMessage(playerid, COLOR_WHITE, text);
        }
        return 1;
    }

    SendClientMessage(playerid, COLOR_ERROR, text);

    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);
    RW_ShowAuthDialog(playerid, !RW_DB_AccountExists(nick), text);
    return 1;
}

stock RW_ClearRegisterPassword(playerid)
{
    for (new i; i < 65; i++) gRegPassword[playerid][i] = 0;
    gRegPending[playerid] = false;
}
stock RW_ShowRegisterConfirm(playerid)
{
    RW_UI_PasswordBegin(playerid);
    ShowPlayerDialog(playerid, DIALOG_REGISTER_CONFIRM, DIALOG_STYLE_PASSWORD, "Railway | Register 2/2",
        "{D8C39B}CONFIRM PASSWORD{FFFFFF}\n\nRepeat your new password.\nYour account will be saved after confirmation.\n\n{939BA6}Your password is hidden and never logged.", "Create", "Back");
    return 1;
}
