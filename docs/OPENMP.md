# Перенос сервера на open.mp

Браузерный интерфейс живёт на плагине `omp-cef`. Его SA-MP-сборка не
отдаёт гейммоду события из браузера (причины — в `docs/УСТАНОВКА.md`,
раздел «Клики пропадают молча»), а open.mp-компонент — отдаёт. Поэтому
рабочий путь один: сервер на open.mp.

## Что уже проверено

- **Нативы.** Все нативы, которые вызывает гейммод, есть в open.mp:
  сверка исходников не нашла ни одного расхождения (`db_open`,
  `SHA256_PassHash`, `SetSpawnInfo`, `TogglePlayerSpectating`,
  `GetVehicleParamsEx`, `SetTimerEx` и остальные).
- **База.** `db_open("railway.db")` в open.mp, как и в SA-MP, ищет файл
  в `<каталог сервера>/scriptfiles/` — `railway.db` остаётся на месте.
- **Модули AMX.** `floatsin`, `floatcos`, `gettime`, `random` живут не в
  нативах, а в модулях AMX: open.mp инициализирует float, time, core,
  string и file для каждого скрипта (`Script/Script.cpp`, вызовы
  `amx_FloatInit` / `amx_TimeInit` при загрузке) — камера входа и
  генерация машин работают.
- **Порт CEF.** Компонент берёт игровой порт и прибавляет два
  (7777 → 7779), настройки `cef.*` необязательны.
- **Версия компонента: нужна ровно 1.4.0, не ниже.** Доставка событий
  на open.mp — это вызов `ProcessMainThreadTasks()` из `onTick`
  компонента; в 1.3.0 нет ни самого метода, ни тика (сверено по
  исходникам обеих версий). Поэтому компонент 1.3.0 загружается без
  ошибок и так же глух, как SA-MP-плагин: ресурсы отдаёт, клики — нет.
- **Таймеры.** `0` означает «не создался» на обеих платформах: в
  open.mp счётчик начинается с единицы (`timers.hpp`, `idx = 1`), ноль
  возвращается только при ошибке. Значит проверки вида
  `if (t != 0) KillTimer(t)` переносятся как есть — править нечего.

Пересобирать `railway.amx` не нужно: open.mp выполняет AMX, собранный
для SA-MP.

## Шаги

1. **Скачать open.mp** (Windows):
   <https://github.com/openmultiplayer/open.mp/releases/download/v1.6.7.3218/open.mp-win-x86.zip>
   — в официальном релизе только 32-битная сборка. Если стоит
   64-битная (из ночных сборок), это допустимо, но компонент тогда
   тоже нужен 64-битный. Распаковать в папку сервера — туда, где лежат
   `gamemodes`, `filterscripts` и `scriptfiles`.
2. **Скачать CEF-компонент версии 1.4.0** — open.mp-сборку той же
   разрядности, что и сервер:
   - сервер 32 бита (официальный `open.mp-win-x86.zip`):
     <https://github.com/aurora-mp/omp-cef/releases/download/v1.4.0/cef-omp-v1.4.0-x86.zip>
   - сервер 64 бита (ночные сборки):
     <https://github.com/aurora-mp/omp-cef/releases/download/v1.4.0/cef-omp-v1.4.0-x64.zip>

   Распаковать архив прямо в папку сервера: внутри лежит
   `components/Cef.dll`, и он должен попасть в существующую папку
   `components`. Архив самодостаточный (CEF вкомпилирован), других
   файлов рядом не нужно. Разрядность обязана совпасть с разрядностью
   `omp-server.exe`, иначе загрузка падает с
   `Failed to load component: %1 не является приложением Win32`.
   Узнать её можно так (PowerShell, в папке сервера):

   ```powershell
   foreach ($f in 'omp-server.exe','components\Cef.dll') {
     $b = [IO.File]::ReadAllBytes($f)
     $p = [BitConverter]::ToUInt32($b, 0x3C)
     $m = [BitConverter]::ToUInt16($b, $p + 4)
     "$f = " + $(if ($m -eq 0x8664) {'x64'} elseif ($m -eq 0x14c) {'x86'} else {'?'})
   }
   ```
