/*  ============================================================================
 *  railway/phoneapps.inc
 *  Приложения телефона: банк, погода, группа, адвокаты, Uber Drive,
 *  Weazel News, Auto Plus и камера.
 *
 *  Сами «иконки» и таблицы настроека уже живут в phone.inc, здесь — то, что
 *  приложения делают. Всё считает сервер: браузерное окно приложения и
 *  TextDraw-меню вызывают одни и те же функции, состояние — в SQLite.
 *
 *  Кодировка исходников: Windows-1251.
 *  ========================================================================= */

#if defined _railway_phoneapps_included
    #endinput
#endif
#define _railway_phoneapps_included

#include <railway/core>
#include <railway/ui>
#include <railway/inventory>
#include <railway/police>
#include <railway/economy>
#include <railway/transport>

#define APP_ICON_ID                 94          /* метка группы на карте   */

/* Погода: индекс в таблице ниже, не игровой id. */
static const gWeatherIds[] = { 0, 1, 4, 8, 9, 16 };
static const gWeatherNames[][] = { "EXTRA SUNNY", "SUNNY", "CLOUDY", "FOGGY", "RAINY", "OVERCAST" };

/* Ориентиры для подписи снимка. */
static const Float:gPhotoSpots[][3] = {
    {  1729.3, -1943.7, 13.5 },     /* 0 STATION  */
    {  1175.8, -1324.7, 15.4 },     /* 1 HOSPITAL */
    {  1660.0, -2260.0, 13.6 },     /* 2 DOCKS    */
    {  1635.0, -1700.0, 13.6 },     /* 3 IMPOUND  */
    {  2645.0, -2020.0, 13.5 },     /* 4 TUNING   */
    {  1352.0, -1759.0, 13.5 }      /* 5 CITY     */
};

new gWeather;
new gWeatherTick;
new gPhoneAppOpen[MAX_PLAYERS];
new gPhoneAppPage[MAX_PLAYERS];
new gAppTick[MAX_PLAYERS];
new gGroupLeader[MAX_PLAYERS];
new gGroupInvite[MAX_PLAYERS];
new bool:gGroupIcon[MAX_PLAYERS];
new gUberWait[MAX_PLAYERS];
new gUberDriver[MAX_PLAYERS];
new gUberPass[MAX_PLAYERS];
new Float:gUberX[MAX_PLAYERS];
new Float:gUberY[MAX_PLAYERS];
new Float:gUberZ[MAX_PLAYERS];
new gUberTick[MAX_PLAYERS];

forward RW_WeatherPulse();
forward RW_GroupPulse();
forward RW_UberPulse();

/*  ----------------------------------- база -------------------------------- */

stock RW_PhoneAppsSchema()
{
    if (!RW_DB_Exec("CREATE TABLE IF NOT EXISTS news_ads (id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INT, body TEXT, stamp INT)")) return 0;
    return 1;
}
stock RW_PhoneAppsReset(playerid)
{
    gPhoneAppOpen[playerid] = -1;
    gPhoneAppPage[playerid] = 0;
    gAppTick[playerid] = 0;
    gUberWait[playerid] = 0;
    if (gUberPass[playerid] >= 0 && IsPlayerConnected(gUberPass[playerid])) {
        gUberDriver[gUberPass[playerid]] = 0;
        gUberWait[gUberPass[playerid]] = 0;
    }
    if (gUberDriver[playerid] >= 0 && IsPlayerConnected(gUberDriver[playerid])) {
        gUberPass[gUberDriver[playerid]] = 0;
        gUberWait[playerid] = 0;
    }
    gUberPass[playerid] = 0;
    gUberDriver[playerid] = 0;
    RW_GroupLeave(playerid, true);
    if (gGroupIcon[playerid]) { RemovePlayerMapIcon(playerid, APP_ICON_ID); gGroupIcon[playerid] = false; }
    return 1;
}

/*  ---------------------------------- погода ------------------------------- */
/*  Погода одна на весь сервер и влияет на оплату заданий: ясно — быстрее     *
 *  дорога, дождь мешает курьеру, туман — дальнобойщику.                     */

stock RW_WeatherId() { return gWeather; }
stock RW_WeatherName(dest[], size = sizeof dest)
{
    format(dest, size, "%s", gWeatherNames[(gWeather >= 0 && gWeather < WEATHER_COUNT) ? (gWeather) : (0)]);
    return 1;
}
// Проценты к оплате задания: плюс или минус в зависимости от погоды.
stock RW_WeatherBonus(job)
{
    switch (gWeather) {
        case 0, 1: return (job == JOB_TRUCKER || job == JOB_COURIER) ? (20) : (0);
        case 4, 9: return (job == JOB_COURIER) ? (-10) : (0);
        case 3: return (job == JOB_TRUCKER) ? (-15) : (0);
    }
    return 0;
}
stock RW_WeatherSet(index)
{
    if (index < 0 || index >= WEATHER_COUNT) index = 0;
    gWeather = index;
    gWeatherTick = 0;
    SetWeather(gWeatherIds[index]);
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i)) SetPlayerWeather(i, gWeatherIds[index]);
    return 1;
}
public RW_WeatherPulse()
{
    gWeatherTick++;
    if (gWeatherTick >= WEATHER_CHANGE_SECONDS) {
        new next = random(WEATHER_COUNT);
        if (next == gWeather) next = (next + 1) % WEATHER_COUNT;
        RW_WeatherSet(next);
    }
    return 1;
}

/*  ---------------------------------- камера ------------------------------- */
/*  Снимок — предмет: хранит ориентир и время, его можно показать рядом.      */

