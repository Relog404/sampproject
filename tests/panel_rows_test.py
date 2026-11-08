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
    # подобное означает, что номер — это индекс в массиве на сервере. Границу
    # могут называть и числом, и через define (IMPOUND_ROWS, MDT_ROWS...).
    if re.search(r'\[playerid\]\[row[^\]]*\]', action):
        for limit in re.findall(r'row < (\w+)', action):
            if limit.isdigit():
                ids |= set(range(0, int(limit)))
        for limit in re.findall(r'row >= (\w+)', action):
            if limit.isdigit():
                continue
            found = re.search(r'#define %s\s+\(?(\d+)' % limit, source)
            if found:
                ids |= set(range(0, int(found.group(1))))
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
            known = handled_rows(action, pawn(module) + pawn('core'))
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


class InlinePanelTest(unittest.TestCase):
    """Панели, которые шлют строки прямо из функции отрисовки.

    Не у каждой панели есть отдельная RW_XxxRows: настройки клавиш, выбор
    клавиши, телепорт по метке и экран раненого рисуются на месте. Их строки
    проверяются так же: номер, который сервер шлёт, обработчик обязан
    разбирать — или строка честно ничего не делает.
    """

    # панель: (модуль, функция отрисовки, ветка обработчика)
    PANELS = {
        'settings': ('interaction', 'stock RW_UI_Settings(playerid)', 'if (mode == UI_SETTINGS) {'),
        'bind': ('interaction', 'stock RW_UI_Click(playerid, row)', 'if (mode == UI_BIND) {'),
        'map': ('interaction', 'stock RW_MapPrompt(playerid)', '} else if (mode == UI_MAP) {'),
        'downed': ('medical', 'stock RW_MedCef(playerid)', 'stock RW_MedCefClick(playerid, id)'),
        'phone': ('phone', 'stock RW_PhoneMenu(playerid)', 'if (mode == UI_PHONE) {'),
    }
    # строки, которые шлются в цикле: номер в RW_UI_Row — переменная
    LOOPS = {'settings': [0, 1, 2]}
    # строки без действия: молчание честное
    INFO = {
        ('settings', 5): 'ключи сохраняются сами: нажимать нечего',
        ('map', 1): '«нет» — панель просто закрывается',
        ('phone', 0): 'баланс и тариф: строка-справка',
    }

    def emitted(self, name):
        module, painter, _ = self.PANELS[name]
        body = block(pawn(module), painter)
        ids = set(int(x) for x in re.findall(r'RW_UI_Row\(playerid, (\d+),', body))
        ids |= set(self.LOOPS.get(name, []))
        return sorted(ids)

    def answers(self, name):
        module, _, branch = self.PANELS[name]
        body = block(pawn(module), branch)
        ids = set(int(x) for x in re.findall(r'(?:row|id) == (\d+)', body))
        for limit in re.findall(r'row < (\d+)', body):
            ids |= set(range(0, int(limit)))
        return ids

    def test_every_inline_row_answers(self):
        for name in self.PANELS:
            known = self.answers(name)
            for row in self.emitted(name):
                if row in known:
                    continue
                self.assertIn((name, row), self.INFO,
                    'панель %s: строка %d на нажатие не отвечает' % (name, row))

    def test_the_settings_are_known_at_all(self):
        self.assertEqual(self.emitted('settings'), [0, 1, 2, 3, 4, 5, 6])
        self.assertEqual(sorted(self.answers('settings')), [0, 1, 2, 3, 4, 6])
        self.assertEqual(self.emitted('downed'), [0, 1])
        self.assertEqual(sorted(self.answers('downed')), [0, 1])

    def test_the_hud_and_the_cursor_are_switched_by_a_row(self):
        """/hud и /mouse — про стрелку на экране: в панели это две строки."""
        panel = block(pawn('interaction'), 'stock RW_UI_Settings(playerid)')
        self.assertIn('RW_UI_Row(playerid, 3, RW_HudIsOn(playerid) ? ("HUD: on (press to hide)") : ("HUD: off (press to show)"), "eye");', panel)
        self.assertIn('RW_UI_Row(playerid, 4, RW_HudMouseIsOn(playerid)', panel)
        branch = block(pawn('interaction'), 'if (mode == UI_SETTINGS) {')
        self.assertIn('else if (row == 3) { RW_HudToggle(playerid, true); RW_UI_Settings(playerid); }', branch)
        self.assertIn('else if (row == 4) { RW_HudMouseToggle(playerid); RW_UI_Settings(playerid); }', branch)
        self.assertIn('stock bool:RW_HudIsOn(playerid) { return gHudOn[playerid]; }', pawn('hud'))
        self.assertIn('stock bool:RW_HudMouseIsOn(playerid) { return gHudMouse[playerid]; }', pawn('hud'))

    def test_a_gas_station_refuels_by_a_row(self):
        """/refuel был единственным способом: теперь это строка в карточке."""
        inter = pawn('interaction')
        self.assertIn('else if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_CefRow(playerid, 5, "Refuel: a full tank for cash", "gas", "fuel");', inter)
        self.assertIn('else if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_UI_Row(playerid, 5, "Refuel: a full tank for cash", "fuel");', inter)
        # клик: кабина у самолёта иначе заправка
        card = block(inter, 'stock RW_CefVehicleClick(playerid, id)')
        self.assertIn('if (RW_TransportAtGas(playerid) && !RW_MechOnDuty(playerid)) RW_FuelRefuel(playerid, v);', card)
        native = block(inter, '} else if (mode == UI_VEHICLE) {')
        self.assertIn('RW_FuelRefuel(playerid, cv); RW_UI_Vehicle(playerid);', native)

    def test_the_gps_mark_is_taken_off_by_a_button(self):
        """/gps off остался командами, в панели метку снимает кнопка."""
        city = pawn('city')
        self.assertIn('RW_CefAction(playerid, 9, "Снять метку", "");', city)
        self.assertIn('RW_UI_Row(playerid, 6, "Снять метку", "close");', city)
        action = block(city, 'stock RW_GpsAction(playerid, row, bool:cef)')
        self.assertIn('if (row == (cef ? 9 : 6)) {', action)
        self.assertIn('RW_GpsClear(playerid);', action)
        self.assertIn('return RW_GpsOpen(playerid);', action)

    def test_the_key_choice_swaps_two_keys(self):
        branch = block(pawn('interaction'), 'if (mode == UI_BIND) {')
        self.assertIn('static const keys[] = { KEY_YES, KEY_NO, KEY_CTRL_BACK, KEY_WALK };', branch)
        self.assertIn('for (new i; i < 3; i++) if (i != index && gControlKeys[playerid][i] == keys[row])', branch)

    def test_the_contacts_are_called_by_the_row(self):
        menu = block(pawn('phone'), 'stock RW_PhoneContactsMenu(playerid)')
        self.assertIn('gPhoneContNum[index2] = number;', menu)
        click = block(pawn('phone'), 'stock RW_PhoneClick(playerid, row, mode)')
        self.assertIn('if (row >= 0 && row < PHONE_CONTACTS_PAGE && gPhoneContNum[row] > 0) {', click)
        self.assertIn('return RW_PhoneCall(playerid, number);', click)

    def test_every_panel_has_a_close_button_in_the_browser(self):
        for name in ('settings', 'bind', 'map'):
            module, painter, _ = self.PANELS[name]
            body = block(pawn(module), painter)
            self.assertIn('if (RW_CefOn(playerid)) RW_CefAction(playerid, 8, "Close", "danger");', body, name)

    def test_the_market_keeps_the_command_for_the_dialog_only(self):
        rows = block(pawn('market'), 'stock RW_MarketRows(playerid)')
        self.assertIn('if (RW_CefOn(playerid)) RW_MarketRowOut(playerid, 0, "Лотов нет",', rows)
        self.assertIn('"Вернитесь позже или выставите свой: /sell"', rows)


