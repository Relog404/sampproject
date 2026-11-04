#if defined _railway_interaction_included
    #endinput
#endif
#define _railway_interaction_included

#define CONTROL_BROWSER 3001
new bool:gControlCreated[MAX_PLAYERS], bool:gControlReady[MAX_PLAYERS];
new gControlMode[MAX_PLAYERS], gControlVehicle[MAX_PLAYERS];
new gControlKeys[MAX_PLAYERS][3], gControlTick[MAX_PLAYERS];
forward RW_ControlReady(playerid, browserid);
forward RW_ControlClose(playerid, browserid);
forward RW_ControlAction(playerid, browserid, action, target);
forward RW_ControlBind(playerid, browserid, door, engine, menu);

stock RW_InteractionRegister()
{
    CEF_RegisterEvent("control:ready", "RW_ControlReady");
    CEF_RegisterEvent("control:close", "RW_ControlClose");
    CEF_RegisterEvent("control:action", "RW_ControlAction", Argument_Integer, Argument_Integer);
    CEF_RegisterEvent("control:bind", "RW_ControlBind", Argument_Integer, Argument_Integer, Argument_Integer);
}
stock RW_InteractionReset(playerid)
{
    gControlCreated[playerid] = false;
    gControlReady[playerid] = false;
    gControlMode[playerid] = 0;
    gControlVehicle[playerid] = INVALID_VEHICLE_ID;
    gControlKeys[playerid][0] = 76;
    gControlKeys[playerid][1] = 69;
    gControlKeys[playerid][2] = 71;
    gControlTick[playerid] = 0;
}
stock RW_InteractionInit(playerid)
{
    CEF_SetKeyCapture(playerid, true);
    for (new i; i < 3; i++) CEF_EnableKey(playerid, gControlKeys[playerid][i], true);
    if (!gControlCreated[playerid]) {
        gControlCreated[playerid] = true;
        CEF_CreateBrowser(playerid, CONTROL_BROWSER, "http://cef/railway_control/index.html", false, false, 0.0, 0.0);
        CEF_SetBrowserLayer(playerid, CONTROL_BROWSER, 30);
    }
}
stock RW_ControlNotify(playerid, const text[])
{
    if (gControlReady[playerid]) CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:toast", CEF_STR(text));
    else RW_Info(playerid, text);
    return 1;
}
// Camera ray against a vehicle-centred sphere. Only streamed, nearby vehicles
// in the player's world are eligible; never select the nearest car blindly.
stock RW_ControlTarget(playerid)
{
    if (IsPlayerInAnyVehicle(playerid)) return GetPlayerVehicleID(playerid);
    new Float:cx, Float:cy, Float:cz, Float:fx, Float:fy, Float:fz;
    GetPlayerCameraPos(playerid, cx, cy, cz);
    GetPlayerCameraFrontVector(playerid, fx, fy, fz);
    new selected = INVALID_VEHICLE_ID;
    new Float:best = 100000.0;
    for (new slot; slot < VEHICLE_SLOTS; slot++) {
        if (!gVeh[slot][vsUsed]) continue;
        new v = gVeh[slot][vsVehicleId];
        if (!IsVehicleStreamedIn(v, playerid) || GetVehicleVirtualWorld(v) != GetPlayerVirtualWorld(playerid) || GetPlayerInterior(playerid) != 0) continue;
        new Float:x, Float:y, Float:z;
        GetVehiclePos(v, x, y, z);
        if (!IsPlayerInRangeOfPoint(playerid, KEY_RANGE, x, y, z)) continue;
        x -= cx; y -= cy; z -= cz;
        new Float:t = x*fx + y*fy + z*fz;
        if (t < 0.0 || t > best) continue;
        if ((x*x+y*y+z*z) - t*t > 2.25) continue;
        best = t; selected = v;
    }
    return selected;
}
stock RW_ControlPublish(playerid)
{
    if (gControlMode[playerid] == 1) {
        CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:settings", CEF_INT(gControlKeys[playerid][0]), CEF_INT(gControlKeys[playerid][1]), CEF_INT(gControlKeys[playerid][2]));
    } else if (gControlMode[playerid] == 2) {
        new v = gControlVehicle[playerid], slot = gVehSlotOf[v];
        if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
        new name[48]; RW_VehicleName(GetVehicleModel(v), name, sizeof name);
        CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:vehicle", CEF_STR(name), CEF_INT(RW_Veh_HudFlags(playerid, v)), CEF_BOOL(gVeh[slot][vsLights]));
        for (new i; i < MAX_PLAYERS; i++) {
            if (!IsPlayerConnected(i) || !IsPlayerInVehicle(i, v) || GetPlayerVehicleSeat(i) <= 0) continue;
            new nick[MAX_PLAYER_NAME]; GetPlayerName(i, nick, sizeof nick);
            CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:passenger", CEF_INT(i), CEF_STR(nick));
        }
    }
    return 1;
}
stock RW_ControlOpen(playerid, bool:settings)
{
    if (!gLogged[playerid] || !gCefReady[playerid]) return RW_ControlNotify(playerid, "CEF ещё загружается.");
    if (!settings) {
        new v = RW_ControlTarget(playerid);
        if (v == INVALID_VEHICLE_ID || gVehSlotOf[v] < 0) return 1;
        gControlVehicle[playerid] = v;
    }
    gControlMode[playerid] = settings ? 1 : 2;
    CEF_FocusBrowser(playerid, CONTROL_BROWSER, true);
    CEF_ToggleChatInput(playerid, false);
    CEF_SetEscapeMenuMode(playerid, CEF_ESCAPE_MENU_DISABLED);
    RW_ControlPublish(playerid);
    return 1;
}
public RW_ControlReady(playerid, browserid)
{
    if (browserid != CONTROL_BROWSER) return 0;
    gControlReady[playerid] = true;
    RW_ControlPublish(playerid);
    return 1;
}
public RW_ControlClose(playerid, browserid)
{
    if (browserid != CONTROL_BROWSER) return 0;
    gControlMode[playerid] = 0;
    CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:hide");
    CEF_FocusBrowser(playerid, CONTROL_BROWSER, false);
    CEF_ToggleChatInput(playerid, true);
    CEF_SetEscapeMenuMode(playerid, CEF_ESCAPE_MENU_GTA);
    return 1;
}
stock RW_ControlExecute(playerid, v, action, target = -1)
{
    if (!gLogged[playerid] || v <= 0 || v >= MAX_VEHICLES) return 0;
    new slot = gVehSlotOf[v];
    if (slot < 0 || !gVeh[slot][vsUsed]) return 0;
    new Float:x, Float:y, Float:z;
    GetVehiclePos(v, x, y, z);
    if (GetVehicleVirtualWorld(v) != GetPlayerVirtualWorld(playerid) || GetPlayerInterior(playerid) != 0 || !IsPlayerInRangeOfPoint(playerid, KEY_RANGE, x, y, z)) return RW_ControlNotify(playerid, "Машина слишком далеко.");
    if (action != 1 && !RW_IsVehicleOwner(playerid, v)) return RW_ControlNotify(playerid, "Не ваша машина.");
    switch (action) {
        case 0: RW_Veh_SetDoors(v, !gVeh[slot][vsDoors]);
        case 1: {
            if (!IsPlayerInVehicle(playerid, v) || GetPlayerVehicleSeat(playerid) != 0) return RW_ControlNotify(playerid, "Сядьте за руль.");
            if (!gVeh[slot][vsKey]) return RW_ControlNotify(playerid, "Сначала вставьте ключ.");
            gVeh[slot][vsEngine] = !gVeh[slot][vsEngine];
            RW_Veh_Sync(v);
        }
        case 2: {
            if (!IsPlayerInVehicle(playerid, v) || GetPlayerVehicleSeat(playerid) != 0) return RW_ControlNotify(playerid, "Сядьте за руль.");
            RW_Veh_SetKey(v, !gVeh[slot][vsKey]);
        }
        case 3: { gVeh[slot][vsLights] = !gVeh[slot][vsLights]; RW_Veh_Sync(v); }
        case 4: {
            if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return 0;
            if (!IsPlayerInVehicle(target, v) || GetPlayerVehicleSeat(target) <= 0) return RW_ControlNotify(playerid, "Пассажир уже вышел.");
            RemovePlayerFromVehicle(target);
        }
        default: return 0;
    }
    return RW_ControlNotify(playerid, "Готово.");
}
public RW_ControlAction(playerid, browserid, action, target)
{
    if (browserid != CONTROL_BROWSER || gControlMode[playerid] != 2) return 0;
    if (GetTickCount() - gControlTick[playerid] < 250) return 0;
    gControlTick[playerid] = GetTickCount();
    RW_ControlExecute(playerid, gControlVehicle[playerid], action, target);
    RW_ControlPublish(playerid);
    return 1;
}
public RW_ControlBind(playerid, browserid, door, engine, menu)
{
    if (browserid != CONTROL_BROWSER || gControlMode[playerid] != 1 || !gLogged[playerid]) return 0;
    if (door < 65 || door > 90 || engine < 65 || engine > 90 || menu < 65 || menu > 90 || door == engine || door == menu || engine == menu) return RW_ControlNotify(playerid, "Выберите разные клавиши A–Z.");
    if (door == 84 || engine == 84 || menu == 84) return RW_ControlNotify(playerid, "T занята чатом.");
    for (new i; i < 3; i++) CEF_EnableKey(playerid, gControlKeys[playerid][i], false);
    gControlKeys[playerid][0] = door; gControlKeys[playerid][1] = engine; gControlKeys[playerid][2] = menu;
    for (new i; i < 3; i++) CEF_EnableKey(playerid, gControlKeys[playerid][i], true);
    return RW_ControlNotify(playerid, "Клавиши сохранены на эту сессию.");
}
public OnCefPressKey(playerid, key, scancode, modifiers, bool:down, bool:repeat)
{
    if (CEF_IsChatInputOpen(playerid)) return 1;
    if (!down || repeat || modifiers || !gLogged[playerid] || gControlMode[playerid] != 0) return 1;
    if (GetTickCount() - gControlTick[playerid] < 250) return 1;
    gControlTick[playerid] = GetTickCount();
    if (key == gControlKeys[playerid][2]) return RW_ControlOpen(playerid, false);
    new v = RW_ControlTarget(playerid);
    if (key == gControlKeys[playerid][0]) RW_ControlExecute(playerid, v, 0);
    if (key == gControlKeys[playerid][1]) RW_ControlExecute(playerid, v, 1);
    return 1;
}
