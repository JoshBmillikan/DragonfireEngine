-- This file handles setup of the materials database

CREATE TABLE material
(
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

CREATE UNIQUE INDEX material_name ON material (name);

CREATE TABLE shader
(
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    spv  BLOB NOT NULL
);