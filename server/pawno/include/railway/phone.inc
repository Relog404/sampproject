/*  ============================================================================
 *  railway/phone.inc
 *  Телефон: сим-карта, номер, баланс, звонки, SMS, контакты, приложения, 911.
 *
 *  Интерфейс — браузер (cefui.inc, страница phone.html). У кого плагина нет,
 *  те же самые механики доступны командами чата.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_phone_included
    #endinput
#endif
#define _railway_phone_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>

#define PHONE_NUM_MIN       100000
#define PHONE_NUM_MAX       999999
#define PHONE_APPS          13
#define PHONE_CONTACTS      24
#define PHONE_CONTACTS_PAGE 6
#define PHONE_RING_MS       20000
#define PHONE_BILL_MS       60000
#define PHONE_SMS_MAX       96
#define PHONE_911_ICON      98
#define PHONE_RING_SOUND    20800
#define PHONE_RANGE_SAY     8.0

/* состояние линии */
#define PHONE_IDLE          0
#define PHONE_DIAL          1
#define PHONE_RING          2
#define PHONE_TALK          3

new gPhoneItem[MAX_PLAYERS];            /* uid телефона в инвентаре            */
new gPhoneNum[MAX_PLAYERS];             /* номер сим-карты (0 — нет симки)     */
new gPhoneBalance[MAX_PLAYERS];
new gPhoneTariff[MAX_PLAYERS];
new bool:gPhoneDnd[MAX_PLAYERS];
new gPhoneWall[MAX_PLAYERS][24];        /* обои                                */
new gPhoneRing_[MAX_PLAYERS][24];       /* рингтон                             */
new gPhoneCall[MAX_PLAYERS];            /* собеседник (-1 — никого)            */
new gPhoneState[MAX_PLAYERS];           /* PHONE_IDLE..PHONE_TALK              */
new gPhoneDial[MAX_PLAYERS];            /* номер на другом конце линии         */
new gPhoneTick[MAX_PLAYERS];            /* начало звонка / гудки               */
new gPhoneBill[MAX_PLAYERS];            /* последнее списание за разговор      */
new bool:gPhoneCaller[MAX_PLAYERS];     /* этот игрок набирал номер            */
new bool:gPhoneShown[MAX_PLAYERS];      /* страница телефона открыта           */
new gPhoneApp[PHONE_APPS];              /* флаги приложений текущего игрока    */
new gPhoneContNum[PHONE_CONTACTS];      /* номера контактов на текущей странице */
new gPhoneContCount;

forward RW_PhonePulse();
/* Браузерная часть телефона живёт в cefui.inc, который подключается позже. */
forward RW_CefPhoneOpen(playerid);
forward RW_CefPhoneClose(playerid);
forward RW_CefPhoneState(playerid);
forward RW_CefPhoneContacts(playerid);
forward RW_CefPhoneCallState(playerid, callstate, number, const name[]);
forward RW_CefPhoneSmsIn(playerid, direction, number, const text[]);

/*  --------------------------------- справочники --------------------------- */

// 0 Phone, 1 Messages, 2 Bank, 3 Weather, 4 Group, 5 Lawyers, 6 Uber Drive,
// 7 Weazel News, 8 Auto Plus, 9 Camera, 10 Settings, 11 Operator.
stock RW_PhoneAppName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "Phone");
        case 1: format(dest, size, "Messages");
        case 2: format(dest, size, "Bank");
        case 3: format(dest, size, "Weather");
        case 4: format(dest, size, "Group");
        case 5: format(dest, size, "Lawyers");
        case 6: format(dest, size, "Uber Drive");
        case 7: format(dest, size, "Weazel News");
        case 8: format(dest, size, "Auto Plus");
        case 9: format(dest, size, "Camera");
        case 10: format(dest, size, "Settings");
        case 11: format(dest, size, "Operator");
        case 12: format(dest, size, "Radio");
        default: format(dest, size, "App");
    }
    return 1;
}
stock RW_PhoneAppIcon(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "phone");
        case 1: format(dest, size, "chat");
        case 2: format(dest, size, "bank");
        case 3: format(dest, size, "sun");
        case 4: format(dest, size, "group");
        case 5: format(dest, size, "law");
        case 6: format(dest, size, "car");
        case 7: format(dest, size, "news");
        case 8: format(dest, size, "key");
        case 9: format(dest, size, "camera");
        case 10: format(dest, size, "gear");
        case 11: format(dest, size, "signal");
        case 12: format(dest, size, "radio");
        default: format(dest, size, "app");
    }
    return 1;
}
// Тарифы: имя, цена минуты разговора, цена SMS.
stock RW_PhoneTariffName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "Start");
        case 1: format(dest, size, "Active");
        case 2: format(dest, size, "Maxi");
        default: format(dest, size, "Start");
    }
    return 1;
}
stock RW_PhoneTariffId(const name[])
{
    if (!strcmp(name, "Active", true)) return 1;
    if (!strcmp(name, "Maxi", true)) return 2;
    return 0;
}
stock RW_PhoneCallCost(tariff)
{
    switch (tariff) { case 1: return 2; case 2: return 5; }
    return 1;
}
stock RW_PhoneSmsCost(tariff)
{
    switch (tariff) { case 1: return 2; case 2: return 1; }
    return 3;
}
// Модель телефона — data предмета ITEM_PHONE.
stock RW_PhoneName(data, dest[], size = sizeof dest)
{
    switch (data) {
        case 0: format(dest, size, "PHONE BRICK");
        case 1: format(dest, size, "PHONE SLIDER");
        case 2: format(dest, size, "PHONE SMART");
        default: format(dest, size, "PHONE");
    }
    return 1;
}
stock RW_PhoneWallName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "dusk");
        case 1: format(dest, size, "ocean");
        case 2: format(dest, size, "sand");
        case 3: format(dest, size, "night");
        default: format(dest, size, "dusk");
    }
    return 1;
}
stock RW_PhoneRingName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "classic");
        case 1: format(dest, size, "vibe");
        case 2: format(dest, size, "alarm");
        default: format(dest, size, "classic");
    }
    return 1;
}

