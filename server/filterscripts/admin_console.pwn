// Custom console/RCON bridge for stock SA-MP 0.3.7. No player chat commands.
#define FILTERSCRIPT
#include <a_samp>

public OnFilterScriptInit()
{
    print("[admin_console] Console relay loaded: setadmin <playerid|name> <0-3>, dbcheck.");
    return 1;
}
public OnRconCommand(cmd[])
{
    return CallRemoteFunction("RW_ConsoleCommand", "s", cmd);
}
