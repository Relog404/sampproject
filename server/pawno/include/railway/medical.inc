/*  ============================================================================
 *  railway/medical.inc
 *  Bandages, the adrenaline syringe and the downed state after death.
 *  A dead player lies where they fell, can wait longer, be revived with
 *  adrenaline, and only respawns when the bleeding-out timer runs out.
 *  Medics are a placeholder: the button exists but the service is not open yet.
 *  ========================================================================= */

#if defined _railway_medical_included
    #endinput
#endif
#define _railway_medical_included

#include <railway/core>

#define DOWN_MS        (180000)   /* 3 minutes before bleeding out          */
#define DOWN_EXTRA_MS  (420000)   /* +7 minutes from the menu               */
#define DOWN_MAX_MS    (1800000)  /* 30 minutes in total                    */
#define MED_USE_MS     (3000)     /* bandage / syringe use time             */
#define MED_COOLDOWN   (1000)     /* 1 second between bandages              */
#define BANDAGE_HP     (15)
#define REVIVE_HP      (30.0)
#define DOWN_RADIUS    (3.0)

forward RW_MedRespawn(playerid, stamp);
forward RW_MedBleedOut(playerid, stamp);
forward RW_MedBandageDone(playerid);
forward RW_MedSyringeDone(playerid, target);

stock bool:RW_MedDowned(playerid) { return playerid >= 0 && playerid < MAX_PLAYERS && gDowned[playerid]; }

/* Экран раненого рисует страница: скрывать больше нечего. */
stock RW_MedHide(playerid)
{
    #pragma unused playerid
    return 1;
}
stock RW_MedReset(playerid)
{
    RW_MedHide(playerid);
    gDowned[playerid] = false; gDownEnd[playerid] = 0;
    gMedBusy[playerid] = false; gMedCooldown[playerid] = 0; gMedTick[playerid] = 0;
    return 1;
}
/* Экран раненого: общая панель — в браузере, без него нативный диалог. */
stock RW_MedOpen(playerid)
{
    if (!gDowned[playerid] || gMedBusy[playerid]) return 1;
    if (gUIInput[playerid] || gInvFitting[playerid]) return 1;
    RW_UI_Close(playerid, true);
    gUIMode[playerid] = UI_DOWNED;
    return RW_MedCef(playerid);
}
/* Текст таймера один для диалога и для страницы. */
stock RW_MedTimeLeft(playerid, dest[], size = sizeof dest)
{
    new left = gDownEnd[playerid] - GetTickCount();
    if (left < 0) left = 0;
    format(dest, size, "%d:%02d", left/60000, (left/1000)%60);
    return left;
}
/* Скорая: один вызов для обоих меню. */
stock RW_MedCall(playerid)
{
    new id = RW_EmsCall(playerid, 3, gDownX[playerid], gDownY[playerid], gDownZ[playerid]);
    if (!id) return RW_Error(playerid, "The call did not go through. Try again in a second.");
    return RW_Ok(playerid, "The ambulance is on its way. Lie still: /calls shows your call to the medics.");
}
/* Подождать ещё семь минут. */
stock RW_MedWait(playerid)
{
    if (gDownEnd[playerid] - GetTickCount() + DOWN_EXTRA_MS > DOWN_MAX_MS) return RW_Error(playerid, "You cannot wait any longer.");
    gDownEnd[playerid] += DOWN_EXTRA_MS;
    RW_MedRender(playerid);
    return RW_Ok(playerid, "You hold on for 7 more minutes.");
}
/* Страница раненого: две кнопки и таймер кровотечения. */
stock RW_MedCef(playerid)
{
    new time[16], subtitle[64];
    RW_MedTimeLeft(playerid, time, sizeof time);
    format(subtitle, sizeof subtitle, "Кровотечение: %s", time);
    RW_UI_Panel(playerid, UI_DOWNED, "Вы ранены", subtitle);
    RW_UI_Row(playerid, 0, "Вызвать скорую");
    RW_UI_Row(playerid, 1, "Подождать ещё 7 минут");
    return 1;
}
stock RW_MedCefClick(playerid, id)
{
    if (gUIMode[playerid] != UI_DOWNED) return 0;
    if (GetTickCount() - gMedTick[playerid] < 400) return 1;
    gMedTick[playerid] = GetTickCount();
    if (id == 0) return RW_MedCall(playerid);
    if (id == 1) return RW_MedWait(playerid);
    return 1;
}
/* Таймер кровотечения обновляем только на странице: нативный диалог
   нельзя перерисовывать каждую секунду — он перезапускался бы. */