/*  ----------------------------------- база -------------------------------- */

stock RW_PhoneSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS phone_apps (player_id INT, app_id VARCHAR(32), installed BOOLEAN)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS phone_settings (player_id INT, wallpaper VARCHAR(64), ringtone VARCHAR(64), dnd BOOLEAN)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS sim_cards (player_id INT, sim_number VARCHAR(16), tariff VARCHAR(32), balance INT)")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS phone_contacts (player_id INT, name VARCHAR(32), sim_number VARCHAR(16))")) return 0;
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS phone_sms (id INTEGER PRIMARY KEY AUTOINCREMENT, from_num VARCHAR(16), to_num VARCHAR(16), body TEXT, stamp INT)")) return 0;
    return 1;
}
stock RW_PhoneReset(playerid)
{
    new partner = gPhoneCall[playerid];
    if (partner >= 0 && partner != playerid) {
        gPhoneCall[partner] = -1; gPhoneState[partner] = PHONE_IDLE; gPhoneDial[partner] = 0;
        gPhoneCaller[partner] = false;
    }
    gPhoneItem[playerid] = 0;
    gPhoneNum[playerid] = 0;
    gPhoneBalance[playerid] = 0;
    gPhoneTariff[playerid] = 0;
    gPhoneDnd[playerid] = false;
    gPhoneWall[playerid][0] = EOS;
    gPhoneRing_[playerid][0] = EOS;
    gPhoneCall[playerid] = -1;
    gPhoneState[playerid] = PHONE_IDLE;
    gPhoneDial[playerid] = 0;
    gPhoneTick[playerid] = 0;
    gPhoneBill[playerid] = 0;
    gPhoneCaller[playerid] = false;
    gPhoneShown[playerid] = false;
    return 1;
}
stock RW_PhoneLoad(playerid)
{
    RW_PhoneReset(playerid);
    if (gAccountId[playerid] <= 0) return 0;
    for (new i; i < PHONE_APPS; i++) gPhoneApp[i] = 1;
    new query[200];
    format(query, sizeof query, "SELECT sim_number,tariff,balance FROM sim_cards WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            new tname[32];
            gPhoneNum[playerid] = db_get_field_assoc_int(r, "sim_number");
            gPhoneBalance[playerid] = db_get_field_assoc_int(r, "balance");
            db_get_field_assoc(r, "tariff", tname, sizeof tname);
            gPhoneTariff[playerid] = RW_PhoneTariffId(tname);
        }
        db_free_result(r);
    }
    format(query, sizeof query, "SELECT wallpaper,ringtone,dnd FROM phone_settings WHERE player_id=%d", gAccountId[playerid]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) {
            db_get_field_assoc(r, "wallpaper", gPhoneWall[playerid], sizeof gPhoneWall[]);
            db_get_field_assoc(r, "ringtone", gPhoneRing_[playerid], sizeof gPhoneRing_[]);
            gPhoneDnd[playerid] = db_get_field_assoc_int(r, "dnd") ? true : false;
        }
        db_free_result(r);
    }
    if (!gPhoneWall[playerid][0]) RW_PhoneWallName(0, gPhoneWall[playerid], sizeof gPhoneWall[]);
    if (!gPhoneRing_[playerid][0]) RW_PhoneRingName(0, gPhoneRing_[playerid], sizeof gPhoneRing_[]);
    format(query, sizeof query, "SELECT app_id,installed FROM phone_apps WHERE player_id=%d", gAccountId[playerid]);
    r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r);
        for (new i; i < rows; i++) {
            new appid = db_get_field_assoc_int(r, "app_id");
            if (appid >= 0 && appid < PHONE_APPS) gPhoneApp[appid] = db_get_field_assoc_int(r, "installed") ? 1 : 0;
            db_next_row(r);
        }
        db_free_result(r);
    }
    return 1;
}
stock RW_PhoneSave(playerid)
{
    if (gAccountId[playerid] <= 0 || gPhoneNum[playerid] <= 0) return 0;
    new query[256], tname[32];
    RW_PhoneTariffName(gPhoneTariff[playerid], tname, sizeof tname);
    format(query, sizeof query, "UPDATE sim_cards SET balance=%d,tariff='%s' WHERE player_id=%d", gPhoneBalance[playerid], tname, gAccountId[playerid]);
    return RW_DB_Exec(query) ? 1 : 0;
}
stock bool:RW_PhoneNumberTaken(number)
{
    if (number <= 0) return false;
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gPhoneNum[i] == number) return true;
    new query[160];
    format(query, sizeof query, "SELECT player_id FROM sim_cards WHERE sim_number=%d", number);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return false;
    new bool:taken = db_num_rows(r) > 0;
    db_free_result(r);
    return taken;
}
stock RW_PhoneNewNumber()
{
    for (new attempt; attempt < 250; attempt++) {
        new number = PHONE_NUM_MIN + random(PHONE_NUM_MAX - PHONE_NUM_MIN + 1);
        if (!RW_PhoneNumberTaken(number)) return number;
    }
    return 0;
}
stock RW_PhoneByNumber(number)
{
    if (number <= 0) return -1;
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gLogged[i] && gPhoneNum[i] == number) return i;
    return -1;
}
// Имя абонента: сначала контакты, потом ник того, кто сейчас в сети.
stock RW_PhoneContactName(playerid, number, dest[], size = sizeof dest)
{
    dest[0] = EOS;
    if (number <= 0 || gAccountId[playerid] <= 0) return 0;
    new query[200];
    format(query, sizeof query, "SELECT name FROM phone_contacts WHERE player_id=%d AND sim_number=%d", gAccountId[playerid], number);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        if (db_num_rows(r) > 0) db_get_field_assoc(r, "name", dest, size);
        db_free_result(r);
    }
    if (!dest[0]) {
        new target = RW_PhoneByNumber(number);
        if (target >= 0) RW_GetNick(target, dest, size);
    }
    return 1;
}

