"""Каждая строка панели отвечает на нажатие.

Строка, которая просто пишет «сделай это командой /xxx», — обман: игрок
нажимает, и ничего не происходит. Этот тест идёт по всем панелям, собирает
номера строк, которые сервер реально шлёт, и смотрит, разбирает ли их
обработчик нажатий.

Молчать имеют право только те строки, у которых и правда нет действия, —
они перечислены в INFO_ROWS с объяснением.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / 'server/pawno/include/railway'


def pawn(name):
    return (INC / f'{name}.inc').read_text(encoding='cp1251')


def block(src, signature):
    start = src.index(signature)
    depth = 0
    for i in range(start + len(signature) - 1, len(src)):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError('не закрыт блок: %s' % signature)


def panels():
    """Все функции вида RW_XxxRows(playerid) и номера строк, что они шлют."""
    found = []
    for path in sorted(INC.glob('*.inc')):
        src = pawn(path.stem)
        for match in re.finditer(r'\nstock (\w*Rows)\(playerid\)', src):
            name = match.group(1)
            body = block(src, 'stock %s(playerid)' % name)
            ids = set(int(x) for x in re.findall(r'RowOut\(playerid, (\d+),', body))
            ids |= set(int(x) for x in re.findall(r'RW_CefRow\(playerid, (\d+),', body))
            found.append((path.stem, name, sorted(ids)))
    return found


def handler(module, name):
    """Обработчик нажатий той же панели: RW_XxxAction."""
    src = pawn(module)
    base = name[:-4]
    for signature in ('stock %sAction(playerid, row, bool:cef)' % base,
                      'stock %sAction(playerid, id, bool:cef)' % base):
        if signature in src:
            return block(src, signature)
    return None


def handled_rows(action, source):
    """Номера, которые обработчик разбирает явно или по данным строки."""
    ids = set()
    for one in re.findall(r'row == (\d+)', action):
        ids.add(int(one))
    for low, high in re.findall(r'row >= (\d+) && row <= (\d+)', action):
        ids |= set(range(int(low), int(high) + 1))
    for low, high in re.findall(r'row >= (\d+) && row < (\d+)', action):
        ids |= set(range(int(low), int(high)))
    for limit in re.findall(r'row < (\w+) &&', action):
        if limit.isdigit():
            ids |= set(range(0, int(limit)))
    # Строки, которые разбираются по данным: gXxxIds[playerid][row] и тому
    # подобное означает, что номер — это индекс в массиве на сервере.
    if re.search(r'\[playerid\]\[row[^\]]*\]', action):
        for limit in re.findall(r'row < (\w+)', action):
            if limit.isdigit():
                ids |= set(range(0, int(limit)))
    return ids


# Строки без действия: их молчание честное, потому что нажимать нечего.
INFO_ROWS = {
    ('government', 2): 'судимости за три дня: цифру читают, а не покупают',
    ('market', 0): 'лот на витрине: номер разбирается по данным строки',
}


class EveryRowAnswersTest(unittest.TestCase):
    """Ни одна строка панели не притворяется кнопкой."""

    def test_no_row_is_deaf(self):
        checked = 0
        for module, name, ids in panels():
            action = handler(module, name)
            if action is None:
                continue
            known = handled_rows(action, action)
            for row in ids:
                checked += 1
                if row in known:
                    continue
                self.assertIn((module, row), INFO_ROWS,
                              'панель %s/%s: строка %d на нажатие не отвечает' % (module, name, row))
        self.assertTrue(checked >= 15, 'строк стало меньше: проверь тест (%d)' % checked)

    def test_the_info_rows_are_really_informational(self):
        """То, что молчит, не должно выглядеть как кнопка с командой."""
        rows = block(pawn('government'), 'stock RW_GovRows(playerid)')
        self.assertIn('"Судимость за 3 дня: %d"', rows)
        line = re.search(r'RW_GovRowOut\(playerid, 2,[^;]*;', rows, re.S)
        self.assertTrue(line)
        self.assertNotIn('/', line.group(0))

    def test_the_panels_are_found_at_all(self):
        names = [name for _, name, _ in panels()]
        for expected in ('RW_MarketRows', 'RW_CrimeRows', 'RW_GovRows', 'RW_FamilyRows', 'RW_GpsRows'):
            self.assertIn(expected, names, expected)


class CrimeClickTest(unittest.TestCase):
    """Криминал: цель, варка, сбыт и доза — по строкам."""

    def setUp(self):
        self.crime = pawn('crime')

    def test_the_target_row_starts_the_job(self):
        action = block(self.crime, 'stock RW_CrimeAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 0) {', action)
        self.assertIn('RW_CrimeNear(playerid, kind, index) < 0', action)
        self.assertIn('return RW_CrimeHeist(playerid);', action)

    def test_a_dose_is_taken_by_a_button(self):
        action = block(self.crime, 'stock RW_CrimeAction(playerid, row, bool:cef)')
        self.assertIn('if (cef && row == 7) return RW_GreenCommand(playerid);', action)
        open_block = block(self.crime, 'stock RW_CrimeOpen(playerid)')
        self.assertIn('RW_CefAction(playerid, 7, "Принять дозу", "ok")', open_block)
        self.assertIn('if (RW_CrimeCount(playerid, ITEM_GREEN) > 0)', open_block)

    def test_the_header_does_not_send_anyone_to_the_chat(self):
        open_block = block(self.crime, 'stock RW_CrimeOpen(playerid)')
        self.assertNotIn('/heist', open_block)


class GovernmentClickTest(unittest.TestCase):
    """Мэрия: цену за дело меняют нажатием, а не командой."""

    def setUp(self):
        self.gov = pawn('government')
        self.police = pawn('police')

    def test_the_price_row_asks_for_the_sum(self):
        action = block(self.gov, 'stock RW_GovAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 3) {', action)
        self.assertIn('RW_InputOpen(playerid, INPUT_LAW_PRICE, 0, "Цена за дело"', action)
        self.assertIn('RW_LawyerPriceSet', pawn('input'))

    def test_only_a_lawyer_sets_the_price(self):
        action = block(self.gov, 'stock RW_GovAction(playerid, row, bool:cef)')
        self.assertIn('if (!RW_IsLawyer(playerid))', action)
        rows = block(self.gov, 'stock RW_GovRows(playerid)')
        self.assertIn('"нажмите: сумму спросит поле ввода"', rows)
        self.assertIn('"сначала лицензия адвоката"', rows)

    def test_the_command_and_the_click_share_one_function(self):
        self.assertIn('stock RW_LawyerPriceSet(playerid, price)', self.police)
        self.assertIn('return RW_LawyerPriceSet(playerid, strval(a));', self.police)
        command = block(self.police, 'stock RW_LawyerPriceCommand(playerid, const params[])')
        self.assertNotIn('gLawyerPrice[playerid] = price;', command)


class FamilyRosterTest(unittest.TestCase):
    """Состав семьи: строка «Бойцов» показывает, кто в семье."""

    def setUp(self):
        self.family = pawn('family')

    def test_the_row_shows_the_roster(self):
        action = block(self.family, 'stock RW_FamilyAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 4 && gFamily[playerid] != FAMILY_NONE) return RW_FamilyList(playerid);', action)
        rows = block(self.family, 'stock RW_FamilyRows(playerid)')
        self.assertIn('"нажмите: покажет состав"', rows)

    def test_the_roster_comes_from_the_database(self):
        listing = block(self.family, 'stock RW_FamilyList(playerid)')
        self.assertIn('FROM family_members m LEFT JOIN accounts a ON a.id=m.player_id', listing)
        self.assertIn('gFamilyRankName[value]', listing)
        self.assertIn('db_free_result(r);', listing)


if __name__ == '__main__':
    unittest.main()
