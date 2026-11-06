"""Economy: jobs, the 24/7 shop and the bank account.

Money in the pocket and money on the account are different things, and every
rule lives on the server: the browser menu and the TextDraw menu call the same
stocks, and the state is kept in SQLite so it survives a restart. SQL from the
Pawn sources is executed here against a real SQLite database.
"""
from pathlib import Path
import re
import sqlite3
import unittest

ROOT = Path(__file__).resolve().parents[1]
def pawn(name):
    return (ROOT / 'server/pawno/include/railway' / f'{name}.inc').read_text(encoding='cp1251')

ECON = pawn('economy')
CORE = pawn('core')
UI = pawn('ui')
INV = pawn('inventory')
INTER = pawn('interaction')
CEFUI = pawn('cefui')
CMDS = pawn('commands')
ACC = pawn('accounts')
MAIN = (ROOT / 'server/gamemodes/railway.pwn').read_text(encoding='cp1251')


def sql(source, prefix, index=0):
    found = [s for s in re.findall(r'"([^"\r\n]+)"', source) if s.startswith(prefix)]
    return found[index]


def block(source, start, end='\nstock '):
    return source.split(start, 1)[1].split(end, 1)[0]


class EconomyTableTest(unittest.TestCase):
    """The account and the work card, executed for real."""

    def setUp(self):
        self.db = sqlite3.connect(':memory:', isolation_level=None)
        self.db.execute(sql(ECON, 'CREATE TABLE IF NOT EXISTS bank_accounts '))
        self.db.execute(sql(ECON, 'CREATE TABLE IF NOT EXISTS job_players '))

    def tearDown(self):
        self.db.close()

    def columns(self, table):
        return [row[1] for row in self.db.execute('PRAGMA table_info(%s)' % table)]

    def test_table_shapes(self):
        self.assertEqual(self.columns('bank_accounts'), ['player_id', 'balance'])
        self.assertEqual(self.columns('job_players'), ['player_id', 'job', 'level', 'exp', 'tasks', 'earned', 'stamp'])

    def test_the_account_is_one_row_per_player(self):
        insert = sql(ECON, 'INSERT OR REPLACE INTO bank_accounts(')
        self.db.execute(insert % (7, 5000))
        self.db.execute(insert % (7, 9000))
        rows = self.db.execute(sql(ECON, 'SELECT balance FROM bank_accounts WHERE player_id=%d') % 7).fetchall()
        self.assertEqual(rows, [(9000,)])

    def test_the_work_card_keeps_level_and_record(self):
        insert = sql(ECON, 'INSERT OR REPLACE INTO job_players(')
        self.db.execute(insert % (7, 2, 3, 21, 40, 18000, 1000))
        row = self.db.execute(sql(ECON, 'SELECT job,level,exp,tasks,earned FROM job_players WHERE player_id=%d') % 7).fetchone()
        self.assertEqual(row, (2, 3, 21, 40, 18000))

    def test_quitting_keeps_the_card_for_the_next_hire(self):
        insert = sql(ECON, 'INSERT OR REPLACE INTO job_players(')
        self.db.execute(insert % (7, 1, 2, 12, 20, 9000, 1000))
        self.db.execute(insert % (7, 0, 2, 12, 20, 9000, 2000))
        row = self.db.execute('SELECT job,level,exp,tasks,earned FROM job_players WHERE player_id=7').fetchone()
        self.assertEqual(row, (0, 2, 12, 20, 9000))

    def test_a_fresh_player_has_no_row_and_no_money(self):
        self.assertIsNone(self.db.execute(sql(ECON, 'SELECT balance FROM bank_accounts WHERE player_id=%d') % 9).fetchone())
        load = block(ECON, 'stock RW_EconomyLoad(playerid)')
        self.assertIn('gBank[playerid] = 0;', load)
        self.assertIn('gJob[playerid] = JOB_NONE;', load)
        self.assertIn('gJobLevel[playerid] = 1;', load)


