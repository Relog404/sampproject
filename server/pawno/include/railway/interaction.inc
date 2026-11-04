#if defined _railway_interaction_included
    #endinput
#endif
#define _railway_interaction_included

#define CONTROL_BROWSER 3001
new bool:gControlCreated[MAX_PLAYERS], bool:gControlReady[MAX_PLAYERS];
new gControlMode[MAX_PLAYERS], gControlVehicle[MAX_PLAYERS];
new gControlKeys[MAX_PLAYERS][3], gControlTick[MAX_PLAYERS];
new gControlWaitTimer[MAX_PLAYERS], gControlLastKey[MAX_PLAYERS];
new bool:gControlOpened[MAX_PLAYERS];
forward RW_ControlTimeout(playerid, attempt);
forward RW_ControlOpened(playerid, browserid);
forward RW_ControlReady(playerid, browserid);
forward RW_ControlClose(playerid, browserid);
forward RW_ControlAction(playerid, browserid, action, target);
forward RW_ControlBind(playerid, browserid, door, engine, menu);
forward bool:RW_ControlValidKey(key);

stock RW_InteractionRegister()
{
    CEF_RegisterEvent("control:ready", "RW_ControlReady");
    CEF_RegisterEvent("control:opened", "RW_ControlOpened");
    CEF_RegisterEvent("control:close", "RW_ControlClose");
    CEF_RegisterEvent("control:action", "RW_ControlAction", Argument_Integer, Argument_Integer);
    CEF_RegisterEvent("control:bind", "RW_ControlBind", Argument_Integer, Argument_Integer, Argument_Integer);
}
stock RW_InteractionReset(playerid)
{
    RW_ControlCancelWait(playerid);
    gControlOpened[playerid] = false;
    gControlLastKey[playerid] = 0;
    gCefStage[playerid] = 0;
    gCefStarted[playerid] = GetTickCount();
    gCefPercent[playerid] = 0;
    gCefReason[playerid] = 0;
    gAuthPageReady[playerid] = false;
    gAuthBrowserCreated[playerid] = false;
    gControlCreated[playerid] = false;
    gControlReady[playerid] = false;
    gControlMode[playerid] = 0;
    gControlVehicle[playerid] = INVALID_VEHICLE_ID;
    gControlKeys[playerid][0] = 76;
    gControlKeys[playerid][1] = 221;
    gControlKeys[playerid][2] = 220;
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
        CEF_SetBrowserVisible(playerid, CONTROL_BROWSER, true);
    }
}
stock RW_ControlNotify(playerid, const text[])
{
    if (gControlReady[playerid]) CEF_EmitEvent(playerid, CONTROL_BROWSER, "control:toast", CEF_STR(text));
    else RW_Info(playerid, text);
    return 1;
}
// Camera ray against model-sized, vehicle-local bounds, not a tiny sphere.
// Only streamed managed vehicles within player reach can be selected.
stock RW_RayAxis(Float:origin, Float:direction, Float:halfSize, &Float:nearHit, &Float:farHit)
{
    if (floatabs(direction) < 0.0001) return floatabs(origin) <= halfSize;
    new Float:a = (-halfSize - origin) / direction;
    new Float:b = (halfSize - origin) / direction;
    if (a > b) { new Float:swap = a; a = b; b = swap; }
    if (a > nearHit) nearHit = a;
    if (b < farHit) farHit = b;
    return nearHit <= farHit;
}
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
        new Float:x, Float:y, Float:z, Float:angle, Float:sx, Float:sy, Float:sz;
        GetVehiclePos(v, x, y, z);
        if (!IsPlayerInRangeOfPoint(playerid, KEY_RANGE, x, y, z)) continue;
        GetVehicleZAngle(v, angle);
        GetVehicleModelInfo(GetVehicleModel(v), VEHICLE_MODEL_INFO_SIZE, sx, sy, sz);
        new Float:c = floatcos(angle, degrees), Float:sn = floatsin(angle, degrees);
        new Float:dx = cx-x, Float:dy = cy-y;
        new Float:nearHit = 0.0, Float:farHit = 1000.0;
        if (!RW_RayAxis(dx*c+dy*sn, fx*c+fy*sn, sx*0.5+0.25, nearHit, farHit)) continue;
        if (!RW_RayAxis(-dx*sn+dy*c, -fx*sn+fy*c, sy*0.5+0.25, nearHit, farHit)) continue;
        if (!RW_RayAxis(cz-z-0.25, fz, sz*0.5+0.35, nearHit, farHit)) continue;
        if (nearHit >= best) continue;
        best = nearHit; selected = v;
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
    if (!gLogged[playerid]) return 1;
    if (!gCefReady[playerid]) return RW_CefStatus(playerid);
    RW_InteractionInit(playerid);
    if (!settings) {
        new v = RW_ControlTarget(playerid);
        if (v == INVALID_VEHICLE_ID || v <= 0 || v >= MAX_VEHICLES || gVehSlotOf[v] < 0) return RW_ControlNotify(playerid, "Наведите центр экрана на машину не дальше 6 метров.");
        gControlVehicle[playerid] = v;
    }
    gControlMode[playerid] = settings ? 1 : 2;
    gControlOpened[playerid] = false;
    RW_ControlCancelWait(playerid);
    gControlWaitTimer[playerid] = SetTimerEx("RW_ControlTimeout", 8000, false, "ii", playerid, 0);
    if (gControlReady[playerid]) RW_ControlActivate(playerid);
    else RW_ControlNotify(playerid, "Открываем CEF-меню…");
    return 1;
}
public RW_ControlReady(playerid, browserid)
{
    if (browserid != CONTROL_BROWSER) return 0;
    gControlReady[playerid] = true;
    if (!gCefReady[playerid]) OnCefReady(playerid);
    CEF_EmitEvent(playerid, CONTROL_BROWSER, "railway:ack");
    if (gControlMode[playerid] != 0) RW_ControlActivate(playerid);
    return 1;
}
public RW_ControlClose(playerid, browserid)
{
    if (browserid != CONTROL_BROWSER) return 0;
    RW_ControlCancelWait(playerid);
    gControlOpened[playerid] = false;
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
    if (!RW_ControlValidKey(door) || !RW_ControlValidKey(engine) || !RW_ControlValidKey(menu) || door == engine || door == menu || engine == menu) return RW_ControlNotify(playerid, "Выберите разные клавиши A–Z, Ъ или обратный слеш.");
    if (door == 84 || engine == 84 || menu == 84) return RW_ControlNotify(playerid, "T занята чатом.");
    for (new i; i < 3; i++) CEF_EnableKey(playerid, gControlKeys[playerid][i], false);
    gControlKeys[playerid][0] = door; gControlKeys[playerid][1] = engine; gControlKeys[playerid][2] = menu;
    for (new i; i < 3; i++) CEF_EnableKey(playerid, gControlKeys[playerid][i], true);
    return RW_ControlNotify(playerid, "Клавиши сохранены на эту сессию.");
}
public OnCefPressKey(playerid, key, scancode, modifiers, bool:down, bool:repeat)
{
    if (down && !repeat) gControlLastKey[playerid] = key;
    if (CEF_IsChatInputOpen(playerid)) return 1;
    if (down && !repeat && key == gControlKeys[playerid][2] && gControlMode[playerid] != 0) return RW_ControlClose(playerid, CONTROL_BROWSER);
    // CEF flags: Shift=2, Ctrl=4, Alt=8. Lock/mouse flags are NOT modifiers to block.
    if (!down || repeat || (modifiers & 14) || !gLogged[playerid] || gControlMode[playerid] != 0) return 1;
    if (GetTickCount() - gControlTick[playerid] < 250) return 1;
    gControlTick[playerid] = GetTickCount();
    if (key == gControlKeys[playerid][2]) return RW_ControlOpen(playerid, false);
    new v = RW_ControlTarget(playerid);
    if (key == gControlKeys[playerid][0]) RW_ControlExecute(playerid, v, 0);
    if (key == gControlKeys[playerid][1]) RW_ControlExecute(playerid, v, 1);
    return 1;
}

