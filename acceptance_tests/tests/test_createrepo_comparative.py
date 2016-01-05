import os
import os.path

from .fixtures import PACKAGES
from .base import BaseTestCase


class TestCaseCreaterepoComparative_emptyrepo(BaseTestCase):
    """Empty input repository"""

    def test_01_createrepo(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_relativepath(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))

    def test_03_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro DISTRO-TAG")

    def test_04_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro CPEID,Footag")

    def test_05_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro cpeid,tag_a --distro tag_b")

    def test_06_createrepo_contenttag(self):
        """--content"""
        self.assert_same_results(self.indir, "--content contenttag_a")

    def test_07_createrepo_contenttag(self):
        """--content"""
        self.assert_same_results(self.indir, "--content contenttag_a --content contettag_b")

    def test_08_createrepo_repotag(self):
        """--repo"""
        self.assert_same_results(self.indir, "--repo repotag_a")

    def test_09_createrepo_repotag(self):
        """--repo"""
        self.assert_same_results(self.indir, "--repo repotag_a --repo repotag_b")

    def test_10_createrepo_nodatabase(self):
        """--no-database"""
        self.assert_same_results(self.indir, "--no-database")

    def test_11_createrepo_uniquemdfilenames(self):
        """--unique-md-filenames"""
        self.assert_same_results(self.indir, "--unique-md-filenames")

    def test_12_createrepo_simplemdfilenames(self):
        """--simple-md-filenames"""
        self.assert_same_results(self.indir, "--simple-md-filenames")

    def test_13_createrepo_revision(self):
        """--revision"""
        self.assert_same_results(self.indir, "--revision XYZ")

    def test_14_createrepo_skipsymlinks(self):
        """--skip-symlinks"""
        self.assert_same_results(self.indir, "--skip-symlinks")


class TestCaseCreaterepoComparative_regularrepo(BaseTestCase):
    """Repo with 3 packages"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_addpkg(PACKAGES[1])
        self.indir_addpkg(PACKAGES[2])

    def test_01_createrepo(self):
        """Regular createrepo"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_relativepath(self):
        """Regular createrepo"""
        self.assert_same_results(os.path.relpath(self.indir))

    def test_03_createrepo_excludes(self):
        """--excludes * param"""
        self.assert_same_results(self.indir, "--excludes '*'")

    def test_04_createrepo_excludes(self):
        """--excludes"""
        self.assert_same_results(self.indir, "--excludes 'Archer-3.4.5-6.x86_64.rpm'")

    def test_05_createrepo_excludes(self):
        """--excludes"""
        self.assert_same_results(self.indir, "--excludes 'Archer-*.rpm'")

    def test_06_createrepo_skipsymlinks(self):
        """--skip-symlinks"""
        self.assert_same_results(self.indir, "--skip-symlinks")

    def test_07_createrepo_pkglist(self):
        """--pkglist"""
        fn_pkglist = self.indir_mkfile("pkglist", "%s\n" % PACKAGES[0])
        self.assert_same_results(self.indir, "--pkglist %s" % fn_pkglist)

    def test_08_createrepo_pkglist(self):
        """--pkglist"""
        fn_pkglist = self.indir_mkfile("pkglist", "%s\n%s\n" % (PACKAGES[0], PACKAGES[1]))
        self.assert_same_results(self.indir, "--pkglist %s" % fn_pkglist)

    def test_09_createrepo_pkglist(self):
        """--pkglist"""
        fn_pkglist = self.indir_mkfile("pkglist", "%s\n\n%s\n\nfoobar.rpm\n\n" % (PACKAGES[0], PACKAGES[1]))
        self.assert_same_results(self.indir, "--pkglist %s" % fn_pkglist)

class TestCaseCreaterepoComparative_regularrepowithsubdirs(BaseTestCase):
    """Repo with 3 packages, each in its own subdir"""

    def setup(self):
        subdir_a = self.indir_makedirs("a")
        self.copy_pkg(PACKAGES[0], subdir_a)
        subdir_b = self.indir_makedirs("b")
        self.copy_pkg(PACKAGES[1], subdir_b)
        subdir_c = self.indir_makedirs("c")
        self.copy_pkg(PACKAGES[2], subdir_c)

    def test_01_createrepo(self):
        """Regular createrepo"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_skipsymlinks(self):
        """--skip-symlinks"""
        self.assert_same_results(self.indir, "--skip-symlinks")

    def test_03_createrepo_excludes(self):
        """--excludes"""
        self.assert_same_results(self.indir, "--excludes 'Archer-3.4.5-6.x86_64.rpm'")

    def test_04_createrepo_excludes(self):
        """--excludes"""
        self.assert_same_results(self.indir, "--excludes 'Archer-*.rpm'")


class TestCaseCreaterepoComparative_repowithsymlinks(BaseTestCase):
    """Repo with 2 packages and 1 symlink to a package"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_addpkg(PACKAGES[1])
        pkg_in_tdir = self.copy_pkg(PACKAGES[2], self.tdir)
        os.symlink(pkg_in_tdir, os.path.join(self.indir, os.path.basename(pkg_in_tdir)))

    def test_01_createrepo(self):
        """Regular createrepo"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_skipsymlinks(self):
        """--skip-symlinks"""
        self.assert_same_results(self.indir, "--skip-symlinks")


class TestCaseCreaterepoComparative_repowithbadpackages(BaseTestCase):
    """Repo with 1 regular package and few broken packages"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_makedirs("adirthatlookslike.rpm")
        self.indir_mkfile("emptyfilethatlookslike.rpm")
        self.indir_mkfile("afilethatlookslike.rpm", content="foobar")

    def test_01_createrepo(self):
        """Regular createrepo"""
        self.assert_same_results(self.indir)


class TestCaseCreaterepoComparative_cachedir(BaseTestCase):
    """Repo with 3 packages and cachedir used"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_addpkg(PACKAGES[1])
        self.indir_addpkg(PACKAGES[2])

    def test_01_createrepo_owncachedir(self):
        """Each createrepo has its own cachedir"""
        # Gen cache
        self.assert_same_results(self.indir, "--cachedir cache")
        # Run again and use the cache
        _, crres, crcres = self.assert_same_results(self.indir, "--cachedir cache")

        # Compare files in the cache files (they should be identical)
        cr_cache = os.path.join(crres.outdir, "cache")
        crc_cache = os.path.join(crcres.outdir, "cache")
        self.assert_same_dir_content(cr_cache, crc_cache)

    def test_02_createrepo_sharedcachedir(self):
        """Use cache mutually"""
        cache_cr = os.path.abspath(os.path.join(self.indir, "cache_cr"))
        cache_crc = os.path.abspath(os.path.join(self.indir, "cache_crc"))

        # Gen cache by the cr then use it by cr_c
        self.assert_run_cr(self.indir, "--cachedir %s" % cache_cr)
        self.assert_run_cr(self.indir, "--cachedir %s" % cache_cr, c=True)

        # Gen cache by the cr then use it by cr_c
        self.assert_run_cr(self.indir, "--cachedir %s" % cache_crc, c=True)
        self.assert_run_cr(self.indir, "--cachedir %s" % cache_crc)

        # Compare files in the cache files (they should be identical)
        self.assert_same_dir_content(cache_cr, cache_crc)

