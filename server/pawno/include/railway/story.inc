/*  railway/story.inc
 *
 *  Сюжетная линия «Ночной груз» и статистика аккаунта.
 *
 *  Восемь глав идут цепочкой: каждая закрывается настоящим действием игрока
 *  (устроился, закрыл задания, купил дом…) и даёт опыт и деньги. Опыт копит
 *  уровень, уровень даёт прибавку к доходу. Прогресс и счётчики лежат в SQLite,
 *  поэтому не теряются между сессиями.
 */

#if defined _railway_story_included
    #endinput
#endif
#define _railway_story_included

#define STORY_CHAPTERS      8
#define STORY_XP_LEVEL      1000        /* опыта на уровень                 */
#define STORY_LEVEL_PAY     250         /* денег за каждый уровень          */
#define STORY_NIGHT_FROM    22          /* ночная смена: с 22:00            */
#define STORY_NIGHT_TO      6           /* до 06:00                         */

new gStoryDone[MAX_PLAYERS];            /* биты закрытых глав               */
new gXp[MAX_PLAYERS];                   /* опыт                             */
new gLevel[MAX_PLAYERS];                /* уровень                          */
new gStatTask[MAX_PLAYERS];             /* закрытых рабочих заданий         */
new gStatRob[MAX_PLAYERS];              /* ограблений                       */
new gStatPayday[MAX_PLAYERS];           /* зарплат                          */
new gStatEarned[MAX_PLAYERS];           /* заработано всего                 */
new gStatMinutes[MAX_PLAYERS];          /* минут в игре                     */
new gStatEnter[MAX_PLAYERS];            /* вошёл в эту сессию               */
new gStoryOpen[MAX_PLAYERS];            /* глава в меню                     */

static const gStoryName[STORY_CHAPTERS][] = {
    "First shift", "Three errands", "Call a cab", "Your own roof",
    "Shopkeeper", "Night freight", "Robber", "Regular"
};
static const gStoryGoal[STORY_CHAPTERS][] = {
    "Get a job at any hiring point",
    "Finish 3 job tasks",
    "Call a taxi from the rank",
    "Buy any house",
    "Buy any business",
    "Finish a task between 22:00 and 06:00",
    "Rob a business till",
    "Take 5 paydays"
};
static const gStoryEvent[STORY_CHAPTERS] = { STORY_EV_JOB, STORY_EV_TASK, STORY_EV_TAXI,
                                             STORY_EV_PROPERTY, STORY_EV_BUSINESS,
                                             STORY_EV_NIGHT, STORY_EV_ROB, STORY_EV_PAYDAY };
static const gStoryNeed[STORY_CHAPTERS] = { 1, 3, 1, 1, 1, 1, 1, 5 };
static const gStoryXp[STORY_CHAPTERS] = { 50, 100, 80, 150, 250, 200, 300, 400 };
static const gStoryPay[STORY_CHAPTERS] = { 500, 1500, 800, 3000, 5000, 2500, 4000, 6000 };

/*  --------------------------------- база -------------------------------- */
stock RW_StorySchema()
{
    return RW_DB_Exec("CREATE TABLE IF NOT EXISTS story (player_id INTEGER PRIMARY KEY, xp INT, level INT, done INT, tasks INT, robberies INT, paydays INT, earned INT, minutes INT)");
}
stock RW_StoryLoad(playerid)
{
    gStoryDone[playerid] = 0; gXp[playerid] = 0; gLevel[playerid] = 1;
    gStatTask[playerid] = 0; gStatRob[playerid] = 0; gStatPayday[playerid] = 0;
    gStatEarned[playerid] = 0; gStatMinutes[playerid] = 0;
    gStatEnter[playerid] = gettime();
    if (gAccountId[playerid] <= 0) return 0;
    new query[128];
    format(query, sizeof query, "SELECT xp,level,done,tasks,robberies,paydays,earned,minutes FROM story WHERE player_id=%d", gAccountId[playerid]);
    new DBResult:r = db_query(gDB, query);
    if (r == DBResult:0) return 0;
    if (db_num_rows(r)) {
        gXp[playerid] = db_get_field_assoc_int(r, "xp");
        gLevel[playerid] = db_get_field_assoc_int(r, "level");
        gStoryDone[playerid] = db_get_field_assoc_int(r, "done");
        gStatTask[playerid] = db_get_field_assoc_int(r, "tasks");
        gStatRob[playerid] = db_get_field_assoc_int(r, "robberies");
        gStatPayday[playerid] = db_get_field_assoc_int(r, "paydays");
        gStatEarned[playerid] = db_get_field_assoc_int(r, "earned");
        gStatMinutes[playerid] = db_get_field_assoc_int(r, "minutes");
    }
    db_free_result(r);
    if (gLevel[playerid] < 1) gLevel[playerid] = 1;
    return 1;
}
stock RW_StorySave(playerid)
{
    if (gAccountId[playerid] <= 0) return 0;
    if (gStatEnter[playerid] > 0) gStatMinutes[playerid] = gStatMinutes[playerid] + (gettime() - gStatEnter[playerid]) / 60;
    gStatEnter[playerid] = gettime();
    new query[256];
    format(query, sizeof query,
        "INSERT OR REPLACE INTO story (player_id,xp,level,done,tasks,robberies,paydays,earned,minutes) VALUES (%d,%d,%d,%d,%d,%d,%d,%d,%d)",
        gAccountId[playerid], gXp[playerid], gLevel[playerid], gStoryDone[playerid],
        gStatTask[playerid], gStatRob[playerid], gStatPayday[playerid], gStatEarned[playerid], gStatMinutes[playerid]);
    return RW_DB_Exec(query);
}
stock RW_StoryReset(playerid)
{
    gStoryDone[playerid] = 0; gXp[playerid] = 0; gLevel[playerid] = 1;
    gStatTask[playerid] = 0; gStatRob[playerid] = 0; gStatPayday[playerid] = 0;
    gStatEarned[playerid] = 0; gStatMinutes[playerid] = 0; gStatEnter[playerid] = 0;
    gStoryOpen[playerid] = 0;
    return 1;
}

