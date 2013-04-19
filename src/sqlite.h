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
 * sqlite3 *primary_db;
 * cr_DbPrimary_Statements primary_statements;
 *
 * // Load pkg (See parsepkg or parsehdr module)
 *
 * // Create primary sqlite database
 * primary_db = cr_db_open_primary("/foo/bar/repodata/primary.sqlite");
 * // Compile statements (to avoid SQL parsing overhead)
 * primary_statements = cr_db_prepare_primary_statements(primary_db, NULL);
 *
 * // Add all packages here
 * cr_db_add_primary_pkg(primary_statements, pkg, NULL);
 *
 * // Add checksum of XML version of file (primary in this case)
 * cr_db_dbinfo_update(primary_db, "foochecksum", NULL);
 *
 * // Cleanup
 * cr_db_destroy_primary_statements(primary_db);
 * cr_db_close_primary(primary_db, NULL);
 * \endcode
 *
 *  \addtogroup sqlite
 *  @{
 */

#define CR_DB_CACHE_DBVERSION       10      /*!< Version of DB api */

#define CR_DB_ERROR cr_db_error_quark()
GQuark cr_db_error_quark (void);

typedef struct _DbPrimaryStatements   * cr_DbPrimaryStatements;
typedef struct _DbFilelistsStatements * cr_DbFilelistsStatements;
typedef struct _DbOtherStatements     * cr_DbOtherStatements;

/** Database type.
 */
typedef enum {
    CR_DB_PRIMARY,         /*!< primary */
    CR_DB_FILELISTS,       /*!< filelists */
    CR_DB_OTHER            /*!< other */
} cr_DatabaseType;

/** Macro over cr_db_open function. Open (create new) primary sqlite sqlite db.
 *  - creates db file
 *  - creates primary tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
 */
#define cr_db_open_primary(PATH, ERR)    cr_db_open(PATH, CR_DB_PRIMARY, ERR)

/** Macro over cr_db_open function. Open (create new) filelists sqlite sqlite db.
 *  - creates db file
 *  - creates filelists tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
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
 * @return                      open db connection
 */
#define cr_db_open_other(PATH, ERR)      cr_db_open(PATH, CR_DB_OTHER, ERR)

/** Macro over cr_db_close function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_db_close_primary(DB, ERR)     cr_db_close(DB, CR_DB_PRIMARY, ERR)

/** Macro over cr_db_close function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_db_close_filelists(DB, ERR)   cr_db_close(DB, CR_DB_FILELISTS, ERR)

/** Macro over cr_db_close function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_db_close_other(DB, ERR)       cr_db_close(DB, CR_DB_OTHER, ERR)

/** Open (create new) other sqlite sqlite db.
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
sqlite3 *cr_db_open(const char *path, cr_DatabaseType db_type, GError **err);

/** Prepare compiled statements for use in the cr_db_add_primary_pkg function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbPrimaryStatements object
 */
cr_DbPrimaryStatements cr_db_prepare_primary_statements(sqlite3 *db,
                                                        GError **err);

/** Prepare compiled statements for use in the cr_db_add_filelists_pkg function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbFilelistsStatements object
 */
cr_DbFilelistsStatements cr_db_prepare_filelists_statements(sqlite3 *db,
                                                            GError **err);

/** Prepare compiled statements for use in the cr_db_add_other_pkg function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbOtherStatements object
 */
cr_DbOtherStatements cr_db_prepare_other_statements(sqlite3 *db, GError **err);

/** Frees cr_DbPrimaryStatements object.
 * @param stmts                 statements object
 */
void cr_db_destroy_primary_statements(cr_DbPrimaryStatements stmts);

/** Frees cr_DbFilelistsStatements object.
 * @param stmts                 statements object
 */
void cr_db_destroy_filelists_statements(cr_DbFilelistsStatements stmts);

/** Frees cr_DbOtherStatements object.
 * @param stmts                 statements object
 */
void cr_db_destroy_other_statements(cr_DbOtherStatements stmts);

/** Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 * @param err                   **GError
 */
void cr_db_add_primary_pkg(cr_DbPrimaryStatements stmts,
                           cr_Package *pkg,
                           GError **err);

/** Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 * @param err                   **GError
 */
void cr_db_add_filelists_pkg(cr_DbFilelistsStatements stmts,
                             cr_Package *pkg,
                             GError **err);

/** Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 * @param err                   **GError
 */
void cr_db_add_other_pkg(cr_DbOtherStatements stmts,
                         cr_Package *pkg,
                         GError **err);

/** Insert record into the updateinfo table
 * @param db                    open db connection
 * @param checksum              compressed xml file checksum
 * @param err                   **GError
 */
void cr_db_dbinfo_update(sqlite3 *db, const char *checksum, GError **err);

/** Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param db                    open db connection
 * @param db_type               Type of database (primary, filelists, other)
 * @param err                   **GError
 */
void cr_db_close(sqlite3 *db, cr_DatabaseType db_type, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_SQLITE_H__ */
