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
    new DBResult:res = db_query(gDB, query);
    if (res != DBResult:0) {
        db_free_result(res);
    }
    return true;
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

/*  Самопроверка схемы: пишем и сразу читаем служебную строку тем же набором
 *  колонок, что и настоящая регистрация. Так видно, что база реально пишется. */
stock bool:RW_DB_Probe()
{
    new DBResult:res;
    new bool:ok = false;

    RW_DB_Exec("DELETE FROM `accounts` WHERE `name` = '__probe__'");

    res = db_query(gDB,
        "INSERT INTO `accounts` (`name`,`hash`,`salt`,`skin`,`money`,`admin`,`created`,`last_login`,`last_ip`) VALUES ('__probe__','probe','probe',0,0,0,0,0,'127.0.0.1')");
    if (res != DBResult:0) {
        db_free_result(res);
    }

    res = db_query(gDB, "SELECT `id` FROM `accounts` WHERE `name` = '__probe__' LIMIT 1");
    if (res != DBResult:0) {
        ok = (db_num_rows(res) > 0);
        db_free_result(res);
    }

    RW_DB_Exec("DELETE FROM `accounts` WHERE `name` = '__probe__'");
    return ok;
}

stock RW_DB_CreateTable()
{
    RW_DB_Exec("CREATE TABLE IF NOT EXISTS `accounts` (`id` INTEGER PRIMARY KEY AUTOINCREMENT,`name` TEXT NOT NULL UNIQUE COLLATE NOCASE,`hash` TEXT NOT NULL,`salt` TEXT NOT NULL,`skin` INTEGER NOT NULL DEFAULT 0,`money` INTEGER NOT NULL DEFAULT 2500,`admin` INTEGER NOT NULL DEFAULT 0,`created` INTEGER NOT NULL DEFAULT 0,`last_login` INTEGER NOT NULL DEFAULT 0,`last_ip` TEXT NOT NULL DEFAULT '')");
    return 1;
}

stock RW_DB_Init()
{
    /*  db_open() сам ищет файл ВНУТРИ scriptfiles/, поэтому путь с префиксом
     *  scriptfiles/ превращался в scriptfiles/scriptfiles/railway.db. */
    gDB = db_open("railway.db");

    if (gDB == DB:0) {
        /*  запасной вариант — если сервер разрешает путь от своего корня */
        gDB = db_open("scriptfiles/railway.db");
    }

    if (gDB == DB:0) {
        print("[railway] База: не удалось открыть scriptfiles/railway.db.");
        print("[railway] Проверьте, что папка scriptfiles есть и доступна на запись.");
        return 0;
    }

    printf("[railway] База: соединение открыто (handle %d).", _:gDB);

    RW_DB_Exec("PRAGMA journal_mode = WAL");
    RW_DB_CreateTable();

    if (!RW_DB_TableExists("accounts")) {
        print("[railway] База: таблица accounts не создалась (файл только для чтения?).");
        return 0;
    }

    if (RW_DB_Probe()) {
        print("[railway] База данных готова.");
        return 1;
    }

    /*  Таблица есть, но запись не проходит: обычно это старая схема, оставшаяся
     *  от прежней версии. Уводим её в сторону и создаём таблицу заново. */
    print("[railway] База: таблица accounts не подходит под текущую схему, пересоздаю.");
    RW_DB_Exec("ALTER TABLE `accounts` RENAME TO `accounts_backup_old`");
    RW_DB_CreateTable();

    if (RW_DB_Probe()) {
        print("[railway] База: пересоздана, старая таблица сохранена как accounts_backup_old.");
        print("[railway] База данных готова.");
        return 1;
    }

    print("[railway] База: запись в accounts не работает.");
    print("[railway] Удалите файл scriptfiles/railway.db и перезапустите сервер.");
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
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);

    gAuthBusy[playerid] = false;
    RW_UI_AuthBusy(playerid, false);
    RW_UI_AuthResult(playerid, ok, code, nick);

    /* Если CEF недоступен и игрок входит через диалог — отвечаем диалогом. */
    if (gAuthFallbackSent[playerid]) {
        RW_AuthDialogRespond(playerid, ok, code);
    }
    return 1;
}

