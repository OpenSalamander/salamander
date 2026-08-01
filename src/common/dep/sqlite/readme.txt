
SQLite sources are available at http://www.sqlite.org/download.html
Use sqlite-amalgamation-*.zip.
Conceptually, new Salamander versions should use the newest SQLite versions
so we can read the newest binary versions of SQLite databases.

Update:
salamand\sqlite\sqlite3.c
salamand\plugins\shared\sqlite\sqlite3.h

Add to the to-do list:
- test the new SQLite version on Google Drive and inspect TRACE to confirm that
  we read the path to its folder
