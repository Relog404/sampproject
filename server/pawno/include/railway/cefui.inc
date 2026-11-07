/*  ============================================================================
 *  railway/cefui.inc
 *  Browser UI on top of omp-cef. The pages live in scriptfiles/cef/railway
 *  and are downloaded by the client itself (http://cef/railway/...).
 *
 *  Players without the plugin keep the native dialogs: every CEF
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
#define CEF_UI_PHONE        (102)
#define CEF_UI_HUD          (103)
#define CEF_UI_AUTH         (104)
#define CEF_UI_MENU_URL     "http://cef/railway/index.html"
#define CEF_UI_TOAST_URL    "http://cef/railway/notify.html"
#define CEF_UI_PHONE_URL    "http://cef/railway/phone.html"
#define CEF_UI_HUD_URL      "http://cef/railway/hud.html"
#define CEF_UI_AUTH_URL     "http://cef/railway/auth.html"

#define CEF_TOAST_PLAIN     (0)
#define CEF_TOAST_OK        (1)
#define CEF_TOAST_ERROR     (2)

/*  Клавиши телефона и инвентаря. SA-MP не отдаёт серверу ни «[», ни «U» —
    их перехватывает плагин на уровне окна игры (CEF_SetKeyCapture), и ловит
    он их независимо от фокуса браузера. Сверяем скан-код, а не виртуальный
    код клавиши: скан-код один и тот же на любой раскладке, а вот виртуальный
    код у русской и английской отличается. Alt — бит EVENTFLAG_ALT_DOWN. */
#define CEF_SCAN_BRACKET    (26)
#define CEF_SCAN_U          (22)
#define CEF_MOD_ALT         (8)
#define CEF_VK_BRACKET      (0xDB)
#define CEF_VK_U            (0x55)

forward RW_CefPulse();
forward RW_CefClick(playerid, browserid, id);
forward RW_CefClosed(playerid, browserid);
/*  phone: the page calls the server, the server answers with phone:* events */
forward RW_CefPhoneApp(playerid, browserid, app);
forward RW_CefPhoneDial(playerid, browserid, number);
forward RW_CefPhonePick(playerid, browserid);
forward RW_CefPhoneHang(playerid, browserid);
forward RW_CefPhoneSms(playerid, browserid, const number[], const text[]);
forward RW_CefPhoneContact(playerid, browserid, const name[], number);
forward RW_CefPhoneTopUp(playerid, browserid, amount);
forward RW_CefPhoneTariff(playerid, browserid, tariff);
forward RW_CefPhoneSet(playerid, browserid, const wallpaper[], const ringtone[], dnd);
forward RW_CefPhone911(playerid, browserid, reason);
forward RW_CefPhoneClosed(playerid, browserid);
/*  browser-independent: the plugin forwards keys even with no focus */
forward OnCefPressKey(playerid, key, scancode, modifiers, bool:down, bool:repeat);

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
    CEF_RegisterEvent("phone:app", "RW_CefPhoneApp", Argument_Integer);
    CEF_RegisterEvent("phone:call", "RW_CefPhoneDial", Argument_Integer);
    CEF_RegisterEvent("phone:pick", "RW_CefPhonePick");
    CEF_RegisterEvent("phone:hang", "RW_CefPhoneHang");
    CEF_RegisterEvent("phone:sendsms", "RW_CefPhoneSms", Argument_String, Argument_String);
    CEF_RegisterEvent("phone:addcontact", "RW_CefPhoneContact", Argument_String, Argument_Integer);
    CEF_RegisterEvent("phone:topup", "RW_CefPhoneTopUp", Argument_Integer);
    CEF_RegisterEvent("phone:tariff", "RW_CefPhoneTariff", Argument_Integer);
    CEF_RegisterEvent("phone:set", "RW_CefPhoneSet", Argument_String, Argument_String, Argument_Integer);
    CEF_RegisterEvent("phone:911", "RW_CefPhone911", Argument_Integer);
    CEF_RegisterEvent("phone:close", "RW_CefPhoneClosed");
    CEF_RegisterEvent("hud:mousetoggle", "RW_HudMouseEvent", Argument_Integer);
    CEF_RegisterEvent("ui:answer", "RW_CefInputSubmit", Argument_String);
    CEF_RegisterEvent("auth:submit", "RW_CefAuthSubmit", Argument_String, Argument_String);
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
    CEF_CreateBrowser(playerid, CEF_UI_PHONE, CEF_UI_PHONE_URL, false, true);
    CEF_SetBrowserLayer(playerid, CEF_UI_PHONE, 150);
    CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, false);
    CEF_CreateBrowser(playerid, CEF_UI_HUD, CEF_UI_HUD_URL, false, false);
    CEF_SetBrowserLayer(playerid, CEF_UI_HUD, 90);
    CEF_CreateBrowser(playerid, CEF_UI_AUTH, CEF_UI_AUTH_URL, false, true);
    CEF_SetBrowserLayer(playerid, CEF_UI_AUTH, 300);
    CEF_SetBrowserVisible(playerid, CEF_UI_AUTH, false);
    RW_CefKeys(playerid);
    return 1;
}
/*  Захват клавиш включается один раз и навсе: плагин шлёт на сервер только
    те клавиши, которые разрешены здесь, так что лишнего трафика нет. */