stock RW_PhotoPlaceName(id, dest[], size = sizeof dest)
{
    switch (id) {
        case 0: format(dest, size, "STATION");
        case 1: format(dest, size, "HOSPITAL");
        case 2: format(dest, size, "DOCKS");
        case 3: format(dest, size, "IMPOUND");
        case 4: format(dest, size, "TUNING");
        default: format(dest, size, "CITY");
    }
    return 1;
}
// Ближайший ориентир: снимок подписывается по нему.
stock RW_PhotoPlaceAt(Float:x, Float:y, Float:z)
{
    new best = PHOTO_PLACES - 1, Float:dist = 99999.0;
    for (new i; i < PHOTO_PLACES; i++) {
        new Float:d = VectorSize(gPhotoSpots[i][0] - x, gPhotoSpots[i][1] - y, gPhotoSpots[i][2] - z);
        if (d < dist) { dist = d; best = i; }
    }
    return (dist <= 250.0) ? (best) : (PHOTO_PLACES - 1);
}
// Снимок стоит батарейки телефона: минута разговора по тарифу.
stock RW_PhotoTake(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You need a phone to take a shot.");
    if (gPhoneBalance[playerid] < RW_PhoneSmsCost(gPhoneTariff[playerid])) return RW_Error(playerid, "Not enough credit on the SIM for a shot.");
    if (GetPlayerState(playerid) == PLAYER_STATE_DRIVER) return RW_Error(playerid, "Stop the car first: no photos at the wheel.");
    new Float:x, Float:y, Float:z, place, minute = (gettime() / 60) % 1440;
    GetPlayerPos(playerid, x, y, z);
    place = RW_PhotoPlaceAt(x, y, z);
    if (RW_InvFree(playerid, 0, ITEM_PHOTO, place, 1) < 0) return RW_Error(playerid, "No room for the photo in your pockets.");
    new uid = RW_InvGrant(playerid, ITEM_PHOTO, place, minute);
    if (!uid) return RW_Error(playerid, "The camera app failed.");
    gPhoneBalance[playerid] -= RW_PhoneSmsCost(gPhoneTariff[playerid]);
    RW_PhoneSave(playerid);
    ApplyAnimation(playerid, "CAMERA", "CAMSTND_TO_CAMCRCH", 4.1, 0, 1, 1, 0, 800);
    new name[24], message[144];
    RW_PhotoPlaceName(place, name, sizeof name);
    format(message, sizeof message, "Shot taken near %s at %02d:%02d.", name, minute / 60, minute % 60);
    return RW_Ok(playerid, message);
}
stock RW_PhotoShow(playerid)
{
    new slot = RW_InvFind(playerid, gInvSelected[playerid]);
    if (slot < 0 || gInv[playerid][slot][iiType] != ITEM_PHOTO) return RW_Error(playerid, "Select a photo in the inventory first.");
    new place[24], message[160], nick[MAX_PLAYER_NAME];
    RW_PhotoPlaceName(gInv[playerid][slot][iiData], place, sizeof place);
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s shows a photo: %s at %02d:%02d.", nick, place,
        gInv[playerid][slot][iiAmount] / 60, gInv[playerid][slot][iiAmount] % 60);
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i]) continue;
        if (!IsPlayerInRangeOfPoint(i, 8.0, x, y, z)) continue;
        SendClientMessage(i, COLOR_GREY, message);
    }
    return 1;
}
stock RW_PhotoCommand(playerid) { return RW_PhotoShow(playerid); }

/*  ---------------------------------- группа ------------------------------- */
/*  Группа: чат с делами и метка лидера на карте. Живёт в памяти сервера.     */

