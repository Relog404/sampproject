#if defined _railway_ui_included
    #endinput
#endif
#define _railway_ui_included
#include <railway/core>

#define UI_COUNT 24
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
new PlayerText:gUIPreview[MAX_PLAYERS];
new PlayerText:gUI[MAX_PLAYERS][UI_COUNT];
new bool:gUICreated[MAX_PLAYERS], bool:gUIInput[MAX_PLAYERS];
new bool:gUIPanelCreated[MAX_PLAYERS], bool:gUIRussianDialogs[MAX_PLAYERS];
new gUILayoutMode[MAX_PLAYERS];
new bool:gUISelecting[MAX_PLAYERS];
new gUIExpectedCancels[MAX_PLAYERS];
new gUIMode[MAX_PLAYERS], gUIBind[MAX_PLAYERS], gUIPage[MAX_PLAYERS];
new gUITarget[MAX_PLAYERS][6], gToastStart[MAX_PLAYERS];
new bool:gToastActive[MAX_PLAYERS], bool:gUIHasNext[MAX_PLAYERS];
forward RW_UITick();
forward RW_UI_FlushNative(playerid);
new gUINativeTimer[MAX_PLAYERS], gUINativeCount[MAX_PLAYERS];
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
public RW_UI_FlushNative(playerid)
{
    gUINativeTimer[playerid] = 0;
    if (!IsPlayerConnected(playerid) || !gUIRussianDialogs[playerid] || gUIInput[playerid] || gUIMode[playerid] == 0 || gUIMode[playerid] == UI_AUTH || gUIMode[playerid] >= UI_INVENTORY) return 1;
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
    if (!gUIRussianDialogs[playerid] || !gLogged[playerid] || gUIMode[playerid] == 0 || gUIMode[playerid] == UI_AUTH || gUIMode[playerid] >= UI_INVENTORY || gUIInput[playerid]) return 1;
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
stock RW_UI_BuildPanel(playerid, mode)
{
    if (gUIPanelCreated[playerid]) {
        for (new i; i < 12; i++) PlayerTextDrawDestroy(playerid, gUI[playerid][i]);
        PlayerTextDrawDestroy(playerid, gUI[playerid][22]);
        PlayerTextDrawDestroy(playerid, gUI[playerid][23]);
    }
    new Float:x = 432.0, Float:y = 142.0, Float:width = 190.0, Float:height = 214.0;
    if (mode == UI_AUTH) { x = 210.0; y = 133.0; width = 220.0; height = 184.0; }
    else if (mode == UI_MAP) { x = 220.0; y = 170.0; width = 200.0; height = 124.0; }
    else if (mode == UI_SETTINGS || mode == UI_BIND) height = 194.0;
    else if (mode == UI_PASSENGERS) height = 240.0;
    else if (mode == UI_VEHICLE) { x = 300.0; width = 322.0; }   /* room for the car preview */
    gUI[playerid][0] = RW_TD_Box(playerid, x, y, width, height, 0x141920F2);
    gUI[playerid][22] = RW_TD_Box(playerid, x, y, 2.0, height, 0xD8C39BFF);
    gUI[playerid][23] = RW_TD_Label(playerid, x+12.0, y+9.0, "RAILWAY / CONTROL", 0.14);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][23], 0.14, 0.72);
    PlayerTextDrawColor(playerid, gUI[playerid][23], 0xD8C39BFF);
    gUI[playerid][1] = RW_TD_Label(playerid, x+12.0, y+23.0, "_", 0.29);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][1], 0.29, 1.25);
    gUI[playerid][2] = RW_TD_Label(playerid, x+12.0, y+43.0, "_", 0.18);
    PlayerTextDrawColor(playerid, gUI[playerid][2], 0x939BA6FF);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][2], 0.18, 0.85);
    for (new i; i < 8; i++) {
        new slot = i;
        if (mode == UI_VEHICLE && i == 7) slot = 6;   /* row 5 is the cabin row */
        else if (mode == UI_SETTINGS && i >= 5) slot = i-2;
        else if (mode == UI_BIND && i == 7) slot = 4;
        new Float:rowY = y+67.0+19.0*slot;
        if (mode == UI_AUTH) {
            if (i == 0) rowY = y+106.0;
            else if (i == 1) rowY = y+65.0;
            else if (i == 2) rowY = y+80.0;
            else rowY = y+137.0;
        }
        gUI[playerid][3+i] = RW_TD_Label(playerid, x+12.0, rowY, "_", 0.20);
        PlayerTextDrawLetterSize(playerid, gUI[playerid][3+i], 0.20, 0.95);
        PlayerTextDrawUseBox(playerid, gUI[playerid][3+i], 1);
        PlayerTextDrawBoxColor(playerid, gUI[playerid][3+i], 0xFFFFFF09);
        PlayerTextDrawTextSize(playerid, gUI[playerid][3+i], x+(mode == UI_VEHICLE ? 165.0 : width-12.0), 13.0);
        PlayerTextDrawSetSelectable(playerid, gUI[playerid][3+i], 1);
    }
    gUI[playerid][11] = RW_TD_Label(playerid, x+12.0, y+height-18.0, "Close / ESC", 0.17);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][11], 0.17, 0.8);
    PlayerTextDrawColor(playerid, gUI[playerid][11], 0x939BA6FF);
    PlayerTextDrawTextSize(playerid, gUI[playerid][11], x+width-12.0, 12.0);
    PlayerTextDrawSetSelectable(playerid, gUI[playerid][11], 1);
    gUIPanelCreated[playerid] = true;
    gUILayoutMode[playerid] = mode;
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
    SetTimer("RW_UITick", 100, true);
    return 1;
}
stock RW_UI_Create(playerid)
{
    if (gUICreated[playerid]) return 1;
    RW_UI_BuildPanel(playerid, 0);
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
    gUI[playerid][19] = RW_TD_Box(playerid, 250.0, 386.0, 160.0, 20.0, 0x171B23EF);
    gUI[playerid][20] = RW_TD_Label(playerid, 258.0, 390.0, "_", 0.15);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][20], 0.15, 0.8);
    PlayerTextDrawTextSize(playerid, gUI[playerid][20], 404.0, 0.0);
    gUI[playerid][21] = RW_TD_Box(playerid, 250.0, 405.0, 160.0, 1.5, 0xD8C39BFF);
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
stock RW_UI_Panel(playerid, mode, const title[], const subtitle[])
{
    RW_GroundHide(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvHide(playerid);
    if (gUIMode[playerid] == UI_GARAGE) RW_GarageHide(playerid);
    RW_UI_Create(playerid);
    for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    gUIMode[playerid] = mode;
    gUIInput[playerid] = false;
    if (gUIRussianDialogs[playerid]) {
        for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
        PlayerTextDrawHide(playerid, gUI[playerid][22]); PlayerTextDrawHide(playerid, gUI[playerid][23]);
        RW_UI_SelectEnd(playerid);
        gUINativeCount[playerid] = 0;
        format(gUINativeTitle[playerid], 64, "%s", title);
        format(gUINativeSubtitle[playerid], 96, "%s", subtitle);
        for (new i; i < 8; i++) gUINativeRows[playerid][i][0] = EOS;
        return RW_UI_ScheduleNative(playerid);
    }
    if (gUILayoutMode[playerid] != mode) RW_UI_BuildPanel(playerid, mode);
    RW_UI_Text(playerid, 11, mode == UI_AUTH ? ("Quit server") : ("Close / ESC"));
    RW_UI_Text(playerid, 1, title); RW_UI_Text(playerid, 2, subtitle);
    for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    for (new i; i < 8; i++) RW_UI_Text(playerid, 3+i, "_");
    PlayerTextDrawShow(playerid, gUI[playerid][0]);
    PlayerTextDrawShow(playerid, gUI[playerid][1]);
    PlayerTextDrawShow(playerid, gUI[playerid][2]);
    PlayerTextDrawShow(playerid, gUI[playerid][11]);
    PlayerTextDrawShow(playerid, gUI[playerid][22]);
    PlayerTextDrawShow(playerid, gUI[playerid][23]);
    RW_UI_SelectBegin(playerid);
    return 1;
}
stock RW_UI_Row(playerid, row, const text[])
{
    if (gUIRussianDialogs[playerid]) {
        format(gUINativeRows[playerid][row], 128, "%s", text);
        return RW_UI_ScheduleNative(playerid);
    }
    RW_UI_Text(playerid, 3+row, text);
    PlayerTextDrawShow(playerid, gUI[playerid][3+row]);
    return 1;
}
stock RW_UI_Close(playerid, bool:keepSelection = false)
{
    RW_CefClose(playerid);
    RW_GroundHide(playerid);
    RW_InvHide(playerid);
    RW_GarageHide(playerid);
    RW_KeyMenuHide(playerid);
    RW_MedHide(playerid);
    RW_UI_PreviewHide(playerid);
    RW_UI_CancelNative(playerid);
    if (IsPlayerConnected(playerid)) ShowPlayerDialog(playerid, -1, 0, "", "", "", "");
    if (gUIMode[playerid] == UI_MAP) gMapPending[playerid] = false;
    gUIMode[playerid] = 0;
    gControlMode[playerid] = 0;
    gUIInput[playerid] = false;
    if (gUICreated[playerid]) for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    if (gUICreated[playerid]) { PlayerTextDrawHide(playerid, gUI[playerid][22]); PlayerTextDrawHide(playerid, gUI[playerid][23]); }
    if (!keepSelection) RW_UI_SelectEnd(playerid);
    if (gLogged[playerid] && GetPlayerState(playerid) == PLAYER_STATE_DRIVER) RW_UI_SpeedoShow(playerid, GetPlayerVehicleID(playerid));
    return 1;
}
stock RW_UI_OpenAuth(playerid)
{
    if (gUIRussianDialogs[playerid]) {
        RW_UI_Create(playerid);
        gUIMode[playerid] = UI_AUTH;
        return RW_ShowLoginDialog(playerid);
    }
    new nick[MAX_PLAYER_NAME]; RW_GetNick(playerid, nick);
    new bool:exists = RW_DB_AccountExists(nick);
    RW_UI_Panel(playerid, UI_AUTH, exists ? ("Welcome back") : ("Welcome"), nick);
    RW_UI_Row(playerid, 0, exists ? ("Login") : ("Create account"));
    RW_UI_Row(playerid, 1, exists ? ("Account found") : ("Begin your Railway story"));
    RW_UI_Row(playerid, 2, exists ? ("Enter password to continue") : ("Registration takes a minute"));
    RW_UI_Row(playerid, 3, "Password input is hidden");
    for (new i = 1; i <= 3; i++) {
        PlayerTextDrawSetSelectable(playerid, gUI[playerid][3+i], 0);
        PlayerTextDrawUseBox(playerid, gUI[playerid][3+i], 0);
        PlayerTextDrawColor(playerid, gUI[playerid][3+i], 0x939BA6FF);
        PlayerTextDrawShow(playerid, gUI[playerid][3+i]);
    }
    PlayerTextDrawBoxColor(playerid, gUI[playerid][3], 0xD8C39B30);
    PlayerTextDrawShow(playerid, gUI[playerid][3]);
    return 1;
}
stock RW_UI_PasswordBegin(playerid)
{
    gUIInput[playerid] = true;
    for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    PlayerTextDrawHide(playerid, gUI[playerid][22]); PlayerTextDrawHide(playerid, gUI[playerid][23]);
    RW_UI_SelectEnd(playerid);
}
stock RW_UI_CloseAuth(playerid) { return RW_UI_Close(playerid); }
stock RW_UI_SpeedoShow(playerid, vehicleid)
{
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
// Notices use the same Russian/Latin conversion as every other label.
// kind: 0 plain, 1 ok, 2 error. With the browser UI the toast is drawn by
// CEF; the TextDraw one stays for players without the plugin.
stock RW_UI_Notify(playerid, const text[], kind = 0)
{
    if (RW_CefToast(playerid, text, kind)) return 1;
    RW_UI_Create(playerid);
    RW_UI_Text(playerid, 20, text);
    gToastStart[playerid] = GetTickCount(); gToastActive[playerid] = true;
    return 1;
}
public RW_UITick()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gUICreated[p] || !gToastActive[p]) continue;
        new elapsed = GetTickCount()-gToastStart[p];
        if (elapsed >= 3000) {
            gToastActive[p] = false;
            for (new i = 19; i < 22; i++) PlayerTextDrawHide(p, gUI[p][i]);
            continue;
        }
        new alpha = elapsed < 200 ? elapsed*255/200 : (elapsed > 2700 ? (3000-elapsed)*255/300 : 255);
        PlayerTextDrawColor(p, gUI[p][19], 0x171B2300 | (alpha*239/255));
        PlayerTextDrawColor(p, gUI[p][20], 0xEEECE700 | alpha);
        PlayerTextDrawColor(p, gUI[p][21], 0xD8C39B00 | alpha);
        PlayerTextDrawTextSize(p, gUI[p][21], 220.0*float(3000-elapsed)/3000.0, 2.0);
        for (new i = 19; i < 22; i++) PlayerTextDrawShow(p, gUI[p][i]);
    }
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
    gUIPanelCreated[playerid] = false;
    gUICreated[playerid] = false;
    if (IsPlayerConnected(playerid)) RW_UI_SelectEnd(playerid);
    gUISelecting[playerid] = false;
    gUIExpectedCancels[playerid] = 0;
    return 1;
}
