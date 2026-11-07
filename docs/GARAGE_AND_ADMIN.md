# Station garage, permanent cars and admin driving tools

The interface is drawn by the browser: menus, inventory, HUD and notices are
pages in `server/scriptfiles/cef/railway` and every label is Russian. Without
the omp-cef client the very same menus open as native SA-MP dialogs, which
support Cyrillic too. There are no TextDraws left on the server, so `/uitd`
and `/tdlatin` are gone; `/uinative`, `/uiru` and `/tdrus` only print a
reminder about the browser.

## Station booth

A small security booth (object 3881) and an information pickup are near Unity
Station spawn. Entry point: **1735.5, -1941.0, 13.55**. Walk onto the marker to
open the garage. A proximity fallback also checks entry every second. Step
outside 3 metres to re-arm entry, so closing the panel does not reopen it
continuously. `/garage` also works while on foot within 4 metres of the booth,
in interior 0 / virtual world 0.

The panel shows six cars per page, with previous/next and close.
It lists permanent cars for which you **currently carry a key**, including
keys in your backpack and keys picked up from another player. It also lists
active temporary cars for which you have a key. Duplicate keys do not produce
duplicate permanent entries. A second click calls/recalls the selected car.

The spawn pad is **1742.0, -1953.0, 13.65**. It must be clear within 5 metres.
An occupied car cannot be recalled. An already active car is recalled rather
than duplicated. Each spawn/call rechecks distance, world, login and current
key possession. A recalled car is repaired, stopped and locked with its key
removed from the ignition. Tuning/flight settings reset on recall.

The booth's ground alignment and pickup accessibility need an in-game check;
these coordinates/models have not been visually tested in GTA here.

## Permanent car grants

Admin level 1+, in game:

```
/givecar <playerid|account_name> <modelid>
/givecar 0 411
/givecar Nick_Name 560
```

A player ID targets a logged-in online player; a name targets an existing
account case-insensitively, including an offline account. Numeric targets
are interpreted as online player IDs. New accounts are not created by this
command. Supported models follow `/spcar`'s current model restrictions.

One permanent car and one permanent key are saved atomically. The key needs
a free slot in the account's **main 50-slot inventory**, within its 50 kg
limit. Failure grants neither the car nor the key. Offline grants are loaded
on the next login. Online grants refresh the recipient's inventory.

Permanent cars survive server restarts as garage records, not as saved world
positions. Spawned entities are recreated/called at the station. Deleting a
spawned permanent entity or leaving the server does **not** revoke its keys.
Ownership is separate from possession: lending/dropping a key gives garage
and lock access, not ownership or owner-only passenger controls.

`/giveitem <playerid> 2 <vehicleid>` makes an additional key for an active
managed car. If that car is permanent, the extra key is also permanent.

`/spcar <modelid>` remains temporary: its car and keys are session-only and
expire when that temporary car is deleted. Startup removes temporary keys
only; tokens belonging to `owned_cars` are retained, including ground keys.

### Storage and upgrade

Existing `accounts` and `inventory_items` are preserved. Two tables are added
with `CREATE TABLE IF NOT EXISTS`: `owned_cars` and `permanent_keys`.
No password/hash changes, no destructive table rebuild, and no DB deletion.

`/givecar` uses a savepoint containing car, item and key metadata creation;
it publishes memory only after release/commit. Conditional item mutations,
UIDs, weight checks and anti-duplicate pickup checks remain unchanged.
A separate spawn-generation stamp prevents a delayed death timer for an old
vehicle from deleting a newly recalled instance with the same persistent key.

Back up `server/scriptfiles/railway.db`, update `railway.amx`, restart the
server and reconnect. Never delete the existing DB to install this update.

## Item context menu

Click an inventory/ground item to open a compact panel with its description,
weight and characteristics. It shows actions when applicable:

- **DROP**: owned items only; existing ignition/backpack-content checks apply.
- **WEAR**: backpacks only. Carrying a backpack already equips it automatically;
  this action confirms/reapplies its attachment, not a separate clothing system.
- **TO BAG**: only with a backpack, for eligible items outside the bag.
- **TO MAIN**: ground items or items currently in the backpack.
- **USE KEY**: owned car keys; insert/remove the key while driving that car.
- **CLOSE**: dismiss context panel, keeping inventory open.