class CloseButtonTest(unittest.TestCase):
    """У каждой браузерной панели есть кнопка, которая её закрывает.

    Esc работает всегда, но кнопка нужна: игрок не обязан знать про Esc, а
    сервер всё равно должен снять курсор. Проверяем обе половины договора:
    панель шлёт кнопку, обработчик на её номер закрывает панель.
    """

    CLOSE_LABELS = ('Close', 'Закрыть', 'Выйти', 'Выход', 'Give up')

    def sources(self):
        return dict((path.stem, pawn(path.stem)) for path in sorted(INC.glob('*.inc')))

    def painters(self, src):
        """Функции, которые открывают браузерную панель."""
        out = []
        for match in re.finditer(r'\nstock (\w+)\(([^)\n]*)\)\n\{', src):
            name, args = match.group(1), match.group(2)
            try:
                body = block(src, 'stock %s(%s)' % (name, args))
            except (ValueError, AssertionError):
                continue
            if 'RW_CefOpen(playerid, CEF_MODE_' in body:
                out.append((name, body))
        return out

    def close_ids(self, body, src_all):
        """Номера кнопок с закрывающей подписью (число или define)."""
        ids = []
        for raw, label in re.findall(r'RW_CefAction\(playerid, (\w+), "([^"]*)"', body):
            if label not in self.CLOSE_LABELS:
                continue
            if raw.isdigit():
                ids.append(int(raw))
                continue
            found = re.search(r'#define %s\s+\(?(\d+)' % raw, src_all)
            self.assertTrue(found, 'не найден define %s' % raw)
            ids.append(int(found.group(1)))
        return ids

    def test_every_browser_panel_has_a_close_button(self):
        all_src = '\n'.join(self.sources().values())
        panels = 0
        for module, src in self.sources().items():
            for name, body in self.painters(src):
                if 'CEF_MODE_NONE' in body:
                    continue
                panels += 1
                ids = self.close_ids(body, all_src)
                self.assertTrue(ids, '%s/%s: панель без кнопки закрытия' % (module, name))
                for one in ids:
                    # номер пишут и числом, и через define (например CEF_JOB_CLOSE)
                    tokens = [str(one)] + [m.group(1) for m in
                                           re.finditer(r'#define (\w+)\s+\(?' + str(one) + r'\b', all_src)]
                    branch = None
                    for token in tokens:
                        branch = re.search(
                            r'(?:id|row) == (?:\(cef \? )?' + token + r'\b[^;]{0,160}', all_src)
                        if branch:
                            break
                    self.assertTrue(branch, '%s/%s: кнопка %d никто не разбирает' % (module, name, one))
                    self.assertIn('RW_UI_Close', branch.group(0),
                        '%s/%s: кнопка %d не закрывает панель' % (module, name, one))
        self.assertGreaterEqual(panels, 20, 'панелей стало меньше: проверь тест (%d)' % panels)

    def test_the_passengers_page_closes_too(self):
        inter = pawn('interaction')
        menu = block(inter, 'stock RW_CefPassengers(playerid)')
        self.assertIn('RW_CefAction(playerid, 8, "Close", "danger");', menu)
        click = block(inter, 'stock RW_CefPassengerClick(playerid, id)')
        self.assertIn('if (id == 8) return RW_UI_Close(playerid);', click)


