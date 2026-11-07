/*  railway/menu.inc
 *
 *  Полноэкранное меню на F3: главная, настройки и донат. Каждая настройка
 *  что-то реально меняет на сервере, а не только рисует галочку.
 */

#if defined _railway_menu_included
    #endinput
#endif
#define _railway_menu_included

#define MENU_MAIN           (0)
#define MENU_SETTINGS       (1)
#define MENU_DONATE         (2)


new gSettings[MAX_PLAYERS];
new gMenuSection[MAX_PLAYERS];
new bool:gMenuShown[MAX_PLAYERS];

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
    gSettings[playerid] = SET_DEFAULT; gMenuSection[playerid] = MENU_MAIN; gMenuShown[playerid] = false;
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

/*  --------------------------------- меню -------------------------------- */
stock RW_MenuSubtitle(playerid, dest[], size = sizeof dest)
{
    return format(dest, size, "Настройки: %d из 6 включено. Разделы - кнопки выше.",
        RW_SettingsOn(playerid, SET_HUD) + RW_SettingsOn(playerid, SET_HINTS)
        + RW_SettingsOn(playerid, SET_SPEEDO) + RW_SettingsOn(playerid, SET_NAMES) + RW_SettingsOn(playerid, SET_MARKERS)
        + RW_SettingsOn(playerid, SET_WELCOME));
}
stock RW_MenuRowOut(playerid, const text[], const action[], on)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefMenuRow(playerid, text, action, on ? "on" : ""); return 1; }
    #else
        #pragma unused action, on
    #endif
    return SendClientMessage(playerid, COLOR_GREY, text);
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
stock RW_MenuRows(playerid)
{
    if (gMenuSection[playerid] == MENU_SETTINGS) {
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_HUD) ? "Экранный интерфейс: включён" : "Экранный интерфейс: выключен", "set:1", RW_SettingsOn(playerid, SET_HUD));
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_HINTS) ? "Подсказки клавиш: включены" : "Подсказки клавиш: выключены", "set:2", RW_SettingsOn(playerid, SET_HINTS));
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_SPEEDO) ? "Спидометр: включён" : "Спидометр: выключен", "set:8", RW_SettingsOn(playerid, SET_SPEEDO));
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_NAMES) ? "Имена игроков: включены" : "Имена игроков: выключены", "set:16", RW_SettingsOn(playerid, SET_NAMES));
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_MARKERS) ? "Маркеры игроков: включены" : "Маркеры игроков: выключены", "set:32", RW_SettingsOn(playerid, SET_MARKERS));
        RW_MenuRowOut(playerid, RW_SettingsOn(playerid, SET_WELCOME) ? "Приветствие при входе: включено" : "Приветствие при входе: выключено", "set:64", RW_SettingsOn(playerid, SET_WELCOME));
        new text[96], name[8];
        for (new slot; slot < 3; slot++) {
            RW_MenuKeyName(gControlKeys[playerid][slot], name, sizeof name);
            format(text, sizeof text, "Клавиша %s: %s (нажмите, чтобы сменить)", slot == 0 ? "дверей" : (slot == 1 ? "двигателя" : "взаимодействия"), name);
            RW_MenuRowOut(playerid, text, slot == 0 ? "key:0" : (slot == 1 ? "key:1" : "key:2"), 1);
        }
        return 1;
    }
    if (gMenuSection[playerid] == MENU_DONATE) {
        for (new row; row < DONATE_MAX; row++) {
            new text[128];
            RW_DonateRowText(playerid, row, text, sizeof text);
            RW_MenuRowOut(playerid, text, row == 0 ? "buy:0" : (row == 1 ? "buy:1" : (row == 2 ? "buy:2" : (row == 3 ? "buy:3" : (row == 4 ? "buy:4" : (row == 5 ? "buy:5" : (row == 6 ? "buy:6" : "buy:7")))))), 1);
        }
        return 1;
    }
    RW_MenuRowOut(playerid, "Вернуться в игру", "resume", 1);
    RW_MenuRowOut(playerid, "Настройки", "settings", 1);
    RW_MenuRowOut(playerid, "Донат-меню", "donate", 1);
    RW_MenuRowOut(playerid, "Выйти из игры", "quit", 1);
    return 1;
}
stock RW_MenuOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    gMenuShown[playerid] = true;
    new title[32], subtitle[200];
    if (gMenuSection[playerid] == MENU_SETTINGS) format(title, sizeof title, "Настройки");
    else if (gMenuSection[playerid] == MENU_DONATE) format(title, sizeof title, "Донат");
    else format(title, sizeof title, "Меню");
    RW_MenuSubtitle(playerid, subtitle, sizeof subtitle);
    TogglePlayerControllable(playerid, false);   /* клик должен дойти до страницы */
    #if RAILWAY_CEF
        if (RW_CefMenuOpen(playerid, title, subtitle)) {
            RW_MenuRows(playerid);
            SetTimerEx("RW_MenuFocus", 300, false, "i", playerid);
            return RW_CefMenuEnd(playerid);
        }
    #endif
    RW_Info(playerid, "Меню: команды работают всегда - /set <hud|hints|bonus|speedo|names|markers|welcome>, /key <0|1|2>, /donate, /menu.");
    return RW_MenuRows(playerid);
}
stock RW_MenuClose(playerid)
{
    gMenuShown[playerid] = false;
    gMenuSection[playerid] = MENU_MAIN;
    /* Управление возвращается, только если курсор мыши тоже выключен. */
    if (!gHudMouse[playerid]) TogglePlayerControllable(playerid, true);
    #if RAILWAY_CEF
        RW_CefMenuClose(playerid);
    #endif
    return 1;
}
stock RW_MenuToggle(playerid)
{
    if (gMenuShown[playerid]) return RW_MenuClose(playerid);
    gMenuSection[playerid] = MENU_MAIN;
    return RW_MenuOpen(playerid);
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
    RW_Ok(playerid, message);
    return RW_MenuOpen(playerid);
}
stock RW_MenuStartsWith(const text[], const prefix[])
{
    new i;
    while (prefix[i]) {
        if (text[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}
public RW_MenuFocus(playerid)
{
    return RW_CefMenuFocus(playerid);
}
public RW_MenuAction(playerid, browserid, const action[])
{
    #pragma unused browserid
    if (!gLogged[playerid]) return 0;
    if (!strcmp(action, "resume", true)) return RW_MenuClose(playerid);
    if (!strcmp(action, "quit", true)) { RW_MenuSave(playerid); return Kick(playerid); }
    if (!strcmp(action, "settings", true)) { gMenuSection[playerid] = MENU_SETTINGS; return RW_MenuOpen(playerid); }
    if (!strcmp(action, "donate", true)) { gMenuSection[playerid] = MENU_DONATE; return RW_MenuOpen(playerid); }
    if (RW_MenuStartsWith(action, "set:")) return RW_SettingsFlip(playerid, strval(action[4]));
    if (RW_MenuStartsWith(action, "key:")) return RW_MenuKeyFlip(playerid, strval(action[4]));
    if (RW_MenuStartsWith(action, "buy:")) { RW_DonateBuy(playerid, strval(action[4])); return RW_MenuOpen(playerid); }
    return 1;
}
stock RW_MenuCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gMenuShown[playerid]) return RW_MenuClose(playerid);
    gMenuSection[playerid] = MENU_MAIN;
    return RW_MenuOpen(playerid);
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
stock RW_SettingsCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    gMenuSection[playerid] = MENU_SETTINGS;
    return RW_MenuOpen(playerid);
}
