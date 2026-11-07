/*  ============================================================================
 *  railway/speedo.inc
 *  Спидометр на HUD-странице: скорость, передача внутри прибора, состояние машины.
 *  ========================================================================= */

#if defined _railway_speedo_included
    #endinput
#endif
#define _railway_speedo_included

#include <railway/core>
#include <railway/ui>
#include <railway/accounts>
#include <railway/vehicles>

#define GEAR_REVERSE    (-1)
#define GEAR_NEUTRAL    (0)

new gCurrentGear[MAX_PLAYERS];

static const Float:gGearBands[6][2] = {
    {  0.0,  26.0 },
    { 26.0,  52.0 },
    { 52.0,  84.0 },
    { 84.0, 120.0 },
    { 120.0, 162.0 },
    { 162.0, 300.0 }
};

forward RW_SpeedoTick();
forward RW_Speedo_Compute(playerid, vehicleid, &speed, &gear, &rpm, &flags);

stock RW_Speedo_ResetPlayer(playerid)
{
    gCurrentGear[playerid] = 0;
    return 1;
}

stock RW_Speedo_Compute(playerid, vehicleid, &speed, &gear, &rpm, &flags)
{
    new Float:vx, Float:vy, Float:vz;
    new Float:angle;
    new Float:kmh;
    new Float:fwd;
    new bool:engine;

    GetVehicleVelocity(vehicleid, vx, vy, vz);
    GetVehicleZAngle(vehicleid, angle);

    kmh = floatsqroot(vx * vx + vy * vy + vz * vz) * 180.0;
    if (kmh < 0.4) {
        kmh = 0.0;
    }

    fwd = vx * floatsin(-angle, degrees) + vy * floatcos(-angle, degrees);

    new slot = gVehSlotOf[vehicleid];
    engine = (slot >= 0 && gVeh[slot][vsUsed]) ? gVeh[slot][vsEngine] : true;

    flags = RW_Veh_HudFlags(playerid, vehicleid);

    if (!engine) {
        gear = GEAR_NEUTRAL;
        rpm = 0;
        speed = floatround(kmh);
        return 1;
    }

    if (fwd < -0.02 && kmh > 1.0) {
        gear = GEAR_REVERSE;
        rpm = 900 + floatround((kmh / 40.0 > 1.0 ? 1.0 : kmh / 40.0) * 2600.0);
        speed = floatround(kmh);
        return 1;
    }

    if (kmh < 1.0) {
        gear = GEAR_NEUTRAL;
        gCurrentGear[playerid] = 0;
        rpm = 900 + random(90);
        speed = 0;
        return 1;
    }

    new current = gCurrentGear[playerid];
    if (current < 1 || current > 6) {
        current = 1;
    }

    while (current < 6 && kmh > gGearBands[current - 1][1] * 1.02) {
        current++;
    }
    while (current > 1 && kmh < gGearBands[current - 1][0] * 0.90) {
        current--;
    }

    gCurrentGear[playerid] = current;
    gear = current;

    new Float:lo = gGearBands[current - 1][0];
    new Float:hi = gGearBands[current - 1][1];
    new Float:fraction = (kmh - lo) / (hi - lo);

    if (fraction < 0.0) {
        fraction = 0.0;
    }
    if (fraction > 1.0) {
        fraction = 1.0;
    }

    rpm = 1000 + floatround(fraction * 5200.0);
    speed = floatround(kmh);
    return 1;
}

public RW_SpeedoTick()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++) {
        if (!IsPlayerConnected(playerid) || !gSpeedoShown[playerid]) {
            continue;
        }
        if (!IsPlayerInAnyVehicle(playerid) || GetPlayerVehicleSeat(playerid) != 0) {
            continue;
        }

        new vehicleid = GetPlayerVehicleID(playerid);
        if (vehicleid <= 0 || vehicleid >= MAX_VEHICLES) {
            continue;
        }

        new speed, gear, rpm, flags;
        RW_Speedo_Compute(playerid, vehicleid, speed, gear, rpm, flags);

        RW_UI_SpeedoUpdate(playerid, speed, gear, rpm, flags);
    }
    return 1;
}

stock RW_Speedo_Init()
{
    SetTimer("RW_SpeedoTick", SPEEDO_INTERVAL, true);
    return 1;
}
