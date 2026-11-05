# Equipped backpack and aimed ground pickup

## Backpack equipment

Backpacks are no longer automatically worn merely because they are carried.
Select a backpack in MAIN, then click the **top-left equipment slot** beside
the character preview (the BACK slot). The item's context **WEAR** action
performs the same operation. The backpack moves out of the 50 MAIN slots
into the equipment slot; its UID is retained, not duplicated.

Only the equipped backpack unlocks the 30 BAG slots / 40 kg. The other seven
equipment slots remain reserved for future clothing/accessory features.
Multiple carried backpacks do not add capacity. Backpack weight (1 kg) counts
toward the main 50 kg carrying budget, including when equipped.

To remove it: click the equipped backpack slot, then **TO MAIN**. BAG must be
empty, and MAIN needs a free slot. Remove first, then DROP from MAIN if desired.
A bag with contents cannot disappear or be replaced by equipping another bag.
Equipment, items and their UIDs are persisted immediately in SQLite.

An additive `inventory_equipment` table stores equipment ownership. An equipped
item's original row has NULL inventory ownership but is explicitly excluded
from all ground loading/pickup queries. No accounts or item tables are rebuilt.
On upgrade, old accounts with occupied BAG slots have one existing backpack
migrated into BACK automatically, preserving their contents. Other backpacks
must be equipped manually. This migration is idempotent on later restarts.

## Backpack alignment

The old sideways 90-degree rotation is removed. Model 3026 is attached to
spine bone 1 with new default offsets (-0.11, -0.08, 0), neutral rotation and
unit scale. A forced reattach after spawning or `/setskin` refreshes the fit.
GTA rendering has not been visually tested in this workspace; different skins
can still require alignment adjustments.

Admin level 1+ with an equipped backpack can use **`/bagfit`** to open SA-MP's
native attached-object editor. Move/rotate the bag onto the back and save.
Cancel restores the previous fit. Positions are bounded; scale stays 1:1.
The fit is stored per account and skin in `backpack_fit`. Changing skins while
editing cannot save the previous editor session onto the new skin/another bag.
No client plugin is required.

## Ground interaction

While on foot, outside all menus, aim the centre of the camera at a dropped
item within 3 metres. A small TextDraw hint shows the item name, weight and
pickup action. Looking away, moving away, changing world/interior, opening a
menu or entering a vehicle hides the hint.

Use the **Secondary Attack** game action (normally **F / Enter**) or **`/take`**.
The hint does not enable a cursor or require opening inventory. Items go to
MAIN; if MAIN is full/overweight, BAG is used only if a backpack is equipped
and that section can accept the item. Backpacks themselves cannot go in BAG.

An arbitrary physical **E** key is not distinguishable in stock server-only
SA-MP. If you bind Secondary Attack to E in GTA controls, it can perform this
action, but the server cannot discover that binding. The default hint therefore
says F / ENTER, not an unimplemented E shortcut. Near a vehicle, this game
action may also start GTA's vehicle-entry animation; `/take` avoids that input
conflict. Picking up is rejected once the player is no longer on foot.

The target is selected by camera-ray proximity, not merely the nearest item.
It is recomputed on pickup; the shared transfer path revalidates UID, range,
world, state, section capacity, weight and SQL ownership. Equipped items are
excluded from pickup SQL even though their original row is not in MAIN/BAG.
Two players cannot take the same UID. Without a collision plugin, the camera
ray test does not provide map/object wall occlusion.

## Key issuance hardening

`/spcar` and `/givecar` now read back the inserted key using its account/slot
identity instead of connection-global `last_insert_rowid()`. Permanent car
records are read back by their unique key token and owner. Key copies reject
unknown/stale tokens before insertion. Invalid grants roll back before the
inventory cache changes. Temporary key revocation and new temporary car
creation immediately refresh open inventory views.

`/givecar` also includes equipped-backpack weight when checking offline or
online inventory capacity. Temporary and permanent key lifetimes are otherwise
unchanged. The reported key bug still needs a concrete reproduction (duplicate,
missing, wrong car, disappearing, or grant error) to establish its exact cause;
these hardening changes alone do not prove every reported symptom fixed.

## Deploy and verify

Back up `server/scriptfiles/railway.db`, update `railway.amx`, restart and
reconnect. Do not delete the DB. Existing console relay configuration remains.

Check in the actual GTA client:

1. A newly granted/carried backpack is not worn and BAG stays locked.
2. Select it, click BACK, verify the MAIN slot frees, the model appears on
   the back and BAG opens. Reconnect and verify equipment persists.
3. Move keys into BAG; removing the backpack must fail. Empty BAG, remove it
   to MAIN and verify capacity locks again. No extra backpack copy should exist.
4. Check the fit from a second client on the intended skins. Use `/bagfit` if
   required, then reconnect/change skin to verify per-skin persistence.
5. Drop a key/backpack, aim toward/away from it, use F/Enter or `/take` and try
   simultaneous pickup with another player. No hint should require a cursor.
6. Verify fresh temporary keys, copies, permanent keys, restarts, destroyed
   cars, full/overweight MAIN, equipped bag and blocked removals. Report the
   precise command, IDs and message for any remaining key issue.

Automated checks execute extracted SQLite transitions and inspect source/AMX
contracts. They do not run the Pawn VM or emulate GTA rendering/input.