Native SA-MP dialogs do not identify the mouse button either, so the browser
page is the only place where a **right-click** opens an item card. Without the
plugin the same actions are reachable from the rows of the native dialog:
click a row, then pick the action. Client support is only needed for the
browser pages; no client plugin is required to play.

## Admin driving commands

Use as a logged-in admin level 1+ in the driver seat of a managed car:

```
/flycar
/speed 240
/usk 35
/speed 0
/usk 0
```

`/speed <0..500>` sets a server-side speed cap in **km/h**. Zero removes the
custom cap. Raising a cap alone does not modify GTA handling; combine with
`/usk` if additional acceleration is needed.

`/usk <0..100>` overrides powered acceleration in **km/h gained per second**,
while the accelerate action is held (not while braking/handbraking).
For example, 35 targets +35 km/h per second until the cap, on a clear flat
road. Zero restores stock acceleration. With acceleration override but no
custom cap, the safety ceiling is 300 km/h. Gravity, slopes, traction, impacts,
network corrections and timer scheduling still affect observed motion.

Ground tuning stays with that active vehicle until recall/deletion/reset,
including if another driver takes over. It is not stored in the database.
Only administrators can change it.

### Flight approximation

`/flycar` toggles a **server-assisted flight imitation**, not native aircraft
handling. The key must be inserted and the engine running. Controls use
SA-MP game actions, so actual physical keys follow the client's bindings:

| Game action | Flight function | Typical binding |
|---|---|---|
| Accelerate / brake | Increase / decrease flight speed | W / S |
| Analog up / down | Pitch down / up | Numpad 8 / 2 |
| Steer left / right | Roll | A / D |
| Look left / right | Yaw | Q / E |

Body-forward/right vectors come from the vehicle quaternion. Angular velocity
controls pitch/roll/yaw; forward velocity provides thrust. The flight default
cap is 350 km/h; `/speed` changes it, `/usk` changes powered acceleration.
This is not a collision-free noclip or a true airplane conversion. Gravity,
client synchronization and latency may cause drift or jitter. Land before
turning flight off: normal gravity resumes immediately.

Flight is cancelled when the original admin stops driving that vehicle,
loses admin rights, turns off the engine, or the entity is recalled/deleted.
Timer time steps are bounded to prevent a stalled server producing a huge
acceleration jump. No plugins or client modifications are added.

## Manual GTA verification

Automated tests run SQLite statements and inspect source/AMX contracts; they
do not run GTA or prove rendering/flight feel. Before deployment:

1. Inspect booth alignment, marker accessibility, garage paging/closing,
   empty list, blocked pad, occupied recall and duplicate-key listing.
2. Grant online by ID and offline by name. Restart, log in, check permanent
   key and spawn. Verify `/spcar` keys expire, permanent copies/ground keys do not.
3. Drop/pass a permanent key: garage access follows possession. Recall from
   the other account, ensuring no duplicate entity or key is created.
4. Check context options for main/bag/ground items, backpack absence, empty
   bag restriction and inserted-key restriction. Check all UI is English.
5. As non-admin, reject all driving/grant commands. As admin, test `/speed`
   below/above stock, `/usk` acceleration, reset values and invalid inputs.
6. In an open area, test takeoff, pitch, bank, yaw, braking, landing, driver
   changes and admin revocation. Check steering response and synchronization
   from a second client; tune flight constants if needed.

## Выдача ключей: диагностика

`/cars` показывает id машин, номера RW-NNN и id постоянных машин (cN).
`/giveitem` принимает `bag|key`, id машины, `pNNN`, `cNNN` или цель наводки,
отказы сообщают конкретную причину. Подробности в docs/BACKPACK_AND_PICKUP.md.

## Будка на спавне и выдача ключа по id машины

Будка (объект 3881), пикап и 3D-подпись стоят в 3–5 метрах от точки спавна
(GARAGE_X/Y/Z = STATION + 2.5 / 2.4). Меню открывается при входе в
будку (радиус 2 м), по пикапу или командой `/garage` в радиусе 6 м: список
своих постоянных машин и временных `/spcar`.

Выдача ключа: `/givekey <id игрока>` открывает список машин по СА-МП id
(плюс cN для машин в гараже); клик по строке выдаёт ключ от этой машины.
Старый вариант `/giveitem <id> key <машина|pNNN|cNNN|цель>` тоже работает.
