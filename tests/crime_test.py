"""Crime: robberies by target type, the heat, Green and the craft.

The criminal slice leans on systems that already exist: the wanted table
belongs to the police, the loot is real inventory items, the robbed points are
the same 24/7 shops and ATMs the rest of the gamemode uses. The tests below
check those links and the numbers that make the economy of crime add up.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


CRIME = pawn('crime')
CORE = pawn('core')
UI = pawn('ui')
CEFUI = pawn('cefui')
INTER = pawn('interaction')
CMDS = pawn('commands')
INV = pawn('inventory')
ECON = pawn('economy')
MARKET = pawn('market')
TRANSPORT = pawn('transport')
ACCOUNTS = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class CrimeTargetTest(unittest.TestCase):
    """Three kinds of target, each with its own timing and heat."""

    def test_three_kinds_of_target(self):
        self.assertIn('#define CRIME_KIND_SHOP         0', CRIME)
        self.assertIn('#define CRIME_KIND_WAREHOUSE    1', CRIME)
        self.assertIn('#define CRIME_KIND_BANK         2', CRIME)
        self.assertIn('#define CRIME_KINDS             3', CRIME)
        names = block(CRIME, 'static const gCrimeName[CRIME_KINDS][] = {', '};')
        self.assertEqual(len(re.findall(r'"[^"]+"', names)), 3)

    def test_the_bank_takes_longer_and_hurts_more(self):
        seconds = block(CRIME, 'static const gCrimeSeconds[CRIME_KINDS] = {', '};')
        wanted = block(CRIME, 'static const gCrimeWanted[CRIME_KINDS] = {', '};')
        self.assertEqual(seconds, ' CRIME_SECONDS_SHOP, CRIME_SECONDS_WAREHOUSE, CRIME_SECONDS_BANK ')
        self.assertEqual(wanted, ' CRIME_WANTED_SHOP, CRIME_WANTED_WAREHOUSE, CRIME_WANTED_BANK ')

    def test_the_crew_is_capped_by_the_target(self):
        self.assertIn('#define CRIME_CREW_SHOP         10', CRIME)
        self.assertIn('#define CRIME_CREW_BANK         30', CRIME)
        finish = block(CRIME, 'public RW_CrimeFinish(playerid)')
        self.assertIn('(kind == CRIME_KIND_BANK) ? (CRIME_CREW_BANK) : (CRIME_CREW_SHOP)', finish)

    def test_the_till_only_opens_at_night_but_the_port_works_always(self):
        window = block(CRIME, 'stock bool:RW_CrimeWindow()')
        self.assertIn('hour >= 12 || hour < 1', window)
        heist = block(CRIME, 'stock RW_CrimeHeist(playerid)')
        self.assertIn('if (kind == CRIME_KIND_SHOP && !RW_CrimeWindow())', heist)

    def test_a_robbed_point_rests_before_it_can_be_hit_again(self):
        self.assertIn('#define CRIME_COOLDOWN          900', CRIME)
        finish = block(CRIME, 'public RW_CrimeFinish(playerid)')
        self.assertIn('gCrimeStamp[kind][index] = gettime() + CRIME_COOLDOWN;', finish)
        heist = block(CRIME, 'stock RW_CrimeHeist(playerid)')
        self.assertIn('if (gettime() < gCrimeStamp[kind][index]) return RW_Error', heist)

    def test_the_targets_are_the_real_points_of_the_map(self):
        near = block(CRIME, 'stock RW_CrimeNear(playerid, &kind, &index)')
        self.assertIn('RW_ShopPoint(i, px, py, pz);', near)
        self.assertIn('RW_AtmPoint(i, px, py, pz);', near)
        self.assertIn('{  2745.0, -2372.0, 13.6 }', CRIME)      # Ocean Docks, пакгаузы
        self.assertIn('{   250.0,  -150.0,   6.0 }', CRIME)     # ферма, амбар
        self.assertIn('{ 2244.4, -1665.4, 13.5 }', CRIME)       # скупщик, Ganton
        self.assertIn('stock RW_ShopPoint(index, &Float:x, &Float:y, &Float:z)', ECON)
        self.assertIn('stock RW_AtmPoint(index, &Float:x, &Float:y, &Float:z)', ECON)


class CrimeLootTest(unittest.TestCase):
    """The loot follows the type of the target."""

    def test_a_shop_gives_cash_and_small_goods(self):
        loot = block(CRIME, 'stock RW_CrimeLoot(playerid, kind)')
        self.assertIn('RW_InvGrant(playerid, ITEM_BANDAGE)', loot)
        self.assertIn('RW_InvGrant(playerid, ITEM_AMMO, random(6) + 1, 30)', loot)

    def test_a_warehouse_gives_precursors_and_sometimes_a_tuning_part(self):
        loot = block(CRIME, 'stock RW_CrimeLoot(playerid, kind)')
        self.assertIn('RW_CrimeGive(playerid, ITEM_CHEM, amount)', loot)
        self.assertIn('RW_InvGrant(playerid, ITEM_TUNING, random(TUNE_PARTS), 1)', loot)

    def test_the_bank_gives_armour_and_precursors(self):
        loot = block(CRIME, 'stock RW_CrimeLoot(playerid, kind)')
        self.assertIn('RW_InvGrant(playerid, ITEM_ARMOUR, RW_CrimeArmour(), 1)', loot)
        self.assertIn('RW_CrimeGive(playerid, ITEM_CHEM, 2)', loot)

    def test_the_crew_shares_the_cash(self):
        finish = block(CRIME, 'public RW_CrimeFinish(playerid)')
        self.assertIn('new share = total / count;', finish)
        self.assertIn('RW_AddMoney(crew[i], share);', finish)
        self.assertIn('RW_StoryEvent(crew[i], STORY_EV_ROB);', finish)
        self.assertIn('RW_StoryEarned(crew[i], share);', finish)

    def test_the_robber_cannot_move_while_the_job_runs(self):
        heist = block(CRIME, 'stock RW_CrimeHeist(playerid)')
        self.assertIn('TogglePlayerControllable(playerid, false);', heist)
        self.assertIn('SetTimerEx("RW_CrimeFinish", gCrimeSeconds[kind] * 1000, false, "i", playerid);', heist)
        finish = block(CRIME, 'public RW_CrimeFinish(playerid)')
        self.assertIn('TogglePlayerControllable(playerid, true);', finish)

    def test_death_ends_the_job(self):
        self.assertIn('RW_CrimeCancel(playerid);', MAIN)
        cancel = block(CRIME, 'public RW_CrimeCancel(playerid)')
        self.assertIn('TogglePlayerControllable(playerid, true);', cancel)

    def test_the_police_hear_the_alarm(self):
        heist = block(CRIME, 'stock RW_CrimeHeist(playerid)')
        self.assertIn('RW_PoliceOnDuty(p) && RW_PoliceBadge(p) > 0', heist)
        self.assertIn('Тревога: %s грабят', heist)

    def test_an_officer_on_duty_does_not_rob(self):
        heist = block(CRIME, 'stock RW_CrimeHeist(playerid)')
        self.assertIn('if (RW_PoliceOnDuty(playerid) && RW_PoliceBadge(playerid) > 0)', heist)


class CrimeWantedTest(unittest.TestCase):
    """The heat is kept in the police table, not in a second one."""

    def test_there_is_no_second_wanted_table(self):
        self.assertNotIn('crime_wanted', CRIME)
        self.assertIn('CREATE TABLE IF NOT EXISTS wanted', CRIME)

    def test_a_job_raises_the_heat_and_it_is_capped(self):
        setw = block(CRIME, 'stock RW_CrimeWantedSet(playerid, stars, reason)')
        self.assertIn('if (stars > CRIME_WANTED_MAX) stars = CRIME_WANTED_MAX;', setw)
        self.assertIn('#define CRIME_WANTED_MAX        5', CRIME)
        self.assertIn('INSERT OR REPLACE INTO wanted', setw)

    def test_a_clean_record_deletes_the_row(self):
        setw = block(CRIME, 'stock RW_CrimeWantedSet(playerid, stars, reason)')
        self.assertIn('DELETE FROM wanted WHERE player_id=%d', setw)

    def test_the_heat_decays_one_star_at_a_time(self):
        self.assertIn('#define CRIME_WANTED_DECAY      600', CRIME)
        pulse = block(CRIME, 'public RW_CrimePulse()')
        self.assertIn('gettime() - gWantedStamp[p] >= CRIME_WANTED_DECAY', pulse)
        self.assertIn('RW_CrimeWantedSet(p, gWantedStars[p] - 1, gWantedReason[p]);', pulse)

    def test_the_heat_survives_a_restart(self):
        self.assertIn('RW_CrimeLoad(playerid);', ACCOUNTS)
        load = block(CRIME, 'stock RW_CrimeLoad(playerid)')
        self.assertIn('SELECT stamp FROM wanted WHERE player_id=%d', load)

    def test_the_police_reason_is_a_real_one(self):
        self.assertIn('#define CRIME_REASON_ROBBERY    2', CRIME)
        self.assertIn('#define CRIME_REASON_DRUGS      5', CRIME)

    def test_the_heat_is_readable_by_command(self):
        self.assertIn('"wanted", true', CMDS)
        command = block(CRIME, 'stock RW_WantedCommand(playerid, const params[])')
        self.assertIn('RW_CrimeWantedText(gWantedStars[target], heat, sizeof heat);', command)
        self.assertIn('RW_PoliceOnDuty(playerid)', command)


class CrimeCraftTest(unittest.TestCase):
    """Three precursors make a dose, and only in a lab."""

    def test_the_recipe(self):
        self.assertIn('#define CRIME_CHEM_PER_DOSE     3', CRIME)
        self.assertIn('#define CRIME_CRAFT_SECONDS     30', CRIME)
        craft = block(CRIME, 'stock RW_CraftCommand(playerid)')
        self.assertIn('RW_CrimeCount(playerid, ITEM_CHEM) < CRIME_CHEM_PER_DOSE', craft)
        self.assertIn('RW_CrimeConsume(playerid, ITEM_CHEM, CRIME_CHEM_PER_DOSE)', craft)
        self.assertIn('SetTimerEx("RW_CrimeCraftDone", CRIME_CRAFT_SECONDS * 1000, false, "i", playerid);', craft)

    def test_the_cook_cannot_walk_away(self):
        craft = block(CRIME, 'stock RW_CraftCommand(playerid)')
        self.assertIn('TogglePlayerControllable(playerid, false);', craft)
        done = block(CRIME, 'public RW_CrimeCraftDone(playerid)')
        self.assertIn('TogglePlayerControllable(playerid, true);', done)

    def test_a_full_bag_returns_the_precursors(self):
        done = block(CRIME, 'public RW_CrimeCraftDone(playerid)')
        self.assertIn('RW_CrimeGive(playerid, ITEM_GREEN, 1)', done)
        self.assertIn('RW_CrimeGive(playerid, ITEM_CHEM, CRIME_CHEM_PER_DOSE);', done)

    def test_the_lab_can_be_raided(self):
        craft = block(CRIME, 'stock RW_CraftCommand(playerid)')
        self.assertIn('if (random(100) < 30)', craft)
        self.assertIn('варят Green', craft)

    def test_precursors_are_removed_for_real(self):
        consume = block(CRIME, 'stock RW_CrimeConsume(playerid, type, amount)')
        self.assertIn('DELETE FROM inventory_items WHERE uid=%d AND owner=%d AND slot=%d', consume)
        self.assertIn('UPDATE inventory_items SET amount=%d WHERE uid=%d', consume)
        self.assertIn('return left == 0;', consume)


class CrimeDrugTest(unittest.TestCase):
    """Green heals, but the fourth dose knocks you out."""

    def test_a_dose_heals_and_fogs_the_screen(self):
        self.assertIn('#define CRIME_GREEN_HEAL        25.0', CRIME)
        self.assertIn('#define CRIME_GREEN_EFFECT      60', CRIME)
        green = block(CRIME, 'stock RW_GreenCommand(playerid)')
        self.assertIn('SetPlayerHealth(playerid, health);', green)
        self.assertIn('SetPlayerDrunkLevel(playerid, 20000);', green)
        self.assertIn('RW_CrimeConsume(playerid, ITEM_GREEN, 1);', green)

    def test_the_effect_keeps_healing_and_then_stops(self):
        self.assertIn('#define CRIME_GREEN_REGEN       4', CRIME)
        pulse = block(CRIME, 'public RW_CrimePulse()')
        self.assertIn('gettime() < gDrugUntil[p]', pulse)
        self.assertIn('SetPlayerDrunkLevel(p, 0);', pulse)

    def test_an_overdose_knocks_the_player_down(self):
        self.assertIn('#define CRIME_GREEN_DOSES       3', CRIME)
        self.assertIn('#define CRIME_GREEN_MINUTES     5', CRIME)
        green = block(CRIME, 'stock RW_GreenCommand(playerid)')
        self.assertIn('if (doses >= CRIME_GREEN_DOSES)', green)
        self.assertIn('return RW_MedDown(playerid, x, y, z);', green)

    def test_the_dealer_stands_in_ganton(self):
        self.assertIn('#define CRIME_SELL_MIN          700', CRIME)
        self.assertIn('#define CRIME_SELL_MAX          1400', CRIME)
        sell = block(CRIME, 'stock RW_SellDrugsCommand(playerid)')
        self.assertIn('RW_CrimeBuyerNear(playerid)', sell)
        self.assertIn('CRIME_SELL_MIN + random(CRIME_SELL_MAX - CRIME_SELL_MIN + 1)', sell)
        self.assertIn('RW_AddMoney(playerid, total);', sell)

    def test_trading_in_front_of_the_police_raises_the_heat(self):
        sell = block(CRIME, 'stock RW_SellDrugsCommand(playerid)')
        self.assertIn('IsPlayerInRangeOfPoint(p, 60.0', sell)
        self.assertIn('RW_CrimeWantedAdd(playerid, 1, CRIME_REASON_DRUGS);', sell)


class CrimeItemTest(unittest.TestCase):
    """Precursors and Green are real inventory items."""

    def test_both_types_are_defined(self):
        self.assertIn('#define ITEM_CHEM                   19', CORE)
        self.assertIn('#define ITEM_GREEN                  20', CORE)
        self.assertIn('#define INV_TYPE_MAX 20', INV)

    def test_the_inventory_knows_them(self):
        model = block(INV, 'stock RW_InvModel(type, data = 0)')
        self.assertIn('if (type == ITEM_CHEM) return 1210;', model)
        self.assertIn('if (type == ITEM_GREEN) return 1578;', model)
        grams = block(INV, 'stock RW_InvGrams(type, data = 0, amount = 1)')
        self.assertIn('if (type == ITEM_CHEM) return 200 * amount;', grams)
        self.assertIn('if (type == ITEM_GREEN) return 50 * amount;', grams)

    def test_a_stack_is_limited(self):
        grant = block(INV, 'stock RW_InvGrant(playerid, type, data = 0, amount = 1)')
        self.assertIn('(type==ITEM_CHEM || type==ITEM_GREEN) && (data!=0 || amount<1 || amount>50)', grant)

    def test_they_have_names_everywhere(self):
        self.assertIn('Прекурсор', MARKET)
        self.assertIn('"Green"', MARKET)
        self.assertIn('"PRECURSOR"', TRANSPORT)
        self.assertIn('"GREEN"', TRANSPORT)


class CrimeInterfaceTest(unittest.TestCase):
    """The same panel in the browser and on TextDraw."""

    def test_both_interfaces_are_wired(self):
        self.assertIn('#define CEF_MODE_CRIME              (17)', CORE)
        self.assertIn('#define UI_CRIME 27', UI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_CRIME) return RW_CrimeCefClick(playerid, id);', CEFUI)
        self.assertIn('} else if (mode == UI_CRIME) {', INTER)
        self.assertIn('return RW_CrimeClick(playerid, row);', INTER)

    def test_the_panel_shows_the_target_the_heat_and_the_stash(self):
        rows = block(CRIME, 'stock RW_CrimeRows(playerid)')
        self.assertIn('Цель рядом: %s', rows)
        self.assertIn('Розыск: %s', rows)
        self.assertIn('Прекурсоры: %d шт.', rows)
        self.assertIn('Green: %d доз', rows)

    def test_the_commands_are_registered(self):
        for command in ('crime', 'heist', 'craft', 'green', 'selldrugs', 'wanted'):
            self.assertIn('"%s", true' % command, CMDS)

    def test_the_panel_works_without_the_browser_too(self):
        panel = block(CRIME, 'stock RW_CrimePanel(playerid)')
        self.assertIn('if (!RW_CefOn(playerid)) RW_Info(playerid,', panel)

    def test_the_pulse_runs_in_the_gamemode(self):
        self.assertIn('SetTimer("RW_CrimePulse", 1000, true);', MAIN)
        self.assertIn('RW_CrimeSchema()', MAIN)
        self.assertIn('#include <railway/crime>', MAIN)
        self.assertIn('RW_CrimeReset(playerid);', MAIN)


if __name__ == '__main__':
    unittest.main()