stock bool:RW_GroupHas(playerid) { return gGroupLeader[playerid] >= 0 && IsPlayerConnected(gGroupLeader[playerid]); }
stock RW_GroupCreate(playerid)
{
    if (RW_GroupHas(playerid)) return RW_Info(playerid, "You are already in a group. /gleave first.");
    gGroupLeader[playerid] = playerid;
    gGroupInvite[playerid] = -1;
    return RW_Ok(playerid, "Group created. /ginvite <id> invites a player, /g <text> writes to the group.");
}
stock RW_GroupInvite(playerid, target)
{
    if (target < 0 || target >= MAX_PLAYERS || target == playerid || !IsPlayerConnected(target) || !gLogged[target])
        return RW_Error(playerid, "Player with that id is not online.");
    if (!RW_GroupHas(playerid)) RW_GroupCreate(playerid);
    gGroupInvite[target] = playerid;
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s invites you into a group. /gaccept to join.", nick);
    RW_Info(target, message);
    format(message, sizeof message, "Invited. Waiting for an answer.");
    return RW_Info(playerid, message);
}
stock RW_GroupAccept(playerid)
{
    new leader = gGroupInvite[playerid];
    if (leader < 0 || !IsPlayerConnected(leader) || !gLogged[leader]) return RW_Error(playerid, "No invitation to accept.");
    gGroupInvite[playerid] = -1;
    gGroupLeader[playerid] = leader;
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "%s joined the group.", nick);
    return RW_GroupSay_(leader, message);
}
stock RW_GroupLeave(playerid, bool:silent = false)
{
    if (!RW_GroupHas(playerid)) return 0;
    new leader = gGroupLeader[playerid];
    gGroupLeader[playerid] = -1;
    if (gGroupIcon[playerid]) { RemovePlayerMapIcon(playerid, APP_ICON_ID); gGroupIcon[playerid] = false; }
    if (leader == playerid) {
        for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gGroupLeader[i] == playerid) gGroupLeader[i] = -1;
        if (!silent) return RW_Info(playerid, "Group closed.");
        return 1;
    }
    if (!silent) {
        new nick[MAX_PLAYER_NAME], message[160];
        RW_GetNick(playerid, nick);
        format(message, sizeof message, "%s left the group.", nick);
        if (IsPlayerConnected(leader)) RW_GroupSay_(leader, message);
        return RW_Info(playerid, "You left the group.");
    }
    return 1;
}
// Внутренняя рассылка без проверок: ею пользуются принятие и выход.
stock RW_GroupSay_(leader, const text[])
{
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gLogged[i] && gGroupLeader[i] == leader) SendClientMessage(i, 0xF5B04CFF, text);
    return 1;
}
stock RW_GroupSay(playerid, const text[])
{
    if (!RW_GroupHas(playerid)) return RW_Error(playerid, "You are not in a group. /job has nothing to do with it: /ginvite first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You need a phone for the group chat.");
    if (gPhoneBalance[playerid] < RW_PhoneSmsCost(gPhoneTariff[playerid])) return RW_Error(playerid, "Not enough credit on the SIM.");
    gPhoneBalance[playerid] -= RW_PhoneSmsCost(gPhoneTariff[playerid]);
    RW_PhoneSave(playerid);
    new nick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(playerid, nick);
    format(message, sizeof message, "[G] %s: %s", nick, text);
    return RW_GroupSay_(gGroupLeader[playerid], message);
}
// Метка лидера: обновляется раз в секунду, пока группа жива.
public RW_GroupPulse()
{
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i]) continue;
        new leader = gGroupLeader[i];
        if (leader < 0 || !IsPlayerConnected(leader) || leader == i) {
            if (gGroupIcon[i]) { RemovePlayerMapIcon(i, APP_ICON_ID); gGroupIcon[i] = false; }
            continue;
        }
        new Float:x, Float:y, Float:z;
        GetPlayerPos(leader, x, y, z);
        SetPlayerMapIcon(i, APP_ICON_ID, x, y, z, 0, 0x7BD88FFF, MAPICON_GLOBAL);
        gGroupIcon[i] = true;
    }
    return 1;
}
stock RW_GroupInfo(playerid)
{
    if (!RW_GroupHas(playerid)) return RW_Info(playerid, "No group. /ginvite <id> creates one.");
    new leader = gGroupLeader[playerid], count, names[256], nick[MAX_PLAYER_NAME];
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || gGroupLeader[i] != leader) continue;
        RW_GetNick(i, nick);
        if (count) strcat(names, ", ", sizeof names);
        strcat(names, nick, sizeof names);
        count++;
    }
    new message[320];
    format(message, sizeof message, "Group of %d: %s. /g <text> writes to them.", count, names);
    return RW_Info(playerid, message);
}

/*  ------------------------------- банк в телефоне ------------------------- */
/*  Переводы и штрафы — из любого места, наличные — только у банкомата.       */

stock RW_AppFinesSum(playerid)
{
    new query[220], total;
    format(query, sizeof query, "SELECT amount FROM fines WHERE player_id=%d AND paid=0", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    for (new i; i < rows; i++) { total += db_get_field_assoc_int(r, "amount"); db_next_row(r); }
    db_free_result(r);
    return total;
}
stock RW_AppPayFines(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new sum = RW_AppFinesSum(playerid);
    if (sum <= 0) return RW_Info(playerid, "No unpaid fines.");
    if (gBank[playerid] >= sum) RW_BankTake(playerid, sum);
    else if (gMoney[playerid] >= sum) RW_TakeMoney(playerid, sum);
    else return RW_Error(playerid, "Not enough money for the fines.");
    new query[200];
    format(query, sizeof query, "UPDATE fines SET paid=1 WHERE player_id=%d AND paid=0", gAccountId[playerid]);
    RW_DB_Exec(query);
    new money[32], message[160];
    RW_FormatMoney(sum, money, sizeof money);
    format(message, sizeof message, "Fines paid: %s. Unpaid left: none.", money);
    return RW_Ok(playerid, message);
}

/*  ------------------------------ адвокаты --------------------------------- */
/*  Вызов только из КПЗ: адвокат получает сообщение и coordinates.           */

stock bool:RW_LawyerLicensed(playerid)
{
    return gLawyerLicense[playerid] && gLawyerExpire[playerid] > gettime();
}
stock RW_AppLawyerCall(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (gJailTime[playerid] <= 0) return RW_Error(playerid, "Lawyers are called from a cell only.");
    new Float:x, Float:y, Float:z, nick[MAX_PLAYER_NAME], found;
    GetPlayerPos(playerid, x, y, z);
    RW_GetNick(playerid, nick);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || !RW_LawyerLicensed(i)) continue;
        new message[160];
        format(message, sizeof message, "%s needs a lawyer in the cells (%d, %d). /lawyer to take the case.", nick, floatround(x), floatround(y));
        RW_Info(i, message);
        found++;
    }
    if (!found) return RW_Error(playerid, "No lawyer is on duty right now. Try later or call 911.");
    return RW_Ok(playerid, "Called: every lawyer on duty got your name and the cell coordinates.");
}
// Раздел залога: половина адвокату, десятая — полицейскому, остаток в казну.
stock RW_LawyerBailShare(lawyer, officer, bail)
{
    if (bail <= 0) return 0;
    new lawyerPart = bail * 50 / 100, copPart = bail * 10 / 100;
    if (lawyer >= 0 && lawyer < MAX_PLAYERS && IsPlayerConnected(lawyer) && gLogged[lawyer]) {
        gBank[lawyer] += lawyerPart;
        RW_EconomySave(lawyer);
        new message[128];
        format(message, sizeof message, "Fee: %d of the bail is on your account.", lawyerPart);
        RW_Ok(lawyer, message);
    }
    if (officer >= 0 && officer < MAX_PLAYERS && IsPlayerConnected(officer) && gLogged[officer]) {
        gBank[officer] += copPart;
        RW_EconomySave(officer);
        new message[128];
        format(message, sizeof message, "Paperwork: %d of the bail is on your account.", copPart);
        RW_Ok(officer, message);
    }
    return 1;
}

