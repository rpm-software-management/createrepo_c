/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#ifndef __C_CREATEREPOLIB_SQLITE_H__
#define __C_CREATEREPOLIB_SQLITE_H__

#include <glib.h>
#include <sqlite3.h>
#include "package.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   sqlite        SQLite metadata API.
 *
 * Module for writing sqlite metadata databases.
 *
 * Example:
 * \code
 * cr_Package *pkg;
 * cr_SqliteDb *primary_db;
 *
 * // Load pkg (See parsepkg or parsehdr module)
 *
 * // Create primary sqlite database
 * primary_db = cr_db_open_primary("/foo/bar/repodata/primary.sqlite", NULL);
 *
 * // Add all packages here
 * cr_db_add_pkg(primary_db, pkg, NULL);
 *
 * // Add checksum of XML version of file (primary in this case)
 * cr_db_dbinfo_update(primary_db, "foochecksum", NULL);
 *
 * // Cleanup
 * cr_db_close(primary_db, NULL);
 * \endcode
 *
 *  \addtogroup sqlite
 *  @{
 */

#define CR_DB_CACHE_DBVERSION       10      /*!< Version of DB api */

/** Database type.
 */
typedef enum {
    CR_DB_PRIMARY,      /*!< primary */
    CR_DB_FILELISTS,    /*!< filelists */
    CR_DB_OTHER,        /*!< other */
    CR_DB_SENTINEL,     /*!< sentinel of the list */
} cr_DatabaseType;

typedef struct _DbPrimaryStatements   * cr_DbPrimaryStatements; /*!<
    Compiled  primary database statements */
typedef struct _DbFilelistsStatements * cr_DbFilelistsStatements; /*!<
    Compiled filelists database statements */
typedef struct _DbOtherStatements     * cr_DbOtherStatements; /*!<
    Compiled other database statements */

/** Union of precompiled database statements
 */
typedef union {
    cr_DbPrimaryStatements pri;     /*!< Primary statements */
    cr_DbFilelistsStatements fil;   /*!< Filelists statements */
    cr_DbOtherStatements oth;       /*!< Other statements */
} cr_Statements;

/** cr_SqliteDb structure.
 */
typedef struct {
    sqlite3 *db; /*!<
        Sqlite database */
    cr_DatabaseType type; /*!<
        Type of Sqlite database. */
    cr_Statements statements; /*!<
        Compiled SQL statements */
} cr_SqliteDb;

/** Macro over cr_db_open function. Open (create new) primary sqlite sqlite db.
 *  - creates db file
 *  - creates primary tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      Opened db or NULL on error
 */
#define cr_db_open_primary(PATH, ERR)    cr_db_open(PATH, CR_DB_PRIMARY, ERR)

/** Macro over cr_db_open function. Open (create new) filelists sqlite sqlite db.
 *  - creates db file
 *  - creates filelists tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      Opened db or NULL on error
 */
#define cr_db_open_filelists(PATH, ERR)  cr_db_open(PATH, CR_DB_FILELISTS, ERR)

/** Macro over cr_db_open function. Open (create new) other sqlite sqlite db.
 *  - creates db file
 *  - opens transaction
 *  - creates other tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      Opened db or NULL on error
 */
#define cr_db_open_other(PATH, ERR)      cr_db_open(PATH, CR_DB_OTHER, ERR)

/** Open (create new) other sqlite sqlite db.
 *  - creates db file
 *  - opens transaction
 *  - creates other tables
 *  - creates info table
 *  - tweak some db params
 * @param path                  Path to the db file.
 * @param db_type               Type of database (primary, filelists, other)
 * @param err                   **GError
 * @return                      Opened db or NULL on error
 */
cr_SqliteDb *cr_db_open(const char *path,
                        cr_DatabaseType db_type,
                        GError **err);

/** Add package into the database.
 * @param sqlitedb              open db connection
 * @param pkg                   package object
 * @param err                   **GError
 * @return                      cr_Error code
 */
int cr_db_add_pkg(cr_SqliteDb *sqlitedb,
                  cr_Package *pkg,
                  GError **err);

/** Insert record into the updateinfo table
 * @param sqlitedb              open db connection
 * @param checksum              compressed xml file checksum
 * @param err                   **GError
 * @return                      cr_Error code
 */
int cr_db_dbinfo_update(cr_SqliteDb *sqlitedb,
                        const char *checksum,
                        GError **err);

/** Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param sqlitedb              open db connection
 * @param err                   **GError
 * @return                      cr_Error code
 */
int cr_db_close(cr_SqliteDb *sqlitedb, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_SQLITE_H__ */