class BankTest(unittest.TestCase):
    """Cash and account are different wallets."""

    def test_only_an_atm_opens_the_terminal(self):
        openfn = block(ECON, 'stock RW_BankOpen(playerid)')
        self.assertIn('if (!RW_BankNear(playerid)) return RW_Error(playerid, "No ATM nearby. Try the station, the hospital or a gas station.");', openfn)
        near = block(ECON, 'stock bool:RW_BankNear(playerid)')
        self.assertIn('BANK_ATM_RANGE', near)

    def test_a_deposit_moves_cash_to_the_account(self):
        deposit = block(ECON, 'stock RW_BankDeposit(playerid, amount)')
        self.assertIn('if (!RW_TakeMoney(playerid, amount)) return RW_Error(playerid, "Not enough cash.");', deposit)
        self.assertIn('gBank[playerid] += amount;', deposit)
        self.assertIn('RW_EconomySave(playerid);', deposit)

    def test_a_withdrawal_needs_money_on_the_account(self):
        withdraw = block(ECON, 'stock RW_BankWithdraw(playerid, amount)')
        self.assertIn('if (gBank[playerid] < amount) return RW_Error(playerid, "Not enough money on the account.");', withdraw)
        self.assertIn('RW_AddMoney(playerid, amount);', withdraw)

    def test_a_transfer_moves_money_between_accounts(self):
        transfer = block(ECON, 'stock RW_BankTransfer(playerid, target, amount)')
        self.assertIn('if (amount < BANK_MIN_TRANSFER) return RW_Error(playerid, "At least 50 on a transfer.");', transfer)
        self.assertIn('gBank[playerid] -= amount;', transfer)
        self.assertIn('gBank[target] += amount;', transfer)
        self.assertIn('RW_EconomySave(target);', transfer)

    def test_the_menu_shows_both_wallets(self):
        self.assertIn('format(dest, size, "Cash %s / Account %s", cash, acc);', ECON)
        self.assertIn('RW_UI_Row(playerid, 0, "Put all cash on the account");', ECON)
        self.assertIn('RW_UI_Row(playerid, 1, "Put 1 000 on the account");', ECON)
        self.assertIn('RW_UI_Row(playerid, 2, "Take 1 000 in cash");', ECON)
        self.assertIn('RW_CefRow(playerid, 0, "Put all cash on the account", "all", "wallet");', ECON)
        self.assertIn('RW_CefRow(playerid, 3, "Transfer: /transfer <id> <sum>", "chat", "users");', ECON)

    def test_every_write_is_saved_at_once(self):
        add = block(ECON, 'stock RW_BankAdd(playerid, amount)')
        self.assertIn('RW_EconomySave(playerid);', add)
        take = block(ECON, 'stock bool:RW_BankTake(playerid, amount)')
        self.assertIn('if (amount <= 0 || gBank[playerid] < amount) return false;', take)

    def test_numbers_are_parsed_from_the_command(self):
        for name in ('RW_DepositCommand', 'RW_WithdrawCommand'):
            fn = block(ECON, 'stock %s(playerid, const params[])' % name)
            self.assertIn('RW_IsNumeric(word)', fn)
        transfer = block(ECON, 'stock RW_TransferCommand(playerid, const params[])')
        self.assertIn('Use: /transfer <player id> <sum>.', transfer)


