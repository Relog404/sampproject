/*  railway/input.inc
 *
 *  Ввод с клавиатуры в браузере: цена лота, ставка на аукционе, сумма в
 *  кассе. Команды в чате остались, но цифры можно набирать и кликом:
 *  сервер открывает карточку с полем, страница отдаёт введённое текстом,
 *  а разбирает и проверяет его снова сервер.
 *
 *  События:  ui:ask     (сервер -> страница: заголовок, подсказка, пример)
 *            ui:answer  (страница -> сервер: что набрал игрок)
 *
 *  Что спросить — константы INPUT_* в core.inc: их читают и маркет,
 *  и инвентарь, а сам модуль подключается последним.
 */

#if defined _railway_input_included
    #endinput
#endif
#define _railway_input_included

new gInputKind[MAX_PLAYERS];        /* что спросить                     */
new gInputArg[MAX_PLAYERS];         /* о чём речь: слот, лот, дом       */
new gInputArg2[MAX_PLAYERS];        /* шаг продажи: уже выбранная цена  */
new bool:gInputOn[MAX_PLAYERS];     /* поле открыто, ввод ждём          */

stock bool:RW_InputBusy(playerid) { return gInputOn[playerid]; }

stock RW_InputReset(playerid)
{
    gInputKind[playerid] = INPUT_NONE; gInputArg[playerid] = 0; gInputArg2[playerid] = 0;
    gInputOn[playerid] = false; gUIInput[playerid] = false; gUIMode[playerid] = 0;
    return 1;
}

stock RW_InputOpen(playerid, kind, arg, const title[], const subtitle[], const hint[])
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Сначала войдите в аккаунт.");
    gInputKind[playerid] = kind;
    gInputArg[playerid] = arg;
    gInputOn[playerid] = true;
    RW_UI_Close(playerid);
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) {
            gUIMode[playerid] = UI_INPUT;
            gUIInput[playerid] = true;   /* пока открыто поле, клавиши заняты */
            return RW_CefInput(playerid, title, subtitle, hint);
        }
    #endif
    gInputOn[playerid] = false;
    return RW_Info(playerid, "Поле ввода работает только в браузере: введите то же самое командой в чат.");
}

stock RW_InputCancel(playerid)
{
    if (!gInputOn[playerid]) return 0;
    RW_InputReset(playerid);
    #if RAILWAY_CEF
        RW_CefInputClose(playerid);
    #endif
    return 1;
}

/* Страница отдаёт текст: числа разбирает и проверяет сервер, как и раньше. */
public RW_CefInputSubmit(playerid, browserid, const value[])
{
    #pragma unused browserid
    if (!gInputOn[playerid]) return 0;
    new kind = gInputKind[playerid], arg = gInputArg[playerid], arg2 = gInputArg2[playerid];
    new text[INPUT_MAX_TEXT];
    strmid(text, value, 0, sizeof text);
    RW_InputReset(playerid);
    #if RAILWAY_CEF
        RW_CefInputClose(playerid);
    #endif
    if (!RW_InputTrim(text)) return RW_Error(playerid, "Пустой ввод: ничего не изменилось.");
    return RW_InputApply(playerid, kind, arg, arg2, text);
}

/* Пустая строка — не ввод. Пробелы по краям не считаются. */
stock bool:RW_InputTrim(text[])
{
    new i, j = strlen(text);
    while (i < j && text[i] <= ' ') i++;
    while (j > i && text[j - 1] <= ' ') j--;
    if (i == 0 && j == strlen(text)) return text[0] ? true : false;
    new copy[INPUT_MAX_TEXT];
    strmid(copy, text, i, j);
    format(text, INPUT_MAX_TEXT, "%s", copy);
    return text[0] ? true : false;
}

/* Текст из поля: обрезать по длине, пустое не пропускать. */
stock bool:RW_InputText(text[], dest[], size = sizeof dest)
{
    if (!RW_InputTrim(text)) return false;
    if (strlen(text) >= size) return false;
    format(dest, size, "%s", text);
    return true;
}

/* Только цифры: буквы и минус в цену не превращаются. */
stock RW_InputNumber(const text[])
{
    new value = 0, digits;
    for (new i; text[i] && i < INPUT_MAX_TEXT; i++) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = value * 10 + (text[i] - '0');
        digits++;
        if (digits > 9) return -1;
    }
    return digits ? value : -1;
}

/*  ------------------------------ продажа -------------------------------- */
stock RW_InputSellPrice(playerid, uid)
{
    new slot = RW_InvFind(playerid, uid);
    if (slot < 0) return RW_Error(playerid, "Эта вещь лежит не в карманах: продать можно только из них.");
    if (!RW_InvCanRemove(playerid, slot)) return RW_Error(playerid, "Эту вещь продать нельзя.");
    return RW_InputOpen(playerid, INPUT_SELL_PRICE, slot, "Продажа",
        "Цена за час показа, от 1 до 100 000 000", "Например: 5000");
}

