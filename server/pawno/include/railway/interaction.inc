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
    gSettingsMode[playerid] = 0;
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
// Y has a context: a car under the crosshair, then a player nearby, then
// yourself (the phone). Same key as /vmenu.
stock RW_ControlOpen(playerid, bool:settings)
{
    if (!gLogged[playerid]) return 1;
    if (settings) { gControlMode[playerid] = 1; return RW_SettingsOpen(playerid); }
    new v = RW_ControlTarget(playerid);
    if (v <= 0 || v >= MAX_VEHICLES || v == INVALID_VEHICLE_ID || gVehSlotOf[v] < 0) {
        new target = RW_PlayerTarget(playerid);
        if (target >= 0) return RW_PlayerMenu(playerid, target);
        return RW_SelfMenu(playerid);
    }
    gControlVehicle[playerid] = v;
    gControlMode[playerid] = 2;
    return RW_UI_Vehicle(playerid);
}
stock RW_SelfMenu(playerid)
{
    if (RW_GovNear(playerid)) return RW_GovOpen(playerid);
    /* На службе Y на себя открывает планшет, а не телефон. */
    if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0) return RW_MdtOpen(playerid);
    if (RW_EmsOnDuty(playerid) && RW_EmsItem(playerid, ITEM_MEDKIT) >= 0) return RW_MdtOpen(playerid);
    /* На работе Y на точке сдачи сдаёт задание, а не достаёт телефон. */
    if (RW_JobHandOver(playerid)) return 1;
    /* На стоянке такси Y вызывает машину. */
    if (RW_TaxiRankCall(playerid)) return 1;
    /* На штрафстоянке Y открывает список: выкуп машины по строке, а не по
       токену из чата. Сюда же попадает механик на службе. */
    if (RW_ImpoundNear(playerid) && RW_ImpoundMayOpen(playerid)) return RW_ImpoundOpen(playerid);
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You have no phone. Buy one at a 24/7 shop.");
    if (gPhoneNum[playerid] <= 0) {
        if (RW_CefOn(playerid)) return RW_Error(playerid, "No SIM card: buy one at a 24/7 shop and insert it from your pockets (Alt + U).");
        return RW_Error(playerid, "No SIM card: buy one and insert it with /sim.");
    }
    return RW_PhoneOpen(playerid);
}
stock RW_PlayerTarget(playerid)
{
    new Float:x, Float:y, Float:z, best = -1;
    new Float:bestDist = 3.5;
    GetPlayerPos(playerid, x, y, z);
    for (new i; i < MAX_PLAYERS; i++) {
        if (i == playerid || !IsPlayerConnected(i) || !gLogged[i]) continue;
        if (GetPlayerState(i) == PLAYER_STATE_SPECTATING) continue;
        new Float:dist = GetPlayerDistanceFromPoint(i, x, y, z);
        if (dist > bestDist) continue;
        bestDist = dist; best = i;
    }
    return best;
}
// Y on a player: call him if his number is in your contacts, otherwise swap
// numbers first.
stock RW_PlayerMenu(playerid, target)
{
    if (target < 0 || !IsPlayerConnected(target)) return RW_Error(playerid, "Stand closer to the player.");
    /* Полицейский на службе — действия; адвокат у задержанного — дело. */
    if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0) return RW_PoliceMenuOpen(playerid, target);
    if (RW_IsLawyer(playerid) && gJailTime[target] > 0) return RW_PoliceCaseStart(playerid, target);
    /* Y на задержанном: вызвать ему адвоката. */
    if (gJailTime[target] > 0) return RW_LawyerCallFor(playerid, target);
    /* Y на свободном игроке: адвокат заключает договор на случай ареста. */
    if (RW_IsLawyer(playerid) && gJailTime[target] <= 0) return RW_ContractOffer(playerid, target);
    /* Упавшего игрока лечат; медик на службе лечит и просто прохожих. */
    if (gDowned[target] || (RW_EmsOnDuty(playerid) && RW_EmsItem(playerid, ITEM_MEDKIT) >= 0)) return RW_EmsMenuOpen(playerid, target);
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "You need a phone with a SIM card.");
    if (RW_PhoneHasItem(target) <= 0 || gPhoneNum[target] <= 0) return RW_Error(playerid, "He has no phone with him.");
    if (RW_PhoneKnows(playerid, gPhoneNum[target])) return RW_PhoneCall(playerid, gPhoneNum[target]);
    new nick[MAX_PLAYER_NAME], mynick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, mynick);
    RW_GetNick(target, nick);
    RW_PhoneContactAdd(playerid, nick, gPhoneNum[target]);
    RW_PhoneContactAdd(target, mynick, gPhoneNum[playerid]);
    format(message, sizeof message, "Numbers swapped with %s. Y on him again to call.", nick);
    RW_Ok(playerid, message);
    format(message, sizeof message, "%s saved your number %d.", mynick, gPhoneNum[playerid]);
    return RW_Ok(target, message);
}
/* Открытие панели всегда с первого экрана: иначе после закрытия на экране
   настроек игрок вернулся бы не туда, откуда пришёл. */
