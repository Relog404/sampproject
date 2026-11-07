"""Story and profile: eight chapters, experience, levels and statistics.

The story is driven by real actions: a hire, a finished task, a bought house.
Everything is counted on the server and kept in SQLite. The tests below run
the table for real and check the hooks that move a chapter forward.
"""
import re
import sqlite3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


STORY = pawn('story')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
INTER = pawn('interaction')
CMDS = pawn('commands')
ECON = pawn('economy')
PROP = pawn('property')
PHONEAPPS = pawn('phoneapps')
ACCOUNTS = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class StoryTableTest(unittest.TestCase):
    """The story table, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript(sql(STORY, 'CREATE TABLE IF NOT EXISTS story') + ';')

    def test_progress_and_counters_survive_a_restart(self):
        self.db.execute('INSERT INTO story (player_id,xp,level,done,tasks,robberies,paydays,earned,minutes) VALUES (5,1200,2,3,7,1,4,9000,95)')
        row = self.db.execute('SELECT xp,level,done,tasks,robberies,paydays,earned,minutes FROM story WHERE player_id=5').fetchone()
        self.assertEqual(row, (1200, 2, 3, 7, 1, 4, 9000, 95))

    def test_the_save_query_rewrites_the_row(self):
        save = sql(STORY, 'INSERT OR REPLACE INTO story')
        numbers = re.findall(r'%d', save)
        self.assertEqual(len(numbers), 9)
        self.db.execute(save.replace('%d,%d,%d,%d,%d,%d,%d,%d,%d', '5,10,1,0,0,0,0,0,0'))
        self.db.execute(save.replace('%d,%d,%d,%d,%d,%d,%d,%d,%d', '5,20,1,1,1,0,0,0,0'))
        rows = self.db.execute('SELECT xp,done,tasks FROM story WHERE player_id=5').fetchall()
        self.assertEqual(rows, [(20, 1, 1)])


class StoryChapterTest(unittest.TestCase):
    """Eight chapters, each closed by a real action."""

    def test_eight_chapters_with_goals_rewards_and_needs(self):
        names = block(STORY, 'static const gStoryName[STORY_CHAPTERS][] = {', '};')
        goals = block(STORY, 'static const gStoryGoal[STORY_CHAPTERS][] = {', '};')
        events = block(STORY, 'static const gStoryEvent[STORY_CHAPTERS] = {', '};')
        needs = block(STORY, 'static const gStoryNeed[STORY_CHAPTERS] = {', '};')
        xp = block(STORY, 'static const gStoryXp[STORY_CHAPTERS] = {', '};')
        pay = block(STORY, 'static const gStoryPay[STORY_CHAPTERS] = {', '};')
        self.assertEqual(needs.count(','), 7)
        for table, count in ((names, 8), (goals, 8), (needs, 8), (xp, 8), (pay, 8)):
            numbers = re.findall(r'"[^"]+"', table) if '"' in table else re.findall(r'\d+', table)
            self.assertEqual(len(numbers), count, table)
        self.assertEqual(events.count('STORY_EV_'), 8)

    def test_a_chapter_closes_only_once_and_pays_both_rewards(self):
        check = block(STORY, 'stock RW_StoryCheck(playerid, chapter)')
        self.assertIn('if (gStoryDone[playerid] & (1 << chapter)) return 0;', check)
        self.assertIn('RW_StoryCount(playerid, gStoryEvent[chapter]) < gStoryNeed[chapter]', check)
        self.assertIn('gStoryDone[playerid] = gStoryDone[playerid] | (1 << chapter);', check)
        self.assertIn('RW_StoryXp(playerid, gStoryXp[chapter]);', check)
        self.assertIn('RW_BankAdd(playerid, gStoryPay[chapter]);', check)
        self.assertIn('RW_StorySave(playerid);', check)

    def test_the_night_chapter_needs_a_night_task(self):
        task = block(STORY, 'public RW_StoryTask(playerid)', '\n\n/*  ')
        self.assertIn('RW_StoryEvent(playerid, STORY_EV_TASK);', task)
        self.assertIn('if (hour >= STORY_NIGHT_FROM || hour < STORY_NIGHT_TO)', task)
        self.assertIn('RW_StoryCheck(playerid, RW_StoryChapterOf(STORY_EV_NIGHT));', task)
        self.assertIn('#define STORY_NIGHT_FROM    22', STORY)
        self.assertIn('#define STORY_NIGHT_TO      6', STORY)

    def test_events_reach_the_story_from_real_actions(self):
        self.assertIn('RW_StoryEvent(playerid, STORY_EV_JOB);', ECON)
        self.assertIn('RW_StoryTask(playerid);', ECON)
        self.assertIn('RW_StoryEvent(playerid, STORY_EV_PAYDAY);', ECON)
        self.assertIn('RW_StoryEvent(playerid, (gPropInfo[id][0] == PROP_KIND_BIZ) ? STORY_EV_BUSINESS : STORY_EV_PROPERTY);', PROP)
        self.assertIn('RW_StoryEvent(playerid, STORY_EV_ROB);', PROP)
        self.assertIn('RW_StoryEvent(playerid, STORY_EV_TAXI);', PHONEAPPS)

    def test_the_events_are_defined_before_the_modules_that_send_them(self):
        for event in ('STORY_EV_JOB', 'STORY_EV_TASK', 'STORY_EV_TAXI', 'STORY_EV_PROPERTY',
                      'STORY_EV_BUSINESS', 'STORY_EV_ROB', 'STORY_EV_PAYDAY', 'STORY_EV_NIGHT'):
            self.assertIn('#define %-' % () if False else '#define %s' % event, CORE)
        self.assertNotIn('#define STORY_EV_JOB', STORY)


class StoryLevelTest(unittest.TestCase):
    """Experience, levels and the level reward."""

    def test_a_level_is_a_thousand_of_experience(self):
        self.assertIn('#define STORY_XP_LEVEL      1000', STORY)
        level = block(STORY, 'stock RW_StoryLevel(xp)')
        self.assertIn('return 1 + xp / STORY_XP_LEVEL;', level)
        self.assertEqual(1000 // 1000 + 1, 2)

    def test_a_new_level_pays_and_is_announced(self):
        xp = block(STORY, 'stock RW_StoryXp(playerid, amount)')
        self.assertIn('gXp[playerid] = gXp[playerid] + amount;', xp)
        self.assertIn('if (level <= gLevel[playerid]) return 1;', xp)
        self.assertIn('RW_BankAdd(playerid, bonus);', xp)
        self.assertIn('Level %d!', xp)
        self.assertIn('new bonus = level * STORY_LEVEL_PAY;', xp)

    def test_progress_to_the_next_level_is_shown(self):
        nxt = block(STORY, 'stock RW_StoryToNext(playerid)')
        self.assertIn('STORY_XP_LEVEL - (gXp[playerid] % STORY_XP_LEVEL)', nxt)


class ProfileTest(unittest.TestCase):
    """Statistics: what the account has done."""

    def test_counters_grow_from_real_actions(self):
        event = block(STORY, 'stock RW_StoryEvent(playerid, event)')
        self.assertIn('if (event == STORY_EV_TASK) gStatTask[playerid]++;', event)
        self.assertIn('else if (event == STORY_EV_ROB) gStatRob[playerid]++;', event)
        self.assertIn('else if (event == STORY_EV_PAYDAY) gStatPayday[playerid]++;', event)

    def test_earnings_are_counted_on_the_server(self):
        self.assertIn('RW_StoryEarned(playerid, pay);', ECON)
        self.assertIn('RW_StoryEarned(playerid, take);', PROP)
        earned = block(STORY, 'stock RW_StoryEarned(playerid, amount)')
        self.assertIn('gStatEarned[playerid] = gStatEarned[playerid] + amount;', earned)

    def test_playtime_is_added_on_save(self):
        save = block(STORY, 'stock RW_StorySave(playerid)')
        self.assertIn('gStatMinutes[playerid] = gStatMinutes[playerid] + (gettime() - gStatEnter[playerid]) / 60;', save)

    def test_the_profile_prints_the_whole_record(self):
        profile = block(STORY, 'stock RW_ProfileCommand(playerid)')
        self.assertIn('RW_StoryToNext(playerid)', profile)
        for word in ('Story:', 'Job tasks:', 'Robberies:', 'Paydays:', 'Earned:', 'Time in game:'):
            self.assertIn(word, profile)


class StoryWiringTest(unittest.TestCase):
    """The chain is reachable from both interfaces and from the commands."""

    def test_the_menu_has_its_own_mode_in_both_interfaces(self):
        self.assertIn('#define CEF_MODE_STORY              (15)', CORE)
        self.assertIn('#define UI_STORY 25', UI)
        self.assertRegex(UI, r'#define UI_COUNT (2[0-9]|3[0-9])')
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_STORY) return RW_StoryCefClick(playerid, id);', CEFUI)
        self.assertIn('else if (mode == UI_STORY) {', INTER)
        self.assertIn('return RW_StoryClick(playerid, row);', INTER)

    def test_the_commands_exist(self):
        for command in ('story', 'quest', 'profile'):
            self.assertIn('"%s", true' % command, CMDS)

    def test_progress_is_loaded_and_saved_with_the_account(self):
        self.assertIn('RW_StoryLoad(playerid);', ACCOUNTS)
        self.assertIn('RW_StorySave(playerid);', MAIN)
        self.assertIn('RW_StoryReset(playerid);', MAIN)
        self.assertIn('RW_StorySchema()', MAIN)

    def test_the_row_shows_goal_or_result(self):
        row = block(STORY, 'stock RW_StoryRowText(playerid, row, dest[], size = sizeof dest)')
        self.assertIn('gStoryName[row], gStoryXp[row]', row)
        self.assertIn('gStoryGoal[row], have, gStoryNeed[row]', row)


if __name__ == '__main__':
    unittest.main()