/*  ------------------------------- статистика ---------------------------- */
stock RW_StoryLevel(xp) { return 1 + xp / STORY_XP_LEVEL; }
stock RW_StoryToNext(playerid)
{
    new left = STORY_XP_LEVEL - (gXp[playerid] % STORY_XP_LEVEL);
    return (left <= 0) ? STORY_XP_LEVEL : left;
}
stock RW_StoryEarned(playerid, amount)
{
    if (amount <= 0) return 0;
    gStatEarned[playerid] = gStatEarned[playerid] + amount;
    return 1;
}
stock RW_StoryXp(playerid, amount)
{
    if (amount <= 0) return 0;
    gXp[playerid] = gXp[playerid] + amount;
    new level = RW_StoryLevel(gXp[playerid]);
    if (level <= gLevel[playerid]) return 1;
    gLevel[playerid] = level;
    new bonus = level * STORY_LEVEL_PAY;
    RW_BankAdd(playerid, bonus);
    new sum[32], message[160];
    RW_FormatMoney(bonus, sum, sizeof sum);
    format(message, sizeof message, "Level %d! %s on the account and a bigger income. /profile - the whole record.", level, sum);
    return RW_Ok(playerid, message);
}

/*  -------------------------------- события ------------------------------ */
stock RW_StoryChapterOf(event)
{
    for (new i; i < STORY_CHAPTERS; i++) if (gStoryEvent[i] == event) return i;
    return -1;
}
stock RW_StoryCount(playerid, event)
{
    switch (event) {
        case STORY_EV_JOB: return 1;
        case STORY_EV_TASK: return gStatTask[playerid];
        case STORY_EV_TAXI: return 1;
        case STORY_EV_PROPERTY: return (RW_PropOwns(playerid) >= 0) ? 1 : 0;
        case STORY_EV_BUSINESS: {
            new own = RW_PropOwns(playerid);
            if (own >= 0 && RW_PropIsBiz(own)) return 1;
            return 0;
        }
        case STORY_EV_ROB: return gStatRob[playerid];
        case STORY_EV_PAYDAY: return gStatPayday[playerid];
        case STORY_EV_NIGHT: return gStatTask[playerid];
    }
    return 0;
}
/* Одно событие закрывает ровно одну главу: ту, что идёт по цепочке. */
stock RW_StoryEvent(playerid, event)
{
    if (!gLogged[playerid]) return 0;
    if (event == STORY_EV_TASK) gStatTask[playerid]++;
    else if (event == STORY_EV_ROB) gStatRob[playerid]++;
    else if (event == STORY_EV_PAYDAY) gStatPayday[playerid]++;
    new chapter = RW_StoryChapterOf(event);
    if (chapter < 0) return 0;
    return RW_StoryCheck(playerid, chapter);
}
stock RW_StoryCheck(playerid, chapter)
{
    if (chapter < 0 || chapter >= STORY_CHAPTERS) return 0;
    if (gStoryDone[playerid] & (1 << chapter)) return 0;
    if (RW_StoryCount(playerid, gStoryEvent[chapter]) < gStoryNeed[chapter]) return 0;
    gStoryDone[playerid] = gStoryDone[playerid] | (1 << chapter);
    RW_StoryXp(playerid, gStoryXp[chapter]);
    RW_BankAdd(playerid, gStoryPay[chapter]);
    RW_StorySave(playerid);
    new sum[32], message[200];
    RW_FormatMoney(gStoryPay[chapter], sum, sizeof sum);
    format(message, sizeof message, "Chapter closed: %s. +%d xp, %s on the account. /story - the next chapter.", gStoryName[chapter], gStoryXp[chapter], sum);
    RW_Accent(playerid, message);
    return 1;
}
/* Закрытое рабочее задание: ночью оно идёт ещё и в ночную главу. */
public RW_StoryTask(playerid)
{
    if (!gLogged[playerid]) return 0;
    RW_StoryEvent(playerid, STORY_EV_TASK);
    new hour, minute, second;
    gettime(hour, minute, second);
    #pragma unused minute, second
    if (hour >= STORY_NIGHT_FROM || hour < STORY_NIGHT_TO) RW_StoryCheck(playerid, RW_StoryChapterOf(STORY_EV_NIGHT));
    return 1;
}

