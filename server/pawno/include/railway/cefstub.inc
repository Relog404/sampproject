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
stock RW_CefInvOpen(playerid, const title[], const subtitle[])
{
    #pragma unused playerid, title, subtitle
    return 0;
}
stock RW_CefInvCell(playerid, id, const label[], const note[], amount, selected, const style[])
{
    #pragma unused playerid, id, label, note, amount, selected, style
    return 0;
}
stock RW_CefInvInfo(playerid, id, const title[], const desc[], const stats[])
{
    #pragma unused playerid, id, title, desc, stats
    return 0;
}
stock RW_CefInvAction(playerid, id, const label[], const style[] = "")
{
    #pragma unused playerid, id, label, style
    return 0;
}
stock RW_CefInvEnd(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefClose(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefHudRadar(playerid, angle, const blips[], const zone[])
{
    #pragma unused playerid, angle, blips, zone
    return 0;
}
stock RW_CefHudHint(playerid, const hints[])
{
    #pragma unused playerid, hints
    return 0;
}
stock RW_CefHudBonus(playerid, const key[], amount, day)
{
    #pragma unused playerid, key, amount, day
    return 0;
}
stock RW_CefHudShow(playerid, on)
{
    #pragma unused playerid, on
    return 0;
}
stock RW_CefAuthOpen(playerid, bool:registerMode, const nick[], const message[])
{
    #pragma unused playerid, registerMode, nick, message
    return 0;
}
stock RW_CefAuthClose(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefHudMouse(playerid, on)
{
    #pragma unused playerid, on
    return 0;
}
stock RW_CefMenuOpen(playerid, const title[], const subtitle[])
{
    #pragma unused playerid, title, subtitle
    return 0;
}
stock RW_CefMenuRow(playerid, const text[], const action[], const style[])
{
    #pragma unused playerid, text, action, style
    return 0;
}
stock RW_CefMenuEnd(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefMenuFocus(playerid)
{
    #pragma unused playerid
    return 0;
}
stock RW_CefMenuClose(playerid)
{
    #pragma unused playerid
    return 0;
}