/*  ------------------------------- предметы -------------------------------- */

// Телефон лежит в инвентаре; сим-карта расходуется при установке в телефон.
stock RW_PhoneHasItem(playerid)
{
    if (!gLogged[playerid]) return 0;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_PHONE) return gInv[playerid][i][iiUid];
    return 0;
}
stock RW_PhoneSimSlot(playerid)
{
    if (!gLogged[playerid]) return -1;
    for (new i; i < INV_SLOTS; i++) if (gInv[playerid][i][iiType] == ITEM_SIM) return i;
    return -1;
}
// Списать предмет из инвентаря (сим-карта уходит в телефон навсегда).
stock RW_PhoneConsume(playerid, slot)
{
    if (slot < 0 || slot >= INV_SLOTS || gInv[playerid][slot][iiUid] <= 0) return 0;
    new query[200];
    format(query, sizeof query, "DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d", gInv[playerid][slot][iiUid], gAccountId[playerid], slot);
    if (!RW_InvWrite(query)) return 0;
    for (new E_INV_ITEM:j; j < E_INV_ITEM; j++) gInv[playerid][slot][j] = 0;
    RW_InvAttach(playerid);
    if (gUIMode[playerid] == UI_INVENTORY) RW_InvRender(playerid);
    return 1;
}
stock RW_PhoneInsertSim(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You need a phone first.");
    new slot = RW_PhoneSimSlot(playerid);
    if (slot < 0) return RW_Error(playerid, "No SIM card in the inventory.");
    new number = RW_PhoneNewNumber();
    if (!number) return RW_Error(playerid, "No free numbers left. Contact an administrator.");
    new tariff = gInv[playerid][slot][iiData];
    if (tariff < 0 || tariff >= PHONE_TARIFFS) tariff = 0;
    if (!RW_PhoneConsume(playerid, slot)) return RW_Error(playerid, "Could not take the SIM card.");
    new query[220], tname[32];
    RW_PhoneTariffName(tariff, tname, sizeof tname);
    format(query, sizeof query, "DELETE FROM sim_cards WHERE player_id=%d", gAccountId[playerid]);
    RW_DB_Exec(query);
    format(query, sizeof query, "INSERT INTO sim_cards(player_id,sim_number,tariff,balance) VALUES(%d,%d,'%s',%d)", gAccountId[playerid], number, tname, 0);
    if (!RW_DB_Exec(query)) return RW_Error(playerid, "Could not register the SIM card.");
    gPhoneNum[playerid] = number;
    gPhoneBalance[playerid] = 0;
    gPhoneTariff[playerid] = tariff;
    new message[96];
    format(message, sizeof message, "SIM card inserted. Your number is %d.", number);
    return RW_Ok(playerid, message);
}
stock RW_PhoneTopUp(playerid, amount)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    if (amount <= 0) return RW_Error(playerid, "Top up amount must be positive.");
    if (amount > 10000) return RW_Error(playerid, "Top up is limited to 10000 per payment.");
    if (!RW_TakeMoney(playerid, amount)) return RW_Error(playerid, "Not enough cash.");
    gPhoneBalance[playerid] += amount;
    RW_PhoneSave(playerid);
    new message[96];
    format(message, sizeof message, "Balance topped up by %d. Now %d.", amount, gPhoneBalance[playerid]);
    #if RAILWAY_CEF
        RW_CefPhoneState(playerid);
    #endif
    return RW_Ok(playerid, message);
}
stock RW_PhoneTariffSet(playerid, tariff)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    if (tariff < 0 || tariff >= PHONE_TARIFFS) return RW_Error(playerid, "Unknown tariff.");
    if (tariff == gPhoneTariff[playerid]) return RW_Info(playerid, "This tariff is already active.");
    gPhoneTariff[playerid] = tariff;
    RW_PhoneSave(playerid);
    new tname[32], message[96];
    RW_PhoneTariffName(tariff, tname, sizeof tname);
    format(message, sizeof message, "Tariff %s: %d per minute, %d per SMS.", tname, RW_PhoneCallCost(tariff), RW_PhoneSmsCost(tariff));
    #if RAILWAY_CEF
        RW_CefPhoneState(playerid);
    #endif
    return RW_Ok(playerid, message);
}
stock RW_PhoneSettings(playerid, const wallpaper[], const ringtone[], bool:dnd)
{
    if (!gLogged[playerid] || gAccountId[playerid] <= 0) return RW_Error(playerid, "Log in first.");
    new query[256];
    format(query, sizeof query, "DELETE FROM phone_settings WHERE player_id=%d", gAccountId[playerid]);
    RW_DB_Exec(query);
    format(query, sizeof query, "INSERT INTO phone_settings(player_id,wallpaper,ringtone,dnd) VALUES(%d,'%s','%s',%d)", gAccountId[playerid], wallpaper, ringtone, dnd ? 1 : 0);
    RW_DB_Exec(query);
    format(gPhoneWall[playerid], sizeof gPhoneWall[], "%s", wallpaper[0] ? wallpaper : ("dusk"));
    format(gPhoneRing_[playerid], sizeof gPhoneRing_[], "%s", ringtone[0] ? ringtone : ("classic"));
    gPhoneDnd[playerid] = dnd;
    return RW_Ok(playerid, "Phone settings saved.");
}
stock RW_PhoneAppSet(playerid, app, bool:installed)
{
    if (!gLogged[playerid] || gAccountId[playerid] <= 0) return 0;
    if (app < 0 || app >= PHONE_APPS) return 0;
    new query[220];
    format(query, sizeof query, "DELETE FROM phone_apps WHERE player_id=%d AND app_id=%d", gAccountId[playerid], app);
    RW_DB_Exec(query);
    format(query, sizeof query, "INSERT INTO phone_apps(player_id,app_id,installed) VALUES(%d,%d,%d)", gAccountId[playerid], app, installed ? 1 : 0);
    RW_DB_Exec(query);
    gPhoneApp[app] = installed ? 1 : 0;
    return 1;
}

