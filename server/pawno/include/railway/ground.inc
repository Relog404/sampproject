#if defined _railway_ground_included
    #endinput
#endif
#define _railway_ground_included
/* Тексдрав-подсказка удалена: её рисует HUD-страница. */

new gGroundAim[MAX_PLAYERS];
forward RW_GroundPulse();

stock RW_GroundHide(playerid)
{
    RW_CefHudGround(playerid, false, "");
    gGroundAim[playerid]=0;
    return 1;
}
stock RW_GroundReset(playerid)
{
    RW_CefHudGround(playerid, false, "");
    gGroundAim[playerid]=0;
    return 1;
}
stock RW_GroundTarget(playerid)
{
    if(!gLogged[playerid] || !gInvLoaded[playerid] || gUIMode[playerid]!=0 || gUIInput[playerid] || gInvFitting[playerid] || GetPlayerState(playerid)!=PLAYER_STATE_ONFOOT || GetPlayerInterior(playerid)!=0 || GetPlayerVirtualWorld(playerid)!=0) return -1;
    new Float:cx,Float:cy,Float:cz,Float:fx,Float:fy,Float:fz,Float:px,Float:py,Float:pz;
    GetPlayerCameraPos(playerid,cx,cy,cz); GetPlayerCameraFrontVector(playerid,fx,fy,fz); GetPlayerPos(playerid,px,py,pz);
    new target=-1,Float:best=1000.0;
    for(new i;i<INV_WORLD;i++) {
        if(!gGround[i][igUid]) continue;
        new Float:dx=gGround[i][igX]-px,Float:dy=gGround[i][igY]-py,Float:dz=gGround[i][igZ]-pz;
        if(dx*dx+dy*dy+dz*dz>9.0) continue;
        dx=gGround[i][igX]-cx; dy=gGround[i][igY]-cy; dz=gGround[i][igZ]+0.15-cz;
        new Float:along=dx*fx+dy*fy+dz*fz;
        if(along<0.0 || along>10.0 || along>=best) continue;
        new Float:side=dx*dx+dy*dy+dz*dz-along*along;
        new Float:radius=gGround[i][igType]==ITEM_BACKPACK ? 0.45 : 0.25;
        if(side>radius*radius) continue;
        best=along; target=i;
    }
    return target;
}
/* Подсказку над предметом рисует HUD-страница: сервер шлёт только текст. */
stock RW_GroundShow(playerid, index)
{
    new label[128], name[32];
    if (gGround[index][igType] == ITEM_BACKPACK) format(name, sizeof name, "Рюкзак / 1 кг");
    else format(name, sizeof name, "Ключ от машины / 0.1 кг");
    format(label, sizeof label, "%s<br>[F / ENTER] — поднять, /take", name);
    RW_CefHudGround(playerid, true, label);
    gGroundAim[playerid] = gGround[index][igUid];
    return 1;
}
stock RW_GroundTake(playerid)
{
    new ground=RW_GroundTarget(playerid);
    if(ground<0) { RW_GroundHide(playerid); return RW_Error(playerid,"Aim at an item on the ground within 3m."); }
    if(GetTickCount()-gInvTick[playerid]<250) return 1;
    gInvTick[playerid]=GetTickCount();
    new section=0;
    if(RW_InvFree(playerid,0,gGround[ground][igType])<0 && RW_InvFree(playerid,1,gGround[ground][igType])>=0) section=1;
    gInvSelected[playerid]=gGround[ground][igUid]; gInvFromGround[playerid]=true;
    RW_InvTransfer(playerid,section);
    // Transfer revalidates UID, range/world/state and performs the conditional SQL.
    RW_GroundHide(playerid);
    return 1;
}
public RW_GroundPulse()
{
    for(new p;p<MAX_PLAYERS;p++) if(IsPlayerConnected(p)) {
        new target=RW_GroundTarget(p);
        if(target<0) { if(gGroundAim[p]) RW_GroundHide(p); }
        else if(gGroundAim[p]!=gGround[target][igUid]) RW_GroundShow(p,target);
    }
    return 1;
}