stock RW_AuthSubmit(playerid, bool:registerMode, const password[], const confirm[])
{
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);

    if (!IsPlayerConnected(playerid) || gLogged[playerid]) {
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
        printf("[railway] Регистрация: запись '%s' не сохранилась (handle %d).", nick, _:gDB);

        if (gDB == DB:0) {
            print("[railway] Причина: база не открыта — смотрите сообщения при старте сервера.");
        } else if (!RW_DB_TableExists("accounts")) {
            print("[railway] Причина: нет таблицы accounts.");
        } else {
            print("[railway] Причина: таблица есть, но INSERT не сохраняет строку.");
        }
        return 0;
    }

    RW_Respond(playerid, true, "ok:register");
    RW_DoLogin(playerid, nick, password, true);

    /* Диалоговый режим: молчаливый вход мог не сработать — показываем вход. */
    if (!gLogged[playerid] && gAuthFallbackSent[playerid]) {
        RW_ShowAuthDialog(playerid, false, "Аккаунт создан. Введите пароль, чтобы войти.");
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
            SendClientMessage(playerid, COLOR_ERROR, "Слишком много неверных попыток входа. Соединение закрыто.");
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

    RW_UI_AuthBusy(playerid, false);
    RW_UI_CloseAuth(playerid);

    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
        gFallbackTimer[playerid] = 0;
    }

    RW_SpawnPlayer(playerid);

    format(string, sizeof string, "Добро пожаловать, {F5B04C}%s{FFFFFF}! Вы на %s.", nick, STATION_NAME);
    SendClientMessage(playerid, COLOR_WHITE, string);

    RW_FormatMoney(money, moneyFmt, sizeof moneyFmt);
    format(string, sizeof string, "Баланс: {7BD88F}%s{FFFFFF} руб.   •   /spcar {AAAAAA}<id>{FFFFFF} — машина   •   /help — помощь", moneyFmt);
    SendClientMessage(playerid, COLOR_GREY, string);

    if (admin > 0) {
        format(string, sizeof string, "Вы вошли как администратор (уровень %d). /ahelp — админ-команды.", admin);
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

/*  Текст для кодов, которыми обмениваются Pawn и CEF-страница. */
stock RW_AuthCodeText(const code[], dest[], size)
{
    if (!strcmp(code, "ok:register", true)) {
        format(dest, size, "Аккаунт создан.");
    } else if (!strcmp(code, "ok:login", true)) {
        format(dest, size, "Вход выполнен.");
    } else if (!strcmp(code, "err:short", true)) {
        format(dest, size, "Слишком короткий пароль (минимум 6 символов).");
    } else if (!strcmp(code, "err:long", true)) {
        format(dest, size, "Слишком длинный пароль (максимум 64 символа).");
    } else if (!strcmp(code, "err:chars", true)) {
        format(dest, size, "В пароле есть недопустимые символы.");
    } else if (!strcmp(code, "err:mismatch", true)) {
        format(dest, size, "Пароли не совпадают.");
    } else if (!strcmp(code, "err:exists", true)) {
        format(dest, size, "Такой ник уже зарегистрирован.");
    } else if (!strcmp(code, "err:noaccount", true)) {
        format(dest, size, "Аккаунт не найден.");
    } else if (!strcmp(code, "err:inuse", true)) {
        format(dest, size, "Этот аккаунт уже в игре.");
    } else if (!strcmp(code, "err:nick", true)) {
        format(dest, size, "Ник не подходит для аккаунта.");
    } else if (!strcmp(code, "err:db", true)) {
        format(dest, size, "Ошибка базы данных. Попробуйте позже.");
    } else if (!strcmp(code, "err:badpass", true, 11)) {
        format(dest, size, "Неверный пароль. Проверьте раскладку и Caps Lock.");
    } else if (!strcmp(code, "err:cooldown", true)) {
        format(dest, size, "Слишком часто. Подождите секунду.");
    } else if (!strcmp(code, "err:attempts", true)) {
        format(dest, size, "Слишком много попыток входа.");
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

    if (registerMode) {
        format(info, sizeof info,
            "%sАккаунт для {F5B04C}%s{FFFFFF} ещё не создан.\nПридумайте пароль (минимум 6 символов).",
            head, nick);
        ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD, "Регистрация", info, "Создать", "Вход");
    } else {
        format(info, sizeof info,
            "%sАккаунт {F5B04C}%s{FFFFFF} найден.\nВведите пароль для входа.", head, nick);
        ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Вход", info, "Войти", "Регистрация");
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

    if (strcmp(code, "err:mismatch", true) == 0 || strcmp(code, "err:short", true) == 0
        || strcmp(code, "err:long", true) == 0 || strcmp(code, "err:chars", true) == 0) {
        RW_ShowAuthDialog(playerid, true, text);
    } else {
        RW_ShowAuthDialog(playerid, false, text);
    }
    return 1;
}

/*  Немедленно переводит игрока на текстовый вход (CEF не работает). */
stock RW_AuthFallbackNow(playerid, const reason[])
{
    if (!IsPlayerConnected(playerid) || gLogged[playerid]) {
        return 0;
    }
    if (gAuthFallbackSent[playerid]) {
        return 1;   /* диалог уже показан */
    }

    gAuthFallbackSent[playerid] = true;

    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
        gFallbackTimer[playerid] = 0;
    }

    if (reason[0] != EOS) {
        printf("[railway] Игрок %d: текстовый вход (%s).", playerid, reason);
    }

    SendClientMessage(playerid, COLOR_GREY, "CEF-интерфейс недоступен — вход через диалог.");

    RW_UI_CloseAuth(playerid);
    CEF_DestroyBrowser(playerid, CEF_BROWSER_AUTH);
    RW_ShowLoginDialog(playerid);
    return 1;
}

forward RW_AuthFallback(playerid);
public RW_AuthFallback(playerid)
{
    gFallbackTimer[playerid] = 0;

    if (!IsPlayerConnected(playerid) || gLogged[playerid]) {
        return 0;
    }

    if (gAuthPageReady[playerid]) {
        return 0;   /* CEF успел подняться */
    }

    return RW_AuthFallbackNow(playerid, "CEF/страница входа не ответили вовремя");
}

stock RW_ArmFallback(playerid)
{
    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
    }
    gFallbackTimer[playerid] = SetTimerEx("RW_AuthFallback", AUTH_FALLBACK_DELAY, false, "i", playerid);
    return 1;
}

