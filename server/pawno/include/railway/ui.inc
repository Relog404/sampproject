#if defined _railway_ui_included
    #endinput
#endif
#define _railway_ui_included
#include <railway/core>

#define UI_ROWS 10               /* строк в панели: 0..5 список, 6..9 кнопки */
#define UI_AUTH 1
#define UI_SETTINGS 2
#define UI_VEHICLE 3
#define UI_PASSENGERS 4
#define UI_BIND 5
#define UI_MAP 6
#define UI_INVENTORY 7
#define UI_GARAGE 8
#define UI_KEYMENU 9
#define UI_DOWNED 10
#define UI_PHONE 11
#define UI_PHONE_CONT 12
#define UI_POLICE 13
#define UI_MDT 14
#define UI_EMS 15
#define UI_TRUNK 16
#define UI_LOCKPICK 17
#define UI_SERVICE 18
#define UI_SHOP 19
#define UI_BANK 20
#define UI_JOB 21
#define UI_PHONE_APP 22
#define UI_PROPERTY 23
#define UI_DONATE 24
#define UI_STORY 25
#define UI_MARKET 26
#define UI_CRIME 27
#define UI_FAMILY 28
#define UI_GOV 29
#define UI_INPUT 30                /* поле ввода: цена, ставка, сумма */
#define UI_GPS 31                  /* навигатор: куда ехать            */
#define UI_IMPOUND 32              /* штрафстоянка: выкуп машины        */
/* Тексдрав-превью удалено: машину в карточке рисует страница. */
new bool:gUIInput[MAX_PLAYERS];
new bool:gUIRussianDialogs[MAX_PLAYERS];   /* старый переключатель, оставлен для совместимости */
new gUIMode[MAX_PLAYERS], gUIBind[MAX_PLAYERS], gUIPage[MAX_PLAYERS];
new gUITarget[MAX_PLAYERS][6], gToastStart[MAX_PLAYERS];
new bool:gToastActive[MAX_PLAYERS], bool:gUIHasNext[MAX_PLAYERS];
new bool:gUIOnCef[MAX_PLAYERS];       /* панель нарисована в браузере      */
forward RW_UITick();
forward RW_UI_FlushNative(playerid);
forward RW_UI_FlushCef(playerid);
new gUINativeTimer[MAX_PLAYERS], gUINativeCount[MAX_PLAYERS];
new gUICefTimer[MAX_PLAYERS];
new gUINativeTitle[MAX_PLAYERS][64], gUINativeSubtitle[MAX_PLAYERS][96];
new gUINativeRows[MAX_PLAYERS][UI_ROWS][128], gUINativeMap[MAX_PLAYERS][UI_ROWS];
stock RW_UI_ScheduleNative(playerid)
{
    if (gUINativeTimer[playerid] == 0) gUINativeTimer[playerid] = SetTimerEx("RW_UI_FlushNative", 50, false, "i", playerid);
    return 1;
}
stock RW_UI_CancelNative(playerid)
{
    if (gUINativeTimer[playerid] != 0) KillTimer(gUINativeTimer[playerid]);
    gUINativeTimer[playerid] = 0;
}
/* Строки браузеру приходят по одной: отрисовать страницу надо один раз,
   когда все уже отправлены — тем же трюком, что и нативный диалог. */
stock RW_UI_ScheduleCef(playerid)
{
    if (gUICefTimer[playerid] == 0) gUICefTimer[playerid] = SetTimerEx("RW_UI_FlushCef", 50, false, "i", playerid);
    return 1;
}
stock RW_UI_CancelCef(playerid)
{
    if (gUICefTimer[playerid] != 0) KillTimer(gUICefTimer[playerid]);
    gUICefTimer[playerid] = 0;
}
public RW_UI_FlushCef(playerid)
{
    gUICefTimer[playerid] = 0;
    if (!IsPlayerConnected(playerid) || !gUIOnCef[playerid]) return 1;
    return RW_CefEnd(playerid);
}
public RW_UI_FlushNative(playerid)
{
    gUINativeTimer[playerid] = 0;
    if (!IsPlayerConnected(playerid) || gUIInput[playerid] || gUIOnCef[playerid] || gUIMode[playerid] == 0 || gUIMode[playerid] == UI_AUTH) return 1;
    new title[96], text[1400];
    format(title, sizeof title, "Railway | %s", gUINativeTitle[playerid]);
    if (gUIMode[playerid] == UI_MAP) {
        format(text, sizeof text, "{D8C39B}%s{FFFFFF}\n\nTeleport to the selected point?\n{939BA6}On foot, outdoors, in the main world only.", gUINativeSubtitle[playerid]);
        ShowPlayerDialog(playerid, DIALOG_NATIVE_MENU, DIALOG_STYLE_MSGBOX, title, text, "Yes", "No");
        return 1;
    }
    format(text, sizeof text, "%s\n", gUINativeSubtitle[playerid]);
    gUINativeCount[playerid] = 0;
    for (new i; i < UI_ROWS; i++) {
        if (!gUINativeRows[playerid][i][0]) continue;
        gUINativeMap[playerid][gUINativeCount[playerid]++] = i;
        strcat(text, gUINativeRows[playerid][i], sizeof text);
        strcat(text, "\n", sizeof text);
    }
    ShowPlayerDialog(playerid, DIALOG_NATIVE_MENU, DIALOG_STYLE_TABLIST_HEADERS, title, text, "Select", "Close");
    return 1;
}
stock RW_UI_NativeResponse(playerid, response, listitem)
{
    if (!gLogged[playerid] || gUIInput[playerid] || gUIOnCef[playerid] || gUIMode[playerid] == 0 || gUIMode[playerid] == UI_AUTH) return 1;
    if (gUINativeTimer[playerid] != 0) return 1;
    if (gUIMode[playerid] == UI_MAP) return RW_UI_Click(playerid, response ? 0 : 1);
    if (!response) return RW_UI_Close(playerid);
    if (listitem < 0 || listitem >= gUINativeCount[playerid]) return RW_UI_ScheduleNative(playerid);
    RW_UI_Click(playerid, gUINativeMap[playerid][listitem]);
    if (gUIMode[playerid] != 0) RW_UI_ScheduleNative(playerid);
    return 1;
}