stock RW_CefStatus(playerid)
{
    new stage[96], text[220];
    switch (gCefStage[playerid]) {
        case -1: format(stage, sizeof stage, "ошибка инициализации (код %d)", gCefReason[playerid]);
        case 0: format(stage, sizeof stage, "нет соединения CEF; проверьте cef.asi и UDP 7779");
        case 1: format(stage, sizeof stage, "соединение есть, ожидаются ресурсы");
        case 2: format(stage, sizeof stage, "скачивание ресурсов: %d%%", gCefPercent[playerid]);
        case 3: format(stage, sizeof stage, "ресурсы скачаны, ожидается OnCefReady");
        case 4: format(stage, sizeof stage, "CEF готов; меню JS=%d, вход JS=%d", _:gControlReady[playerid], _:gAuthPageReady[playerid]);
    }
    format(text, sizeof text, "CEF [%d сек]: %s", (GetTickCount()-gCefStarted[playerid])/1000, stage);
    SendClientMessage(playerid, COLOR_INFO, text);
    printf("[railway:cef] player=%d stage=%d elapsed=%dms percent=%d reason=%d controlJS=%d authJS=%d", playerid, gCefStage[playerid], GetTickCount()-gCefStarted[playerid], gCefPercent[playerid], gCefReason[playerid], _:gControlReady[playerid], _:gAuthPageReady[playerid]);
    format(text, sizeof text, "CEF menu: mode=%d page=%d opened=%d lastVK=%d; /vmenu", gControlMode[playerid], _:gControlReady[playerid], _:gControlOpened[playerid], gControlLastKey[playerid]);
    SendClientMessage(playerid, COLOR_INFO, text);
    return 1;
}
stock RW_EngineCommand(playerid)
{
    if (!IsPlayerInAnyVehicle(playerid)) return RW_ControlNotify(playerid, "Сядьте за руль.");
    return RW_ControlExecute(playerid, GetPlayerVehicleID(playerid), 1);
}