/*  ------------------------------- Uber Drive ------------------------------ */
/*  Пассажир заказывает машину, водитель на такси принимает заказ; оплата    *
 *  снимается, когда пассажир выходит: база плюс расстояние.                 */

#define UBER_ICON 93

static const gTaxiModels[] = { 420, 438 };

stock bool:RW_UberTaxi(playerid)
{
    new vehicleid = GetPlayerVehicleID(playerid);
    if (!vehicleid) return false;
    new model = GetVehicleModel(vehicleid);
    for (new i; i < sizeof gTaxiModels; i++) if (gTaxiModels[i] == model) return true;
    return false;
}
stock RW_UberOrder(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You need a phone to order a taxi.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Step out of the car first.");
    if (gUberWait[playerid] == 1) return RW_Info(playerid, "You already have an order. Wait for a driver.");
    if (gUberDriver[playerid] > 0) return RW_Info(playerid, "Your taxi is already on the way.");
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);
    gUberWait[playerid] = 1;
    gUberX[playerid] = x; gUberY[playerid] = y; gUberZ[playerid] = z;
    gUberTick[playerid] = GetTickCount();
    new nick[MAX_PLAYER_NAME], found;
    RW_GetNick(playerid, nick);
    for (new i; i < MAX_PLAYERS; i++) {
        if (!IsPlayerConnected(i) || !gLogged[i] || !RW_UberTaxi(i) || gUberPass[i] > 0) continue;
        new Float:vx, Float:vy, Float:vz;
        GetPlayerPos(i, vx, vy, vz);
        new message[160];
        format(message, sizeof message, "Order: %s is %d m away. /uber to take it.", nick, floatround(VectorSize(vx - x, vy - y, vz - z)));
        RW_Info(i, message);
        found++;
    }
    if (!found) return RW_Info(playerid, "Ordered, but no taxi is free right now. Try again in a minute.");
    return RW_Ok(playerid, "Ordered: the drivers got your coordinates.");
}
stock RW_UberAccept(driver, passenger)
{
    if (passenger < 0 || passenger >= MAX_PLAYERS || !IsPlayerConnected(passenger) || gUberWait[passenger] != 1)
        return RW_Error(driver, "That order is gone.");
    if (!RW_UberTaxi(driver) || GetPlayerState(driver) != PLAYER_STATE_DRIVER) return RW_Error(driver, "Take a taxi first.");
    if (gUberPass[driver] > 0) return RW_Error(driver, "You already carry a passenger.");
    gUberDriver[passenger] = driver;
    gUberPass[driver] = passenger;
    gUberWait[passenger] = 2;
    SetPlayerMapIcon(driver, UBER_ICON, gUberX[passenger], gUberY[passenger], gUberZ[passenger], 0, 0xF5B04CFF, MAPICON_GLOBAL);
    new dnick[MAX_PLAYER_NAME], pnick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(driver, dnick);
    RW_GetNick(passenger, pnick);
    format(message, sizeof message, "%s accepted: the mark on the map is %s.", dnick, pnick);
    RW_Ok(passenger, message);
    format(message, sizeof message, "%s is waiting at the mark. Fare: 50 plus 2 for every 10 m.", pnick);
    return RW_Ok(driver, message);
}
// Расчёт в момент высадки: база плюс расстояние от посадки.
stock RW_UberFinish(driver)
{
    new passenger = gUberPass[driver];
    if (passenger <= 0 || !IsPlayerConnected(passenger)) return RW_Error(driver, "No passenger in the car.");
    new Float:x, Float:y, Float:z;
    GetPlayerPos(driver, x, y, z);
    new metres = floatround(VectorSize(x - gUberX[passenger], y - gUberY[passenger], z - gUberZ[passenger]));
    new fare = UBER_BASE_FARE + metres * UBER_PER_METRE / 10;
    RemovePlayerMapIcon(driver, UBER_ICON);
    gUberPass[driver] = 0;
    gUberDriver[passenger] = 0;
    gUberWait[passenger] = 0;
    if (gMoney[passenger] < fare) {
        if (gBank[passenger] >= fare) RW_BankTake(passenger, fare);
        else fare = gMoney[passenger];
    }
    RW_TakeMoney(passenger, fare);
    RW_AddMoney(driver, fare);
    new dnick[MAX_PLAYER_NAME], pnick[MAX_PLAYER_NAME], message[160];
    RW_GetNick(driver, dnick);
    RW_GetNick(passenger, pnick);
    format(message, sizeof message, "Ride with %s: %d m, fare %d. Thanks for riding Uber Drive.", pnick, metres, fare);
    RW_Ok(driver, message);
    format(message, sizeof message, "You paid %s for %d m. Cash: %d.", dnick, metres, gMoney[passenger]);
    return RW_Ok(passenger, message);
}
public RW_UberPulse()
{
    for (new d; d < MAX_PLAYERS; d++) {
        new passenger = gUberPass[d];
        if (passenger <= 0) continue;
        if (!IsPlayerConnected(d) || !IsPlayerConnected(passenger)) { gUberPass[d] = 0; gUberDriver[passenger] = 0; continue; }
        new vehicleid = GetPlayerVehicleID(d);
        if (gUberWait[passenger] == 3 && (vehicleid == 0 || GetPlayerVehicleID(passenger) != vehicleid)) RW_UberFinish(d);
        else if (gUberWait[passenger] == 2 && vehicleid > 0 && GetPlayerVehicleID(passenger) == vehicleid) gUberWait[passenger] = 3;
    }
    for (new i; i < MAX_PLAYERS; i++) {
        if (gUberWait[i] != 1) continue;
        if (GetTickCount() - gUberTick[i] > 120000) { gUberWait[i] = 0; if (IsPlayerConnected(i)) RW_Info(i, "No taxi took the order. It is cancelled."); }
    }
    return 1;
}

