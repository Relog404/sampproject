#if defined _railway_ui_included
    #endinput
#endif
#define _railway_ui_included
#include <railway/core>

#define UI_COUNT 22
#define UI_AUTH 1
#define UI_SETTINGS 2
#define UI_VEHICLE 3
#define UI_PASSENGERS 4
#define UI_BIND 5
#define UI_MAP 6
new PlayerText:gUI[MAX_PLAYERS][UI_COUNT];
new bool:gUICreated[MAX_PLAYERS], bool:gUIInput[MAX_PLAYERS];
new gUIMode[MAX_PLAYERS], gUIBind[MAX_PLAYERS], gUIPage[MAX_PLAYERS];
new gUITarget[MAX_PLAYERS][6], gToastStart[MAX_PLAYERS];
new bool:gToastActive[MAX_PLAYERS], bool:gUIHasNext[MAX_PLAYERS];
forward RW_UITick();

stock PlayerText:RW_TD_Label(playerid, Float:x, Float:y, const text[], Float:scale = 0.26)
{
    new buf[144]; format(buf, sizeof buf, "%s", text);
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
    gUI[playerid][0] = RW_TD_Box(playerid, 378.0, 112.0, 246.0, 302.0, 0x171B23EF);
    gUI[playerid][1] = RW_TD_Label(playerid, 394.0, 130.0, "RAILWAY", 0.38);
    gUI[playerid][2] = RW_TD_Label(playerid, 394.0, 157.0, "CONTROL", 0.21);
    for (new i; i < 8; i++) {
        gUI[playerid][3+i] = RW_TD_Label(playerid, 394.0, 183.0+24.0*i, "_", 0.26);
        PlayerTextDrawUseBox(playerid, gUI[playerid][3+i], 1);
        PlayerTextDrawBoxColor(playerid, gUI[playerid][3+i], 0xFFFFFF0A);
        PlayerTextDrawTextSize(playerid, gUI[playerid][3+i], 610.0, 17.0);
        PlayerTextDrawSetSelectable(playerid, gUI[playerid][3+i], 1);
    }
    gUI[playerid][11] = RW_TD_Label(playerid, 394.0, 389.0, "ZAKRYT / ESC", 0.22);
    PlayerTextDrawTextSize(playerid, gUI[playerid][11], 610.0, 16.0);
    PlayerTextDrawSetSelectable(playerid, gUI[playerid][11], 1);
    gUI[playerid][12] = RW_TD_Box(playerid, 490.0, 332.0, 136.0, 99.0, 0x171B23E8);
    gUI[playerid][13] = RW_TD_Label(playerid, 501.0, 338.0, "TRANSPORT", 0.22);
    gUI[playerid][14] = RW_TD_Label(playerid, 501.0, 351.0, "000", 0.9);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][14], 0.9, 3.1);
    gUI[playerid][15] = RW_TD_Label(playerid, 501.0, 383.0, "KM/H", 0.18);
    gUI[playerid][16] = RW_TD_Label(playerid, 587.0, 359.0, "N", 0.5);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][16], 0.5, 2.0);
    gUI[playerid][17] = RW_TD_Label(playerid, 501.0, 398.0, "KEY -   ENG -", 0.18);
    PlayerTextDrawLetterSize(playerid, gUI[playerid][17], 0.18, 0.9);
    gUI[playerid][18] = RW_TD_Box(playerid, 501.0, 422.0, 1.0, 2.0, 0xD8C39BFF);
    gUI[playerid][19] = RW_TD_Box(playerid, 178.0, 350.0, 284.0, 46.0, 0x171B23EF);
    gUI[playerid][20] = RW_TD_Label(playerid, 190.0, 360.0, "_", 0.25);
    PlayerTextDrawTextSize(playerid, gUI[playerid][20], 451.0, 0.0);
    gUI[playerid][21] = RW_TD_Box(playerid, 178.0, 394.0, 284.0, 2.0, 0xD8C39BFF);
    gUICreated[playerid] = true;
    gUIMode[playerid] = 0;
    gUIInput[playerid] = false;
    gToastActive[playerid] = false;
    return 1;
}
stock RW_UI_Text(playerid, index, const text[])
{
    new buf[144]; format(buf, sizeof buf, "%s", text);
    PlayerTextDrawSetString(playerid, gUI[playerid][index], buf);
}
stock RW_UI_Panel(playerid, mode, const title[], const subtitle[])
{
    RW_UI_Create(playerid);
    for (new i = 12; i <= 18; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    gUIMode[playerid] = mode;
    gUIInput[playerid] = false;
    RW_UI_Text(playerid, 1, title); RW_UI_Text(playerid, 2, subtitle);
    for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    for (new i; i < 8; i++) RW_UI_Text(playerid, 3+i, "_");
    PlayerTextDrawShow(playerid, gUI[playerid][0]);
    PlayerTextDrawShow(playerid, gUI[playerid][1]);
    PlayerTextDrawShow(playerid, gUI[playerid][2]);
    PlayerTextDrawShow(playerid, gUI[playerid][11]);
    SelectTextDraw(playerid, 0xD8C39BFF);
    return 1;
}
stock RW_UI_Row(playerid, row, const text[])
{
    RW_UI_Text(playerid, 3+row, text);
    PlayerTextDrawShow(playerid, gUI[playerid][3+row]);
}
stock RW_UI_Close(playerid)
{
    if (gUIMode[playerid] == UI_MAP) gMapPending[playerid] = false;
    gUIMode[playerid] = 0;
    gControlMode[playerid] = 0;
    gUIInput[playerid] = false;
    if (gUICreated[playerid]) for (new i; i < 12; i++) PlayerTextDrawHide(playerid, gUI[playerid][i]);
    CancelSelectTextDraw(playerid);
    if (gLogged[playerid] && GetPlayerState(playerid) == PLAYER_STATE_DRIVER) RW_UI_SpeedoShow(playerid, GetPlayerVehicleID(playerid));
    return 1;
}
stock RW_UI_OpenAuth(playerid)
{
    new nick[MAX_PLAYER_NAME]; RW_GetNick(playerid, nick);
    new bool:exists = RW_DB_AccountExists(nick);
    RW_UI_Panel(playerid, UI_AUTH, "RAILWAY / ACCOUNT", nick);
    RW_UI_Row(playerid, 0, exists ? ("VESTI PAROL / LOGIN") : ("SOZDAT AKKAUNT / REGISTER"));
    RW_UI_Row(playerid, 2, "SQLite / local account");
    return 1;
}
stock RW_UI_PasswordBegin(playerid)
{
    gUIInput[playerid] = true;
    CancelSelectTextDraw(playerid);
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
    format(buf, sizeof buf, "KEY %s  ENG %s~n~DOORS %s", (flags & 2) ? ("+") : ("-"), (flags & 1) ? ("+") : ("-"), (flags & 4) ? ("OPEN") : ("LOCK"));
    RW_UI_Text(playerid, 17, buf);
    new Float:width = float(rpm)*114.0/6500.0;
    if (width < 1.0) width = 1.0; if (width > 114.0) width = 114.0;
    PlayerTextDrawTextSize(playerid, gUI[playerid][18], width, 2.0);
    PlayerTextDrawShow(playerid, gUI[playerid][18]);
    return 1;
}
// Stock GTA fonts do not reliably render raw CP1251. Transliterated labels
// and notifications work on an unmodified client; password dialogs remain Russian.
stock RW_UI_Notify(playerid, const text[])
{
    RW_UI_Create(playerid);
    static const map[][] = {"a","b","v","g","d","e","zh","z","i","y","k","l","m","n","o","p","r","s","t","u","f","h","ts","ch","sh","sch","","y","","e","yu","ya"};
    new out[140];
    for (new i; text[i] && strlen(out) < 110; i++) {
        new c = text[i] & 255;
        if (c >= 192) strcat(out, map[(c-192)%32], sizeof out);
        else if (c == 168 || c == 184) strcat(out, "e", sizeof out);
        else if (c >= 32 && c < 127) { new ch[2]; ch[0] = c == '~' ? '-' : c; strcat(out, ch, sizeof out); }
    }
    RW_UI_Text(playerid, 20, out);
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
        PlayerTextDrawTextSize(p, gUI[p][21], 284.0*float(3000-elapsed)/3000.0, 2.0);
        for (new i = 19; i < 22; i++) PlayerTextDrawShow(p, gUI[p][i]);
    }
    return 1;
}
stock RW_UI_Reset(playerid)
{
    gUIMode[playerid] = 0; gControlMode[playerid] = 0;
    gUIInput[playerid] = false; gToastActive[playerid] = false;
    if (gUICreated[playerid]) for (new i; i < UI_COUNT; i++) PlayerTextDrawDestroy(playerid, gUI[playerid][i]);
    gUICreated[playerid] = false;
    if (IsPlayerConnected(playerid)) CancelSelectTextDraw(playerid);
    return 1;
}