class ShopTest(unittest.TestCase):
    """The 24/7 sells legal goods for cash."""

    def test_goods_are_inventory_items_with_a_price(self):
        table = ECON.split('static const gShopItems[][4] = {', 1)[1].split('};', 1)[0]
        self.assertEqual(len(re.findall(r'\{[^}]+\}', table)), 7)
        for item in ('ITEM_PHONE', 'ITEM_SIM', 'ITEM_BACKPACK', 'ITEM_BANDAGE', 'ITEM_SYRINGE', 'ITEM_LOCKPICK', 'ITEM_ARMOUR'):
            self.assertIn(item, table)
        self.assertIn('{ ITEM_PHONE,    0, 1,  900 }', ECON)
        self.assertIn('{ ITEM_BANDAGE,  0, 1,  150 }', ECON)

    def test_a_purchase_checks_cash_then_room_then_grants(self):
        buy = block(ECON, 'stock RW_ShopBuy(playerid, row)')
        self.assertIn('if (gMoney[playerid] < price) return RW_Error(playerid, "Not enough cash for it.");', buy)
        self.assertIn('if (RW_InvFree(playerid, 0, type, data, amount) < 0) return RW_Error(playerid, "No room for it in your pockets.");', buy)
        self.assertIn('new uid = RW_InvGrant(playerid, type, data, amount);', buy)
        self.assertIn('RW_TakeMoney(playerid, price);', buy)

    def test_the_counter_is_a_place(self):
        openfn = block(ECON, 'stock RW_ShopOpen(playerid)')
        self.assertIn('if (!RW_ShopNear(playerid)) return RW_Error(playerid, "No 24/7 shop nearby.");', openfn)
        action = block(ECON, 'stock RW_ShopAction(playerid, row, bool:cef)')
        self.assertIn('if (!RW_ShopNear(playerid)) return RW_Error(playerid, "Step away from the counter: the shop is closed.");', action)

    def test_the_price_is_shown_on_the_row(self):
        self.assertIn('format(dest, size, "%s - %s", name, price);', ECON)
        self.assertIn('RW_TrunkName(gShopItems[row][0], gShopItems[row][1], name, sizeof name);', ECON)

    def test_both_faces_share_one_handler(self):
        self.assertIn('stock RW_ShopCefClick(playerid, row) { return RW_ShopAction(playerid, row, true); }', ECON)
        self.assertIn('stock RW_ShopClick(playerid, row) { return RW_ShopAction(playerid, row, false); }', ECON)
        self.assertIn('RW_UI_Row(playerid, 7, "Close");', ECON)
        self.assertIn('RW_CefAction(playerid, 8, "Close", "danger");', ECON)


