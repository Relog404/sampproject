"""Coordinates: every point in the mode is a real place on the GTA SA map.

The gamemode must not invent numbers: the station, the hospital, the police
plaza, the shops, the ATMs, the hiring points and the job spots all sit where
they sit on the San Andreas map. These checks keep it that way - they verify
that each point lies inside the right part of the state and is labelled with
the real district or town.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')


CORE = pawn('core')
ECON = pawn('economy')
TRANSPORT = pawn('transport')
PHONEAPPS = pawn('phoneapps')
GARAGE = pawn('garage')


def define(source, name):
    return float(re.search(r'#define %s\s+\(?(-?[0-9.]+)' % name, source).group(1))


def table(source, name):
    body = source.split('static const Float:%s' % name, 1)[1]
    body = body.split('{', 1)[1].split('};', 1)[0]
    rows = []
    for line in body.splitlines():
        line = line.strip().rstrip(',')
        if line.startswith('{'):
            numbers = line.split('}', 1)[0].lstrip('{')
            rows.append([float(v) for v in numbers.split(',')])
    return rows


def point(source, name):
    """Одна точка: static const Float:name[3] = { x, y, z };"""
    body = source.split('static const Float:%s[3]' % name, 1)[1]
    body = body.split('{', 1)[1].split('}', 1)[0]
    return [float(v) for v in body.split(',')]


class StationTest(unittest.TestCase):
    """Unity Station: the spawn, the login camera and the garage."""

    def test_the_station_is_the_real_unity_station(self):
        self.assertIn('Unity Station', CORE)
        self.assertAlmostEqual(define(CORE, 'STATION_X'), 1742.9, places=1)
        self.assertAlmostEqual(define(CORE, 'STATION_Y'), -1862.6, places=1)
        self.assertTrue(0.0 < define(CORE, 'STATION_Z') < 40.0)

    def test_the_spawn_pad_is_next_to_the_station(self):
        pad_x = define(GARAGE, 'GARAGE_PAD_X')
        pad_y = define(GARAGE, 'GARAGE_PAD_Y')
        # Площадка выдачи рядом со станцией, а не в другом конце города.
        distance = ((pad_x - define(CORE, 'STATION_X')) ** 2 + (pad_y - define(CORE, 'STATION_Y')) ** 2) ** 0.5
        self.assertLess(distance, 60.0)


class LandmarkTest(unittest.TestCase):
    """Hospital, police, tuning and the impound lot."""

    def test_the_hospital_is_all_saints_general(self):
        self.assertIn('All Saints General', CORE)
        x, y, z = define(CORE, 'HOSPITAL_X'), define(CORE, 'HOSPITAL_Y'), define(CORE, 'HOSPITAL_Z')
        self.assertTrue(1000.0 < x < 1300.0 and -1400.0 < y < -1200.0 and 10.0 < z < 30.0)

    def test_the_police_plaza_is_lspd_headquarters(self):
        self.assertIn('LSPD', CORE)
        x, y = define(CORE, 'POLICE_STATION_X'), define(CORE, 'POLICE_STATION_Y')
        # Pershing Square, Downtown Los Santos.
        self.assertTrue(1500.0 < x < 1600.0 and -1720.0 < y < -1650.0)

    def test_the_tuning_shop_is_loco_low_co(self):
        self.assertIn('Loco Low Co', CORE)
        x, y = define(CORE, 'TUNING_X'), define(CORE, 'TUNING_Y')
        self.assertTrue(2600.0 < x < 2700.0 and -2060.0 < y < -2000.0)   # Willowfield

    def test_the_impound_lot_is_behind_the_station(self):
        x, y = define(CORE, 'IMPOUND_X'), define(CORE, 'IMPOUND_Y')
        self.assertTrue(1600.0 < x < 1670.0 and -1730.0 < y < -1670.0)


class LosSantosTest(unittest.TestCase):
    """Shops, ATMs, gas stations and photo spots: districts of Los Santos."""

    def test_the_shops_sit_in_named_districts(self):
        rows = table(ECON, 'gShops')
        self.assertEqual(len(rows), 4)
        block = ECON.split('static const Float:gShops', 1)[1].split('};', 1)[0]
        for district in ('Downtown', 'Ganton', 'Las Colinas', 'Unity'):
            self.assertIn(district, block)
        for x, y, z in rows:
            self.assertTrue(1000.0 < x < 2700.0 and -2000.0 < y < -1200.0 and 0.0 < z < 50.0)

    def test_the_atms_are_at_real_addresses(self):
        rows = table(ECON, 'gAtms')
        self.assertEqual(len(rows), 4)
        block = ECON.split('static const Float:gAtms', 1)[1].split('};', 1)[0]
        for place in ('Unity Station', 'All Saints', 'Temple', 'LSPD'):
            self.assertIn(place, block)
        for x, y, z in rows:
            self.assertTrue(900.0 < x < 1900.0 and -2400.0 < y < -900.0 and 0.0 < z < 60.0)

    def test_the_gas_stations_are_real_stations(self):
        rows = table(TRANSPORT, 'gGasStations')
        self.assertEqual(len(rows), 4)
        block = TRANSPORT.split('static const Float:gGasStations', 1)[1].split('};', 1)[0]
        for district in ('Temple', 'Ocean Docks', 'Idlewood', 'Willowfield'):
            self.assertIn(district, block)

    def test_the_photo_spots_are_landmarks(self):
        rows = table(PHONEAPPS, 'gPhotoSpots')
        self.assertEqual(len(rows), 6)
        block = PHONEAPPS.split('static const Float:gPhotoSpots', 1)[1].split('};', 1)[0]
        for place in ('UNITY STATION', 'ALL SAINTS', 'OCEAN DOCKS', 'LSPD HQ', 'LOCO LOW CO', 'DOWNTOWN LS'):
            self.assertIn(place, PHONEAPPS)
        for x, y, z in rows:
            self.assertTrue(1000.0 < x < 2700.0 and -2300.0 < y < -1300.0 and 0.0 < z < 50.0)


class JobMapTest(unittest.TestCase):
    """Hiring points and task spots: real districts, towns and cities."""

    def test_every_job_hires_in_its_own_place(self):
        rows = table(ECON, 'gJobHire')
        self.assertEqual(len(rows), 12)
        block = ECON.split('static const Float:gJobHire', 1)[1].split('};', 1)[0]
        for place in ('Ocean Docks', 'Unity Station', 'Willowfield', 'Blueberry Acres',
                      'Palomino Creek', 'Hunter Quarry', 'Angel Pine', 'Idlewood',
                      'El Corona', 'LS International'):
            self.assertIn(place, block)
        for job, x, y, z in rows:
            self.assertTrue(-3000.0 < x < 3000.0 and -3000.0 < y < 3000.0, (x, y))
            self.assertTrue(-100.0 < z < 100.0, z)

    def test_the_loader_works_in_the_docks(self):
        for x, y, z in table(ECON, 'gLoaderCrates') + [point(ECON, 'gLoaderDrop')]:
            # Ocean Docks: причалы южнее города.
            self.assertTrue(2650.0 < x < 2850.0 and -2450.0 < y < -2300.0)

    def test_the_courier_delivers_to_real_addresses(self):
        rows = table(ECON, 'gCourierPoints')
        self.assertGreaterEqual(len(rows), 6)
        block = ECON.split('static const Float:gCourierPoints', 1)[1].split('};', 1)[0]
        for place in ('Unity Station', 'All Saints', 'LSPD', 'Ganton', 'Las Colinas', 'Idlewood'):
            self.assertIn(place, block)
        for x, y, z in rows:
            self.assertTrue(1000.0 < x < 2600.0 and -2000.0 < y < -1300.0)

    def test_the_trucker_drives_to_other_cities(self):
        rows = table(ECON, 'gTruckerPoints')
        self.assertEqual(len(rows), 3)
        # San Fierro, Bayside и Dillimore - далеко от Лос-Сантоса.
        for x, y, z in rows:
            self.assertGreater(((x - 1742.9) ** 2 + (y + 1862.6) ** 2) ** 0.5, 500.0)

    def test_the_bus_route_runs_through_los_santos(self):
        rows = table(ECON, 'gBusStops')
        self.assertEqual(len(rows), 6)
        block = ECON.split('static const Float:gBusStops', 1)[1].split('};', 1)[0]
        for place in ('Willowfield', 'Las Colinas', 'Ganton', 'Unity Station', 'Downtown', 'All Saints'):
            self.assertIn(place, block)
        for x, y, z in rows:
            # Весь маршрут внутри Лос-Сантоса.
            self.assertTrue(1100.0 < x < 2600.0 and -2000.0 < y < -1300.0)
            self.assertTrue(0.0 < z < 50.0)

    def test_the_out_of_town_jobs_are_in_the_countryside(self):
        # Ферма, лес и карьер - за городом, а не в центре.
        for name in ('gFarmRows', 'gMushroomSpots', 'gQuarryVeins', 'gTreeSpots'):
            for x, y, z in table(ECON, name):
                self.assertGreater(((x - 1742.9) ** 2 + (y + 1862.6) ** 2) ** 0.5, 500.0, name)

    def test_each_job_returns_to_its_own_drop_point(self):
        # Точка сдачи отличается от точек сбора.
        for spots, drop in (('gGarbageBins', 'gGarbageDump'), ('gFarmRows', 'gFarmBarn'),
                            ('gMushroomSpots', 'gMushroomBuyer'), ('gQuarryVeins', 'gQuarryCrusher'),
                            ('gTreeSpots', 'gSawmill'), ('gPostBoxes', 'gPostOffice')):
            drop_point = point(ECON, drop)
            for spot in table(ECON, spots):
                self.assertNotEqual(tuple(spot), tuple(drop_point), spots)
