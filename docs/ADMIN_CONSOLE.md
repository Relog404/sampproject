# Admin assignment: stock SA-MP console / RCON

The `admin_console` filterscript relays custom RCON commands to the gamemode's
`RW_ConsoleCommand` public. Stock SA-MP dispatches custom `OnRconCommand`
callbacks to filterscripts, so a gamemode-only handler is insufficient.
No player chat command or client plugin is used.

## Install the repair

Stop the server, preserve/back up `scriptfiles/railway.db`, and update:

- `gamemodes/railway.amx`
- `filterscripts/admin_console.amx`
- `server.cfg`: include `admin_console` on the `filterscripts` line.

Example, when no other filterscripts are used:

```
filterscripts admin_console
```

If you have other filterscripts, keep them on that line too. Restart the
server; the log should contain `[admin_console] Console relay loaded`.
`setup.ps1` now checks the relay binary and the build script compiles both
Pawn sources. Both supplied AMX files use version 8 for SA-MP 0.3.7.

## Usage

In the server console (no slash):

```
setadmin 0 3
setadmin Nick_Name 3
setadmin Nick_Name 0
```

- Numeric target: an online player ID; the player must already be logged in.
- Name: an existing account, case-insensitive; online or offline.
- Levels are exactly 0, 1, 2 or 3. Level 0 removes admin rights.
- Nonexistent accounts are not created and invalid levels are not clamped.
- Rights and player colour update immediately after successful SQL commit.
- Accounts, hashes, salts and inventory are preserved.

From an authenticated in-game RCON session, `/rcon setadmin 0 3` uses the
same path. Ordinary `/setadmin` chat remains disabled for everyone, including
admins. Keep your RCON password private and configure a strong one locally.

Run `dbcheck` in the console if the command reports a database error. It prints
the actual SQLite path and runs the existing non-destructive write check.
Do not delete or recreate railway.db to fix a failed grant. The command only
reports success after reading back the level and committing the transaction.

## Checks

Automated tests cover extracted SQLite updates, case-insensitive lookup on a
legacy case-sensitive schema, preserving hashes/salts, rollback, removal of
rights, relay registration, command authorization and AMX version/publics.
They do not start the Windows SA-MP server. Verify the relay startup message,
console grant, reconnect persistence and removal of rights on the actual server.
