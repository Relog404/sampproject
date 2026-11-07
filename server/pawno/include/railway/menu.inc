/*  railway/menu.inc
 *
 *  Настройки интерфейса и клавиш. Каждая настройка что-то реально меняет
 *  на сервере, а не только рисует галочку: /set и /key работают всегда.
 */

#if defined _railway_menu_included
    #endinput
#endif
#define _railway_menu_included



new gSettings[MAX_PLAYERS];

/*  --------------------------------- база -------------------------------- */
stock RW_MenuSchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS settings (player_id INTEGER PRIMARY KEY, flags INT)");
}
stock RW_MenuLoad(playerid)
{
    gSettings[playerid] = SET_DEFAULT;
    if (gAccountId[playerid] <= 0) return 0;
    new query[128];
    format(query, sizeof query, "SELECT flags FROM settings WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r)) gSettings[playerid] = db_get_field_assoc_int(r, "flags");
    db_free_result(r);
    return 1;
}
stock RW_MenuSave(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[128];
    format(query, sizeof query, "INSERT OR REPLACE INTO settings (player_id,flags) VALUES (%d,%d)", gAccountId[playerid], gSettings[playerid]);
    return RW_DB_Exec(query);
}
stock RW_MenuReset(playerid)
{
    gSettings[playerid] = SET_DEFAULT;
    return 1;
}
stock RW_SettingsOn(playerid, flag) { return (gSettings[playerid] & flag) ? 1 : 0; }
stock RW_SettingsFlip(playerid, flag)
{
    if (RW_SettingsOn(playerid, flag)) gSettings[playerid] = gSettings[playerid] & ~flag;
    else gSettings[playerid] = gSettings[playerid] | flag;
    RW_MenuSave(playerid);
    RW_SettingsApply(playerid);
    return 1;
}
/* Настройки применяются к живому игроку, а не только пишутся в базу. */
stock RW_SettingsApply(playerid)
{
    if (!gLogged[playerid]) return 0;
    RW_HudShow(playerid, RW_SettingsOn(playerid, SET_HUD) ? true : false);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i)) continue;
        ShowPlayerNameTagForPlayer(playerid, i, RW_SettingsOn(playerid, SET_NAMES));
        if (!RW_SettingsOn(playerid, SET_MARKERS)) SetPlayerMarkerForPlayer(playerid, i, 0x00000000);
        else SetPlayerMarkerForPlayer(playerid, i, GetPlayerColor(i));
    }
    return 1;
}

stock RW_MenuKeyName(key, dest[], size = sizeof dest)
{
    switch (key) {
        case KEY_YES: return format(dest, size, "Y");
        case KEY_NO: return format(dest, size, "N");
        case KEY_CTRL_BACK: return format(dest, size, "H");
        case KEY_WALK: return format(dest, size, "LAlt");
    }
    return format(dest, size, "?");
}
/* Клавиши действий меняются местами: берём следующую свободную из набора. */
stock RW_MenuKeyFlip(playerid, slot)
{
    static const keys[] = { KEY_YES, KEY_NO, KEY_CTRL_BACK, KEY_WALK };
    new current = gControlKeys[playerid][slot], next = keys[0];
    for (new i; i < sizeof keys; i++) {
        if (keys[i] != current) continue;
        next = keys[(i + 1) % sizeof keys];
        break;
    }
    for (new i; i < 3; i++) if (i != slot && gControlKeys[playerid][i] == next) return RW_Error(playerid, "Эта клавиша уже занята другим действием.");
    gControlKeys[playerid][slot] = next;
    new name[8], message[120];
    RW_MenuKeyName(next, name, sizeof name);
    format(message, sizeof message, "Теперь это действие на клавише %s.", name);
    return RW_Ok(playerid, message);
}
/* /set <имя>: то же, что строка в меню, но без мыши. */
stock RW_SetCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!strcmp(params, "hud", true)) return RW_SettingsFlip(playerid, SET_HUD);
    if (!strcmp(params, "hints", true)) return RW_SettingsFlip(playerid, SET_HINTS);
    if (!strcmp(params, "speedo", true)) return RW_SettingsFlip(playerid, SET_SPEEDO);
    if (!strcmp(params, "names", true)) return RW_SettingsFlip(playerid, SET_NAMES);
    if (!strcmp(params, "markers", true)) return RW_SettingsFlip(playerid, SET_MARKERS);
    if (!strcmp(params, "welcome", true)) return RW_SettingsFlip(playerid, SET_WELCOME);
    return RW_Error(playerid, "Настройка: /set <hud|hints|speedo|names|markers|welcome>.");
}
/* /key <0|1|2>: двери, двигатель, взаимодействие - без мыши. */
stock RW_KeyCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new slot = strval(params);
    if (slot < 0 || slot > 2) return RW_Error(playerid, "Клавиши: /key 0 - двери, /key 1 - двигатель, /key 2 - взаимодействие.");
    return RW_MenuKeyFlip(playerid, slot);
}
/* Компактная панель: те же настройки, что /set, но списком. */
stock RW_SettingsCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    return RW_UI_Settings(playerid);
}
