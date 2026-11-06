/*  ============================================================================
 *  railway/cefstub.inc
 *  Build without the omp-cef plugin: RAILWAY_CEF 0 in railway.pwn.
 *
 *  Same entry points as cefui.inc, all switched off, so the rest of the
 *  gamemode compiles unchanged and simply keeps the TextDraw interface.
 *  ========================================================================= */

#if defined _railway_cefstub_included
    #endinput
#endif
#define _railway_cefstub_included

#include <railway/core>

stock bool:RW_CefOn(playerid)
{
    #pragma unused playerid
    return false;
}
stock RW_CefInit() return 0;
stock RW_CefStart(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefReset(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefToast(playerid, const text[], kind = 0)
{
    #pragma unused playerid, text, kind
    return 0;
}
stock RW_CefClose(playerid)
{
    #pragma unused playerid
    return 0;
}
