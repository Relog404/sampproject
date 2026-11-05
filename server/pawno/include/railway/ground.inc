#if defined _railway_ground_included
    #endinput
#endif
#define _railway_ground_included
new PlayerText:gGroundHint[MAX_PLAYERS][2];
new bool:gGroundHintCreated[MAX_PLAYERS];
new gGroundAim[MAX_PLAYERS];
forward RW_GroundPulse();

stock RW_GroundHide(playerid)
{
    if(gGroundHintCreated[playerid]) for(new i;i<2;i++) PlayerTextDrawHide(playerid,gGroundHint[playerid][i]);
    gGroundAim[playerid]=0;
    return 1;
}
stock RW_GroundReset(playerid)
{
    if(gGroundHintCreated[playerid]) for(new i;i<2;i++) PlayerTextDrawDestroy(playerid,gGroundHint[playerid][i]);
    gGroundHintCreated[playerid]=false; gGroundAim[playerid]=0;
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
stock RW_GroundShow(playerid, index)
{
    if(!gGroundHintCreated[playerid]) {
        gGroundHint[playerid][0]=CreatePlayerTextDraw(playerid,244.0,277.0,"LD_SPAC:white");
        PlayerTextDrawFont(playerid,gGroundHint[playerid][0],4); PlayerTextDrawTextSize(playerid,gGroundHint[playerid][0],152.0,32.0);
        PlayerTextDrawColor(playerid,gGroundHint[playerid][0],0x141920E8);
        gGroundHint[playerid][1]=CreatePlayerTextDraw(playerid,254.0,282.0,"_");
        PlayerTextDrawFont(playerid,gGroundHint[playerid][1],1); PlayerTextDrawLetterSize(playerid,gGroundHint[playerid][1],0.19,0.95);
        PlayerTextDrawSetShadow(playerid,gGroundHint[playerid][1],0); PlayerTextDrawColor(playerid,gGroundHint[playerid][1],0xD8C39BFF);
        gGroundHintCreated[playerid]=true;
    }
    new label[128]; format(label,sizeof label,"%s~n~[F / ENTER] TAKE / /take",gGround[index][igType]==ITEM_BACKPACK ? ("BACKPACK / 1kg") : ("CAR KEY / 0.1kg"));
    PlayerTextDrawSetString(playerid,gGroundHint[playerid][1],label);
    for(new i;i<2;i++) PlayerTextDrawShow(playerid,gGroundHint[playerid][i]);
    gGroundAim[playerid]=gGround[index][igUid];
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