stock RW_SettingsOpen(playerid)
{
    gSettingsMode[playerid] = 0;
    return RW_UI_Settings(playerid);
}
/* Строка переключателя: названием и тем, что будет после нажатия. */
stock RW_SettingsRow(playerid, row, const text[], flag, const icon[])
{
    new label[72];
    format(label, sizeof label, "%s: %s", text, RW_SettingsOn(playerid, flag) ? ("on") : ("off"));
    RW_UI_Row(playerid, row, label, icon);
    return 1;
}
stock RW_UI_Settings(playerid)
{
    if (gSettingsMode[playerid]) {
        RW_UI_Panel(playerid, UI_SETTINGS, "Settings", "What the screen shows: press a row to switch");
        RW_SettingsRow(playerid, 0, "Key hints", SET_HINTS, "info");
        RW_SettingsRow(playerid, 1, "Speedometer", SET_SPEEDO, "car");
        RW_SettingsRow(playerid, 2, "Player names", SET_NAMES, "users");
        RW_SettingsRow(playerid, 3, "Player markers", SET_MARKERS, "map");
        RW_SettingsRow(playerid, 4, "Welcome message", SET_WELCOME, "check");
        RW_UI_Row(playerid, 6, "Back: keys", "back");
        RW_UI_Row(playerid, 7, "Close", "close");
        if (RW_CefOn(playerid)) RW_CefAction(playerid, 8, "Close", "danger");
        return 1;
    }
    RW_UI_Panel(playerid, UI_SETTINGS, "Settings", "Select an action to change its key");
    new label[64], key[16];
    for (new i; i < 3; i++) {
        RW_KeyName(gControlKeys[playerid][i], key, sizeof key);
        format(label, sizeof label, "%s: [%s]", i == 0 ? ("Doors") : (i == 1 ? ("Engine") : ("Car menu")), key);
        RW_UI_Row(playerid, i, label, i == 0 ? ("door") : (i == 1 ? ("engine") : ("car")));
    }
    RW_UI_Row(playerid, 3, RW_HudIsOn(playerid) ? ("HUD: on (press to hide)") : ("HUD: off (press to show)"), "eye");
    RW_UI_Row(playerid, 4, RW_HudMouseIsOn(playerid) ? ("Mouse: on (press to take the cursor away)") : ("Mouse: off (press to take the cursor)"), "cursor");
    RW_UI_Row(playerid, 5, "Screen: hints, speedo, names", "eye");
    RW_UI_Row(playerid, 6, "Reset: H / N / Y", "refresh");
    RW_UI_Row(playerid, 7, "Close", "close");
    if (RW_CefOn(playerid)) RW_CefAction(playerid, 8, "Close", "danger");
    return 1;
}
stock RW_UI_Vehicle(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefVehicle(playerid);
    #endif
    new v = gControlVehicle[playerid];
    if (v <= 0 || v >= MAX_VEHICLES || gVehSlotOf[v] < 0) return RW_UI_Close(playerid);
    new slot = gVehSlotOf[v], name[48];
    RW_VehicleName(GetVehicleModel(v), name, sizeof name);
    RW_UI_Panel(playerid, UI_VEHICLE, name, RW_IsVehicleOwner(playerid, v) ? ("Your car") : ("Another player's car"));
    RW_UI_Row(playerid, 0, gVeh[slot][vsDoors] ? ("Lock doors") : ("Unlock doors"));
    RW_UI_Row(playerid, 1, gVeh[slot][vsEngine] ? ("Stop engine") : ("Start engine"));
    RW_UI_Row(playerid, 2, gVeh[slot][vsKey] ? ("Remove key") : ("Insert key"));
    RW_UI_Row(playerid, 3, gVeh[slot][vsLights] ? ("Lights off") : ("Lights on"));
    RW_UI_Row(playerid, 4, "Passengers / eject");
    new transLabel[64];
    if (RW_TransportRowLabel(playerid, v, transLabel, sizeof transLabel)) RW_UI_Row(playerid, 6, transLabel);
    if (RW_CabinIsPlane(GetVehicleModel(v))) {
        new label[64];
        format(label, sizeof label, "%s (%d / %d)", RW_CabinInside(playerid) ? ("Leave cabin") : ("Board cabin"), RW_CabinCount(v), CABIN_MAX);
        RW_UI_Row(playerid, 5, label);
    } else if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_UI_Row(playerid, 5, "Refuel: a full tank for cash", "fuel");
    RW_UI_Row(playerid, 7, "Refresh status");
    return 1;
}
stock RW_UI_Passengers(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefPassengers(playerid);
    #endif
    new v = gControlVehicle[playerid], count;
    RW_UI_Panel(playerid, UI_PASSENGERS, "Passengers", "Select a passenger to eject");
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
    if (!count) RW_UI_Row(playerid, 0, "No passengers");
    RW_UI_Row(playerid, 6, "Back to car");
    if (gUIHasNext[playerid]) RW_UI_Row(playerid, 7, "Next page >");
    else if (gUIPage[playerid] > 0) RW_UI_Row(playerid, 7, "First page");
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
        /* Кнопка «Закрыть» (8) приходит и из браузера: панель закрывается,
           а экран настроек сбрасывается на первый. */
        if (row == 8) { gSettingsMode[playerid] = 0; return RW_UI_Close(playerid); }
        if (gSettingsMode[playerid]) {
            if (row == 6) { gSettingsMode[playerid] = 0; return RW_UI_Settings(playerid); }
            if (row == 7) { gSettingsMode[playerid] = 0; return RW_UI_Close(playerid); }
            /* Тот же переключатель, что и /set <имя>, только по строке. */
            if (row == 0) RW_SettingsFlip(playerid, SET_HINTS);
            else if (row == 1) RW_SettingsFlip(playerid, SET_SPEEDO);
            else if (row == 2) RW_SettingsFlip(playerid, SET_NAMES);
            else if (row == 3) RW_SettingsFlip(playerid, SET_MARKERS);
            else if (row == 4) RW_SettingsFlip(playerid, SET_WELCOME);
            return RW_UI_Settings(playerid);
        }
        if (row < 3) {
            gUIBind[playerid] = row;
            RW_UI_Panel(playerid, UI_BIND, "Choose key", "Assigned keys will swap");
            RW_UI_Row(playerid, 0, "Y", "key"); RW_UI_Row(playerid, 1, "N", "key");
            RW_UI_Row(playerid, 2, "H", "key"); RW_UI_Row(playerid, 3, "LALT", "key");
            RW_UI_Row(playerid, 7, "Back");
            if (RW_CefOn(playerid)) RW_CefAction(playerid, 8, "Close", "danger");
        } else if (row == 3) { RW_HudToggle(playerid, true); RW_UI_Settings(playerid); }
        else if (row == 4) { RW_HudMouseToggle(playerid); RW_UI_Settings(playerid); }
        else if (row == 5) { gSettingsMode[playerid] = 1; RW_UI_Settings(playerid); }
        else if (row == 6) { RW_InteractionReset(playerid); gControlMode[playerid] = 1; RW_UI_Settings(playerid); }
        else if (row == 7) { gSettingsMode[playerid] = 0; return RW_UI_Close(playerid); }
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
            if (!RW_IsVehicleOwner(playerid, gControlVehicle[playerid])) return RW_ControlNotify(playerid, "You do not own this car.");
            gUIPage[playerid] = 0; RW_UI_Passengers(playerid);
        } else if (row == 5) {
            new cv = gControlVehicle[playerid];
            if (RW_CabinIsPlane(GetVehicleModel(cv))) {
                if (RW_CabinInside(playerid) || GetPlayerVehicleID(playerid) == cv) RW_CabinLeave(playerid);
                else RW_CabinBoard(playerid, cv);
                RW_UI_Vehicle(playerid);
            } else if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) { RW_FuelRefuel(playerid, cv); RW_UI_Vehicle(playerid); }
        } else if (row == 6) return RW_TransportRow(playerid, gControlVehicle[playerid]);
        else if (row == 7) RW_UI_Vehicle(playerid);
    } else if (mode == UI_PASSENGERS) {
        if (row < 6 && gUITarget[playerid][row] != INVALID_PLAYER_ID) {
            RW_ControlExecute(playerid, gControlVehicle[playerid], 4, gUITarget[playerid][row]); RW_UI_Passengers(playerid);
        } else if (row == 6) RW_UI_Vehicle(playerid);
        else if (row == 7) { if (gUIHasNext[playerid]) gUIPage[playerid]++; else gUIPage[playerid] = 0; RW_UI_Passengers(playerid); }
    } else if (mode == UI_GARAGE) {
        return RW_GarageCefClick(playerid, row);
    } else if (mode == UI_KEYMENU) {
        return RW_KeyMenuCefClick(playerid, row);
    } else if (mode == UI_DOWNED) {
        return RW_MedCefClick(playerid, row);
    } else if (mode == UI_POLICE) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_PoliceClick(playerid, row);
    } else if (mode == UI_MDT) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_MdtClick(playerid, row);
    } else if (mode == UI_EMS) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_EmsClick(playerid, row);
    } else if (mode == UI_TRUNK) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_TrunkClick(playerid, row);
    } else if (mode == UI_IMPOUND) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_ImpoundClick(playerid, row);
    } else if (mode == UI_LOCKPICK) {
        return RW_PickClick(playerid, row, false);
    } else if (mode == UI_SERVICE) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_ServiceClick(playerid, row);
    } else if (mode == UI_SHOP) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_ShopClick(playerid, row);
    } else if (mode == UI_BANK) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_BankClick(playerid, row);
    } else if (mode == UI_JOB) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_JobClick(playerid, row);
    } else if (mode == UI_PROPERTY) {
        return RW_PropClick(playerid, row);
    } else if (mode == UI_MARKET) {
        return RW_MarketClick(playerid, row);
    } else if (mode == UI_CRIME) {
        return RW_CrimeClick(playerid, row);
    } else if (mode == UI_FAMILY) {
        return RW_FamilyClick(playerid, row);
    } else if (mode == UI_GOV) {
        return RW_GovClick(playerid, row);
    } else if (mode == UI_GPS) {
        return RW_GpsAction(playerid, row, false);
    } else if (mode == UI_DONATE) {
        return RW_DonateClick(playerid, row);
    } else if (mode == UI_STORY) {
        return RW_StoryClick(playerid, row);
    } else if (mode == UI_PHONE_APP) {
        return RW_PhoneAppClick(playerid, row);
    } else if (mode == UI_PHONE || mode == UI_PHONE_CONT) {
        if (GetTickCount()-gControlTick[playerid] < 250) return 1;
        gControlTick[playerid] = GetTickCount();
        return RW_PhoneClick(playerid, row, mode);
    } else if (mode == UI_MAP) {
        new bool:confirm = gMapPending[playerid] && gMapHasPoint[playerid] && row == 0;
        gMapPending[playerid] = false;
        RW_UI_Close(playerid);
        if (!confirm || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return 1;
        SetPlayerPosFindZ(playerid, gMapPoint[playerid][0], gMapPoint[playerid][1], gMapPoint[playerid][2]+1.0);
        SetCameraBehindPlayer(playerid);
        RW_ControlNotify(playerid, "Teleported to your marker.");
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
    if (GetVehicleVirtualWorld(v) != GetPlayerVirtualWorld(playerid) || GetPlayerInterior(playerid) != 0 || !IsPlayerInRangeOfPoint(playerid, KEY_RANGE, x, y, z)) return RW_ControlNotify(playerid, "Car is too far away.");
    if ((action == 0 || action == 2) && !RW_InvHasKey(playerid, v)) return RW_ControlNotify(playerid, "You need this car's key in your inventory.");
    if ((action == 3 || action == 4) && !RW_IsVehicleOwner(playerid, v)) return RW_ControlNotify(playerid, "You do not own this car.");
    switch (action) {
        case 0: RW_Veh_SetDoors(v, !gVeh[slot][vsDoors]);
        case 1: {
            if (!IsPlayerInVehicle(playerid, v) || GetPlayerVehicleSeat(playerid) != 0) return RW_ControlNotify(playerid, "Sit in the driver seat.");
            if (!gVeh[slot][vsKey]) return RW_ControlNotify(playerid, "Insert the key first.");
            gVeh[slot][vsEngine] = !gVeh[slot][vsEngine];
            RW_Veh_Sync(v);
        }
        case 2: {
            if (!IsPlayerInVehicle(playerid, v) || GetPlayerVehicleSeat(playerid) != 0) return RW_ControlNotify(playerid, "Sit in the driver seat.");
            RW_Veh_SetKey(v, !gVeh[slot][vsKey]);
        }
        case 3: { gVeh[slot][vsLights] = !gVeh[slot][vsLights]; RW_Veh_Sync(v); }
        case 4: {
            if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return 0;
            if (!IsPlayerInVehicle(target, v) || GetPlayerVehicleSeat(target) <= 0) return RW_ControlNotify(playerid, "Passenger has already left.");
            RemovePlayerFromVehicle(target);
        }
        default: return 0;
    }
    return RW_ControlNotify(playerid, "Done.");
}
stock RW_EngineCommand(playerid)
{
    if (!IsPlayerInAnyVehicle(playerid)) return RW_ControlNotify(playerid, "Sit in the driver seat.");
    return RW_ControlExecute(playerid, GetPlayerVehicleID(playerid), 1);
}

stock RW_MapPrompt(playerid)
{
    if (!gLogged[playerid]) return 1;
    if (!gMapHasPoint[playerid]) return RW_ControlNotify(playerid, "Set a marker first: ESC > Map > right click.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return RW_ControlNotify(playerid, "Teleport is available on foot in the main world only.");
    if (gUIMode[playerid] != 0 && gUIMode[playerid] != UI_MAP) return RW_ControlNotify(playerid, "Marker saved. Close the menu and use /tpm.");
    new coords[64]; format(coords, sizeof coords, "Marker: %.0f, %.0f", gMapPoint[playerid][0], gMapPoint[playerid][1]);
    gMapPending[playerid] = true;
    RW_UI_Panel(playerid, UI_MAP, "Teleport to marker?", coords);
    RW_UI_Row(playerid, 0, "Yes, teleport", "map");
    RW_UI_Row(playerid, 1, "No, stay here", "close");
    if (RW_CefOn(playerid)) RW_CefAction(playerid, 8, "Close", "danger");
    return 1;
}
/* Переключать нечего: панели рисуются в браузере, без него — русские диалоги. */
stock RW_UI_Language(playerid)
{
    if (gUIInput[playerid]) return RW_ControlNotify(playerid, "Сначала закончите ввод пароля.");
    gUIRussianDialogs[playerid] = true;
    RW_UI_Close(playerid);
    if (!gLogged[playerid]) return RW_UI_OpenAuth(playerid);
    return RW_ControlNotify(playerid, "Меню открываются в браузере; без него — русские окна SA-MP.");
}

#if RAILWAY_CEF
/*  ============================================================================
 *  Browser card of the car (omp-cef). Same actions as the native dialog.
 *  ========================================================================= */
stock RW_CefVehicle(playerid)
{
    new v = gControlVehicle[playerid];
    if (v <= 0 || v >= MAX_VEHICLES || gVehSlotOf[v] < 0) return RW_UI_Close(playerid);
    new slot = gVehSlotOf[v], name[48];
    RW_VehicleName(GetVehicleModel(v), name, sizeof name);
    if (!RW_CefOpen(playerid, CEF_MODE_VEHICLE, name, RW_IsVehicleOwner(playerid, v) ? ("Your car") : ("Another player's car"))) return RW_UI_Vehicle(playerid);
    RW_CefRow(playerid, 0, gVeh[slot][vsDoors] ? ("Unlock doors") : ("Lock doors"), gVeh[slot][vsDoors] ? ("open") : ("locked"), "door", "", gVeh[slot][vsDoors] ? ("state-on") : ("state-off"));
    RW_CefRow(playerid, 1, gVeh[slot][vsEngine] ? ("Stop engine") : ("Start engine"), gVeh[slot][vsEngine] ? ("running") : ("off"), "engine", "", gVeh[slot][vsEngine] ? ("state-on") : ("state-off"));
    RW_CefRow(playerid, 2, gVeh[slot][vsKey] ? ("Remove key") : ("Insert key"), gVeh[slot][vsKey] ? ("inside") : ("no key"), "key", "", gVeh[slot][vsKey] ? ("state-on") : ("state-off"));
    RW_CefRow(playerid, 3, gVeh[slot][vsLights] ? ("Lights off") : ("Lights on"), gVeh[slot][vsLights] ? ("on") : ("off"), "light", "", gVeh[slot][vsLights] ? ("state-on") : ("state-off"));
    if (RW_IsVehicleOwner(playerid, v)) RW_CefRow(playerid, 4, "Passengers", "eject", "users");
    new transLabel[64];
    if (RW_TransportRowLabel(playerid, v, transLabel, sizeof transLabel)) RW_CefRow(playerid, 6, transLabel, "", "bag");
    if (RW_CabinIsPlane(GetVehicleModel(v))) {
        new label[64];
        format(label, sizeof label, "%s (%d / %d)", RW_CabinInside(playerid) ? ("Leave cabin") : ("Board cabin"), RW_CabinCount(v), CABIN_MAX);
        RW_CefRow(playerid, 5, label, "", "plane");
    } else if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_CefRow(playerid, 5, "Refuel: a full tank for cash", "gas", "fuel");
    RW_CefPreview(playerid, GetVehicleModel(v), name);
    RW_CefAction(playerid, 7, "Refresh", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_CefVehicleClick(playerid, id)
{
    new v = gControlVehicle[playerid];
    if (v <= 0 || v >= MAX_VEHICLES || gVehSlotOf[v] < 0) return RW_UI_Close(playerid);
    if (id == 8 || id == -1) return RW_UI_Close(playerid);
    if (id < 4) { RW_ControlExecute(playerid, v, id); return RW_CefVehicle(playerid); }
    if (id == 4) {
        if (!RW_IsVehicleOwner(playerid, v)) return RW_Error(playerid, "You do not own this car.");
        gUIPage[playerid] = 0; return RW_CefPassengers(playerid);
    }
    if (id == 5) {
        if (RW_CabinIsPlane(GetVehicleModel(v))) {
            if (RW_CabinInside(playerid) || GetPlayerVehicleID(playerid) == v) RW_CabinLeave(playerid);
            else RW_CabinBoard(playerid, v);
            return RW_CefVehicle(playerid);
        }
        if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_FuelRefuel(playerid, v);
        return RW_CefVehicle(playerid);
    }
    if (id == 6) return RW_TransportRow(playerid, v);
    return RW_CefVehicle(playerid);
}
stock RW_CefPassengers(playerid)
{
    new v = gControlVehicle[playerid], count;
    if (!RW_CefOpen(playerid, CEF_MODE_PASSENGERS, "Passengers", "Select a passenger to eject")) return RW_UI_Passengers(playerid);
    for (new j; j < 6; j++) gUITarget[playerid][j] = INVALID_PLAYER_ID;
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !IsPlayerInVehicle(i, v) || GetPlayerVehicleSeat(i) <= 0) continue;
        new index = count++ - gUIPage[playerid]*6;
        if (index < 0 || index >= 6) continue;
        gUITarget[playerid][index] = i;
        new nick[MAX_PLAYER_NAME], buf[64];
        GetPlayerName(i, nick, sizeof nick);
        format(buf, sizeof buf, "%s [%d]", nick, i);
        RW_CefRow(playerid, index, buf, "eject", "eject");
    }
    if (gUIPage[playerid] > 0 && count <= gUIPage[playerid]*6) { gUIPage[playerid] = 0; return RW_CefPassengers(playerid); }
    gUIHasNext[playerid] = count > (gUIPage[playerid]+1)*6;
    if (!count) RW_CefRow(playerid, 9, "No passengers", "", "users");
    RW_CefAction(playerid, 6, "Back to car", "");
    if (gUIHasNext[playerid]) RW_CefAction(playerid, 7, "Next page", "");
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_CefPassengerClick(playerid, id)
{
    if (id == 8) return RW_UI_Close(playerid);
    if (id == 6 || id == -1) return RW_CefVehicle(playerid);
    if (id == 7) { if (gUIHasNext[playerid]) gUIPage[playerid]++; else gUIPage[playerid] = 0; return RW_CefPassengers(playerid); }
    if (id < 6 && gUITarget[playerid][id] != INVALID_PLAYER_ID) {
        RW_ControlExecute(playerid, gControlVehicle[playerid], 4, gUITarget[playerid][id]);
        return RW_CefPassengers(playerid);
    }
    return 1;
}
#endif