/*  ------------------------------ Weazel News ------------------------------ */

stock RW_NewsList(playerid)
{
    new query[260];
    format(query, sizeof query, "SELECT a.name AS who,n.body,n.stamp FROM news_ads n LEFT JOIN accounts a ON a.id=n.player_id ORDER BY n.id DESC LIMIT 10");
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return RW_Info(playerid, "No adverts yet. /ad <text> posts one for 100.");
    new rows = db_num_rows(r);
    if (!rows) { db_free_result(r); return RW_Info(playerid, "No adverts yet. /ad <text> posts one for 100."); }
    RW_Accent(playerid, "Weazel News, last adverts:");
    for (new i; i < rows; i++) {
        new name[MAX_PLAYER_NAME], body[128], line[200];
        db_get_field_assoc(r, "who", name, sizeof name);
        db_get_field_assoc(r, "body", body, sizeof body);
        format(line, sizeof line, "%s: %s", name, body);
        SendClientMessage(playerid, COLOR_GREY, line);
        db_next_row(r);
    }
    db_free_result(r);
    return 1;
}
stock RW_NewsPost(playerid, const text[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (!text[0]) return RW_Error(playerid, "Use: /ad <text>.");
    if (strlen(text) > NEWS_MAX_LEN) return RW_Error(playerid, "Too long: keep it under 96 characters.");
    if (gMoney[playerid] < NEWS_PRICE) return RW_Error(playerid, "An advert costs 100 in cash.");
    RW_TakeMoney(playerid, NEWS_PRICE);
    new nick[MAX_PLAYER_NAME], safe[MAX_PLAYER_NAME], query[320];
    RW_GetNick(playerid, nick);
    RW_PoliceSqlName(nick, safe, sizeof safe);
    format(query, sizeof query, "INSERT INTO news_ads(player_id,body,stamp) VALUES(%d,'%s',%d)", gAccountId[playerid], safe, gettime());
    RW_DB_Exec(query);
    new message[160];
    format(message, sizeof message, "Advert posted for %d. It stays in the app for everyone.", NEWS_PRICE);
    return RW_Ok(playerid, message);
}

/*  ------------------------------- Auto Plus ------------------------------- */
/*  Список личных машин и доставка: машина переезжает к владельцу за деньги.  */

stock RW_AutoCars(playerid, dest[][2], limit)
{
    new count;
    new query[400];
    format(query, sizeof query, "SELECT DISTINCT c.id,c.model FROM owned_cars c JOIN inventory_items i ON i.data=c.token AND i.type=2 WHERE i.owner=%d ORDER BY c.id LIMIT %d", gAccountId[playerid], limit);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    new rows = db_num_rows(r);
    for (new i; i < rows && count < limit; i++) {
        dest[count][0] = db_get_field_assoc_int(r, "id");
        dest[count][1] = db_get_field_assoc_int(r, "model");
        count++;
        db_next_row(r);
    }
    db_free_result(r);
    return count;
}
// Где машина сейчас: слот в gVeh по токену, иначе её нет в мире.
stock RW_AutoSlot(carid)
{
    new query[160], token;
    format(query, sizeof query, "SELECT token FROM owned_cars WHERE id=%d", carid);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return -1;
    if (db_num_rows(r) == 1) token = db_get_field_assoc_int(r, "token");
    db_free_result(r);
    if (token <= 0) return -1;
    for (new s; s < VEHICLE_SLOTS; s++) if (gVeh[s][vsUsed] && gVehKeyUid[s] == token) return s;
    return -1;
}
stock RW_AutoDeliver(playerid, carid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new slot = RW_AutoSlot(carid);
    if (slot < 0) return RW_Error(playerid, "That car is in the garage: take it from the garage lot.");
    new vehicleid = gVeh[slot][vsVehicleId];
    for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && IsPlayerInAnyVehicle(i) && GetPlayerVehicleID(i) == vehicleid)
        return RW_Error(playerid, "Somebody sits in it: delivery refused.");
    new Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz;
    GetPlayerPos(playerid, x, y, z);
    GetVehiclePos(vehicleid, vx, vy, vz);
    if (VectorSize(vx - x, vy - y, vz - z) < AUTO_DELIVER_DISTANCE) return RW_Info(playerid, "The car is already near you.");
    if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return RW_Error(playerid, "Step out of the car first.");
    if (gMoney[playerid] < AUTO_DELIVER_FEE) return RW_Error(playerid, "Delivery costs 200 in cash.");
    RW_TakeMoney(playerid, AUTO_DELIVER_FEE);
    new Float:a;
    GetPlayerFacingAngle(playerid, a);
    SetVehiclePos(vehicleid, x + 5.0 * floatsin(-a, degrees), y + 5.0 * floatcos(-a, degrees), z + 0.5);
    SetVehicleZAngle(vehicleid, a);
    PutPlayerInVehicle(playerid, vehicleid, 0);
    new name[48], message[160];
    RW_VehicleName(gVeh[slot][vsModel], name, sizeof name);
    format(message, sizeof message, "%s delivered for 200. Cash: %d.", name, gMoney[playerid]);
    return RW_Ok(playerid, message);
}

/*  ------------------------------ оболочка --------------------------------- */
/*  Один режим меню на все приложения: список приложений и экран каждого.     */

new gAppIds[MAX_PLAYERS][6];
new gAppCount[MAX_PLAYERS];