class JobTest(unittest.TestCase):
    """Three jobs, two-point tasks, level from experience."""

    def test_constants(self):
        for name, value in (('JOB_NONE', 0), ('JOB_LOADER', 1), ('JOB_COURIER', 2), ('JOB_TRUCKER', 3), ('JOB_COUNT', 4)):
            self.assertRegex(ECON, r'#define %s\s+%d' % (name, value))
        self.assertRegex(ECON, r'#define JOB_MAX_LEVEL\s+5')
        self.assertRegex(ECON, r'#define JOB_EXP_PER_LEVEL\s+10')

    def test_each_job_pays_its_own_rate(self):
        base = block(ECON, 'stock RW_JobBasePay(job)')
        self.assertIn('if (job == JOB_LOADER) return 200;', base)
        self.assertIn('if (job == JOB_COURIER) return 120;', base)
        self.assertIn('if (job == JOB_TRUCKER) return 300;', base)
        salary = block(ECON, 'stock RW_JobSalary(job)')
        self.assertIn('if (job == JOB_TRUCKER) return 400;', salary)

    def test_hiring_happens_at_the_exchange(self):
        hire = block(ECON, 'stock RW_JobHire(playerid, job)')
        self.assertIn('if (RW_JobHireIndex(playerid) < 0) return RW_Error(playerid, "Come to the labour exchange: the docks, the depot or the truck base.");', hire)
        self.assertIn('RW_EconomySave(playerid);', hire)
        self.assertIn('Hired as %s, level %d. /work takes a task, /quitjob leaves.', hire)

    def test_quitting_keeps_the_record(self):
        quitfn = block(ECON, 'stock RW_JobQuit(playerid)')
        self.assertIn('if (gJobStage[playerid] > 0) RW_JobTaskCancel(playerid, true);', quitfn)
        self.assertIn('The level and the record stay on the card.', quitfn)

    def test_a_task_is_two_points_with_a_deadline(self):
        take = block(ECON, 'stock RW_JobTaskTake(playerid)')
        self.assertIn('gJobStage[playerid] = 1;', take)
        self.assertIn('gJobEnd[playerid] = gettime() + JOB_TASK_SECONDS;', take)
        self.assertIn('SetPlayerMapIcon(playerid, ECON_TASK_ICON, x, y, z, 0, 0xF5B04CFF, MAPICON_GLOBAL);', take)
        self.assertIn('if (GetTickCount() - gJobTick[playerid] < JOB_COOLDOWN_MS) return RW_Info(playerid, "Catch your breath for a second.");', take)

    def test_each_job_has_its_own_conditions(self):
        take = block(ECON, 'stock RW_JobTaskTake(playerid)')
        self.assertIn('if (gJob[playerid] == JOB_TRUCKER && !RW_JobVehicleOk(playerid))', take)
        self.assertIn('if (gJob[playerid] == JOB_LOADER && GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)', take)
        self.assertIn('return RW_Error(playerid, "A loader works on foot: step out of the car.");', take)
        self.assertIn('for (new i; i < sizeof gTruckModels; i++) if (gTruckModels[i] == model) return true;', ECON)

    def test_a_task_is_taken_and_handed_in_at_the_mark(self):
        work = block(ECON, 'stock RW_WorkCommand(playerid)')
        self.assertIn('if (gJobStage[playerid] == 0) return RW_JobTaskTake(playerid);', work)
        self.assertIn('IsPlayerInRangeOfPoint(playerid, JOB_TASK_RANGE, gJobX[playerid], gJobY[playerid], gJobZ[playerid])', work)
        self.assertIn('Not there yet: %d m to the mark.', work)
        self.assertIn('gJobStage[playerid] = 2;', work)
        self.assertIn('gJobPay[playerid] = floatround(RW_EconomyDistance(playerid, x, y, z) / 6.0);', work)

    def test_paying_grows_with_the_level(self):
        done = block(ECON, 'stock RW_JobTaskDone(playerid)')
        self.assertIn('new pay = RW_JobBasePay(gJob[playerid]) + gJobPay[playerid];', done)
        self.assertIn('pay = pay + floatround(float(pay) * 0.15 * float(gJobLevel[playerid] - 1));', done)
        self.assertIn('RW_AddMoney(playerid, pay);', done)
        self.assertIn('gJobTasks[playerid]++;', done)
        self.assertIn('gJobExp[playerid]++;', done)
        self.assertIn('if (gJobExp[playerid] >= gJobLevel[playerid] * JOB_EXP_PER_LEVEL && gJobLevel[playerid] < JOB_MAX_LEVEL)', done)

    def test_a_task_expires(self):
        pulse = ECON.split('public RW_EconomyPulse()', 1)[1]
        self.assertIn('if (gJobStage[playerid] > 0 && gettime() > gJobEnd[playerid]) RW_JobTaskCancel(playerid);', pulse)
        cancel = block(ECON, 'stock RW_JobTaskCancel(playerid, bool:silent = false)')
        self.assertIn('RemovePlayerMapIcon(playerid, ECON_TASK_ICON);', cancel)
        self.assertIn('Out of time. The task is gone, take another one.', cancel)

    def test_the_salary_arrives_on_the_account(self):
        payday = block(ECON, 'stock RW_EconomyPayday(playerid)')
        self.assertIn('new salary = RW_JobSalary(gJob[playerid]) * gJobLevel[playerid];', payday)
        self.assertIn('new interest = gBank[playerid] * BANK_INTEREST_PERCENT / 100;', payday)
        self.assertIn('if (interest > BANK_INTEREST_MAX) interest = BANK_INTEREST_MAX;', payday)
        self.assertIn('gBank[playerid] += salary + interest;', payday)
        pulse = ECON.split('public RW_EconomyPulse()', 1)[1]
        self.assertIn('if (gPayday[playerid] >= PAYDAY_SECONDS)', pulse)

    def test_the_menu_offers_three_jobs_and_a_way_out(self):
        self.assertIn('RW_UI_Row(playerid, 0, "Loader: crates in the docks");', ECON)
        self.assertIn('RW_UI_Row(playerid, 1, "Courier: parcels across the city");', ECON)
        self.assertIn('RW_UI_Row(playerid, 2, "Trucker: runs to other cities");', ECON)
        self.assertIn('RW_UI_Row(playerid, 3, "Quit the job");', ECON)
        self.assertIn('if (gJob[playerid] != JOB_NONE) RW_CefAction(playerid, 7, "Quit the job", "danger");', ECON)
        action = block(ECON, 'stock RW_JobAction(playerid, row, bool:cef)')
        self.assertIn('if (row == 3 && !cef) return RW_JobQuit(playerid);', action)
        self.assertIn('if (row == 7 && cef) return RW_JobQuit(playerid);', action)


