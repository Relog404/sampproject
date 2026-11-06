/*  ============================================================================
 *  railway/cefui.inc
 *  Browser UI on top of omp-cef. The pages live in scriptfiles/cef/railway
 *  and are downloaded by the client itself (http://cef/railway/...).
 *
 *  Players without the plugin keep the native TextDraw interface: every CEF
 *  entry point here is only used when RW_CefOn(playerid) is true.
 *
 *  Readiness: SA-MP builds of the plugin may never fire OnCefReady, so the
 *  state is polled with CEF_PlayerHasPlugin once per second. The callbacks
 *  are handled too, whichever comes first.
 *  ========================================================================= */

#if defined _railway_cefui_included
    #endinput
#endif
#define _railway_cefui_included

#include <cef>
#include <railway/core>

#define CEF_RES             "railway"
#define CEF_UI_MENU         (100)
#define CEF_UI_NOTIFY       (101)
#define CEF_UI_MENU_URL     "http://cef/railway/index.html"
#define CEF_UI_TOAST_URL    "http://cef/railway/notify.html"

#define CEF_TOAST_PLAIN     (0)
#define CEF_TOAST_OK        (1)
#define CEF_TOAST_ERROR     (2)

forward RW_CefPulse();
forward RW_CefClick(playerid, browserid, id);
forward RW_CefClosed(playerid, browserid);

/*  ------------------------------- lifecycle ------------------------------- */
// A missing plugin aborts this call, which leaves CEF switched off.
stock bool:RW_CefProbe() { CEF_PlayerHasPlugin(0); return true; }
stock RW_CefInit()
{
    gCefEnabled = RW_CefProbe();
    if (!gCefEnabled) { print("[cef] plugin unavailable; native interface kept."); return 0; }
    CEF_AddResource(CEF_RES);
    CEF_RegisterEvent("ui:click", "RW_CefClick", Argument_Integer);
    CEF_RegisterEvent("ui:closed", "RW_CefClosed");
    SetTimer("RW_CefPulse", 1000, true);
    print("[cef] browser interface ready (resource railway).");
    return 1;
}
stock bool:RW_CefOn(playerid) { return gCefEnabled && gCefReady[playerid]; }

stock RW_CefStart(playerid)
{
    if (gCefReady[playerid]) return 1;
    gCefReady[playerid] = true;
    CEF_CreateBrowser(playerid, CEF_UI_MENU, CEF_UI_MENU_URL, false, true);
    CEF_SetBrowserLayer(playerid, CEF_UI_MENU, 100);
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);
    CEF_CreateBrowser(playerid, CEF_UI_NOTIFY, CEF_UI_TOAST_URL, false, true);
    CEF_SetBrowserLayer(playerid, CEF_UI_NOTIFY, 200);
    return 1;
}
stock RW_CefReset(playerid)
{
    if (gCefReady[playerid]) {
        CEF_DestroyBrowser(playerid, CEF_UI_MENU);
        CEF_DestroyBrowser(playerid, CEF_UI_NOTIFY);
    }
    gCefReady[playerid] = false; gCefMode[playerid] = CEF_MODE_NONE;
    return 1;
}
public RW_CefPulse()
{
    if (!gCefEnabled) return 1;
    for (new p; p < MAX_PLAYERS; p++) {
        if (gCefReady[p] || !IsPlayerConnected(p) || !gLogged[p]) continue;
        if (CEF_PlayerHasPlugin(p)) RW_CefStart(p);
    }
    return 1;
}
public OnCefReady(playerid)
{
    if (gCefEnabled) RW_CefStart(playerid);
    return 1;
}
public OnCefInitialize(playerid, bool:success, E_CEF_INIT_REASON:reason, const message[])
{
    if (!success) printf("[cef] player %d: init failed (reason %d): %s", playerid, _:reason, message);
    return 1;
}
public OnCefBrowserCreated(playerid, browserid, bool:success, E_CEF_CREATE_STATUS:code, const reason[])
{
    if (!success) printf("[cef] player %d: browser %d failed (%d): %s", playerid, browserid, _:code, reason);
    return 1;
}

/*  --------------------------------- output -------------------------------- */
stock RW_CefToast(playerid, const text[], kind = CEF_TOAST_PLAIN)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_NOTIFY, "ui:toast", CEF_STR(text), CEF_INT(kind));
    return 1;
}
stock RW_CefOpen(playerid, mode, const title[], const subtitle[])
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = mode;
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, true);
    CEF_FocusBrowser(playerid, CEF_UI_MENU, true);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:open", CEF_STR(title), CEF_STR(subtitle));
    return 1;
}
stock RW_CefRow(playerid, id, const label[], const value[] = "", const icon[] = "", const note[] = "", const style[] = "")
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:row", CEF_INT(id), CEF_STR(label), CEF_STR(value), CEF_STR(icon), CEF_STR(note), CEF_STR(style));
    return 1;
}
stock RW_CefAction(playerid, id, const label[], const style[] = "")
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:action", CEF_INT(id), CEF_STR(label), CEF_STR(style));
    return 1;
}
stock RW_CefEnd(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:end");
    return 1;
}
stock RW_CefClose(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = CEF_MODE_NONE;
    CEF_FocusBrowser(playerid, CEF_UI_MENU, false);
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:close");
    return 1;
}
public RW_CefClosed(playerid, browserid)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    RW_CefClose(playerid);
    gControlMode[playerid] = 0;
    return 1;
}

/*  --------------------------------- routing ------------------------------- */
public RW_CefClick(playerid, browserid, id)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    if (gCefMode[playerid] == CEF_MODE_VEHICLE) return RW_CefVehicleClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_PASSENGERS) return RW_CefPassengerClick(playerid, id);
    return 1;
}