// Заголовок окна: название приложения и строка состояния.
stock RW_AppTitle(playerid, title[], tsize, subtitle[], ssize)
{
    new app = gPhoneAppOpen[playerid];
    if (app < 0) {
        format(title, tsize, "%s", "Applications");
        format(subtitle, ssize, "%s", "Page 1 of 2");
        return 1;
    }
    new name[32];
    RW_PhoneAppName(app, name, sizeof name);
    format(title, tsize, "%s", name);
    new weather[24];
    RW_WeatherName(weather, sizeof weather);
    switch (app) {
        case PHONE_APP_BANK: {
            new cash[32], acc[32];
            RW_FormatMoney(gMoney[playerid], cash, sizeof cash);
            RW_FormatMoney(gBank[playerid], acc, sizeof acc);
            format(subtitle, ssize, "Cash %s / Account %s", cash, acc);
        }
        case PHONE_APP_WEATHER: format(subtitle, ssize, "%s, next change in %d s", weather, WEATHER_CHANGE_SECONDS - gWeatherTick);
        case PHONE_APP_GROUP: format(subtitle, ssize, "%s", RW_GroupHas(playerid) ? ("In a group") : ("No group"));
        case PHONE_APP_LAWYER: format(subtitle, ssize, "%s", gJailTime[playerid] > 0 ? ("You are in a cell") : ("Free"));
        case PHONE_APP_UBER: format(subtitle, ssize, "Fare 50 plus 2 for every 10 m");
        case PHONE_APP_NEWS: format(subtitle, ssize, "An advert costs 100");
        case PHONE_APP_AUTO: format(subtitle, ssize, "Delivery 200 per car");
        case PHONE_APP_CAMERA: format(subtitle, ssize, "One shot costs one SMS");
        default: format(subtitle, ssize, "%s", "App");
    }
    return 1;
}
// Строки экрана: общий для браузера и TextDraw, чтобы лица не расходились.
stock RW_AppRow(playerid, row, const text[], const arg[], const icon[])
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefRow(playerid, row, text, arg, icon); return 1; }
    #else
        #pragma unused arg, icon
    #endif
    RW_UI_Row(playerid, row, text);
    return 1;
}
stock RW_AppRows(playerid)
{
    new app = gPhoneAppOpen[playerid];
    for (new i; i < 6; i++) gAppIds[playerid][i] = 0;
    gAppCount[playerid] = 0;
    if (app < 0) {
        new offset = gPhoneAppPage[playerid] * 6;
        for (new i; i < 6; i++) {
            new id = offset + i;
            if (id >= PHONE_APPS) break;
            new name[32], icon[16], label[64];
            RW_PhoneAppName(id, name, sizeof name);
            RW_PhoneAppIcon(id, icon, sizeof icon);
            format(label, sizeof label, "%s%s", name, gPhoneApp[id] ? ("") : (" (not installed)"));
            gAppIds[playerid][i] = id + 1;
            gAppCount[playerid]++;
            RW_AppRow(playerid, i, label, "", icon);
        }
        if (gPhoneAppPage[playerid] > 0) RW_AppRow(playerid, 6, "First page", "", "back");
        else RW_AppRow(playerid, 6, "Next page >", "", "next");
        return 1;
    }
    new row[80], money[32];
    switch (app) {
        case PHONE_APP_BANK: {
            new cash[32], acc[32];
            RW_FormatMoney(gMoney[playerid], cash, sizeof cash);
            RW_FormatMoney(gBank[playerid], acc, sizeof acc);
            format(row, sizeof row, "Balance: cash %s, account %s", cash, acc);
            RW_AppRow(playerid, 0, row, "", "wallet");
            new fines = RW_AppFinesSum(playerid);
            RW_FormatMoney(fines, money, sizeof money);
            format(row, sizeof row, "Pay my fines: %s", fines > 0 ? (money) : ("none"));
            RW_AppRow(playerid, 1, row, "pay", "law");
            RW_AppRow(playerid, 2, "Cash terminal (only at an ATM)", "atm", "bank");
            RW_AppRow(playerid, 3, "Transfer: /transfer <id> <sum>", "", "users");
            RW_AppRow(playerid, 4, "Salary and interest: /payday", "", "signal");
        }
        case PHONE_APP_WEATHER: {
            new weather[24];
            RW_WeatherName(weather, sizeof weather);
            format(row, sizeof row, "Now: %s", weather);
            RW_AppRow(playerid, 0, row, "", "sun");
            new bonus = RW_WeatherBonus(gJob[playerid]);
            format(row, sizeof row, "Your job today: %+d%% to the task pay", bonus);
            RW_AppRow(playerid, 1, row, "", "gear");
            format(row, sizeof row, "Next change in %d s", WEATHER_CHANGE_SECONDS - gWeatherTick);
            RW_AppRow(playerid, 2, row, "", "clock");
            RW_AppRow(playerid, 3, "Clear sky pays couriers and truckers +20%", "", "info");
            RW_AppRow(playerid, 4, "Rain and fog cut the pay: -10% and -15%", "", "info");
        }
        case PHONE_APP_GROUP: {
            RW_AppRow(playerid, 0, RW_GroupHas(playerid) ? ("Members: see /group in chat") : ("No group yet"), "", "group");
            RW_AppRow(playerid, 1, RW_GroupHas(playerid) ? ("Leave the group") : ("Create a group"), "group", "group");
            RW_AppRow(playerid, 2, "Invite: /ginvite <player id>", "", "users");
            RW_AppRow(playerid, 3, "Write: /g <text>", "", "chat");
            RW_AppRow(playerid, 4, "The leader's mark is on your map", "", "map");
        }
        case PHONE_APP_LAWYER: {
            RW_AppRow(playerid, 0, "Call a lawyer (from a cell only)", "call", "law");
            new found;
            for (new i; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gLogged[i] && RW_LawyerLicensed(i)) found++;
            format(row, sizeof row, "Lawyers on duty: %d", found);
            RW_AppRow(playerid, 1, row, "", "users");
            format(row, sizeof row, "My licence: %s", RW_LawyerLicensed(playerid) ? ("valid") : ("none"));
            RW_AppRow(playerid, 2, row, "", "law");
            RW_AppRow(playerid, 3, "Bail: 50% to the lawyer, 10% to the officer", "", "wallet");
            RW_AppRow(playerid, 4, "Price for a case: /lawprice <sum>", "", "gear");
        }
        case PHONE_APP_UBER: {
            if (gUberDriver[playerid] > 0) {
                new nick[MAX_PLAYER_NAME];
                RW_GetNick(gUberDriver[playerid], nick);
                format(row, sizeof row, "Taxi: %s is on the way", nick);
                RW_AppRow(playerid, 0, row, "", "car");
                RW_AppRow(playerid, 1, "Cancel the order", "cancel", "danger");
            } else if (gUberPass[playerid] > 0) {
                new nick[MAX_PLAYER_NAME];
                RW_GetNick(gUberPass[playerid], nick);
                format(row, sizeof row, "Passenger: %s", nick);
                RW_AppRow(playerid, 0, row, "", "users");
                RW_AppRow(playerid, 1, "Finish the ride and take the fare", "finish", "wallet");
            } else if (RW_UberTaxi(playerid)) {
                for (new i; i < MAX_PLAYERS && gAppCount[playerid] < 6; i++) {
                    if (!IsPlayerConnected(i) || !gLogged[i] || gUberWait[i] != 1) continue;
                    new nick[MAX_PLAYER_NAME], Float:x, Float:y, Float:z;
                    GetPlayerPos(playerid, x, y, z);
                    RW_GetNick(i, nick);
                    format(row, sizeof row, "%s - %d m", nick, floatround(VectorSize(x - gUberX[i], y - gUberY[i], z - gUberZ[i])));
                    gAppIds[playerid][gAppCount[playerid]] = i;
                    RW_AppRow(playerid, gAppCount[playerid], row, "take", "car");
                    gAppCount[playerid]++;
                }
                if (!gAppCount[playerid]) RW_AppRow(playerid, 0, "No orders yet: drive around the city", "", "car");
            } else {
                RW_AppRow(playerid, 0, "Order a taxi to this spot", "order", "car");
                RW_AppRow(playerid, 1, "Fare: 50 plus 2 for every 10 m", "", "wallet");
                RW_AppRow(playerid, 2, "Only a taxi (Cabbie or Taxi) takes orders", "", "info");
            }
        }
        case PHONE_APP_NEWS: {
            RW_AppRow(playerid, 0, "Post an advert: /ad <text>", "", "news");
            new query[260], count;
            format(query, sizeof query, "SELECT a.name AS who,n.body FROM news_ads n LEFT JOIN accounts a ON a.id=n.player_id ORDER BY n.id DESC LIMIT 5");
            new DBResult:r = db_query(gDB, query);
            if (r != DBResult:0) {
                new rows = db_num_rows(r);
                for (new i; i < rows && count < 5; i++) {
                    new name[MAX_PLAYER_NAME], body[96];
                    db_get_field_assoc(r, "who", name, sizeof name);
                    db_get_field_assoc(r, "body", body, sizeof body);
                    format(row, sizeof row, "%s: %s", name, body);
                    RW_AppRow(playerid, 1 + count, row, "", "news");
                    count++;
                    db_next_row(r);
                }
                db_free_result(r);
            }
            if (!count) RW_AppRow(playerid, 1, "No adverts yet", "", "news");
        }
        case PHONE_APP_AUTO: {
            new cars[6][2], count = RW_AutoCars(playerid, cars, 6);
            if (!count) { RW_AppRow(playerid, 0, "No cars: buy one at the garage", "", "car"); return 1; }
            for (new i; i < count; i++) {
                new name[48], Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz, away;
                RW_VehicleName(cars[i][1], name, sizeof name);
                GetPlayerPos(playerid, x, y, z);
                new slot = RW_AutoSlot(cars[i][0]);
                if (slot >= 0) { GetVehiclePos(gVeh[slot][vsVehicleId], vx, vy, vz); away = floatround(VectorSize(vx - x, vy - y, vz - z)); }
                else away = -1;
                if (away < 0) format(row, sizeof row, "%s - in the garage", name);
                else format(row, sizeof row, "%s - %d m away", name, away);
                gAppIds[playerid][i] = cars[i][0];
                RW_AppRow(playerid, i, row, "deliver", "car");
            }
            gAppCount[playerid] = count;
        }
        case PHONE_APP_CAMERA: {
            RW_AppRow(playerid, 0, "Take a shot", "shot", "camera");
            RW_AppRow(playerid, 1, "Look at a photo: /photo", "", "camera");
            RW_AppRow(playerid, 2, "One shot costs one SMS", "", "wallet");
        }
        default: RW_AppRow(playerid, 0, "This app is empty", "", "app");
    }
    return 1;
}
stock RW_CefAppMenu(playerid)
{
    new title[48], subtitle[96];
    RW_AppTitle(playerid, title, sizeof title, subtitle, sizeof subtitle);
    if (!RW_CefOpen(playerid, CEF_MODE_APP, title, subtitle)) return RW_PhoneAppMenu(playerid);
    RW_AppRows(playerid);
    RW_CefAction(playerid, 8, "Close", "danger");
    return RW_CefEnd(playerid);
}
stock RW_PhoneAppMenu(playerid)
{
    new title[48], subtitle[96];
    RW_AppTitle(playerid, title, sizeof title, subtitle, sizeof subtitle);
    RW_UI_Panel(playerid, UI_PHONE_APP, title, subtitle);
    RW_AppRows(playerid);
    RW_UI_Row(playerid, 6, "Back");
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_PhoneAppRender(playerid)
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) return RW_CefAppMenu(playerid);
    #endif
    return RW_PhoneAppMenu(playerid);
}
stock RW_PhoneAppList(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You have no phone. Buy one at a 24/7 shop.");
    gPhoneAppOpen[playerid] = -1;
    gPhoneAppPage[playerid] = 0;
    return RW_PhoneAppRender(playerid);
}
stock RW_PhoneAppOpen(playerid, app)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    if (RW_PhoneHasItem(playerid) <= 0) return RW_Error(playerid, "You have no phone. Buy one at a 24/7 shop.");
    if (app < 0 || app >= PHONE_APPS) return 1;
    if (GetTickCount() - gAppTick[playerid] < 250) return 1;
    gAppTick[playerid] = GetTickCount();
    gPhoneAppOpen[playerid] = app;
    gPhoneAppPage[playerid] = 0;
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefPhoneClose(playerid); return RW_CefAppMenu(playerid); }
    #endif
    return RW_PhoneAppMenu(playerid);
}
stock RW_PhoneAppAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) { gPhoneAppOpen[playerid] = -1; return RW_UI_Close(playerid); }
    if (!cef && row == 6) return RW_PhoneAppList(playerid);
    if (GetTickCount() - gAppTick[playerid] < 250) return 1;
    gAppTick[playerid] = GetTickCount();
    new app = gPhoneAppOpen[playerid];
    if (app < 0) {
        if (row == 6) {
            if (gPhoneAppPage[playerid] > 0) gPhoneAppPage[playerid] = 0;
            else gPhoneAppPage[playerid] = 1;
            return RW_PhoneAppRender(playerid);
        }
        new id = gAppIds[playerid][row] - 1;
        if (id < 0) return 1;
        if (!gPhoneApp[id]) return RW_Error(playerid, "This app is not installed.");
        return RW_PhoneAppOpen(playerid, id);
    }
    switch (app) {
        case PHONE_APP_BANK: {
            if (row == 1) return RW_AppPayFines(playerid);
            if (row == 2) return RW_BankOpen(playerid);
            if (row == 3) return RW_Info(playerid, "Use: /transfer <player id> <sum>.");
            if (row == 4) return RW_PaydayCommand(playerid);
            return 1;
        }
        case PHONE_APP_WEATHER: return RW_WeatherCommand(playerid);
        case PHONE_APP_GROUP: {
            if (row == 0) return RW_GroupInfo(playerid);
            if (row == 1) { if (RW_GroupHas(playerid)) return RW_GroupLeave(playerid); return RW_GroupCreate(playerid); }
            if (row == 2) return RW_Info(playerid, "Use: /ginvite <player id>.");
            if (row == 3) return RW_Info(playerid, "Use: /g <text>.");
            return 1;
        }
        case PHONE_APP_LAWYER: {
            if (row == 0) return RW_AppLawyerCall(playerid);
            if (row == 4) return RW_Info(playerid, "Use: /lawprice <sum>.");
            return 1;
        }
        case PHONE_APP_UBER: {
            if (gUberDriver[playerid] > 0) {
                if (row == 1) {
                    new driver = gUberDriver[playerid];
                    RemovePlayerMapIcon(driver, UBER_ICON);
                    gUberPass[driver] = 0;
                    gUberDriver[playerid] = 0;
                    gUberWait[playerid] = 0;
                    return RW_Info(playerid, "Order cancelled.");
                }
                return 1;
            }
            if (gUberPass[playerid] > 0) {
                if (row == 1) return RW_UberFinish(playerid);
                return 1;
            }
            if (RW_UberTaxi(playerid)) {
                new target = gAppIds[playerid][row];
                if (target <= 0 || !IsPlayerConnected(target)) return 1;
                return RW_UberAccept(playerid, target - 1);
            }
            if (row == 0) return RW_UberOrder(playerid);
            return 1;
        }
        case PHONE_APP_NEWS: {
            if (row == 0) return RW_Info(playerid, "Use: /ad <text>. It costs 100 in cash.");
            return RW_NewsList(playerid);
        }
        case PHONE_APP_AUTO: {
            new carid = gAppIds[playerid][row];
            if (carid <= 0) return 1;
            return RW_AutoDeliver(playerid, carid);
        }
        case PHONE_APP_CAMERA: {
            if (row == 0) return RW_PhotoTake(playerid);
            if (row == 1) return RW_PhotoShow(playerid);
            return 1;
        }
    }
    return 1;
}
stock RW_PhoneAppCefClick(playerid, row) { return RW_PhoneAppAction(playerid, row, true); }
stock RW_PhoneAppClick(playerid, row) { return RW_PhoneAppAction(playerid, row, false); }

/*  -------------------------------- команды -------------------------------- */

stock RW_WeatherCommand(playerid)
{
    new weather[24], message[192];
    RW_WeatherName(weather, sizeof weather);
    format(message, sizeof message, "Weather: %s. Your job now: %+d%%. Next change in %d s.",
        weather, RW_WeatherBonus(gJob[playerid]), WEATHER_CHANGE_SECONDS - gWeatherTick);
    return RW_Info(playerid, message);
}

/*  --------------------------- обёртки для чата ---------------------------- */

stock RW_GroupInviteCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /ginvite <player id>.");
    return RW_GroupInvite(playerid, strval(word));
}
stock RW_AutoDeliverCommand(playerid, const params[])
{
    new word[32];
    if (!RW_PhoneWord(params, 0, word, sizeof word) || !RW_IsNumeric(word)) return RW_Error(playerid, "Use: /deliver <car id>. /auto shows the list.");
    return RW_AutoDeliver(playerid, strval(word));
}
stock RW_NewsPostCommand(playerid, const params[])
{
    return RW_NewsPost(playerid, params);
}
stock RW_GroupSayCommand(playerid, const params[])
{
    return RW_GroupSay(playerid, params);
}