stock RW_CefKeys(playerid)
{
    CEF_SetKeyCapture(playerid, true);
    CEF_EnableKey(playerid, CEF_VK_BRACKET, true);
    CEF_EnableKey(playerid, CEF_VK_U, true);
    return 1;
}
stock RW_CefReset(playerid)
{
    if (gCefReady[playerid]) {
        CEF_SetKeyCapture(playerid, false);
        CEF_DestroyBrowser(playerid, CEF_UI_MENU);
        CEF_DestroyBrowser(playerid, CEF_UI_NOTIFY);
        CEF_DestroyBrowser(playerid, CEF_UI_PHONE);
        CEF_DestroyBrowser(playerid, CEF_UI_HUD);
        CEF_DestroyBrowser(playerid, CEF_UI_AUTH);
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
    /* Страховка: если ни одной панели не открыто, фокус держать некому.
       Застрявший курсор мыши на экране лечится сам, за секунду. */
    for (new p; p < MAX_PLAYERS; p++) {
        if (!gCefReady[p] || !IsPlayerConnected(p) || !gLogged[p]) continue;
        if (gCefMode[p] == CEF_MODE_NONE && gUIMode[p] == 0) RW_CefFocusRelease(p);
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

/*  Alt + [ — телефон, Alt + U — инвентарь. Повторное нажатие закрывает.
    Клавиша приходит и тогда, когда браузер не в фокусе, поэтому курсор для
    этого не нужен; пока открыт чат, нажатие уходит в чат. */
public OnCefPressKey(playerid, key, scancode, modifiers, bool:down, bool:repeat)
{
    #pragma unused key
    if (!down || repeat) return 1;
    if (!gLogged[playerid] || !RW_CefOn(playerid)) return 1;
    if (!(modifiers & CEF_MOD_ALT)) return 1;
    if (CEF_IsChatInputOpen(playerid)) return 1;
    if (scancode == CEF_SCAN_BRACKET) {
        if (gPhoneShown[playerid]) return RW_PhoneClose(playerid);
        return RW_PhoneOpen(playerid);
    }
    if (scancode == CEF_SCAN_U) return RW_InvOpen(playerid);
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
/* Превью машины в карточке: страница рисует силуэт по номеру модели. */
stock RW_CefPreview(playerid, model, const name[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:preview", CEF_INT(model), CEF_STR(name));
    return 1;
}
stock RW_CefEnd(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:end");
    return 1;
}
/* Курсор мыши появляется, пока браузер в фокусе, и убирается, когда фокус
   снят. Если фокус остался у панели, которую сервер уже закрыл, курсор висит
   на экране и мешает играть, поэтому фокус снимается всегда и со всех окон. */
stock RW_CefFocusRelease(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_FocusBrowser(playerid, CEF_UI_MENU, false);
    CEF_FocusBrowser(playerid, CEF_UI_AUTH, false);
    return 1;
}
stock RW_CefClose(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = CEF_MODE_NONE;
    RW_CefFocusRelease(playerid);
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:close");
    return 1;
}
public RW_CefClosed(playerid, browserid)
{
    #pragma unused browserid
    /* Закрытое поле ввода: иначе «ждём ввод» останется навсегда, а вместе
       с ним и запрет на остальные действия. */
    if (RW_InputBusy(playerid)) return RW_InputCancel(playerid);
    RW_CefClose(playerid);
    RW_CefFocusRelease(playerid);
    gControlMode[playerid] = 0;
    return 1;
}

/*  ---------------------------------- phone -------------------------------- */
stock RW_CefPhoneOpen(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    gPhoneShown[playerid] = true;
    CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, true);
    CEF_FocusBrowser(playerid, CEF_UI_PHONE, true);
    RW_CefPhoneState(playerid);
    RW_CefPhoneApps(playerid);
    RW_CefPhoneContacts(playerid);
    RW_CefPhoneHistory(playerid);
    return 1;
}
stock RW_CefPhoneClose(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    if (!gPhoneShown[playerid]) return 0;
    gPhoneShown[playerid] = false;
    CEF_FocusBrowser(playerid, CEF_UI_PHONE, false);
    CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, false);
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:close");
    return 1;
}
// number, balance, tariff, dnd, wallpaper, ringtone
stock RW_CefPhoneState(playerid)
{
    if (!RW_CefOn(playerid) || !gPhoneShown[playerid]) return 0;
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:state", CEF_INT(gPhoneNum[playerid]), CEF_INT(gPhoneBalance[playerid]),
                  CEF_INT(gPhoneTariff[playerid]), CEF_INT(gPhoneDnd[playerid] ? 1 : 0),
                  CEF_STR(gPhoneWall[playerid]), CEF_STR(gPhoneRing_[playerid]));
    return 1;
}
stock RW_CefPhoneApps(playerid)
{
    if (!RW_CefOn(playerid) || !gPhoneShown[playerid]) return 0;
    for (new i; i < PHONE_APPS; i++) {
        new name[24], icon[16];
        RW_PhoneAppName(i, name, sizeof name);
        RW_PhoneAppIcon(i, icon, sizeof icon);
        CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:apps", CEF_INT(i), CEF_STR(name), CEF_STR(icon), CEF_INT(gPhoneApp[i]));
    }
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:appsend");
    return 1;
}
stock RW_CefPhoneContacts(playerid)
{
    if (!RW_CefOn(playerid) || !gPhoneShown[playerid] || gAccountId[playerid] <= 0) return 0;
    new query[220];
    format(query, sizeof query, "SELECT name,sim_number FROM phone_contacts WHERE player_id=%d ORDER BY name", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) {
            new name[MAX_PLAYER_NAME];
            db_get_field_assoc(r, "name", name, sizeof name);
            CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:contact", CEF_INT(db_get_field_assoc_int(r, "sim_number")), CEF_STR(name));
            db_next_row(r);
        }
        db_free_result(r);
    }
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:contactend");
    return 1;
}
// last messages of this number, both directions
stock RW_CefPhoneHistory(playerid)
{
    if (!RW_CefOn(playerid) || !gPhoneShown[playerid] || gPhoneNum[playerid] <= 0) return 0;
    new query[256];
    format(query, sizeof query, "SELECT from_num,to_num,body FROM phone_sms WHERE from_num=%d OR to_num=%d ORDER BY id DESC LIMIT 12", gPhoneNum[playerid], gPhoneNum[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) {
            new from = db_get_field_assoc_int(r, "from_num");
            new to = db_get_field_assoc_int(r, "to_num");
            new body[PHONE_SMS_MAX + 8];
            db_get_field_assoc(r, "body", body, sizeof body);
            new other = (from == gPhoneNum[playerid]) ? (to) : (from);
            CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:smsin", CEF_INT(from == gPhoneNum[playerid] ? 1 : 0), CEF_INT(other), CEF_STR(body));
            db_next_row(r);
        }
        db_free_result(r);
    }
    return 1;
}
// state: 0 idle, 1 dialling, 2 incoming, 3 talking
stock RW_CefPhoneCallState(playerid, callstate, number, const name[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:callstate", CEF_INT(callstate), CEF_INT(number), CEF_STR(name));
    return 1;
}
stock RW_CefPhoneSmsIn(playerid, direction, number, const text[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:smsin", CEF_INT(direction), CEF_INT(number), CEF_STR(text));
    return 1;
}

/*  --------------------------------- routing ------------------------------- */
public RW_CefClick(playerid, browserid, id)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    if (gCefMode[playerid] == CEF_MODE_VEHICLE) return RW_CefVehicleClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_PASSENGERS) return RW_CefPassengerClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_POLICE) return RW_PoliceCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_MDT) return RW_MdtCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_EMS) return RW_EmsCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_TRUNK) return RW_TrunkCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_LOCKPICK) return RW_PickCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_SERVICE) return RW_ServiceCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_SHOP) return RW_ShopCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_BANK) return RW_BankCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_JOB) return RW_JobCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_APP) return RW_PhoneAppCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_PROPERTY) return RW_PropCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_DONATE) return RW_DonateCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_STORY) return RW_StoryCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_MARKET) return RW_MarketCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_CRIME) return RW_CrimeCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_FAMILY) return RW_FamilyCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_GOV) return RW_GovCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_INVENTORY) return RW_InvCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_GARAGE) return RW_GarageCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_KEYS) return RW_KeyMenuCefClick(playerid, id);
    if (gCefMode[playerid] == CEF_MODE_DOWNED) return RW_MedCefClick(playerid, id);
    /* Поле ввода: строк в нём нет, поэтому любой клик его закрывает. */
    if (gCefMode[playerid] == CEF_MODE_INPUT) { RW_InputCancel(playerid); return 1; }
    /* Настройки, бинды и карта: панель общая, поэтому клик разбирает
       тот же RW_UI_Click, что и раньше — по gUIMode. */
    if (gCefMode[playerid] == CEF_MODE_SETTINGS || gCefMode[playerid] == CEF_MODE_BIND
        || gCefMode[playerid] == CEF_MODE_MAP) return RW_UI_Click(playerid, id);
    return 1;
}

