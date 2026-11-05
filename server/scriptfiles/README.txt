Runtime data directory: KEEP THIS FOLDER next to samp-server.exe.

The gamemode automatically creates railway.db here when the server starts.
Do not create an empty database by hand. The folder must be writable by the
account that runs samp-server.exe. Start the server with server/ as its working
directory. setup.ps1 creates this folder if it is missing.

railway.db contains player accounts. It is intentionally excluded from Git.
Do not delete or overwrite an existing database to fix a registration error.
Stop the server before making a backup; preserve any existing -wal/-shm files
with it if SQLite has not checkpointed them yet.

For diagnostics, run dbcheck in the server console and inspect server_log.txt.
