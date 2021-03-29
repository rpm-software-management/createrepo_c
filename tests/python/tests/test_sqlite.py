import unittest
import shutil
import tempfile
import os.path
import sqlite3
import createrepo_c as cr

from .fixtures import *

class TestCaseSqlite(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_sqlite_basic_operations(self):
        db_pri = cr.Sqlite(self.tmpdir+"/primary.db", cr.DB_PRIMARY)
        self.assertTrue(db_pri)
        self.assertTrue(os.path.isfile(self.tmpdir+"/primary.db"))

        db_pri = cr.PrimarySqlite(self.tmpdir+"/primary2.db")
        self.assertTrue(db_pri)
        self.assertTrue(os.path.isfile(self.tmpdir+"/primary2.db"))

        db_fil = cr.Sqlite(self.tmpdir+"/filelists.db", cr.DB_FILELISTS)
        self.assertTrue(db_fil)
        self.assertTrue(os.path.isfile(self.tmpdir+"/filelists.db"))

        db_fil = cr.FilelistsSqlite(self.tmpdir+"/filelists2.db")
        self.assertTrue(db_fil)
        self.assertTrue(os.path.isfile(self.tmpdir+"/filelists2.db"))

        db_oth = cr.Sqlite(self.tmpdir+"/other.db", cr.DB_OTHER)
        self.assertTrue(db_oth)
        self.assertTrue(os.path.isfile(self.tmpdir+"/other.db"))

        db_oth = cr.OtherSqlite(self.tmpdir+"/other2.db")
        self.assertTrue(db_oth)
        self.assertTrue(os.path.isfile(self.tmpdir+"/other2.db"))


    def test_sqlite_error_cases(self):
        self.assertRaises(cr.CreaterepoCError, cr.Sqlite, self.tmpdir, cr.DB_PRIMARY)
        self.assertRaises(ValueError, cr.Sqlite, self.tmpdir+"/foo.db", 55)
        self.assertRaises(TypeError, cr.Sqlite, self.tmpdir+"/foo.db", None)
        self.assertRaises(TypeError, cr.Sqlite, None, cr.DB_PRIMARY)

    def test_sqlite_operations_on_closed_db(self):
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        path = os.path.join(self.tmpdir, "primary.db")
        db = cr.Sqlite(path, cr.DB_PRIMARY)
        self.assertTrue(db)
        db.close()

        self.assertRaises(cr.CreaterepoCError, db.add_pkg, pkg)
        self.assertRaises(cr.CreaterepoCError, db.dbinfo_update, "somechecksum")
        self.assertEqual("<createrepo_c.Sqlite Closed object>", db.__str__())

        db.close()  # No error should be raised
        del db      # No error should be raised

    def test_sqlite_primary_schema(self):
        path = os.path.join(self.tmpdir, "primary.db")
        cr.PrimarySqlite(path)
        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)
        # Check tables
        self.assertEqual(con.execute("""select name from sqlite_master where type="table";""").fetchall(),
            [(u'db_info',),
             (u'packages',),
             (u'files',),
             (u'requires',),
             (u'provides',),
             (u'conflicts',),
             (u'obsoletes',),
             (u'suggests',),
             (u'enhances',),
             (u'recommends',),
             (u'supplements',),
            ])
        # Check indexes
        self.assertEqual(con.execute("""select name from sqlite_master where type="index";""").fetchall(),
            [(u'packagename',),
             (u'packageId',),
             (u'filenames',),
             (u'pkgfiles',),
             (u'pkgrequires',),
             (u'requiresname',),
             (u'pkgprovides',),
             (u'providesname',),
             (u'pkgconflicts',),
             (u'pkgobsoletes',),
             (u'pkgsuggests',),
             (u'pkgenhances',),
             (u'pkgrecommends',),
             (u'pkgsupplements',),
            ])
        # Check triggers
        self.assertEqual(con.execute("""select name from sqlite_master where type="trigger";""").fetchall(),
            [(u'removals',)])

    def test_sqlite_filelists_schema(self):
        path = os.path.join(self.tmpdir, "filelists.db")
        cr.FilelistsSqlite(path)
        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)
        # Check tables
        self.assertEqual(con.execute("""select name from sqlite_master where type="table";""").fetchall(),
            [(u'db_info',), (u'packages',), (u'filelist',)])
        # Check indexes
        self.assertEqual(con.execute("""select name from sqlite_master where type="index";""").fetchall(),
            [(u'keyfile',), (u'pkgId',), (u'dirnames',)])
        # Check triggers
        self.assertEqual(con.execute("""select name from sqlite_master where type="trigger";""").fetchall(),
            [(u'remove_filelist',)])

    def test_sqlite_other_schema(self):
        path = os.path.join(self.tmpdir, "other.db")
        cr.OtherSqlite(path)
        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)
        # Check tables
        self.assertEqual(con.execute("""select name from sqlite_master where type="table";""").fetchall(),
            [(u'db_info',), (u'packages',), (u'changelog',)])
        # Check indexes
        self.assertEqual(con.execute("""select name from sqlite_master where type="index";""").fetchall(),
            [(u'keychange',), (u'pkgId',)])
        # Check triggers
        self.assertEqual(con.execute("""select name from sqlite_master where type="trigger";""").fetchall(),
            [(u'remove_changelogs',)])

    def test_sqlite_primary(self):
        path = os.path.join(self.tmpdir, "primary.db")
        db = cr.Sqlite(path, cr.DB_PRIMARY)
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        db.add_pkg(pkg)
        self.assertRaises(TypeError, db.add_pkg, None)
        self.assertRaises(TypeError, db.add_pkg, 123)
        self.assertRaises(TypeError, db.add_pkg, "foo")
        db.dbinfo_update("somechecksum")
        self.assertRaises(TypeError, db.dbinfo_update, pkg)
        self.assertRaises(TypeError, db.dbinfo_update, None)
        self.assertRaises(TypeError, db.dbinfo_update, 123)
        db.close()

        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)

        # Check packages table
        res = con.execute("select * from packages").fetchall()
        self.assertEqual(res,
            [(1, u'4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e',
              u'Archer', u'x86_64', u'3.4.5', u'2', u'6', u'Complex package.',
              u'Archer package', u'http://soo_complex_package.eu/',
              res[0][10], 1365416480, u'GPL', u'ISIS', u'Development/Tools',
              u'localhost.localdomain', u'Archer-3.4.5-6.src.rpm', 280, 2865,
              u'Sterling Archer', 3101, 0, 544, None, None, u'sha256')])

        # Check provides table
        self.assertEqual(con.execute("select * from provides").fetchall(),
            [(u'bara', u'LE', u'0', u'22', None, 1),
             (u'barb', u'GE', u'0', u'11.22.33', u'44', 1),
             (u'barc', u'EQ', u'0', u'33', None, 1),
             (u'bard', u'LT', u'0', u'44', None, 1),
             (u'bare', u'GT', u'0', u'55', None, 1),
             (u'Archer', u'EQ', u'2', u'3.4.5', u'6', 1),
             (u'Archer(x86-64)', u'EQ', u'2', u'3.4.5', u'6', 1)])

        # Check conflicts table
        self.assertEqual(con.execute("select * from conflicts").fetchall(),
            [(u'bba', u'LE', u'0', u'2222', None, 1),
             (u'bbb', u'GE', u'0', u'1111.2222.3333', u'4444', 1),
             (u'bbc', u'EQ', u'0', u'3333', None, 1),
             (u'bbd', u'LT', u'0', u'4444', None, 1),
             (u'bbe', u'GT', u'0', u'5555', None, 1)])

        # Check obsoletes table
        self.assertEqual(con.execute("select * from obsoletes").fetchall(),
           [(u'aaa', u'LE', u'0', u'222', None, 1),
            (u'aab', u'GE', u'0', u'111.2.3', u'4', 1),
            (u'aac', u'EQ', u'0', u'333', None, 1),
            (u'aad', u'LT', u'0', u'444', None, 1),
            (u'aae', u'GT', u'0', u'555', None, 1)])

        # Check requires table
        self.assertEqual(con.execute("select * from requires").fetchall(),
            [(u'fooa', u'LE', u'0', u'2', None, 1, u'FALSE'),
             (u'foob', u'GE', u'0', u'1.0.0', u'1', 1, u'FALSE'),
             (u'fooc', u'EQ', u'0', u'3', None, 1, u'FALSE'),
             (u'food', u'LT', u'0', u'4', None, 1, u'FALSE'),
             (u'fooe', u'GT', u'0', u'5', None, 1, u'FALSE'),
             (u'foof', u'EQ', u'0', u'6', None, 1, u'TRUE')])

        # Check files table
        self.assertEqual(con.execute("select * from files").fetchall(),
            [(u'/usr/bin/complex_a', u'file', 1)])

        # Check db_info table
        self.assertEqual(con.execute("select * from db_info").fetchall(),
            [(10, u'somechecksum')])

    def test_sqlite_filelists(self):
        path = os.path.join(self.tmpdir, "filelists.db")
        db = cr.Sqlite(path, cr.DB_FILELISTS)
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        db.add_pkg(pkg)
        db.dbinfo_update("somechecksum2")
        db.close()

        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)

        # Check packages table
        self.assertEqual(con.execute("select * from packages").fetchall(),
            [(1, u'4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e')])

        # Check files table
        self.assertEqual(set(con.execute("select * from filelist").fetchall()),
            set([(1, u'/usr/share/doc', u'Archer-3.4.5', u'd'),
             (1, u'/usr/bin', u'complex_a', u'f'),
             (1, u'/usr/share/doc/Archer-3.4.5', u'README', u'f')]))

        # Check db_info table
        self.assertEqual(con.execute("select * from db_info").fetchall(),
            [(10, u'somechecksum2')])

    def test_sqlite_other(self):
        path = os.path.join(self.tmpdir, "other.db")
        db = cr.Sqlite(path, cr.DB_FILELISTS)
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        db.add_pkg(pkg)
        db.dbinfo_update("somechecksum3")
        db.close()

        self.assertTrue(os.path.isfile(path))

        con = sqlite3.connect(path)

        # Check packages table
        self.assertEqual(con.execute("select * from packages").fetchall(),
            [(1, u'4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e')])

        # Check filelist table
        self.assertEqual(set(con.execute("select * from filelist").fetchall()),
            set([(1, u'/usr/share/doc', u'Archer-3.4.5', u'd'),
             (1, u'/usr/bin', u'complex_a', u'f'),
             (1, u'/usr/share/doc/Archer-3.4.5', u'README', u'f')]))

        # Check db_info table
        self.assertEqual(con.execute("select * from db_info").fetchall(),
            [(10, u'somechecksum3')])