/*  --------------------------- инвентарь (страница) ------------------------ */
/*  У инвентаря своя страница: сетка ячеек, по правой кнопке сервер присылает
    карточку вещи (название, описание, вес) и кнопки действий.                 */

stock RW_CefInvOpen(playerid, const title[], const subtitle[])
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = CEF_MODE_INVENTORY;
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, true);
    CEF_FocusBrowser(playerid, CEF_UI_MENU, true);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:open", CEF_STR(title), CEF_STR(subtitle));
    return 1;
}
stock RW_CefInvCell(playerid, id, const label[], const note[], amount, selected, const style[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:cell", CEF_INT(id), CEF_STR(label), CEF_STR(note),
        CEF_INT(amount), CEF_INT(selected), CEF_STR(style));
    return 1;
}
/* Обновление открытого инвентаря: только ячейки. Фокус не трогаем — иначе
   браузер перехватывает ввод заново каждую секунду, курсор не убирается и
   первый клик после перерисовки пропадает. */
stock RW_CefInvRefresh(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:redraw");
    return 1;
}
stock RW_CefInvInfo(playerid, id, const title[], const desc[], const stats[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:context", CEF_INT(id), CEF_STR(title), CEF_STR(desc), CEF_STR(stats));
    return 1;
}
stock RW_CefInvAction(playerid, id, const label[], const style[] = "")
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:action", CEF_INT(id), CEF_STR(label), CEF_STR(style));
    return 1;
}
stock RW_CefInvEnd(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_MENU, "inv:end");
    return 1;
}

