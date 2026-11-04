/*  ============================================================================
 *  railway/ui.inc
 *  Вся работа с CEF-интерфейсом в одном месте: браузеры, слои, события.
 *  ========================================================================= */

#if defined _railway_ui_included
    #endinput
#endif
#define _railway_ui_included

#include <railway/core>

/* ------------------------------------------------------------------ события */
forward RW_OnAuthReady(playerid, browserid);
forward RW_OnSpeedoReady(playerid, browserid);
forward RW_OnAuthLogin(playerid, browserid, const password[]);
forward RW_OnAuthRegister(playerid, browserid, const password[], const confirm[]);

/* ------------------------------------------------------------------ ресурсы */

stock RW_UI_Init()
{
    /* Ресурсы отдаются сервером: scriptfiles/cef/<resource>/... */
    CEF_AddResource("railway_control");
    RW_InteractionRegister();
    CEF_AddResource(CEF_RESOURCE_AUTH);
    CEF_AddResource(CEF_RESOURCE_SPEEDO);

    CEF_RegisterEvent("auth:ready", "RW_OnAuthReady");
    CEF_RegisterEvent("speedo:ready", "RW_OnSpeedoReady");
    CEF_RegisterEvent("auth:login", "RW_OnAuthLogin", Argument_String);
    CEF_RegisterEvent("auth:register", "RW_OnAuthRegister", Argument_String, Argument_String);
    return 1;
}

/* ------------------------------------------------------------------ экран входа */

stock RW_UI_OpenAuth(playerid)
{
    if (gAuthBrowserShown[playerid]) {
        return 1;
    }

    CEF_CreateBrowser(playerid, CEF_BROWSER_AUTH, CEF_AUTH_URL, true, true, 0.0, 0.0);
    CEF_SetBrowserLayer(playerid, CEF_BROWSER_AUTH, CEF_LAYER_AUTH);
    CEF_SetBrowserVisible(playerid, CEF_BROWSER_AUTH, true);
    CEF_FocusBrowser(playerid, CEF_BROWSER_AUTH, true);
    CEF_ToggleChatInput(playerid, false);
    CEF_SetEscapeMenuMode(playerid, CEF_ESCAPE_MENU_DISABLED);

    gAuthBrowserShown[playerid] = true;
    return 1;
}

stock RW_UI_CloseAuth(playerid)
{
    if (!gAuthBrowserShown[playerid]) {
        return 1;
    }

    /* Даём странице проиграть красивую анимацию закрытия */
    CEF_EmitEvent(playerid, CEF_BROWSER_AUTH, "auth:hide");

    CEF_FocusBrowser(playerid, CEF_BROWSER_AUTH, false);
    CEF_SetBrowserVisible(playerid, CEF_BROWSER_AUTH, false);
    CEF_ToggleChatInput(playerid, true);
    CEF_SetEscapeMenuMode(playerid, CEF_ESCAPE_MENU_GTA);

    gAuthBrowserShown[playerid] = false;
    return 1;
}

stock RW_UI_AuthResult(playerid, bool:ok, const message[], const nick[] = "")
{
    if (nick[0] == EOS) {
        CEF_EmitEvent(playerid, CEF_BROWSER_AUTH, "auth:result", CEF_BOOL(ok), CEF_STR(message));
    } else {
        CEF_EmitEvent(playerid, CEF_BROWSER_AUTH, "auth:result", CEF_BOOL(ok), CEF_STR(message), CEF_STR(nick));
    }
    return 1;
}

stock RW_UI_AuthBusy(playerid, bool:busy)
{
    CEF_EmitEvent(playerid, CEF_BROWSER_AUTH, "auth:busy", CEF_BOOL(busy));
    return 1;
}

/* ------------------------------------------------------------------ спидометр */

stock RW_UI_SpeedoShow(playerid, vehicleid)
{
    new model = GetVehicleModel(vehicleid);
    new name[48];
    RW_VehicleName(model, name, sizeof name);

    if (!gSpeedoShown[playerid]) {
        CEF_CreateBrowser(playerid, CEF_BROWSER_SPEEDO, CEF_SPEEDO_URL, false, false, 0.0, 0.0);
        CEF_SetBrowserLayer(playerid, CEF_BROWSER_SPEEDO, CEF_LAYER_SPEEDO);
        gSpeedoShown[playerid] = true;
    }

    CEF_SetBrowserVisible(playerid, CEF_BROWSER_SPEEDO, true);
    CEF_EmitEvent(playerid, CEF_BROWSER_SPEEDO, "speedo:show",
        CEF_STR(name), CEF_INT(model), CEF_BOOL(RW_IsVehicleOwner(playerid, vehicleid)));

    gSpeedoVehicle[playerid] = vehicleid;
    gSpeedoLastTick[playerid] = 0;
    return 1;
}

stock RW_UI_SpeedoHide(playerid)
{
    if (!gSpeedoShown[playerid]) {
        return 1;
    }
    CEF_EmitEvent(playerid, CEF_BROWSER_SPEEDO, "speedo:hide");
    CEF_SetBrowserVisible(playerid, CEF_BROWSER_SPEEDO, false);
    gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    return 1;
}

stock RW_UI_SpeedoUpdate(playerid, speed, gear, rpm, flags)
{
    CEF_EmitEvent(playerid, CEF_BROWSER_SPEEDO, "speedo:update",
        CEF_INT(speed), CEF_INT(gear), CEF_INT(rpm), CEF_INT(flags));
    return 1;
}

/* ------------------------------------------------------------------ прочее */

public RW_OnSpeedoReady(playerid, browserid)
{
    if (browserid != CEF_BROWSER_SPEEDO) {
        return 0;
    }
    if (gSpeedoVehicle[playerid] == INVALID_VEHICLE_ID) {
        return 0;
    }
    /* Страница загрузилась позже, чем пришёл первый show — повторяем состояние. */
    RW_UI_SpeedoShow(playerid, gSpeedoVehicle[playerid]);
    return 1;
}

stock RW_UI_OnBrowserCreated(playerid, browserid, bool:success)
{
    #pragma unused playerid
    #pragma unused browserid

    if (!success) {
        return 0;
    }

    /* Страница сама сообщит о готовности событием auth:ready,
       после чего мы отдадим ей данные игрока. */
    return 1;
}

stock RW_UI_Reset(playerid)
{
    gAuthBrowserShown[playerid] = false;
    gSpeedoShown[playerid] = false;
    gSpeedoVehicle[playerid] = INVALID_VEHICLE_ID;
    return 1;
}