/*  --------------------------------- контакты ------------------------------ */

stock RW_PhoneContactAdd(playerid, const name[], number)
{
    if (!gLogged[playerid] || gAccountId[playerid] <= 0) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    new clean[MAX_PLAYER_NAME];
    format(clean, sizeof clean, "%s", name);
    if (strlen(clean) < 2 || strlen(clean) > 20) return RW_Error(playerid, "Name must be 2 to 20 characters.");
    if (!RW_SafeSqlText(clean)) return RW_Error(playerid, "Name contains forbidden characters.");
    if (number < PHONE_NUM_MIN || number > PHONE_NUM_MAX) return RW_Error(playerid, "Number must be six digits.");
    new query[256];
    format(query, sizeof query, "DELETE FROM phone_contacts WHERE player_id=%d AND sim_number=%d", gAccountId[playerid], number);
    RW_DB_Exec(query);
    format(query, sizeof query, "INSERT INTO phone_contacts(player_id,name,sim_number) VALUES(%d,'%s',%d)", gAccountId[playerid], clean, number);
    if (!RW_DB_Exec(query)) return RW_Error(playerid, "Could not save the contact.");
    new message[128];
    format(message, sizeof message, "%s saved as %d.", clean, number);
    #if RAILWAY_CEF
        RW_CefPhoneContacts(playerid);
    #endif
    return RW_Ok(playerid, message);
}
stock RW_PhoneContactCount(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    new query[200], count;
    format(query, sizeof query, "SELECT name FROM phone_contacts WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    count = db_num_rows(r);
    db_free_result(r);
    return count;
}

/*  ---------------------------------- звонки ------------------------------- */

stock RW_PhoneCall(playerid, number)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    if (gPhoneState[playerid] != PHONE_IDLE) return RW_Error(playerid, "The line is busy.");
    if (number == 911) return RW_Phone911(playerid, 0);
    if (number == gPhoneNum[playerid]) return RW_Error(playerid, "That is your own number.");
    if (number < PHONE_NUM_MIN || number > PHONE_NUM_MAX) return RW_Error(playerid, "Number must be six digits or 911.");
    if (gPhoneBalance[playerid] < RW_PhoneCallCost(gPhoneTariff[playerid])) return RW_Error(playerid, "Not enough credit for a call. Top up in Operator.");
    new target = RW_PhoneByNumber(number);
    if (target < 0) return RW_Error(playerid, "Subscriber is out of coverage.");
    if (gPhoneDnd[target] || gPhoneState[target] != PHONE_IDLE) return RW_Error(playerid, "Subscriber is busy. Try again later.");
    gPhoneCall[playerid] = target; gPhoneState[playerid] = PHONE_DIAL; gPhoneDial[playerid] = number;
    gPhoneCaller[playerid] = true; gPhoneTick[playerid] = GetTickCount(); gPhoneBill[playerid] = GetTickCount();
    gPhoneCall[target] = playerid; gPhoneState[target] = PHONE_RING; gPhoneDial[target] = gPhoneNum[playerid];
    gPhoneCaller[target] = false; gPhoneTick[target] = GetTickCount(); gPhoneBill[target] = GetTickCount();
    new nick[MAX_PLAYER_NAME], who[64], message[128];
    RW_GetNick(playerid, nick);
    RW_PhoneContactName(target, gPhoneNum[playerid], who, sizeof who);
    if (!who[0]) format(who, sizeof who, "%d", gPhoneNum[playerid]);
    format(message, sizeof message, "Incoming call: %s (/pickup or /hangup)", who);
    RW_Info(target, message);
    PlayerPlaySound(target, PHONE_RING_SOUND, 0.0, 0.0, 0.0);
    format(message, sizeof message, "Calling %d...", number);
    RW_Info(playerid, message);
    #if RAILWAY_CEF
        new name[64];
        RW_PhoneContactName(playerid, number, name, sizeof name);
        RW_CefPhoneCallState(playerid, PHONE_DIAL, number, name);
        RW_CefPhoneCallState(target, PHONE_RING, gPhoneNum[playerid], nick);
    #endif
    return 1;
}
stock RW_PhoneAccept(playerid)
{
    if (gPhoneState[playerid] != PHONE_RING) return RW_Error(playerid, "No incoming call.");
    new caller = gPhoneCall[playerid];
    if (caller < 0 || !IsPlayerConnected(caller)) { return RW_PhoneEnd(playerid); }
    new cost = RW_PhoneCallCost(gPhoneTariff[caller]);
    if (gPhoneBalance[caller] < cost) {
        RW_Error(caller, "Not enough credit for the call.");
        return RW_PhoneEnd(playerid);
    }
    gPhoneBalance[caller] -= cost;
    RW_PhoneSave(caller);
    gPhoneState[playerid] = PHONE_TALK; gPhoneState[caller] = PHONE_TALK;
    gPhoneTick[playerid] = GetTickCount(); gPhoneTick[caller] = GetTickCount();
    gPhoneBill[caller] = GetTickCount();
    new nick[MAX_PLAYER_NAME], message[128];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s answered. Talk in chat.", nick);
    RW_Ok(caller, message);
    format(message, sizeof message, "Connected. Talk in chat.");
    RW_Ok(playerid, message);
    #if RAILWAY_CEF
        RW_CefPhoneCallState(playerid, PHONE_TALK, gPhoneDial[playerid], "");
        RW_CefPhoneCallState(caller, PHONE_TALK, gPhoneDial[caller], "");
        RW_CefPhoneState(caller);
    #endif
    return 1;
}
stock RW_PhoneEnd(playerid)
{
    new partner = gPhoneCall[playerid];
    new bool:wasTalk = gPhoneState[playerid] == PHONE_TALK;
    gPhoneCall[playerid] = -1; gPhoneState[playerid] = PHONE_IDLE; gPhoneDial[playerid] = 0;
    gPhoneCaller[playerid] = false;
    if (partner >= 0 && partner != playerid && IsPlayerConnected(partner)) {
        gPhoneCall[partner] = -1; gPhoneState[partner] = PHONE_IDLE; gPhoneDial[partner] = 0;
        gPhoneCaller[partner] = false;
        RW_Info(partner, "Call ended.");
        #if RAILWAY_CEF
            RW_CefPhoneCallState(partner, PHONE_IDLE, 0, "");
        #endif
    }
    if (wasTalk) RW_Info(playerid, "Call ended.");
    #if RAILWAY_CEF
        RW_CefPhoneCallState(playerid, PHONE_IDLE, 0, "");
        RW_CefPhoneState(playerid);
    #endif
    return 1;
}
// Реплика в чате во время разговора уходит собеседнику, а рядом стоящие
// слышат только её половину.
stock RW_PhoneSay(playerid, const text[])
{
    new partner = gPhoneCall[playerid];
    if (gPhoneState[playerid] != PHONE_TALK || partner < 0 || !IsPlayerConnected(partner)) return 1;
    new nick[MAX_PLAYER_NAME], message[256];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "[Phone] %s: %s", nick, text);
    SendClientMessage(partner, 0xF5B04CFF, message);
    format(message, sizeof message, "[Phone] You: %s", text);
    SendClientMessage(playerid, 0xFFD9A0FF, message);
    format(message, sizeof message, "%s (phone): %s", nick, text);
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    for (new i; i < MAX_PLAYERS; i++) {
        if (i == playerid || !IsPlayerConnected(i) || !gLogged[i]) continue;
        if (!IsPlayerInRangeOfPoint(i, PHONE_RANGE_SAY, x, y, z)) continue;
        SendClientMessage(i, COLOR_GREY, message);
    }
    return 0;
}