/*  ------------------------------- HUD events ------------------------------ */
/*  HUD получает только готовые данные: радар, подсказки и карточку бонуса.  */
/* Подсказка над предметом на земле: текст и команда подбора. */
stock RW_CefHudGround(playerid, bool:on, const text[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:ground", CEF_INT(on ? 1 : 0), CEF_STR(text));
    return 1;
}
/* Спидометр: имя машины, скорость, передача, обороты и флаги (ключ, мотор, двери). */
stock RW_CefHudSpeedo(playerid, bool:on, const name[], speed, gear, rpm, flags)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:speedo", CEF_INT(on ? 1 : 0), CEF_STR(name),
        CEF_INT(speed), CEF_INT(gear), CEF_INT(rpm), CEF_INT(flags));
    return 1;
}
stock RW_CefHudRadar(playerid, angle, const blips[], const zone[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:radar", CEF_INT(angle), CEF_STR(blips), CEF_STR(zone));
    return 1;
}
stock RW_CefHudHint(playerid, const hints[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:hint", CEF_STR(hints));
    return 1;
}
/*  --------------------------- поле ввода --------------------------------- */
/*  Сервер просит число или текст: страница показывает поле, ответ приходит
    событием ui:answer. Проверяет ввод снова сервер — странице не верят. */
stock RW_CefInput(playerid, const title[], const subtitle[], const hint[])
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = CEF_MODE_INPUT;
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, true);
    CEF_FocusBrowser(playerid, CEF_UI_MENU, true);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:ask", CEF_STR(title), CEF_STR(subtitle), CEF_STR(hint));
    return 1;
}
stock RW_CefInputClose(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    gCefMode[playerid] = CEF_MODE_NONE;
    RW_CefFocusRelease(playerid);
    CEF_SetBrowserVisible(playerid, CEF_UI_MENU, false);
    CEF_EmitEvent(playerid, CEF_UI_MENU, "ui:close");
    return 1;
}

