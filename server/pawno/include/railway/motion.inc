#if defined _railway_motion_included
    #endinput
#endif
#define _railway_motion_included
new Float:gMotionMax[MAX_VEHICLES], Float:gMotionAccel[MAX_VEHICLES], Float:gMotionTarget[MAX_VEHICLES];
new bool:gMotionFly[MAX_VEHICLES], gMotionDriver[MAX_VEHICLES], gMotionLastTick;
forward RW_MotionPulse();
stock RW_MotionReset(v)
{
    if (v <= 0 || v >= MAX_VEHICLES) return 0;
    gMotionMax[v] = 0.0; gMotionAccel[v] = 0.0; gMotionTarget[v] = 0.0; gMotionFly[v] = false; gMotionDriver[v] = INVALID_PLAYER_ID;
    SetVehicleAngularVelocity(v, 0.0, 0.0, 0.0);
    return 1;
}
stock RW_MotionCommand(playerid, const command[], const params[])
{
    if (!gLogged[playerid] || gAdminLevel[playerid] < 1) return RW_Error(playerid, "Admin only.");
    if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return RW_Error(playerid, "Sit in the driver seat.");
    new v = GetPlayerVehicleID(playerid), slot = RW_IsOwnedSlot(v);
    if (slot < 0 || !gVeh[slot][vsUsed]) return RW_Error(playerid, "Use a managed car.");
    if (!strcmp(command, "flycar", true)) {
        if (params[0]) return RW_Info(playerid, "/flycar - toggle car flight");
        if (!gMotionFly[v] && !RW_Veh_CanDrive(playerid, v)) return RW_Error(playerid, "Insert a key and start the engine first.");
        gMotionFly[v] = !gMotionFly[v]; gMotionDriver[v] = playerid;
        new Float:x, Float:y, Float:z; GetVehicleVelocity(v, x, y, z);
        gMotionTarget[v] = floatsqroot(x*x+y*y+z*z)*180.0;
        if (!gMotionFly[v]) SetVehicleAngularVelocity(v, 0.0, 0.0, 0.0);
        return RW_Info(playerid, gMotionFly[v] ? ("Flight ON: accelerate/brake, analog up/down = pitch, steer = roll, look L/R = yaw.") : ("Flight OFF."));
    }
    if (!RW_IsNumeric(params) || strlen(params)>3) return RW_Info(playerid, "/speed <0-500 km/h> | /usk <0-100 km/h per second>; 0 = reset");
    new value = strval(params);
    if (!strcmp(command, "speed", true)) {
        if (value < 0 || value > 500) return RW_Error(playerid, "Speed range: 0-500 km/h.");
        gMotionMax[v] = float(value);
    } else {
        if (value < 0 || value > 100) return RW_Error(playerid, "Acceleration range: 0-100 km/h per second.");
        gMotionAccel[v] = float(value);
    }
    new msg[128]; format(msg, sizeof msg, "Car tuning: max %.0f km/h (0=stock), acceleration %.0f km/h/s (0=stock).", gMotionMax[v], gMotionAccel[v]);
    return RW_Ok(playerid, msg);
}
public RW_MotionPulse()
{
    new now = GetTickCount(), elapsed = now-gMotionLastTick;
    gMotionLastTick = now;
    if (elapsed <= 0 || elapsed > 200) elapsed = 50;
    new Float:dt = float(elapsed)/1000.0;
    for (new p; p < MAX_PLAYERS; p++) if (gLogged[p] && GetPlayerState(p) == PLAYER_STATE_DRIVER) {
        new v = GetPlayerVehicleID(p), slot = RW_IsOwnedSlot(v);
        if (slot < 0 || !gVeh[slot][vsUsed]) continue;
        if (gMotionFly[v] && (gAdminLevel[p] < 1 || gMotionDriver[v] != p || !RW_Veh_CanDrive(p, v))) {
            gMotionFly[v] = false; SetVehicleAngularVelocity(v, 0.0, 0.0, 0.0);
        }
        if (!RW_Veh_CanDrive(p, v)) continue;
        new keys, ud, lr; GetPlayerKeys(p, keys, ud, lr);
        new Float:vx, Float:vy, Float:vz; GetVehicleVelocity(v, vx, vy, vz);
        new Float:speed = floatsqroot(vx*vx+vy*vy+vz*vz)*180.0;
        if (gMotionFly[v]) {
            new Float:w, Float:x, Float:y, Float:z; GetVehicleRotationQuat(v, w, x, y, z);
            new Float:fx = 2.0*(x*y-w*z), Float:fy = 1.0-2.0*(x*x+z*z), Float:fz = 2.0*(y*z+w*x);
            new Float:rx = 1.0-2.0*(y*y+z*z), Float:ry = 2.0*(x*y+w*z), Float:rz = 2.0*(x*z-w*y);
            new Float:pitch, Float:roll, Float:yaw;
            if (keys & KEY_ANALOG_UP) pitch -= 0.025;
            if (keys & KEY_ANALOG_DOWN) pitch += 0.025;
            if (lr == KEY_LEFT) roll += 0.03;
            if (lr == KEY_RIGHT) roll -= 0.03;
            if (keys & KEY_LOOK_LEFT) yaw += 0.012;
            if (keys & KEY_LOOK_RIGHT) yaw -= 0.012;
            SetVehicleAngularVelocity(v, rx*pitch+fx*roll, ry*pitch+fy*roll, rz*pitch+fz*roll+yaw);
            new Float:accel = gMotionAccel[v] > 0.0 ? gMotionAccel[v] : 35.0;
            if (keys & KEY_SPRINT) gMotionTarget[v] += accel*dt;
            if (keys & KEY_JUMP) gMotionTarget[v] -= 50.0*dt;
            new Float:cap = gMotionMax[v] > 0.0 ? gMotionMax[v] : 350.0;
            if (gMotionTarget[v] < 0.0) gMotionTarget[v] = 0.0;
            if (gMotionTarget[v] > cap) gMotionTarget[v] = cap;
            vx=fx*gMotionTarget[v]/180.0; vy=fy*gMotionTarget[v]/180.0; vz=fz*gMotionTarget[v]/180.0+0.008;
            speed=floatsqroot(vx*vx+vy*vy+vz*vz)*180.0;
            if(speed>cap) { vx*=cap/speed; vy*=cap/speed; vz*=cap/speed; }
            SetVehicleVelocity(v,vx,vy,vz);
        } else {
            if (gMotionDriver[v] != p) { gMotionTarget[v] = speed; gMotionDriver[v] = p; }
            if (gMotionAccel[v] > 0.0 && (keys & KEY_SPRINT) && !(keys & KEY_JUMP) && !(keys & KEY_HANDBRAKE)) {
                new Float:horizontal = floatsqroot(vx*vx+vy*vy)*180.0;
                if (floatabs(horizontal-gMotionTarget[v]) > 25.0) gMotionTarget[v] = horizontal;
                gMotionTarget[v] += gMotionAccel[v]*dt;
                new Float:cap = gMotionMax[v] > 0.0 ? gMotionMax[v] : 300.0;
                if (gMotionTarget[v] > cap) gMotionTarget[v] = cap;
                if (horizontal > 1.0) { vx *= gMotionTarget[v]/horizontal; vy *= gMotionTarget[v]/horizontal; }
                else { new Float:a; GetVehicleZAngle(v, a); vx = floatsin(-a, degrees)*gMotionTarget[v]/180.0; vy = floatcos(-a, degrees)*gMotionTarget[v]/180.0; }
                SetVehicleVelocity(v, vx, vy, vz);
            } else gMotionTarget[v] = floatsqroot(vx*vx+vy*vy)*180.0;
            speed = floatsqroot(vx*vx+vy*vy+vz*vz)*180.0;
            if (gMotionMax[v] > 0.0 && speed > gMotionMax[v]) SetVehicleVelocity(v, vx*gMotionMax[v]/speed, vy*gMotionMax[v]/speed, vz*gMotionMax[v]/speed);
        }
    }
    // A driverless car must never retain flight angular velocity / authorization.
    for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed]) {
        new v = gVeh[s][vsVehicleId], p = gMotionDriver[v];
        if (gMotionFly[v] && (p < 0 || p >= MAX_PLAYERS || !gLogged[p] || GetPlayerState(p) != PLAYER_STATE_DRIVER || GetPlayerVehicleID(p) != v)) {
            gMotionFly[v] = false; gMotionDriver[v] = INVALID_PLAYER_ID; SetVehicleAngularVelocity(v, 0.0, 0.0, 0.0);
        }
    }
    return 1;
}
stock RW_MotionInit()
{
    gMotionLastTick = GetTickCount(); SetTimer("RW_MotionPulse", 50, true); return 1;
}
