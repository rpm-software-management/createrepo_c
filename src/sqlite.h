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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
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
 */

#define CR_SQLITE_CACHE_DBVERSION       10      /*!< Version of DB api */

#define CR_DB_ERROR cr_db_error_quark()
GQuark cr_db_error_quark (void);

typedef struct _DbPrimaryStatements   * cr_DbPrimaryStatements;
typedef struct _DbFilelistsStatements * cr_DbFilelistsStatements;
typedef struct _DbOtherStatements     * cr_DbOtherStatements;

/** \ingroup sqlite
 * Database type.
 */
typedef enum {
    CR_DB_PRIMARY,         /*!< primary */
    CR_DB_FILELISTS,       /*!< filelists */
    CR_DB_OTHER            /*!< other */
} cr_DatabaseType;

/** \ingroup sqlite
 * Macro over cr_open_db function. Open (create new) primary sqlite sqlite db.
 *  - creates db file
 *  - creates primary tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
 */
#define cr_open_primary_db(PATH, ERR)    cr_open_db(PATH, CR_DB_PRIMARY, ERR)

/** \ingroup sqlite
 * Macro over cr_open_db function. Open (create new) filelists sqlite sqlite db.
 *  - creates db file
 *  - creates filelists tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db pointer
 */
#define cr_open_filelists_db(PATH, ERR)  cr_open_db(PATH, CR_DB_FILELISTS, ERR)

/** \ingroup sqlite
 * Macro over cr_open_db function. Open (create new) other sqlite sqlite db.
 *  - creates db file
 *  - opens transaction
 *  - creates other tables
 *  - creates info table
 *  - tweak some db params
 * @param PATH                  Path to the db file.
 * @param ERR                   **GError
 * @return                      open db connection
 */
#define cr_open_other_db(PATH, ERR)      cr_open_db(PATH, CR_DB_OTHER, ERR)

/** \ingroup sqlite
 * Macro over cr_close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_close_primary_db(DB, ERR)     cr_close_db(DB, CR_DB_PRIMARY, ERR)

/** \ingroup sqlite
 * Macro over cr_close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_close_filelists_db(DB, ERR)   cr_close_db(DB, CR_DB_FILELISTS, ERR)

/** \ingroup sqlite
 * Macro over cr_close_db function. Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param DB                    open db connection
 * @param ERR                   **GError
 */
#define cr_close_other_db(DB, ERR)       cr_close_db(DB, CR_DB_OTHER, ERR)

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
sqlite3 *cr_open_db(const char *path, cr_DatabaseType db_type, GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the cr_add_primary_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbPrimaryStatements object
 */
cr_DbPrimaryStatements cr_prepare_primary_db_statements(sqlite3 *db, GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the cr_add_filelists_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbFilelistsStatements object
 */
cr_DbFilelistsStatements cr_prepare_filelists_db_statements(sqlite3 *db,
                                                         GError **err);

/** \ingroup sqlite
 * Prepare compiled statements for use in the cr_add_other_pkg_db function.
 * @param db                    Open db connection
 * @param err                   **GError
 * @return                      cr_DbOtherStatements object
 */
cr_DbOtherStatements cr_prepare_other_db_statements(sqlite3 *db, GError **err);

/** \ingroup sqlite
 * Frees cr_DbPrimaryStatements object.
 * @param stmts                 statements object
 */
void cr_destroy_primary_db_statements(cr_DbPrimaryStatements stmts);

/** \ingroup sqlite
 * Frees cr_DbFilelistsStatements object.
 * @param stmts                 statements object
 */
void cr_destroy_filelists_db_statements(cr_DbFilelistsStatements stmts);

/** \ingroup sqlite
 * Frees cr_DbOtherStatements object.
 * @param stmts                 statements object
 */
void cr_destroy_other_db_statements(cr_DbOtherStatements stmts);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void cr_add_primary_pkg_db(cr_DbPrimaryStatements stmts, cr_Package *pkg);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void cr_add_filelists_pkg_db(cr_DbFilelistsStatements, cr_Package *pkg);

/** \ingroup sqlite
 * Add package into the database.
 * @param stmts                 object with compiled statements
 * @param pkg                   package object
 */
void cr_add_other_pkg_db(cr_DbOtherStatements stmts, cr_Package *pkg);

/** \ingroup sqlite
 * Insert record into the updateinfo table
 * @param                       open db connection
 * @checksum                    compressed xml file checksum
 * @err                         **GError
 */
void cr_dbinfo_update(sqlite3 *db, const char *checksum, GError **err);

/** \ingroup sqlite
 * Close db.
 *  - creates indexes on tables
 *  - commits transaction
 *  - closes db
 * @param db                    open db connection
 * @param db_type               Type of database (primary, filelists, other)
 * @param err                   **GError
 */
void cr_close_db(sqlite3 *db, cr_DatabaseType db_type, GError **err);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_SQLITE_H__ */