/* ------------------------------------------------------------------ CEF-события входа */

public RW_OnAuthReady(playerid, browserid)
{
    if (browserid != CEF_BROWSER_AUTH || gLogged[playerid]) return 0;
    gAuthPageReady[playerid] = true;
    printf("[railway:cef] auth JS ready player=%d", playerid);
    if (gFallbackTimer[playerid] != 0) {
        KillTimer(gFallbackTimer[playerid]);
        gFallbackTimer[playerid] = 0;
    }
    new nick[MAX_PLAYER_NAME];
    new bool:hasAccount;

    if (!gLogged[playerid] && IsPlayerConnected(playerid)) {
        RW_GetNick(playerid, nick);
        hasAccount = RW_DB_AccountExists(nick);

        CEF_EmitEvent(playerid, CEF_BROWSER_AUTH, "auth:init",
            CEF_STR(nick), CEF_BOOL(hasAccount), CEF_STR("1.0.0"));
    }
    return 1;
}

public RW_OnAuthLogin(playerid, browserid, const password[])
{
    if (browserid != CEF_BROWSER_AUTH) {
        return 0;
    }
    return RW_AuthSubmit(playerid, false, password, "");
}

public RW_OnAuthRegister(playerid, browserid, const password[], const confirm[])
{
    if (browserid != CEF_BROWSER_AUTH) {
        return 0;
    }
    return RW_AuthSubmit(playerid, true, password, confirm);
}