/* Кошелёк: счёт в банке, наличные и московское время — справа от радара. */
stock RW_CefHudWallet(playerid, bank, cash, hour, minute)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:wallet", CEF_INT(bank), CEF_INT(cash),
        CEF_INT(hour), CEF_INT(minute));
    return 1;
}
stock RW_CefHudShow(playerid, on)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:show", CEF_INT(on));
    return 1;
}
/* Курсор нужен только чтобы нажать карточку: /mouse включает его и выключает. */
stock RW_CefHudMouse(playerid, on)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_FocusBrowser(playerid, CEF_UI_HUD, on ? true : false);
    CEF_EmitEvent(playerid, CEF_UI_HUD, "hud:mouse", CEF_INT(on ? 1 : 0));
    return 1;
}

/*  --------------------------- авторизация -------------------------------- */
/*  Вход и регистрация в браузере: страница отдаёт пароль, решает сервер.     */
stock RW_CefAuthOpen(playerid, bool:registerMode, const nick[], const message[])
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_SetBrowserVisible(playerid, CEF_UI_AUTH, true);
    CEF_FocusBrowser(playerid, CEF_UI_AUTH, true);
    CEF_EmitEvent(playerid, CEF_UI_AUTH, "auth:open", CEF_INT(registerMode ? 1 : 0), CEF_STR(nick), CEF_STR(message));
    return 1;
}
stock RW_CefAuthClose(playerid)
{
    if (!RW_CefOn(playerid)) return 0;
    CEF_SetBrowserVisible(playerid, CEF_UI_AUTH, false);
    CEF_FocusBrowser(playerid, CEF_UI_AUTH, false);
    CEF_EmitEvent(playerid, CEF_UI_AUTH, "auth:close");
    return 1;
}
public RW_CefAuthSubmit(playerid, browserid, const password[], const confirm[])
{
    #pragma unused browserid
    if (!IsPlayerConnected(playerid) || gLogged[playerid]) return 0;
    new nick[MAX_PLAYER_NAME];
    RW_GetNick(playerid, nick);
    return RW_AuthSubmit(playerid, !RW_DB_AccountExists(nick), password, confirm);
}