/*  ------------------------------------ SMS -------------------------------- */

stock RW_PhoneSms(playerid, number, const text[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    if (!text[0]) return RW_Error(playerid, "Empty message.");
    if (strlen(text) > PHONE_SMS_MAX) return RW_Error(playerid, "Message is too long (96 characters).");
    if (number < PHONE_NUM_MIN || number > PHONE_NUM_MAX) return RW_Error(playerid, "Number must be six digits.");
    if (number == gPhoneNum[playerid]) return RW_Error(playerid, "You cannot text yourself.");
    new cost = RW_PhoneSmsCost(gPhoneTariff[playerid]);
    if (gPhoneBalance[playerid] < cost) return RW_Error(playerid, "Not enough credit for an SMS. Top up in Operator.");
    gPhoneBalance[playerid] -= cost;
    RW_PhoneSave(playerid);
    new query[512];
    format(query, sizeof query, "INSERT INTO phone_sms(from_num,to_num,body,stamp) VALUES(%d,%d,'%s',%d)", gPhoneNum[playerid], number, text, gettime());
    RW_DB_Exec(query);
    new target = RW_PhoneByNumber(number);
    if (target >= 0 && !gPhoneDnd[target]) {
        new nick[MAX_PLAYER_NAME], who[64], message[256];
        RW_GetNick(playerid, nick);
        RW_PhoneContactName(target, gPhoneNum[playerid], who, sizeof who);
        if (!who[0]) format(who, sizeof who, "%s", nick);
        format(message, sizeof message, "SMS from %s: %s", who, text);
        RW_Info(target, message);
        PlayerPlaySound(target, 1057, 0.0, 0.0, 0.0);
        #if RAILWAY_CEF
            RW_CefPhoneSmsIn(target, 0, gPhoneNum[playerid], text);
            RW_CefPhoneState(target);
        #endif
    }
    #if RAILWAY_CEF
        RW_CefPhoneSmsIn(playerid, 1, number, text);
        RW_CefPhoneState(playerid);
    #endif
    new receipt[128];
    format(receipt, sizeof receipt, "SMS sent to %d (%d).", number, cost);
    return RW_Ok(playerid, receipt);
}
stock RW_PhoneHistory(playerid, number, dest[], size)
{
    /* Последние сообщения с этим номером — для экрана переписки. */
    dest[0] = EOS;
    if (gAccountId[playerid] <= 0 || gPhoneNum[playerid] <= 0) return 0;
    new query[256];
    format(query, sizeof query, "SELECT from_num,body FROM phone_sms WHERE (from_num=%d AND to_num=%d) OR (from_num=%d AND to_num=%d) ORDER BY id DESC LIMIT 4", gPhoneNum[playerid], number, number, gPhoneNum[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    new body[PHONE_SMS_MAX + 16];
    new line[PHONE_SMS_MAX + 24];
    for (new i; i < rows; i++) {
        new from = db_get_field_assoc_int(r, "from_num");
        db_get_field_assoc(r, "body", body, sizeof body);
        format(line, sizeof line, "%s%s: %s", dest[0] ? ("\n") : (""), from == gPhoneNum[playerid] ? ("You") : ("Them"), body);
        strcat(dest, line, size);
        db_next_row(r);
    }
    db_free_result(r);
    return rows;
}

/*  ------------------------------------- 911 ------------------------------ */

stock RW_Phone911Reason(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "Robbery");
        case 1: format(dest, size, "Shooting");
        case 2: format(dest, size, "Hostages");
        case 3: format(dest, size, "Injured");
        case 4: format(dest, size, "Fire");
        default: format(dest, size, "Other");
    }
    return 1;
}
// Фракций пока нет: вызов уходит всем, кто в сети, и ставит метку на карте.
stock RW_Phone911(playerid, reason)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    if (reason < 0 || reason > 5) reason = 5;
    new nick[MAX_PLAYER_NAME], why[24], message[160];
    RW_GetNick(playerid, nick);
    RW_Phone911Reason(reason, why, sizeof why);
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    format(message, sizeof message, "[911] %s: %s (number %d)", nick, why, gPhoneNum[playerid]);
    RW_EmsCall(playerid, reason, x, y, z);         /* запись для планшета служб */
    new pass, sent;
    for (pass = 0; pass < 2 && !sent; pass++) {
        /* первый проход — только своей службе, второй — всем, если смены нет */
        for (new i; i < MAX_PLAYERS; i++) {
            if (i == playerid || !IsPlayerConnected(i) || !gLogged[i]) continue;
            if (!pass && !RW_EmsWants911(i, reason)) continue;
            SendClientMessage(i, 0xFF6B6BFF, message);
            SetPlayerMapIcon(i, PHONE_911_ICON, x, y, z, 0, 0xFF6B6BFF, MAPICON_GLOBAL);
            sent++;
        }
    }
    RW_Ok(playerid, "Call sent to 911. Stay where you are.");
    if (!sent) RW_Info(playerid, "Nobody else is online right now.");
    return 1;
}

