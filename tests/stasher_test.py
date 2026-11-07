"""The stasher: a job that feeds the family.

The tenth profession closes the loop between work, families and crime: the
stash the courier hides pays him cash, and his family gets a precursor on the
common shelf. The tests below check the job is wired like every other one —
hire point, two stages, weather and night bonuses — and that the family share
is real.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


ECON = pawn('economy')
FAMILY = pawn('family')
CORE = pawn('core')


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class StasherJobTest(unittest.TestCase):
    """A twelfth profession, wired like the others."""

    def test_the_job_exists(self):
        self.assertIn('#define JOB_STASHER             12', ECON)
        self.assertIn('#define JOB_COUNT               13', ECON)

    def test_the_labour_exchange_names_it_in_russian(self):
        name = block(ECON, 'stock RW_JobName(job, dest[], size = sizeof dest)')
        title = block(ECON, 'stock RW_JobTitle(job, dest[], size = sizeof dest)')
        self.assertIn('job == JOB_STASHER', name)
        self.assertIn('job == JOB_STASHER', title)
        self.assertIn('Закладчик', name)
        self.assertIn('тайники по районам', title)

    def test_it_pays_like_a_dangerous_job(self):
        base = block(ECON, 'stock RW_JobBasePay(job)')
        salary = block(ECON, 'stock RW_JobSalary(job)')
        self.assertIn('if (job == JOB_STASHER) return 210;', base)
        self.assertIn('if (job == JOB_STASHER) return 360;', salary)

    def test_the_hire_point_is_a_real_place(self):
        hire = block(ECON, 'static const Float:gJobHire[][4] = {', '};')
        self.assertIn('{ 12.0, 1755.0, -1855.0, 13.6 }', hire)
        self.assertIn('JOB_STASHER_HIRE       11', ECON)

    def test_the_task_has_two_stages(self):
        point = block(ECON, 'stock RW_JobPoint(playerid, job, stage, &Float:x, &Float:y, &Float:z)')
        stasher = point.split('if (job == JOB_STASHER)', 1)[1].split('\n    }', 1)[0]
        self.assertIn('gJobHire[JOB_STASHER_HIRE]', stasher)
        self.assertIn('gStashSpots', stasher)

    def test_the_stashes_stand_in_the_family_districts(self):
        spots = block(ECON, 'static const Float:gStashSpots[][3] = {', '};')
        self.assertEqual(len(re.findall(r'\{[^}]+\}', spots)), 6)
        for place in ('2244.4, -1665.4',      # Гантон
                      '2503.4, -1722.4',      # Лас-Колинас
                      '1944.3, -1772.5',      # заправка в Idlewood
                      '1833.4, -1842.4',      # Юнити
                      '2202.2, -1965.0',      # Уиллоуфилд
                      '2645.6, -2020.3'):     # Loco Low Co
            self.assertIn(place, ECON)

    def test_the_night_pays_more(self):
        bonus = block(ECON, 'stock RW_JobTimeBonus(job)')
        self.assertIn('if (job == JOB_STASHER && (hour >= 22 || hour < 6)) return 20;', bonus)


class StasherFamilyTest(unittest.TestCase):
    """Every stash feeds the crew: a precursor and a share of the cash."""

    def setUp(self):
        self.done = block(ECON, 'stock RW_JobTaskDone(playerid)')

    def test_the_share_is_taken_before_the_pay(self):
        self.assertTrue(self.done.index('JOB_STASHER') < self.done.index('RW_JobBasePay'))

    def test_the_shelf_gets_a_precursor(self):
        self.assertIn('RW_FamilyStash(gFamily[playerid], ITEM_CHEM, 0, 1)', self.done)
        self.assertIn('RW_FamilyAddMoney(gFamily[playerid], 30);', self.done)

    def test_a_full_shelf_does_not_break_the_job(self):
        self.assertIn('RW_FamilyStash(gFamily[playerid], ITEM_CHEM, 0, 1)', self.done)
        self.assertIn('Склад семьи забит', self.done)
        self.assertIn('RW_FamilyAddMoney(gFamily[playerid], 30);', self.done)

    def test_only_a_family_man_shares(self):
        self.assertIn('gFamily[playerid] != FAMILY_NONE', self.done)

    def test_the_family_globals_are_visible_to_the_economy(self):
        self.assertIn('new gFamily[MAX_PLAYERS];', CORE)
        self.assertIn('#define FAMILY_NONE             0', CORE)
        self.assertNotIn('new gFamily[MAX_PLAYERS];', FAMILY)


class FamilyStashTest(unittest.TestCase):
    """Putting an item on the shelf without touching an inventory."""

    def test_it_writes_straight_into_the_table(self):
        stash = block(FAMILY, 'stock RW_FamilyStash(family, type, data, amount)')
        self.assertIn('INSERT INTO family_items(family,type,data,amount)', stash)

    def test_it_respects_the_limit_and_the_arguments(self):
        stash = block(FAMILY, 'stock RW_FamilyStash(family, type, data, amount)')
        self.assertIn('if (family <= 0 || type <= 0 || amount < 1) return 0;', stash)
        self.assertIn('RW_FamilyItemCount(family) >= FAMILY_SLOTS', stash)

    def test_the_economy_can_call_it(self):
        self.assertIn('forward RW_FamilyStash(family, type, data, amount);', CORE)
        self.assertIn('forward RW_FamilyAddMoney(family, amount);', CORE)


if __name__ == '__main__':
    unittest.main()
