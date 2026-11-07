#if defined _railway_ui_included
    #endinput
#endif
#define _railway_ui_included
#include <railway/core>

#define UI_COUNT 30
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
new PlayerText:gUIPreview[MAX_PLAYERS];
new PlayerText:gUI[MAX_PLAYERS][UI_COUNT];
new bool:gUICreated[MAX_PLAYERS], bool:gUIInput[MAX_PLAYERS];
new bool:gUIRussianDialogs[MAX_PLAYERS];   /* старый переключатель, оставлен для совместимости */
new bool:gUISelecting[MAX_PLAYERS];
new gUIExpectedCancels[MAX_PLAYERS];
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
new gUINativeRows[MAX_PLAYERS][8][128], gUINativeMap[MAX_PLAYERS][8];
// CancelSelectTextDraw generates INVALID_TEXT_DRAW too. Never cancel an
// inactive cursor, and clear state BEFORE the native can invoke callbacks.
stock RW_UI_SelectBegin(playerid)
{
    gUISelecting[playerid] = true;
    SelectTextDraw(playerid, 0xD8C39BFF);
    return 1;
}
stock RW_UI_SelectEnd(playerid)
{
    if (!gUISelecting[playerid]) return 1;
    gUISelecting[playerid] = false;
    gUIExpectedCancels[playerid]++;
    CancelSelectTextDraw(playerid);
    return 1;
}
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
    for (new i; i < 8; i++) {
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

// Never send raw Cyrillic to GTA TextDraw fonts. Native SA-MP dialogs render
// Russian independently; TextDraw mode/HUD use only safe ASCII glyphs.
stock RW_TD_Encode(playerid, const text[], out[], size)
{
    static const map[][] = {"a","b","v","g","d","e","zh","z","i","y","k","l","m","n","o","p","r","s","t","u","f","h","ts","ch","sh","sch","","y","","e","yu","ya"};
    out[0] = EOS;
    new length = strlen(text);
    for (new i; text[i] && strlen(out) < size-4; i++) {
        new c = text[i] & 255;
        // Remove chat colour codes; native TD colours are configured separately.
        if (c == '{' && i+7 < length && text[i+7] == '}') {
            new bool:hex = true;
            for (new k = 1; k <= 6; k++) {
                new h = text[i+k];
                if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'))) hex = false;
            }
            if (hex) { i += 7; continue; }
        }
        if (c == '~' && i+2 < length && text[i+2] == '~' && text[i+1] == 'n') { strcat(out, "~n~", size); i += 2; continue; }
        if (c >= 192) {
            new latin[5]; format(latin, sizeof latin, "%s", map[(c-192)%32]);
            if (c < 224) for (new k; latin[k]; k++) if (latin[k] >= 'a' && latin[k] <= 'z') latin[k] -= 32;
            strcat(out, latin, size);
        } else if ((c == 168 || c == 184)) strcat(out, c == 168 ? ("E") : ("e"), size);
        else if (c >= 127) strcat(out, "-", size);
        else if (c >= 32) { new ch[2]; ch[0] = c == '~' ? '-' : c; strcat(out, ch, size); }
    }
    return 1;
}
stock PlayerText:RW_TD_Label(playerid, Float:x, Float:y, const text[], Float:scale = 0.22)
{
    new buf[144]; RW_TD_Encode(playerid, text, buf, sizeof buf);
    new PlayerText:t = CreatePlayerTextDraw(playerid, x, y, buf);
    PlayerTextDrawFont(playerid, t, 1);
    PlayerTextDrawLetterSize(playerid, t, scale, 1.15);
    PlayerTextDrawColor(playerid, t, 0xEEECE7FF);
    PlayerTextDrawSetShadow(playerid, t, 0);
    PlayerTextDrawSetOutline(playerid, t, 0);
    PlayerTextDrawSetProportional(playerid, t, 1);
    return t;
}
stock PlayerText:RW_TD_Box(playerid, Float:x, Float:y, Float:w, Float:h, color)
{
    new PlayerText:t = CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white");
    PlayerTextDrawFont(playerid, t, 4);
    PlayerTextDrawTextSize(playerid, t, w, h);
    PlayerTextDrawColor(playerid, t, color);
    return t;
}
stock RW_UI_Init()
{
    /* Тосты рисует браузер: тексдрав-уведомлений больше нет, таймер не нужен. */
    return 1;
}
stock RW_UI_Create(playerid)
{
    if (gUICreated[playerid]) return 1;
    gUI[playerid][12] = RW_TD_Box(playerid, 508.0, 349.0, 118.0, 82.0, 0x171B23E8);
    gUI[playerid][13] = RW_TD_Label(playerid, 518.0, 355.0, "Vehicle", 0.18);
    gUI[playerid][14] = RW_TD_Label(playerid, 518.0, 366.0, "000", 0.72);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][14], 0.72, 2.5);
    gUI[playerid][15] = RW_TD_Label(playerid, 518.0, 392.0, "km/h", 0.16);
    gUI[playerid][16] = RW_TD_Label(playerid, 597.0, 373.0, "N", 0.4);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][16], 0.4, 1.6);
    gUI[playerid][17] = RW_TD_Label(playerid, 518.0, 405.0, "Key -  Engine -", 0.14);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][17], 0.14, 0.75);
    gUI[playerid][18] = RW_TD_Box(playerid, 518.0, 425.0, 1.0, 2.0, 0xD8C39BFF);
    /* 19..21 были тексдравом всплывашки: уведомления ушли в браузер. */
    gUICreated[playerid] = true;
    gUIMode[playerid] = 0;
    gUIInput[playerid] = false;
    gToastActive[playerid] = false;
    return 1;
}
// Card preview: recreated on every show so its pool id stays above the panel.
stock RW_UI_PreviewHide(playerid)
{
    if (gUIPreview[playerid] != PlayerText:INVALID_TEXT_DRAW) {
        PlayerTextDrawDestroy(playerid, gUIPreview[playerid]);
        gUIPreview[playerid] = PlayerText:INVALID_TEXT_DRAW;
    }
    return 1;
}
stock RW_UI_PreviewShow(playerid, model, Float:x, Float:y, Float:w, Float:h)
{
    RW_UI_PreviewHide(playerid);
    if (model < 400 || model > 611) return 1;
    gUIPreview[playerid] = CreatePlayerTextDraw(playerid, x, y, "_");
    PlayerTextDrawFont(playerid, gUIPreview[playerid], 5);
    PlayerTextDrawTextSize(playerid, gUIPreview[playerid], w, h);
    PlayerTextDrawColor(playerid, gUIPreview[playerid], 0xFFFFFFFF);
    PlayerTextDrawBackgroundColor(playerid, gUIPreview[playerid], 0x14192000);
    PlayerTextDrawSetPreviewModel(playerid, gUIPreview[playerid], model);
    PlayerTextDrawSetPreviewRot(playerid, gUIPreview[playerid], -16.0, 0.0, -35.0, 0.9);
    PlayerTextDrawShow(playerid, gUIPreview[playerid]);
    return 1;
}
stock RW_UI_Text(playerid, index, const text[])
{
    new buf[144]; RW_TD_Encode(playerid, text, buf, sizeof buf);
    PlayerTextDrawSetString(playerid, gUI[playerid][index], buf);
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
    for (new i; i < 8; i++) gUINativeRows[playerid][i][0] = EOS;
    new cef = RW_UI_CefMode(mode);
    if (cef != CEF_MODE_NONE && RW_CefOn(playerid) && RW_CefOpen(playerid, cef, title, subtitle)) {
        gUIOnCef[playerid] = true;
        return 1;
    }
    gUIOnCef[playerid] = false;
    RW_UI_SelectEnd(playerid);
    return RW_UI_ScheduleNative(playerid);
}
stock RW_UI_Row(playerid, row, const text[])
{
    if (gUIOnCef[playerid]) { RW_CefRow(playerid, row, text, "", "bag"); return RW_UI_ScheduleCef(playerid); }
    format(gUINativeRows[playerid][row], 128, "%s", text);
    return RW_UI_ScheduleNative(playerid);
}
stock RW_UI_Close(playerid, bool:keepSelection = false)
{
    gUIOnCef[playerid] = false;
    RW_UI_CancelCef(playerid);
    RW_CefClose(playerid);
    RW_GroundHide(playerid);
    RW_InvHide(playerid);
    RW_GarageHide(playerid);
    RW_KeyMenuHide(playerid);
    RW_MedHide(playerid);
    RW_UI_PreviewHide(playerid);
    RW_UI_CancelNative(playerid);
    RW_UI_CancelCef(playerid);
    gUIOnCef[playerid] = false;
    if (IsPlayerConnected(playerid)) ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
    if (gUIMode[playerid] == UI_MAP) gMapPending[playerid] = false;
    gUIMode[playerid] = 0;
    gControlMode[playerid] = 0;
    gUIInput[playerid] = false;
    if (!keepSelection) RW_UI_SelectEnd(playerid);
    if (gLogged[playerid] && GetPlayerState(playerid) == PLAYER_STATE_DRIVER) RW_UI_SpeedoShow(playerid, GetPlayerVehicleID(playerid));
    return 1;
}
stock RW_UI_OpenAuth(playerid)
{
    /* Зависший курсор мешает войти: сначала убираем выбор текстов. */
    RW_UI_SelectEnd(playerid);
    if (!gLogged[playerid]) {
        new authNick[MAX_PLAYER_NAME];
        RW_GetNick(playerid, authNick);
        if (RW_CefAuthOpen(playerid, !RW_DB_AccountExists(authNick), authNick, "")) return 1;
        /* Браузера нет: логин и пароль спрашивает нативный диалог. */
        gUIMode[playerid] = UI_AUTH;
        return RW_ShowLoginDialog(playerid);
    }
    return 1;
}
stock RW_UI_PasswordBegin(playerid)
{
    gUIInput[playerid] = true;
    RW_UI_SelectEnd(playerid);
}
stock RW_UI_CloseAuth(playerid) { return RW_UI_Close(playerid); }
stock RW_UI_SpeedoShow(playerid, vehicleid)
{
    if (!RW_SettingsOn(playerid, SET_SPEEDO)) return 0;
    RW_UI_Create(playerid);
    new name[48]; RW_VehicleName(GetVehicleModel(vehicleid), name, sizeof name);
    RW_UI_Text(playerid, 13, name);
    if (gUIMode[playerid] == 0) for (new i = 12; i <= 18; i++) PlayerTextDrawShow(playerid, gUI[playerid][i]);
    gSpeedoShown[playerid] = true; gSpeedoVehicle[playerid] = vehicleid;
    return 1;
}
stock RW_UI_SpeedoHide(playerid)
{
    if (gUICreated[playerid]) for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    gSpeedoShown[playerid] = false; gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    return 1;
}
stock RW_UI_SpeedoUpdate(playerid, speed, gear, rpm, flags)
{
    if (gUIMode[playerid] != 0) return 1;
    new buf[96]; format(buf, sizeof buf, "%03d", speed); RW_UI_Text(playerid, 14, buf);
    if (gear < 0) format(buf, sizeof buf, "R");
    else if (gear == 0) format(buf, sizeof buf, "N");
    else format(buf, sizeof buf, "%d", gear);
    RW_UI_Text(playerid, 16, buf);
    format(buf, sizeof buf, "Key %s  Engine %s~n~Doors %s", (flags & 2) ? ("+") : ("-"), (flags & 1) ? ("+") : ("-"), (flags & 4) ? ("open") : ("locked"));
    RW_UI_Text(playerid, 17, buf);
    new Float:width = float(rpm)*96.0/6500.0;
    if (width < 1.0) width = 1.0; if (width > 96.0) width = 96.0;
    PlayerTextDrawTextSize(playerid, gUI[playerid][18], width, 2.0);
    PlayerTextDrawShow(playerid, gUI[playerid][18]);
    return 1;
}
// Notices: kind 0 plain, 1 ok, 2 error. The text already went to the chat in
// RW_Info / RW_Ok / RW_Error, so the notice is only the browser toast — no
// TextDraw pops up over the screen, not even before the browser is ready.
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
    RW_UI_PreviewHide(playerid);
    RW_UI_CancelNative(playerid);
    if (IsPlayerConnected(playerid)) ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
    gUIMode[playerid] = 0; gControlMode[playerid] = 0;
    gUIInput[playerid] = false; gToastActive[playerid] = false;
    if (gUICreated[playerid]) for (new i; i < UI_COUNT; i++) PlayerTextDrawDestroy(playerid, gUI[playerid][i]);
    gUICreated[playerid] = false;
    if (IsPlayerConnected(playerid)) RW_UI_SelectEnd(playerid);
    gUISelecting[playerid] = false;
    gUIExpectedCancels[playerid] = 0;
    return 1;
}