stock RW_MedRender(playerid)
{
    #if RAILWAY_CEF
        if (gUIMode[playerid] == UI_DOWNED && RW_CefOn(playerid)) return RW_MedCef(playerid);
    #endif
    return 1;
}
// Keeps the player lying at the place of death instead of respawning at once.
stock RW_MedDown(playerid, Float:x, Float:y, Float:z)
{
    if (!gLogged[playerid]) return 1;
    gDowned[playerid] = true;
    gDownX[playerid] = x; gDownY[playerid] = y; gDownZ[playerid] = z;
    gDownEnd[playerid] = GetTickCount() + DOWN_MS;
    gDownStamp[playerid]++;
    gMedBusy[playerid] = false;
    RW_UI_Close(playerid);
    SetTimerEx("RW_MedRespawn", 1200, false, "ii", playerid, gDownStamp[playerid]);
    SetTimerEx("RW_MedBleedOut", DOWN_MS, false, "ii", playerid, gDownStamp[playerid]);
    return RW_Error(playerid, "You are downed. Lie still, wait, or ask for adrenaline.");
}
public RW_MedRespawn(playerid, stamp)
{
    if (!IsPlayerConnected(playerid) || !gDowned[playerid] || stamp != gDownStamp[playerid]) return 1;
    if (GetPlayerState(playerid) == PLAYER_STATE_SPAWNED) return 1;
    SpawnPlayer(playerid);
    return 1;
}
public RW_MedBleedOut(playerid, stamp)
{
    if (!IsPlayerConnected(playerid) || !gDowned[playerid] || stamp != gDownStamp[playerid]) return 1;
    RW_MedClearDown(playerid);
    RW_Error(playerid, "You blacked out. Waking up at Unity Station.");
    SpawnPlayer(playerid);
    return 1;
}
stock RW_MedClearDown(playerid)
{
    gDowned[playerid] = false;
    gDownStamp[playerid]++;
    RW_UI_Close(playerid);
    RW_MedHide(playerid);
    ClearAnimations(playerid, 1);
    TogglePlayerControllable(playerid, true);
    return 1;
}
// Called from OnPlayerSpawn: keeps a downed player where they fell.
stock RW_MedSpawn(playerid)
{
    if (!gDowned[playerid]) return 0;
    SetPlayerPos(playerid, gDownX[playerid], gDownY[playerid], gDownZ[playerid]);
    SetPlayerHealth(playerid, 20.0);
    SetPlayerArmour(playerid, 0.0);
    TogglePlayerControllable(playerid, false);
    ApplyAnimation(playerid, "CRACK", "crckdeth2", 4.1, 1, 1, 1, 1, 0);
    RW_MedOpen(playerid);
    return 1;
}
stock RW_MedPulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gDowned[p]) continue;
        if (GetPlayerState(p) == PLAYER_STATE_WASTED) continue;
        if (gUIMode[p] == UI_DOWNED) RW_MedRender(p);
        else if (gUIMode[p] == 0 && !gUIInput[p] && !gMedBusy[p]) RW_MedOpen(p);
    }
    return 1;
}
stock RW_MedRevive(target)
{
    if (!gDowned[target]) return 0;
    RW_MedClearDown(target);
    SetPlayerHealth(target, REVIVE_HP);
    RW_EmsShakeStart(target);   /* адреналин: десять секунд трясёт руки */
    return RW_Ok(target, "Adrenaline kicks in. You are on your feet.");
}
stock RW_MedFinish(playerid)
{
    gMedBusy[playerid] = false;
    TogglePlayerControllable(playerid, true);
    ClearAnimations(playerid, 1);
    if (gDowned[playerid]) RW_MedSpawn(playerid);
    return 1;
}
stock RW_MedConsume(playerid, slot)
{
    new query[220]; format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return 0;
    gInv[playerid][slot][iiUid] = 0; gInv[playerid][slot][iiType] = 0; gInv[playerid][slot][iiData] = 0; gInv[playerid][slot][iiAmount] = 0;
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return 1;
}
stock RW_UseBandage(playerid, slot)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return 0;
    if (slot < 0 || slot >= INV_SLOTS || gInv[playerid][slot][iiType] != ITEM_BANDAGE) return RW_Error(playerid, "Select a bandage.");
    if (gDowned[playerid]) return RW_Error(playerid, "You are downed. Adrenaline can revive you.");
    if (gMedBusy[playerid]) return RW_Info(playerid, "You are already busy.");
    if (GetTickCount()-gMedCooldown[playerid] < MED_COOLDOWN) return RW_Info(playerid, "Wait a second between bandages.");
    new Float:hp; GetPlayerHealth(playerid, hp);
    if (hp >= 100.0) return RW_Info(playerid, "You are not hurt.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Use bandages on foot.");
    if (!RW_MedConsume(playerid, slot)) return RW_Error(playerid, "Could not use the bandage.");
    gMedBusy[playerid] = true; gMedCooldown[playerid] = GetTickCount();
    RW_UI_Close(playerid);
    TogglePlayerControllable(playerid, false);
    ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 1, 1, 1, 1, 0);
    SetTimerEx("RW_MedBandageDone", MED_USE_MS, false, "i", playerid);
    return RW_Info(playerid, "Applying a bandage...");
}
public RW_MedBandageDone(playerid)
{
    if (!IsPlayerConnected(playerid) || !gLogged[playerid]) return 1;
    if (!gMedBusy[playerid]) return 1;
    RW_MedFinish(playerid);
    if (gDowned[playerid]) return RW_Error(playerid, "The bandage was wasted: you are downed.");
    new Float:hp; GetPlayerHealth(playerid, hp);
    if (hp <= 0.0) return 1;
    new Float:healed = hp + BANDAGE_HP;
    if (healed > 100.0) healed = 100.0;
    SetPlayerHealth(playerid, healed);
    new message[96]; format(message, sizeof message, "Bandaged: +%d HP (%.0f / 100).", BANDAGE_HP, healed);
    return RW_Ok(playerid, message);
}
stock RW_UseSyringe(playerid, slot)
{
    if (!gLogged[playerid] || !gInvLoaded[playerid]) return 0;
    if (slot < 0 || slot >= INV_SLOTS || gInv[playerid][slot][iiType] != ITEM_SYRINGE) return RW_Error(playerid, "Select an adrenaline syringe.");
    if (gDowned[playerid]) return RW_Error(playerid, "You are downed. Someone else must inject you.");
    if (gMedBusy[playerid]) return RW_Info(playerid, "You are already busy.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Use the syringe on foot.");
    new target = -1, Float:best = DOWN_RADIUS*DOWN_RADIUS, Float:x, Float:y, Float:z, Float:dx, Float:dy, Float:dz;
    GetPlayerPos(playerid, x, y, z);
    for (new p; p < MAX_PLAYERS; p++) {
        if (p == playerid || !IsPlayerConnected(p) || !gDowned[p]) continue;
        GetPlayerPos(p, dx, dy, dz);
        dx -= x; dy -= y; dz -= z;
        new Float:d = dx*dx + dy*dy + dz*dz;
        if (d <= best) { best = d; target = p; }
    }
    if (target < 0) return RW_Error(playerid, "No downed player within 3 metres.");
    if (!RW_MedConsume(playerid, slot)) return RW_Error(playerid, "Could not use the syringe.");
    gMedBusy[playerid] = true;
    RW_UI_Close(playerid);
    TogglePlayerControllable(playerid, false);
    ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 1, 1, 1, 1, 0);
    SetTimerEx("RW_MedSyringeDone", MED_USE_MS, false, "ii", playerid, target);
    new nick[MAX_PLAYER_NAME], message[110];
    RW_GetNick(target, nick);
    format(message, sizeof message, "Injecting %s with adrenaline...", nick);
    return RW_Info(playerid, message);
}
public RW_MedSyringeDone(playerid, target)
{
    if (IsPlayerConnected(playerid) && gLogged[playerid] && gMedBusy[playerid]) RW_MedFinish(playerid);
    if (!IsPlayerConnected(target) || !gLogged[target] || !gDowned[target]) return RW_Error(playerid, "The patient is gone.");
    new Float:x, Float:y, Float:z, Float:tx, Float:ty, Float:tz;
    GetPlayerPos(playerid, x, y, z); GetPlayerPos(target, tx, ty, tz);
    if (VectorSize(x-tx, y-ty, z-tz) > 5.0) return RW_Error(playerid, "The patient moved away.");
    RW_MedRevive(target);
    new nick[MAX_PLAYER_NAME], message[110];
    RW_GetNick(target, nick);
    format(message, sizeof message, "%s is back on their feet.", nick);
    return RW_Ok(playerid, message);
}