/*  ------------------------------- открытие -------------------------------- */

stock RW_PhoneOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You have no phone. Buy one at a 24/7 shop.");
    if (gPhoneNum[playerid] <= 0) {
        if (RW_CefOn(playerid)) return RW_Error(playerid, "No SIM card: buy one at a 24/7 shop and insert it from your pockets (Alt + U).");
        return RW_Error(playerid, "No SIM card: buy one and insert it with /sim.");
    }
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefPhoneOpen(playerid);
    #endif
    return RW_PhoneMenu(playerid);
}
stock RW_PhoneClose(playerid)
{
    #if RAILWAY_CEF
        RW_CefPhoneClose(playerid);
    #endif
    if (gUIMode[playerid] == UI_PHONE || gUIMode[playerid] == UI_PHONE_CONT) RW_UI_Close(playerid);
    return 1;
}
// Меню для тех, у кого браузера нет: те же действия, что и на странице.
stock RW_PhoneMenu(playerid)
{
    new title[48], balance[32], row[64];
    RW_FormatMoney(gPhoneBalance[playerid], balance, sizeof balance);
    format(title, sizeof title, "%d", gPhoneNum[playerid]);
    RW_UI_Panel(playerid, UI_PHONE, title, "Phone / chat commands do the same");
    format(row, sizeof row, "Balance: %s / tariff %d", balance, gPhoneTariff[playerid]);
    RW_UI_Row(playerid, 0, row, "wallet");
    RW_UI_Row(playerid, 1, "Contacts / call by number", "users");
    RW_UI_Row(playerid, 2, "Messages: /sms <number> <text>", "phone");
    RW_UI_Row(playerid, 3, "Top up: /topup <amount>", "cash");
    RW_UI_Row(playerid, 4, gPhoneDnd[playerid] ? ("Do not disturb: ON") : ("Do not disturb: OFF"), "settings");
    RW_UI_Row(playerid, 5, "Emergency: /911 <reason 0-5>", "alert");
    RW_UI_Row(playerid, 6, "Applications", "info");
    RW_UI_Row(playerid, 7, "Close");
    if (gPhoneState[playerid] == PHONE_RING) RW_Info(playerid, "Incoming call: /pickup or /hangup.");
    return 1;
}
stock RW_PhoneContactsMenu(playerid)
{
    gPhoneContCount = 0;
    new query[220];
    format(query, sizeof query, "SELECT name,sim_number FROM phone_contacts WHERE player_id=%d ORDER BY name", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r != DBResult:0) {
        new rows = db_num_rows(r), index;
        for (new i; i < rows; i++) {
            new number = db_get_field_assoc_int(r, "sim_number");
            new index2 = index++ - gUIPage[playerid]*PHONE_CONTACTS_PAGE;
            if (index2 < 0 || index2 >= PHONE_CONTACTS_PAGE) { db_next_row(r); continue; }
            new name[MAX_PLAYER_NAME], row[64];
            db_get_field_assoc(r, "name", name, sizeof name);
            gPhoneContNum[index2] = number;
            format(row, sizeof row, "%s - %d", name, number);
            RW_UI_Row(playerid, index2, row);
            db_next_row(r);
        }
        gPhoneContCount = index;
        db_free_result(r);
    }
    if (gUIPage[playerid] > 0 && gPhoneContCount <= gUIPage[playerid]*PHONE_CONTACTS_PAGE) {
        gUIPage[playerid] = 0;
        return RW_PhoneContactsMenu(playerid);
    }
    if (!gPhoneContCount) RW_UI_Row(playerid, 0, "No contacts: /addcontact <name> <number>");
    return 1;
}
// Клик по строке меню телефона.
stock RW_PhoneClick(playerid, row, mode)
{
    if (mode == UI_PHONE) {
        if (row == 1) {
            gUIPage[playerid] = 0;
            RW_UI_Panel(playerid, UI_PHONE_CONT, "Contacts", "Click a contact to call");
            RW_PhoneContactsMenu(playerid);
            RW_UI_Row(playerid, 6, "Back to phone");
            if (gPhoneContCount > (gUIPage[playerid]+1)*PHONE_CONTACTS_PAGE) RW_UI_Row(playerid, 7, "Next page >");
            else if (gUIPage[playerid] > 0) RW_UI_Row(playerid, 7, "First page");
            return 1;
        }
        if (row == 4) {
            gPhoneDnd[playerid] = !gPhoneDnd[playerid];
            RW_PhoneSettings(playerid, gPhoneWall[playerid], gPhoneRing_[playerid], gPhoneDnd[playerid]);
            return RW_PhoneMenu(playerid);
        }
        if (row == 7) return RW_UI_Close(playerid);
        if (row == 6) return RW_PhoneAppList(playerid);
        if (row == 3) return RW_Info(playerid, "Type /topup <amount> in chat.");
        if (row == 5) return RW_Info(playerid, "Type /911 <reason 0-5> in chat.");
        if (row == 2) return RW_Info(playerid, "Type /sms <number> <text> in chat.");
        return 1;
    }
    if (mode == UI_PHONE_CONT) {
        if (row == 6) { gUIPage[playerid] = 0; return RW_PhoneMenu(playerid); }
        if (row == 7) {
            if (gPhoneContCount > (gUIPage[playerid]+1)*PHONE_CONTACTS_PAGE) gUIPage[playerid]++;
            else gUIPage[playerid] = 0;
            RW_UI_Panel(playerid, UI_PHONE_CONT, "Contacts", "Click a contact to call");
            RW_PhoneContactsMenu(playerid);
            RW_UI_Row(playerid, 6, "Back to phone");
            if (gPhoneContCount > (gUIPage[playerid]+1)*PHONE_CONTACTS_PAGE) RW_UI_Row(playerid, 7, "Next page >");
            else if (gUIPage[playerid] > 0) RW_UI_Row(playerid, 7, "First page");
            return 1;
        }
        if (row >= 0 && row < PHONE_CONTACTS_PAGE && gPhoneContNum[row] > 0) {
            new number = gPhoneContNum[row];
            return RW_PhoneCall(playerid, number);
        }
        return 1;
    }
    return 1;
}