/*  ---------------------------------- меню ------------------------------- */
stock RW_StorySubtitle(playerid, dest[], size = sizeof dest)
{
    new done;
    for (new i; i < STORY_CHAPTERS; i++) if (gStoryDone[playerid] & (1 << i)) done++;
    return format(dest, size, "Level %d, %d xp. Chapters: %d of %d.", gLevel[playerid], gXp[playerid], done, STORY_CHAPTERS);
}
stock RW_StoryRowText(playerid, row, dest[], size = sizeof dest)
{
    if (row < 0 || row >= STORY_CHAPTERS) return 0;
    if (gStoryDone[playerid] & (1 << row)) return format(dest, size, "%s - done (+%d xp)", gStoryName[row], gStoryXp[row]);
    new have = RW_StoryCount(playerid, gStoryEvent[row]);
    return format(dest, size, "%s: %s (%d/%d)", gStoryName[row], gStoryGoal[row], have, gStoryNeed[row]);
}
stock RW_StoryRows(playerid)
{
    for (new row; row < STORY_CHAPTERS; row++) {
        new text[128];
        RW_StoryRowText(playerid, row, text, sizeof text);
        RW_StoryRowOut(playerid, row, text, (gStoryDone[playerid] & (1 << row)) ? "ok" : "open", "quest");
    }
    return 1;
}
stock RW_StoryRowOut(playerid, row, const text[], const arg[], const icon[])
{
    #if RAILWAY_CEF
        if (RW_CefOn(playerid)) { RW_CefRow(playerid, row, text, arg, icon); return 1; }
    #else
        #pragma unused arg, icon
    #endif
    RW_UI_Row(playerid, row, text);
    return 1;
}
stock RW_StoryOpen(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new subtitle[120];
    RW_StorySubtitle(playerid, subtitle, sizeof subtitle);
    #if RAILWAY_CEF
        if (RW_CefOpen(playerid, CEF_MODE_STORY, "Story", subtitle)) {
            RW_StoryRows(playerid);
            RW_CefAction(playerid, 8, "Close", "danger");
            return RW_CefEnd(playerid);
        }
    #endif
    RW_UI_Panel(playerid, UI_STORY, "Story", subtitle);
    RW_StoryRows(playerid);
    RW_UI_Row(playerid, 7, "Close");
    return 1;
}
stock RW_StoryAction(playerid, row, bool:cef)
{
    if (row == (cef ? 8 : 7)) return RW_UI_Close(playerid);
    gStoryOpen[playerid] = row;
    return RW_StoryOpen(playerid);
}
stock RW_StoryClick(playerid, row) { return RW_StoryAction(playerid, row, false); }
stock RW_StoryCefClick(playerid, id) { return RW_StoryAction(playerid, id, true); }

/*  --------------------------------- команды ----------------------------- */
stock RW_StoryCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    return RW_StoryOpen(playerid);
}
stock RW_ProfileCommand(playerid)
{
    if (!gLogged[playerid]) return RW_Error(playerid, "Log in first.");
    new done;
    for (new i; i < STORY_CHAPTERS; i++) if (gStoryDone[playerid] & (1 << i)) done++;
    new minutes = gStatMinutes[playerid] + (gettime() - gStatEnter[playerid]) / 60;
    new message[200];
    RW_Accent(playerid, "--- Profile ---");
    format(message, sizeof message, "Level %d: %d xp, %d xp to the next level.", gLevel[playerid], gXp[playerid], RW_StoryToNext(playerid));
    RW_Info(playerid, message);
    format(message, sizeof message, "Story: %d chapters of %d. Job tasks: %d. Robberies: %d. Paydays: %d.", done, STORY_CHAPTERS, gStatTask[playerid], gStatRob[playerid], gStatPayday[playerid]);
    RW_Info(playerid, message);
    format(message, sizeof message, "Earned: %d. Time in game: %d h %d min.", gStatEarned[playerid], minutes / 60, minutes % 60);
    RW_Info(playerid, message);
    return 1;
}