class EconomyWiringTest(unittest.TestCase):
    """The slice is wired into the menus, the commands and the game mode."""

    def test_the_mode_knows_the_slice(self):
        self.assertIn('#include <railway/economy>', MAIN)
        self.assertIn('RW_EconomySchema()', MAIN)
        self.assertIn('RW_EconomyInit();', MAIN)
        self.assertIn('SetTimer("RW_EconomyPulse", 1000, true);', MAIN)
        self.assertIn('RW_EconomySave(playerid);', MAIN)
        self.assertIn('RW_EconomyReset(playerid);', MAIN)

    def test_the_account_is_loaded_on_login(self):
        login = block(ACC, 'stock RW_LoginSuccess(playerid, accid, skin, money, admin)')
        self.assertIn('RW_EconomyLoad(playerid);', login)

    def test_menu_modes_are_registered(self):
        self.assertRegex(UI, r'#define UI_SHOP 19')
        self.assertRegex(UI, r'#define UI_BANK 20')
        self.assertRegex(UI, r'#define UI_JOB 21')
        count = int(re.search(r'#define UI_COUNT (\d+)', UI)[1])
        self.assertLess(21, count)
        self.assertRegex(CORE, r'#define CEF_MODE_SHOP\s+\(9\)')
        self.assertRegex(CORE, r'#define CEF_MODE_BANK\s+\(10\)')
        self.assertRegex(CORE, r'#define CEF_MODE_JOB\s+\(11\)')

    def test_clicks_reach_the_right_menu(self):
        self.assertIn('else if (mode == UI_SHOP) {', INTER)
        self.assertIn('return RW_ShopClick(playerid, row);', INTER)
        self.assertIn('return RW_BankClick(playerid, row);', INTER)
        self.assertIn('return RW_JobClick(playerid, row);', INTER)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_SHOP) return RW_ShopCefClick(playerid, id);', CEFUI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_BANK) return RW_BankCefClick(playerid, id);', CEFUI)
        self.assertIn('if (gCefMode[playerid] == CEF_MODE_JOB) return RW_JobCefClick(playerid, id);', CEFUI)

    def test_commands_exist_in_both_faces(self):
        for cmd, stock in (('shop', 'RW_ShopCommand'), ('bank', 'RW_BankCommand'), ('balance', 'RW_BalanceCommand'),
                           ('job', 'RW_JobCommand'), ('work', 'RW_WorkCommand'), ('quitjob', 'RW_QuitJobCommand'),
                           ('payday', 'RW_PaydayCommand')):
            self.assertIn('if (!strcmp(cmd, "%s", true)) return %s(playerid)' % (cmd, stock), CMDS)
        for cmd, stock in (('deposit', 'RW_DepositCommand'), ('withdraw', 'RW_WithdrawCommand'), ('transfer', 'RW_TransferCommand')):
            self.assertIn('if (!strcmp(cmd, "%s", true)) return %s(playerid, params);' % (cmd, stock), CMDS)
        self.assertIn('Money: /shop, /bank, /balance, /deposit, /withdraw, /transfer <id> <sum>, /job, /work, /quitjob, /payday.', CMDS)

    def test_the_points_are_marked_in_the_world(self):
        init = block(ECON, 'stock RW_EconomyInit()')
        self.assertIn('CreatePickup(1239, 1, gShops[i][0]', init)
        self.assertIn('CreatePickup(1559, 1, gAtms[i][0]', init)
        self.assertIn('Create3DTextLabel("24/7 SHOP', init)
        self.assertIn('Create3DTextLabel("ATM', init)
        self.assertIn('LABOUR EXCHANGE', init)

    def test_every_menu_has_a_text_draw_twin(self):
        for cef, td in (('RW_CefBank', 'RW_BankPanel'), ('RW_CefShop', 'RW_ShopPanel'), ('RW_CefJob', 'RW_JobPanel')):
            self.assertIn('stock %s(playerid)' % cef, ECON)
            self.assertIn('stock %s(playerid)' % td, ECON)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefBank(playerid);', ECON)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefShop(playerid);', ECON)
        self.assertIn('if (RW_CefOn(playerid)) return RW_CefJob(playerid);', ECON)

    def test_no_cyrillic_reaches_the_player(self):
        """Comments are Russian, every string the player can read is English."""
        code = re.sub(r'/\*[\s\S]*?\*/', '', ECON)
        for line in code.splitlines():
            line = line.split('//', 1)[0]
            self.assertNotRegex(line, r'[А-Яа-я]')


if __name__ == '__main__':
    unittest.main()