/* Страница успевает прогрузиться: фокус повторяем через долю секунды,
   иначе первый клик после открытия пропадает. */

/*  ------------------------------ phone events ----------------------------- */
/* Приложение адвокатов: список лицензированных адвокатов, доступно из КПЗ. */
stock RW_CefPhoneLawyers(playerid)
{
    if (!RW_CefOn(playerid) || gAccountId[playerid] <= 0) return 0;
    new query[320];
    format(query, sizeof query, "SELECT a.name AS who,p.price,p.rating,(SELECT sim_number FROM sim_cards s WHERE s.player_id=p.player_id) AS num FROM lawyers p LEFT JOIN accounts a ON a.id=p.player_id WHERE p.license=1 AND p.expire>%d ORDER BY p.rating DESC LIMIT 20", gettime());
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) {
            new name[MAX_PLAYER_NAME];
            db_get_field_assoc(r, "who", name, sizeof name);
            CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:lawyer", CEF_STR(name), CEF_INT(db_get_field_assoc_int(r, "rating")), CEF_INT(db_get_field_assoc_int(r, "price")), CEF_INT(db_get_field_assoc_int(r, "num")));
            db_next_row(r);
        }
        db_free_result(r);
    }
    CEF_EmitEvent(playerid, CEF_UI_PHONE, "phone:lawyerend");
    return 1;
}
public RW_CefPhoneApp(playerid, browserid, app)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    if (app < 0 || app >= PHONE_APPS) return 1;
    /* эти приложения рисуются на клиенте сами */
    if (app == 0 || app == 1 || app == 10 || app == 11) return 1;
    if (app == 5) {
        if (gJailTime[playerid] <= 0) {
            #if RAILWAY_CEF
                RW_CefToast(playerid, "Lawyers are listed only while you sit in a cell.");
            #endif
            return 1;
        }
        return RW_CefPhoneLawyers(playerid);
    }
    if (app == PHONE_APP_BANK || app == PHONE_APP_WEATHER || app == PHONE_APP_GROUP || app == PHONE_APP_UBER
        || app == PHONE_APP_NEWS || app == PHONE_APP_AUTO || app == PHONE_APP_CAMERA
        || app == PHONE_APP_RADIO) return RW_PhoneAppOpen(playerid, app);
    #if RAILWAY_CEF
        RW_CefToast(playerid, "Phone, messages, contacts, lawyer, settings and 911 live here.");
    #endif
    return 1;
}
public RW_CefPhoneDial(playerid, browserid, number)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneCall(playerid, number);
}
public RW_CefPhonePick(playerid, browserid)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneAccept(playerid);
}
public RW_CefPhoneHang(playerid, browserid)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneEnd(playerid);
}
public RW_CefPhoneSms(playerid, browserid, const number[], const text[])
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    if (!RW_IsNumeric(number)) return RW_Error(playerid, "Number must be six digits.");
    return RW_PhoneSms(playerid, strval(number), text);
}
public RW_CefPhoneContact(playerid, browserid, const name[], number)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneContactAdd(playerid, name, number);
}
public RW_CefPhoneTopUp(playerid, browserid, amount)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneTopUp(playerid, amount);
}
public RW_CefPhoneTariff(playerid, browserid, tariff)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneTariffSet(playerid, tariff);
}
public RW_CefPhoneSet(playerid, browserid, const wallpaper[], const ringtone[], dnd)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_PhoneSettings(playerid, wallpaper, ringtone, dnd ? true : false);
}
public RW_CefPhone911(playerid, browserid, reason)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    return RW_Phone911(playerid, reason);
}
public RW_CefPhoneClosed(playerid, browserid)
{
    #pragma unused browserid
    if (!RW_CefOn(playerid)) return 0;
    gPhoneShown[playerid] = false;
    CEF_FocusBrowser(playerid, CEF_UI_PHONE, false);
    CEF_SetBrowserVisible(playerid, CEF_UI_PHONE, false);
    return 1;
}
