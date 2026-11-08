/*  ============================================================================
 *  railway/cabin.inc
 *  Several players can fly together in one plane. Real seats are used first;
 *  when the model has no seats left, extra passengers ride in a virtual cabin
 *  and follow the plane. Maximum 20 people per plane.
 *  ========================================================================= */

#if defined _railway_cabin_included
    #endinput
#endif
#define _railway_cabin_included

#include <railway/core>

forward RW_CabinPulse();

stock RW_CabinReset(playerid)
{
    gCabinVehicle[playerid] = 0; gCabinSeat[playerid] = 0;
    return 1;
}
// How many people are already in or on this plane.
stock RW_CabinCount(vehicleid)
{
    new count;
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p)) continue;
        if (GetPlayerVehicleID(p) == vehicleid || gCabinVehicle[p] == vehicleid) count++;
    }
    return count;
}
stock bool:RW_CabinInside(playerid) { return gCabinVehicle[playerid] > 0; }
// First free real seat after the pilot's seat, or -1 when the cabin is full.
stock RW_CabinFreeSeat(vehicleid)
{
    new taken[10];
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || GetPlayerVehicleID(p) != vehicleid) continue;
        new seat = GetPlayerVehicleSeat(p);
        if (seat >= 0 && seat < 10) taken[seat] = 1;
    }
    for (new s = 1; s < 10; s++) if (!taken[s]) return s;
    return -1;
}
stock RW_CabinLeave(playerid)
{
    if (gCabinVehicle[playerid] <= 0) {
        if (GetPlayerVehicleID(playerid) > 0) RemovePlayerFromVehicle(playerid);
        return RW_Error(playerid, "You are not in a plane cabin.");
    }
    new v = gCabinVehicle[playerid], Float:x, Float:y, Float:z, Float:a;
    GetVehiclePos(v, x, y, z); GetVehicleZAngle(v, a);
    gCabinVehicle[playerid] = 0; gCabinSeat[playerid] = 0;
    TogglePlayerControllable(playerid, true);
    if (GetVehicleModel(v)) {
        x -= 4.0*floatsin(a, degrees); y -= 4.0*floatcos(a, degrees);
        SetPlayerPos(playerid, x, y, z + 1.0);
    }
    return RW_Ok(playerid, "You left the cabin.");
}
stock RW_CabinBoard(playerid, vehicleid)
{
    if (vehicleid <= 0 || vehicleid >= MAX_VEHICLES) return RW_Error(playerid, "No plane in range.");
    new model = GetVehicleModel(vehicleid);
    if (!model) return RW_Error(playerid, "No plane in range.");
    if (!RW_CabinIsPlane(model)) return RW_Error(playerid, "Only planes have a passenger cabin.");
    if (GetPlayerState(playerid) == PLAYER_STATE_WASTED || GetPlayerState(playerid) == PLAYER_STATE_SPECTATING) return RW_Error(playerid, "You cannot board now.");
    if (GetPlayerVehicleID(playerid) == vehicleid) return RW_Info(playerid, "You are already on board.");
    new Float:x, Float:y, Float:z;
    GetVehiclePos(vehicleid, x, y, z);
    if (!IsPlayerInRangeOfPoint(playerid, 25.0, x, y, z)) return RW_Error(playerid, "Stand within 25 metres of the plane.");
    if (RW_CabinCount(vehicleid) >= CABIN_MAX) return RW_Error(playerid, "The plane cabin is full (20).");
    new seat = RW_CabinFreeSeat(vehicleid);
    if (seat >= 0) {
        PutPlayerInVehicle(playerid, vehicleid, seat);
        if (GetPlayerVehicleID(playerid) == vehicleid && GetPlayerVehicleSeat(playerid) == seat) {
            gCabinVehicle[playerid] = 0; gCabinSeat[playerid] = 0;
            new message[96]; format(message, sizeof message, "Boarded seat %d of %d.", seat + 1, RW_CabinCount(vehicleid));
            return RW_Ok(playerid, message);
        }
    }
    // No real seat left: ride in the cabin and follow the plane.
    if (GetPlayerVehicleID(playerid)) RemovePlayerFromVehicle(playerid);
    gCabinVehicle[playerid] = vehicleid; gCabinSeat[playerid] = RW_CabinCount(vehicleid);
    TogglePlayerControllable(playerid, false);
    RW_CabinPlace(playerid);
    new message[96]; format(message, sizeof message, "Cabin seat %d of %d. /unboard to get out.", gCabinSeat[playerid], RW_CabinCount(vehicleid));
    return RW_Ok(playerid, message);
}
// The pilot defines the world the cabin follows.
stock RW_CabinPilot(vehicleid)
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p)) continue;
        if (GetPlayerVehicleID(p) == vehicleid && GetPlayerVehicleSeat(p) == 0) return p;
    }
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p)) continue;
        if (GetPlayerVehicleID(p) == vehicleid) return p;
    }
    for (new p; p < MAX_PLAYERS; p++) if (IsPlayerConnected(p) && gCabinVehicle[p] == vehicleid) return p;
    return INVALID_PLAYER_ID;
}
stock RW_CabinPlace(playerid)
{
    new v = gCabinVehicle[playerid];
    if (!v || !GetVehicleModel(v)) return RW_CabinLeave(playerid);
    new Float:x, Float:y, Float:z, Float:a;
    GetVehiclePos(v, x, y, z); GetVehicleZAngle(v, a);
    new row = gCabinSeat[playerid] / 4, col = gCabinSeat[playerid] % 4;
    new Float:fwd = -1.2 - float(row)*1.1, Float:right = (float(col) - 1.5)*0.75;
    new Float:sx = floatsin(a, degrees), Float:cy = floatcos(a, degrees);
    new Float:px = x + fwd*sx + right*cy;
    new Float:py = y + fwd*cy - right*sx;
    SetPlayerPos(playerid, px, py, z + 0.55);
    SetPlayerVelocity(playerid, 0.0, 0.0, 0.0);
    SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(RW_CabinPilot(v)));
    return 1;
}
public RW_CabinPulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || gCabinVehicle[p] <= 0) continue;
        new v = gCabinVehicle[p];
        if (!GetVehicleModel(v)) { RW_CabinLeave(p); continue; }
        if (GetPlayerState(p) == PLAYER_STATE_WASTED) { gCabinVehicle[p] = 0; TogglePlayerControllable(p, true); continue; }
        if (GetPlayerVehicleID(p) == v) { gCabinVehicle[p] = 0; TogglePlayerControllable(p, true); continue; }
        RW_CabinPlace(p);
    }
    return 1;
}
stock RW_CabinClearVehicle(vehicleid)
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || gCabinVehicle[p] != vehicleid) continue;
        gCabinVehicle[p] = 0; gCabinSeat[p] = 0;
        TogglePlayerControllable(p, true);
        RW_Error(p, "The plane is gone. You are on your own.");
    }
    return 1;
}
// /board [vehicle id] - board the aimed plane, /unboard - get out.
stock RW_CabinCommand(playerid, const params[])
{
    if (!gLogged[playerid]) return 0;
    if (gDowned[playerid]) return RW_Error(playerid, "You are downed.");
    new a[32], rest[144];
    RW_ParseCommand(params, a, sizeof a, rest, sizeof rest);
    if (!a[0]) {
        new v = RW_ControlTarget(playerid);
        if (v <= 0 || v >= MAX_VEHICLES) {
            if (RW_CefOn(playerid)) return RW_Info(playerid, "Aim at a plane and press Y: the card there has a \"board\" row. Max 20 people.");
            return RW_Info(playerid, "Aim at a plane, or use /board <vehicle id>. Max 20 people.");
        }
        return RW_CabinBoard(playerid, v);
    }
    if (!RW_IsNumeric(a) || strval(a) <= 0 || strval(a) >= MAX_VEHICLES) return RW_Info(playerid, "/board <vehicle id> - aim at a plane or give its id.");
    return RW_CabinBoard(playerid, strval(a));
}