/*  --------------------------------- таймер -------------------------------- */

public RW_PhonePulse()
{
    for (new p; p < MAX_PLAYERS; p++) {
        if (!IsPlayerConnected(p) || !gLogged[p]) continue;
        if (gPhoneState[p] == PHONE_IDLE) continue;
        new partner = gPhoneCall[p];
        if (partner < 0 || !IsPlayerConnected(partner)) { gPhoneState[p] = PHONE_IDLE; gPhoneCall[p] = -1; gPhoneDial[p] = 0; continue; }
        if (gPhoneState[p] == PHONE_DIAL || gPhoneState[p] == PHONE_RING) {
            if (GetTickCount() - gPhoneTick[p] > PHONE_RING_MS) {
                if (gPhoneState[p] == PHONE_RING) RW_Info(p, "Missed call.");
                else RW_Info(p, "No answer.");
                RW_PhoneEnd(p);
            }
            continue;
        }
        /* разговор: минута стоит денег, платит набравший номер */
        if (GetTickCount() - gPhoneBill[p] < PHONE_BILL_MS) continue;
        gPhoneBill[p] = GetTickCount();
        new payer = gPhoneCaller[p] ? (p) : (partner);
        new cost = RW_PhoneCallCost(gPhoneTariff[payer]);
        if (gPhoneBalance[payer] < cost) {
            RW_Error(payer, "Out of credit: the call was dropped.");
            RW_PhoneEnd(p);
            continue;
        }
        gPhoneBalance[payer] -= cost;
        RW_PhoneSave(payer);
        #if RAILWAY_CEF
            RW_CefPhoneState(payer);
        #endif
    }
    return 1;
}