stock RW_UI_Init()
{
    /* Тосты рисует браузер: тексдрав-уведомлений больше нет, таймер не нужен. */
    return 1;
}
/* Тексдрав-панель удалена: панель рисует браузер, а без него — диалог. */
stock RW_UI_Create(playerid)
{
    gUIMode[playerid] = 0;
    gUIInput[playerid] = false;
    gToastActive[playerid] = false;
    return 1;
}
/*  Панель: браузер, а если его нет — нативный диалог. Тексдравов больше нет:
 *  клиент SA-MP не умеет кириллицу, поэтому запасной путь — диалог.          */
stock RW_UI_CefMode(mode)
{
    if (mode == UI_VEHICLE) return CEF_MODE_VEHICLE;
    if (mode == UI_PASSENGERS) return CEF_MODE_PASSENGERS;
    if (mode == UI_POLICE) return CEF_MODE_POLICE;
    if (mode == UI_MDT) return CEF_MODE_MDT;
    if (mode == UI_EMS) return CEF_MODE_EMS;
    if (mode == UI_TRUNK) return CEF_MODE_TRUNK;
    if (mode == UI_LOCKPICK) return CEF_MODE_LOCKPICK;
    if (mode == UI_SERVICE) return CEF_MODE_SERVICE;
    if (mode == UI_SHOP) return CEF_MODE_SHOP;
    if (mode == UI_BANK) return CEF_MODE_BANK;
    if (mode == UI_JOB) return CEF_MODE_JOB;
    if (mode == UI_PHONE_APP) return CEF_MODE_APP;
    if (mode == UI_PROPERTY) return CEF_MODE_PROPERTY;
    if (mode == UI_DONATE) return CEF_MODE_DONATE;
    if (mode == UI_STORY) return CEF_MODE_STORY;
    if (mode == UI_MARKET) return CEF_MODE_MARKET;
    if (mode == UI_INPUT) return CEF_MODE_INPUT;
    if (mode == UI_GPS) return CEF_MODE_GPS;
    if (mode == UI_IMPOUND) return CEF_MODE_IMPOUND;
    if (mode == UI_CRIME) return CEF_MODE_CRIME;
    if (mode == UI_FAMILY) return CEF_MODE_FAMILY;
    if (mode == UI_GOV) return CEF_MODE_GOV;
    if (mode == UI_INVENTORY) return CEF_MODE_INVENTORY;
    if (mode == UI_GARAGE) return CEF_MODE_GARAGE;
    if (mode == UI_KEYMENU) return CEF_MODE_KEYS;
    if (mode == UI_DOWNED) return CEF_MODE_DOWNED;
    if (mode == UI_SETTINGS) return CEF_MODE_SETTINGS;
    if (mode == UI_BIND) return CEF_MODE_BIND;
    if (mode == UI_MAP) return CEF_MODE_MAP;
    return CEF_MODE_NONE;                   /* своей страницы нет: диалог */
}
stock RW_UI_Panel(playerid, mode, const title[], const subtitle[])
{
    RW_GroundHide(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvHide(playerid);
    if (gUIMode[playerid] == UI_GARAGE) RW_GarageHide(playerid);
    gUIMode[playerid] = mode;
    gUIInput[playerid] = false;
    /* Нативный диалог готовим всегда: он остаётся запасным путём. */
    gUINativeCount[playerid] = 0;
    format(gUINativeTitle[playerid], 64, "%s", title);
    format(gUINativeSubtitle[playerid], 96, "%s", subtitle);
    for (new i; i < UI_ROWS; i++) gUINativeRows[playerid][i][0] = EOS;
    new cef = RW_UI_CefMode(mode);
    if (cef != CEF_MODE_NONE && RW_CefOn(playerid) && RW_CefOpen(playerid, cef, title, subtitle)) {
        gUIOnCef[playerid] = true;
        return 1;
    }
    gUIOnCef[playerid] = false;
    return RW_UI_ScheduleNative(playerid);
}
/* Иконку можно не передавать: по умолчанию строка получает «сумку», как
   раньше. Панели, где иконка важна (ключи, метка, бинт), называют её сами. */
stock RW_UI_Row(playerid, row, const text[], const icon[] = "bag")
{
    if (gUIOnCef[playerid]) { RW_CefRow(playerid, row, text, "", icon); return RW_UI_ScheduleCef(playerid); }
    format(gUINativeRows[playerid][row], 128, "%s", text);
    return RW_UI_ScheduleNative(playerid);
}
stock RW_UI_Close(playerid, bool:keepSelection = false)
{
    #pragma unused keepSelection
    gUIOnCef[playerid] = false;
    RW_UI_CancelCef(playerid);
    RW_CefClose(playerid);
    RW_GroundHide(playerid);
    RW_InvHide(playerid);
    RW_GarageHide(playerid);
    RW_KeyMenuHide(playerid);
    RW_MedHide(playerid);
    RW_UI_CancelNative(playerid);
    RW_UI_CancelCef(playerid);
    gUIOnCef[playerid] = false;
    if (IsPlayerConnected(playerid)) ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
    if (gUIMode[playerid] == UI_MAP) gMapPending[playerid] = false;
    gUIMode[playerid] = 0;
    gControlMode[playerid] = 0;
    gUIInput[playerid] = false;
    gCefAuth[playerid] = false;
    if (gLogged[playerid] && GetPlayerState(playerid) == PLAYER_STATE_DRIVER) RW_UI_SpeedoShow(playerid, GetPlayerVehicleID(playerid));
    return 1;
}
stock RW_UI_OpenAuth(playerid)
{
    /* Карточка входа в браузере; без него спрашивает нативный диалог. */
    if (!gLogged[playerid]) {
        new authNick[MAX_PLAYER_NAME];
        RW_GetNick(playerid, authNick);
        if (RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, "")) {
            gCefAuth[playerid] = true;
            /* Диалог-запасник больше не нужен: иначе он висит поверх окна. */
            ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
            return 1;
        }
        /* Браузера нет: логин и пароль спрашивает нативный диалог. */
        gUIMode[playerid] = UI_AUTH;
        return RW_ShowLoginDialog(playerid);
    }
    return 1;
}
stock RW_UI_PasswordBegin(playerid)
{
    gUIInput[playerid] = true;
}
stock RW_UI_CloseAuth(playerid) { return RW_UI_Close(playerid); }
stock RW_UI_SpeedoShow(playerid, vehicleid)
{
    if (!RW_SettingsOn(playerid, SET_SPEEDO)) return 0;
    new name[48];
    RW_VehicleName(GetVehicleModel(vehicleid), name, sizeof name);
    RW_CefHudSpeedo(playerid, true, name, 0, 0, 0, 0);
    gSpeedoShown[playerid] = true; gSpeedoVehicle[playerid] = vehicleid;
    return 1;
}
stock RW_UI_SpeedoHide(playerid)
{
    RW_CefHudSpeedo(playerid, false, "", 0, 0, 0, 0);
    gSpeedoShown[playerid] = false; gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    return 1;
}
stock RW_UI_SpeedoUpdate(playerid, speed, gear, rpm, flags)
{
    if (gUIMode[playerid] != 0) return 1;
    return RW_CefHudSpeedo(playerid, true, "", speed, gear, rpm, flags);
}
// Notices: kind 0 plain, 1 ok, 2 error. The text already went to the chat in
// RW_Info / RW_Ok / RW_Error, so the notice is only the browser toast — no
// Nothing pops up over the screen, not even before the browser is ready.
stock RW_UI_Notify(playerid, const text[], kind = 0)
{
    return RW_CefToast(playerid, text, kind);
}
/* Тосты рисуются только в браузере: таймер оставлен на случай старых
   сборок, сам он больше ничего не показывает. */
public RW_UITick()
{
    return 1;
}
stock RW_UI_Reset(playerid)
{
    RW_GroundReset(playerid);
    RW_InvReset(playerid);
    RW_GarageReset(playerid);
    RW_KeyMenuReset(playerid);
    RW_MedReset(playerid);
    RW_UI_CancelNative(playerid);
    if (IsPlayerConnected(playerid)) ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
    gUIMode[playerid] = 0; gControlMode[playerid] = 0;
    gUIInput[playerid] = false; gToastActive[playerid] = false;
    return 1;
}