class HintTest(unittest.TestCase):
    """Подсказка называет кнопку там, где кнопка есть.

    Одно и то же правило нельзя объяснять одинаково: в браузере действие
    лежит на строке или кнопке, а команда — запасной путь для тех, у кого
    браузера нет. Поэтому в каждом таком месте две подсказки: одна про
    нажатие, другая про команду.
    """

    PAIRS = [
        ('market', 'Этот лот на аукционе: выберите его — под списком появится кнопка ставки.',
         'Этот лот уже на аукционе: ставка — /bid <лот> <сумма>.'),
        ('market', 'Свой лот покупать нельзя: выберите его — под списком появится кнопка снятия.',
         'Свой лот покупать нельзя: снимите его командой /cancel <лот>.'),
        ('market', 'Аукцион закончился: заберите предмет кнопкой «Забрать со склада».',
         'Аукцион закончился. Заберите предмет на складе выдачи: /take.'),
        ('property', 'The city sealed the doors: press the tax row in the property panel.',
         'The city sealed the doors: pay the tax with /paytax.'),
        ('vehicles', 'No key in the ignition: press the key row in the car card (Y on the car).',
         'No key in the ignition. Use /key to insert it.'),
        ('economy', 'Сначала устройтесь: подойдите к точке найма и нажмите Y.',
         'Сначала устройтесь на работу: /job на бирже труда.'),
        ('economy', 'Таксист берёт заказы в приложении Uber Drive: иконка такси в телефоне.',
         'Таксист берёт заказы в приложении Uber Drive: /uber или /taxi.'),
        ('family', 'Предмет ушёл на склад семьи: забрать его — нажатием на строку склада.',
         'Забрать: /ftake <номер>, список: /fstore list.'),
        ('family', 'Машина передана семье: ключи лежат на складе, забрать — нажатием.',
         'ключи лежат на складе. Взять: /ftake <номер>.'),
        ('property', 'You are already inside: \\"Go outside\\" in the panel takes you out.',
         'You are already inside. /exit gets you out.'),
        ('inventory', 'Item received: Alt + U opens your pockets.',
         'Item received. /inv'),
        ('inventory', 'SIM card, tariff %s (item %d): Alt + U, then click the SIM.',
         'SIM card, tariff %s (item %d). /sim inserts it.'),
        ('inventory', 'Medkit granted (item %d): Alt + U, then click it to heal.',
         'Medkit granted (item %d). /heal uses it.'),
        ('hud', 'HUD is off: the same row in Settings brings it back.',
         'HUD is off. /hud brings it back.'),
        ('city', 'Метка стоит: езжайте за ней. Снять — кнопка в навигаторе.',
         'Метка стоит: езжайте за ней. Снять — /gps off.'),
        ('vehicles', 'Your car-menu key opens the card: doors, key and engine are rows in it.',
         '/door{FFFFFF} - unlock; /key - insert key'),
        ('ems', 'The calls are rows in the tablet: Y with a badge or a medkit opens it.',
         '/take <id> - take a call, /mdt - the tablet.'),
        ('interaction', 'No SIM card: buy one at a 24/7 shop and insert it from your pockets (Alt + U).',
         'No SIM card: buy one and insert it with /sim.'),
        ('phone', 'No SIM card: buy one at a 24/7 shop and insert it from your pockets (Alt + U).',
         'No SIM card: buy one and insert it with /sim.'),
        ('family', 'Вы уже в семье: выйти можно строкой в панели семьи.',
         'Вы уже в семье: сначала /fleave.'),
        ('phoneapps', 'You are already in a group: the group app has a leave row.',
         'You are already in a group. /gleave first.'),
        ('police', 'You already have an open case: close it in the lawyer app.',
         'You already have an open case: /endcase first.'),        ('cabin', 'Aim at a plane and press Y: the card there has a \\"board\\" row. Max 20 people.',
         'Aim at a plane, or use /board <vehicle id>. Max 20 people.'),
    ]

    def test_the_browser_names_the_button_and_the_dialog_the_command(self):
        for module, in_browser, in_dialog in self.PAIRS:
            src = pawn(module)
            self.assertIn(in_browser, src, '%s: подсказка про кнопку' % module)
            self.assertIn(in_dialog, src, '%s: подсказка про команду' % module)
            self.assertNotRegex(in_browser, r'/[A-Za-z]',
                '%s: в браузере не должно быть команды' % module)
            self.assertRegex(in_dialog, r'/[A-Za-z]',
                '%s: без браузера команда — единственный путь' % module)

    def test_the_browser_text_is_chosen_by_the_face(self):
        for module, in_browser, in_dialog in self.PAIRS:
            src = pawn(module)
            start = src.index(in_browser)
            head = src[max(0, start - 320):start]
            self.assertIn('RW_CefOn(', head, '%s: выбор по лицу, а не наугад' % module)

    def test_a_gifted_key_does_not_send_anyone_to_the_chat(self):
        garage = pawn('garage')
        self.assertIn('RW_Ok(target, "Car key received: it lies in your pockets.");', garage)
        self.assertNotIn('Car key received. /inv', garage)


if __name__ == '__main__':
    unittest.main()