stock RW_InputApply(playerid, kind, arg, arg2, const text[])
{
    if (kind == INPUT_SELL_PRICE) {
        new price = RW_InputNumber(text);
        if (price < 1) return RW_Error(playerid, "Цена — числом, от 1 до 100 000 000.");
        gInputArg2[playerid] = price;
        return RW_InputOpen(playerid, INPUT_SELL_HOURS, arg, "Продажа",
            "Срок, от 5 до 24 часов. Комиссия 500 за час", "Например: 12");
    }
    if (kind == INPUT_SELL_HOURS) {
        new hours = RW_InputNumber(text);
        if (hours < 1) return RW_Error(playerid, "Срок — числом, от 5 до 24 часов.");
        RW_MarketSell(playerid, arg, arg2, hours);
        return RW_MarketOpen(playerid);
    }
    if (kind == INPUT_BID) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Ставка — числом, и не меньше минимальной.");
        RW_MarketBid(playerid, arg, amount);
        return RW_MarketOpen(playerid);
    }
    if (kind == INPUT_PROP_BID) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Ставка — числом, и выше текущей.");
        RW_PropBid(playerid, arg, amount);
        return RW_PropRender(playerid);
    }
    if (kind == INPUT_BANK_PUT) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        RW_BankDeposit(playerid, amount);
        return RW_BankRender(playerid);
    }
    if (kind == INPUT_BANK_TAKE) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        RW_BankWithdraw(playerid, amount);
        return RW_BankRender(playerid);
    }
    if (kind == INPUT_BANK_SUM) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        RW_BankTransfer(playerid, arg, amount);
        return RW_BankRender(playerid);
    }
    if (kind == INPUT_FAMILY_NAME) {
        new name[FAMILY_MAX_NAME], copy[INPUT_MAX_TEXT];
        format(copy, sizeof copy, "%s", text);
        if (!RW_InputText(copy, name, sizeof name)) return RW_Error(playerid, "Название — до 24 знаков, без кавычек.");
        RW_FamilyCreateNamed(playerid, name);
        return RW_FamilyOpen(playerid);
    }
    if (kind == INPUT_FAMILY_CASH) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        RW_FamilyCashTake(playerid, amount);
        return RW_FamilyOpen(playerid);
    }
    if (kind == INPUT_LAW_PRICE) {
        new amount = RW_InputNumber(text);
        if (amount < 0) return RW_Error(playerid, "Цена — числом, от 0 до 50 000.");
        RW_LawyerPriceSet(playerid, amount);
        return RW_GovOpen(playerid);
    }
    if (kind == INPUT_TRANSFER_ID) {
        new target = RW_InputNumber(text);
        if (target < 0 || target >= MAX_PLAYERS) return RW_Error(playerid, "Нет игрока с таким номером.");
        if (target == playerid || !IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Этот игрок не в сети.");
        gInputArg2[playerid] = target;
        new nick[MAX_PLAYER_NAME], title[64];
        RW_GetNick(target, nick);
        format(title, sizeof title, "Перевод: %s", nick);
        return RW_InputOpen(playerid, INPUT_TRANSFER_SUM, arg, title,
            "Сколько перевести со счёта", "Например: 5000");
    }
    if (kind == INPUT_TRANSFER_SUM) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        return RW_BankTransfer(playerid, arg2, amount);
    }
    if (kind == INPUT_GROUP_ID) {
        new target = RW_InputNumber(text);
        if (target < 0 || target >= MAX_PLAYERS) return RW_Error(playerid, "Нет игрока с таким номером.");
        if (!IsPlayerConnected(target) || !gLogged[target]) return RW_Error(playerid, "Этот игрок не в сети.");
        return RW_GroupInvite(playerid, target);
    }
    if (kind == INPUT_GROUP_TEXT) {
        new line[INPUT_MAX_TEXT], copy[INPUT_MAX_TEXT];
        format(copy, sizeof copy, "%s", text);
        if (!RW_InputText(copy, line, sizeof line)) return RW_Error(playerid, "Пустое сообщение.");
        return RW_GroupSay(playerid, line);
    }
    if (kind == INPUT_NEWS_TEXT) {
        new line[INPUT_MAX_TEXT], copy[INPUT_MAX_TEXT];
        format(copy, sizeof copy, "%s", text);
        if (!RW_InputText(copy, line, sizeof line)) return RW_Error(playerid, "Пустое объявление.");
        return RW_NewsPost(playerid, line);
    }
    if (kind == INPUT_TILL_PUT || kind == INPUT_TILL_TAKE) {
        new amount = RW_InputNumber(text);
        if (amount < 1) return RW_Error(playerid, "Сумма — числом, больше нуля.");
        RW_PropTillAmount(playerid, arg, amount, kind == INPUT_TILL_PUT);
        return RW_PropRender(playerid);
    }
    return 1;
}
