/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_SQLITE_H__
#define __C_CREATEREPOLIB_SQLITE_H__

#include <glib.h>
#include <sqlite3.h>
#include "package.h"


/** \defgroup   sqlite        SQLite metadata API.
 */

#define CR_SQLITE_CACHE_DBVERSION       10      /*!< Version of DB api */

#define CR_DB_ERROR cr_db_error_quark()
GQuark cr_db_error_quark (void);

typedef struct _DbPrimaryStatements * DbPrimaryStatements;
typedef struct _DbFilelistsStatements * DbFilelistsStatements;
typedef struct _DbOtherStatements * DbOtherStatements;

/** \ingroup sqlite
 * Database type.
 */
typedef enum {
    DB_PRIMARY,         /*!< primary */
    DB_FILELISTS,       /*!< filelists */
    DB_OTHER            /*!< other */
} DatabaseType;

/** \ingroup sqlite
 * Macro over open_db function. Open (create new) primary sqlite sqlite db.
 *  - creates db file
 *  - creates primary tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
 */
#define open_primary_db(PATH, ERR)    open_db(PATH, DB_PRIMARY, ERR)

/** \ingroup sqlite
 * Macro over open_db function. Open (create new) filelists sqlite sqlite db.
 *  - creates db file
 *  - creates filelists tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
 */
#define open_filelists_db(PATH, ERR)  open_db(PATH, DB_FILELISTS, ERR)

/** \ingroup sqlite
 * Macro over open_db function. Open (create new) other sqlite sqlite db.
 *  - creates db file
 *  - opens transaction
 *  - creates other tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db connection
 */
#define open_other_db(PATH, ERR)      open_db(PATH, DB_OTHER, ERR)

/** \ingroup sqlite
 * Macro over close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define close_primary_db(DB, ERR)     close_db(DB, DB_PRIMARY, ERR)

/** \ingroup sqlite
 * Macro over close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define close_filelists_db(DB, ERR)   close_db(DB, DB_FILELISTS, ERR)

/** \ingroup sqlite
 * Macro over close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define close_other_db(DB, ERR)       close_db(DB, DB_OTHER, ERR)

/** \ingroup sqlite
 * Open (create new) other sqlite sqlite db.
 *  - creates db file
 *  - opens transaction
 *  - creates other tables
 *  - creates info table
 *  - tweak some db params
 * @param path                  Path to the db file.
 * @param db_type               Type of database (primary, filelists, other)
 * @param err                   **GError
 * @return                      open db connection
 */
sqlite3 *open_db(const char *path, DatabaseType db_type, GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the add_primary_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      DbPrimaryStatements object
 */
DbPrimaryStatements prepare_primary_db_statements(sqlite3 *db, GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the add_filelists_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      DbFilelistsStatements object
 */
DbFilelistsStatements prepare_filelists_db_statements(sqlite3 *db, GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the add_other_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      DbOtherStatements object
 */
DbOtherStatements prepare_other_db_statements(sqlite3 *db, GError **err);

/** \ingroup sqlite
 * Frees DbPrimaryStatements object.
 * @param stmts                 statements object
 */
void destroy_primary_db_statements(DbPrimaryStatements stmts);

/** \ingroup sqlite
 * Frees DbFilelistsStatements object.
 * @param stmts                 statements object
 */
void destroy_filelists_db_statements(DbFilelistsStatements stmts);

/** \ingroup sqlite
 * Frees DbOtherStatements object.
 * @param stmts                 statements object
 */
void destroy_other_db_statements(DbOtherStatements stmts);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void add_primary_pkg_db(DbPrimaryStatements stmts, Package *pkg);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void add_filelists_pkg_db(DbFilelistsStatements, Package *pkg);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void add_other_pkg_db(DbOtherStatements stmts, Package *pkg);

/** \ingroup sqlite
 * Insert record into the updateinfo table
 * @param                       open db connection
 * @checksum                    compressed xml file checksum
 * @err                         **GError
 */
void dbinfo_update(sqlite3 *db, const char *checksum, GError **err);

/** \ingroup sqlite
 * Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param db                    open db connection
 * @param db_type               Type of database (primary, filelists, other)
 * @param err                   **GError
 */
void close_db(sqlite3 *db, DatabaseType db_type, GError **err);

#endif /* __C_CREATEREPOLIB_SQLITE_H__ */
