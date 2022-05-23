# This is a simple cmake sript to compile the Sqlite3 amalgam source

add_library(Sqlite3 SHARED sqlite3.c sqlite3.h)