3. **Убрать старый SA-MP плагин.** Удалить `plugins/cef.dll` и строку
   `plugins cef` из `server.cfg`. Иначе open.mp загрузит его как
   legacy-плагин (`pawn.legacy_plugins`) — а это та самая сборка,
   которая не отдаёт клики.
4. **Клиент.** Из того же релиза 1.4.0 — `cef.asi` и
   `client-files-v1.4.0.zip` в папку с GTA. Версии сервера и клиента
   совпадают: клиент 1.3.0 с сервером 1.4.0 не сойдётся, в логе будет
   `RequestJoin refused: version mismatch (pid=0 client=1.3.0 server=1.4.0)`.
5. **Удалить кэш ресурсов:** `scriptfiles/cef/railway.pak`. Он собран
   прошлой версией компонента, 1.4.0 пересоберёт его при старте.
6. **Остальное не трогать:** `gamemodes/railway.amx`,
   `filterscripts/admin_console.amx`, `scriptfiles/` вместе с
   `railway.db` (базу не перезаписывать).
7. **Первый запуск.** `omp-server.exe` прочитает `server.cfg`
   (компонент LegacyConfig) и сам создаст `config.json`. Если нужен
   чистый файл настроек — `omp-server.exe --default-config`.
8. Запустить `omp-server.exe` из папки сервера.

## Как понять, что клики пошли

| Проверка | Ожидаемо |
|---|---|
| лог при старте | `Successfully loaded component Cef Component (1.4.0.0)` и `[CEF] Component initialized (v1.4.0).` |
| лог при старте | нет ни `Loading plugin: cef`, ни `Tried to register native which is already registered` |
| вход | нативный диалог на секунду, затем карточка в браузере |
| нажать «Войти» | в логе `[auth] submit from 0 (ник): 8 chars`, карточка отвечает текстом |
| карточка молчит | в логе `the page never answered` — значит загружен не тот `Cef.dll` |

## Что обычно идёт не так

- **`plugins cef` остался в server.cfg** — грузится legacy-плагин, клики
  пропадают. Проверить `components\Cef.dll` и отсутствие
  `plugins\cef.dll`.
- **Разная разрядность:** сервер x86 требует компонент x86
  (`cef-omp-v1.4.0-x86.zip`). Компонент x64 open.mp пишет
  `Failed to load component: %1 не является приложением Win32.` —
  это ровно про битность, а не про «файл испорчен».
- **В логе есть `Loading plugin: cef` и `Plugin loaded (v1.3.0)`:**
  старый плагин всё ещё подключён через `plugins cef` в `server.cfg`.
  Он перехватывает порт 7779 и не отдаёт события, даже если
  компонент загрузился.
- **Компонент загрузился, но версия 1.3.0.0** (в строке
  `Successfully loaded component Cef Component (1.3.0.0)`): он глухой
  по устройству, события не придут никогда. Нужен 1.4.0.
- **В логе `Tried to register native which is already registered`:**
  компонент и плагин грузятся одновременно, нативы подменяют друг
  друга. Убрать `plugins cef` из `server.cfg`.
- **Не та рабочая папка:** open.mp ищет `scriptfiles/railway.db`
  относительно текущего каталога, запускать нужно из папки сервера.
  В логе это видно по строке `[railway:db] SQLite file: ...`.
- **Клиент другой версии:** `version mismatch` в логе — обновить
  `cef.asi` и `client-files` до 1.4.0.
- **Порт 7779 занят или закрыт:** CEF-компонент слушает его по UDP
  (игровой порт + 2); фаервол на сервере должен его пропускать.

## Если всё же остаться на SA-MP

Гейммод сам определяет немое окно и показывает нативные диалоги —
играть можно, но браузер остаётся только картинкой (см.
`docs/АВТОРИЗАЦИЯ.md`, раздел «Немой браузер»).