/*  --------------------------------- команды -------------------------------- */
// Номер есть в контактах?
stock bool:RW_PhoneKnows(playerid, number)
{
    if (gAccountId[playerid] <= 0 || number <= 0) return false;
    new query[200];
    format(query, sizeof query, "SELECT name FROM phone_contacts WHERE player_id=%d AND sim_number=%d", gAccountId[playerid], number);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return false;
    new bool:known = db_num_rows(r) > 0;
    db_free_result(r);
    return known;
}
// Слово params под номером index (считая с нуля).
stock RW_PhoneWord(const params[], index, dest[], size)
{
    dest[0] = EOS;
    new pos, word;
    while (params[pos]) {
        while (params[pos] == ' ' || params[pos] == '\t') pos++;
        if (!params[pos]) break;
        new start = pos;
        while (params[pos] && params[pos] != ' ' && params[pos] != '\t') pos++;
        if (word == index) {
            new len = pos - start;
            if (len >= size) len = size - 1;
            strmid(dest, params, start, start + len, size);
            return 1;
        }
        word++;
    }
    return 0;
}
// Всё, что после первого слова, — это текст сообщения.
stock RW_PhoneRest(const params[], dest[], size)
{
    dest[0] = EOS;
    new pos;
    while (params[pos] == ' ' || params[pos] == '\t') pos++;
    while (params[pos] && params[pos] != ' ' && params[pos] != '\t') pos++;
    while (params[pos] == ' ' || params[pos] == '\t') pos++;
    strmid(dest, params, pos, strlen(params), size);
    return 1;
}
stock RW_PhoneCallCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /call <number> or /call 911");
    return RW_PhoneCall(playerid, strval(word));
}
stock RW_PhoneSmsCommand(playerid, const params[])
{
    new word[32], text[PHONE_SMS_MAX + 16];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /sms <number> <text>");
    RW_PhoneRest(params, text, sizeof text);
    return RW_PhoneSms(playerid, strval(word), text);
}
stock RW_PhoneContactCommand(playerid, const params[])
{
    new name[32], word[32];
    if (!RW_PhoneWord(params, 0, name, sizeof name) || !RW_PhoneWord(params, 1, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /addcontact <name> <number>");
    return RW_PhoneContactAdd(playerid, name, strval(word));
}
stock RW_PhoneTopUpCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /topup <amount>");
    return RW_PhoneTopUp(playerid, strval(word));
}
stock RW_PhoneTariffCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /tariff <0-2>: Start, Active, Maxi");
    return RW_PhoneTariffSet(playerid, strval(word));
}
stock RW_Phone911Command(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word)) return RW_Phone911(playerid, 5);
    if (!RW_IsNumeric(word)) return RW_Error(playerid, "Use: /911 <reason 0-5>");
    return RW_Phone911(playerid, strval(word));
}
stock RW_PhoneContactsCommand(playerid)
{
    if (gPhoneNum[playerid] <= 0) return RW_Error(playerid, "No SIM card inserted.");
    new count = RW_PhoneContactCount(playerid);
    if (!count) return RW_Info(playerid, "No contacts: /addcontact <name> <number>");
    new query[220];
    format(query, sizeof query, "SELECT name,sim_number FROM phone_contacts WHERE player_id=%d ORDER BY name", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "Contacts unavailable.");
    new rows = db_num_rows(r);
    new name[MAX_PLAYER_NAME];
    new line[80];
    RW_Accent(playerid, "Contacts:");
    for (new i; i < rows; i++) {
        db_get_field_assoc(r, "name", name, sizeof name);
        format(line, sizeof line, "%s - %d", name, db_get_field_assoc_int(r, "sim_number"));
        SendClientMessage(playerid, COLOR_GREY, line);
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