stock bool:RW_ControlValidKey(key)
{
    return (key >= 65 && key <= 90 && key != 84) || key == 220 || key == 221;
}
stock RW_ControlActivate(playerid)
{
    CEF_SetBrowserVisible(playerid, CONTROL_BROWSER, true);
    CEF_SetBrowserLayer(playerid, CONTROL_BROWSER, 30);
    CEF_FocusBrowser(playerid, CONTROL_BROWSER, true);
    CEF_ToggleChatInput(playerid, false);
    CEF_SetEscapeMenuMode(playerid, CEF_ESCAPE_MENU_DISABLED);
    return RW_ControlPublish(playerid);
}

stock RW_ControlCancelWait(playerid)
{
    if (gControlWaitTimer[playerid] != 0) KillTimer(gControlWaitTimer[playerid]);
    gControlWaitTimer[playerid] = 0;
}
public RW_ControlOpened(playerid, browserid)
{
    if (browserid != CONTROL_BROWSER || !gLogged[playerid] || gControlMode[playerid] == 0) return 0;
    gControlOpened[playerid] = true;
    RW_ControlCancelWait(playerid);
    return 1;
}
public RW_ControlTimeout(playerid, attempt)
{
    gControlWaitTimer[playerid] = 0;
    if (!IsPlayerConnected(playerid) || !gLogged[playerid] || gControlMode[playerid] == 0 || gControlOpened[playerid]) return 1;
    printf("[railway:cef] control open timeout player=%d page=%d attempt=%d", playerid, _:gControlReady[playerid], attempt);
    if (attempt >= 2) {
        RW_ControlClose(playerid, CONTROL_BROWSER);
        return RW_ControlNotify(playerid, "CEF-меню не ответило. Управление восстановлено; /cefstatus — диагностика.");
    }
    if (gControlReady[playerid]) RW_ControlActivate(playerid);
    else if (gControlCreated[playerid]) CEF_ReloadBrowser(playerid, CONTROL_BROWSER, true);
    else RW_InteractionInit(playerid);
    gControlWaitTimer[playerid] = SetTimerEx("RW_ControlTimeout", 8000, false, "ii", playerid, attempt+1);
    return 1;
}
