#if defined _railway_interaction_included
    #endinput
#endif
#define _railway_interaction_included

stock RW_InteractionReset(playerid)
{
    gControlMode[playerid] = 0;
    gControlVehicle[playerid] = INVALID_VEHICLE_ID;
    gControlTick[playerid] = 0;
    gControlKeys[playerid][0] = KEY_CTRL_BACK;
    gControlKeys[playerid][1] = KEY_NO;
    gControlKeys[playerid][2] = KEY_YES;
}
stock RW_ControlNotify(playerid, const text[]) { return RW_UI_Notify(playerid, text); }
stock RW_KeyName(key, dest[], size)
{
    switch (key) {
        case KEY_YES: format(dest, size, "Y");
        case KEY_NO: format(dest, size, "N");
        case KEY_CTRL_BACK: format(dest, size, "H");
        case KEY_WALK: format(dest, size, "LALT");
        default: format(dest, size, "?");
    }
}
stock RW_ControlOpen(playerid, bool:settings)
{
    if (!gLogged[playerid]) return 1;
    if (settings) { gControlMode[playerid] = 1; return RW_UI_Settings(playerid); }
    new v = RW_ControlTarget(playerid);
    if (v <= 0 || v >= MAX_VEHICLES || v == INVALID_VEHICLE_ID || gVehSlotOf[v] < 0) return RW_ControlNotify(playerid, "Наведите центр экрана на машину не дальше 6 метров.");
    gControlVehicle[playerid] = v;
    gControlMode[playerid] = 2;
    return RW_UI_Vehicle(playerid);
}
stock RW_UI_Settings(playerid)
{
    RW_UI_Panel(playerid, UI_SETTINGS, "NASTROYKI", "Native SA-MP actions / click to bind");
    new label[64], key[16];
    for (new i; i < 3; i++) {
        RW_KeyName(gControlKeys[playerid][i], key, sizeof key);
        format(label, sizeof label, "%s: [%s]", i == 0 ? ("DVERI") : (i == 1 ? ("DVIGATEL") : ("MENU AVTO")), key);
        RW_UI_Row(playerid, i, label);
    }
    RW_UI_Row(playerid, 5, "NAZNACHENIYA NA SESSIYU");
    RW_UI_Row(playerid, 6, "SBROSIT: H / N / Y");
    return 1;
}
stock RW_UI_Vehicle(playerid)
{
    new v = gControlVehicle[playerid];
    if (v <= 0 || v >= MAX_VEHICLES || gVehSlotOf[v] < 0) return RW_UI_Close(playerid);
    new slot = gVehSlotOf[v], name[48];
    RW_VehicleName(GetVehicleModel(v), name, sizeof name);
    RW_UI_Panel(playerid, UI_VEHICLE, name, RW_IsVehicleOwner(playerid, v) ? ("VASH TRANSPORT") : ("CHUZHOY TRANSPORT"));
    RW_UI_Row(playerid, 0, gVeh[slot][vsDoors] ? ("ZAKRYT DVERI") : ("OTKRYT DVERI"));
    RW_UI_Row(playerid, 1, gVeh[slot][vsEngine] ? ("ZAGLUSHIT DVIGATEL") : ("ZAVESTI DVIGATEL"));
    RW_UI_Row(playerid, 2, gVeh[slot][vsKey] ? ("VYNUT KLYUCH") : ("VSTAVIT KLYUCH"));
    RW_UI_Row(playerid, 3, gVeh[slot][vsLights] ? ("VYKLYUCHIT FARY") : ("VKLYUCHIT FARY"));
    RW_UI_Row(playerid, 4, "PASSAZHIRY / VYSADIT");
    RW_UI_Row(playerid, 7, "OBNOVIT SOSTOYANIE");
    return 1;
}
stock RW_UI_Passengers(playerid)
{
    new v = gControlVehicle[playerid], count;
    RW_UI_Panel(playerid, UI_PASSENGERS, "PASSAZHIRY", "Click a player to remove");
    for (new j; j < 6; j++) gUITarget[playerid][j] = INVALID_PLAYER_ID;
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !IsPlayerInVehicle(i, v) || GetPlayerVehicleSeat(i) <= 0) continue;
        new index = count++ - gUIPage[playerid]*6;
        if (index < 0 || index >= 6) continue;
        gUITarget[playerid][index] = i;
        new nick[MAX_PLAYER_NAME], buf[64]; GetPlayerName(i, nick, sizeof nick);
        format(buf, sizeof buf, "%s [%d]", nick, i); RW_UI_Row(playerid, index, buf);
    }
    if (gUIPage[playerid] > 0 && count <= gUIPage[playerid]*6) {
        gUIPage[playerid] = 0; return RW_UI_Passengers(playerid);
    }
    gUIHasNext[playerid] = count > (gUIPage[playerid]+1)*6;
    if (!count) RW_UI_Row(playerid, 0, "PASSAZHIROV NET");
    RW_UI_Row(playerid, 6, "NAZAD K MASHINE");
    if (gUIHasNext[playerid]) RW_UI_Row(playerid, 7, "SLEDUYUSCHIE >>>");
    else if (gUIPage[playerid] > 0) RW_UI_Row(playerid, 7, "V NACHALO");
    return 1;
}
stock RW_UI_Click(playerid, row)
{
    if (!gLogged[playerid]) {
        if (gUIMode[playerid] == UI_AUTH && row == 0 && !gUIInput[playerid]) RW_ShowLoginDialog(playerid);
        return 1;
    }
    if (gUIInput[playerid]) return 1;
    new mode = gUIMode[playerid];
    if (mode == UI_SETTINGS) {
        if (row < 3) {
            gUIBind[playerid] = row;
            RW_UI_Panel(playerid, UI_BIND, "VYBOR KLAVISHI", "Existing assignment will be swapped");
            RW_UI_Row(playerid, 0, "Y"); RW_UI_Row(playerid, 1, "N");
            RW_UI_Row(playerid, 2, "H"); RW_UI_Row(playerid, 3, "LALT");
            RW_UI_Row(playerid, 7, "NAZAD");
        } else if (row == 6) { RW_InteractionReset(playerid); gControlMode[playerid] = 1; RW_UI_Settings(playerid); }
    } else if (mode == UI_BIND) {
        if (row < 4) {
            static const keys[] = { KEY_YES, KEY_NO, KEY_CTRL_BACK, KEY_WALK };
            new index = gUIBind[playerid], old = gControlKeys[playerid][index];
            for (new i; i < 3; i++) if (i != index && gControlKeys[playerid][i] == keys[row]) gControlKeys[playerid][i] = old;
            gControlKeys[playerid][index] = keys[row];
            RW_UI_Settings(playerid);
        } else if (row == 7) RW_UI_Settings(playerid);
    } else if (mode == UI_VEHICLE) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        if (row < 4) { RW_ControlExecute(playerid, gControlVehicle[playerid], row); RW_UI_Vehicle(playerid); }
        else if (row == 4) {
            if (!RW_IsVehicleOwner(playerid, gControlVehicle[playerid])) return RW_ControlNotify(playerid, "Не ваша машина.");
            gUIPage[playerid] = 0; RW_UI_Passengers(playerid);
        } else if (row == 7) RW_UI_Vehicle(playerid);
    } else if (mode == UI_PASSENGERS) {
        if (row < 6 && gUITarget[playerid][row] != INVALID_PLAYER_ID) {
            RW_ControlExecute(playerid, gControlVehicle[playerid], 4, gUITarget[playerid][row]); RW_UI_Passengers(playerid);
        } else if (row == 6) RW_UI_Vehicle(playerid);
        else if (row == 7) { if (gUIHasNext[playerid]) gUIPage[playerid]++; else gUIPage[playerid] = 0; RW_UI_Passengers(playerid); }
    } else if (mode == UI_MAP) {
        new bool:confirm = gMapPending[playerid] && row == 0;
        gMapPending[playerid] = false;
        RW_UI_Close(playerid);
        if (!confirm || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return 1;
        SetPlayerPosFindZ(playerid, gMapPoint[playerid][0], gMapPoint[playerid][1], gMapPoint[playerid][2]+1.0);
        SetCameraBehindPlayer(playerid);
        RW_ControlNotify(playerid, "Вы телепортировались к метке.");
    }
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
stock RW_EngineCommand(playerid)
{
    if (!IsPlayerInAnyVehicle(playerid)) return RW_ControlNotify(playerid, "Сядьте за руль.");
    return RW_ControlExecute(playerid, GetPlayerVehicleID(playerid), 1);
}
