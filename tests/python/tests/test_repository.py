import re
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *


class TestCaseRepositoryReader(unittest.TestCase):

    def test_manual_construction(self):
        reader = cr.RepositoryReader.from_metadata_files(REPO_01_PRIXML, REPO_01_FILXML, REPO_01_OTHXML)

        pkgs = [pkg for pkg in reader.iter_packages()]
        assert len(pkgs) == 1
        assert len(list(reader.advisories())) == 0

    def test_iter_packages(self):
        reader = cr.RepositoryReader.from_path(REPO_01_PATH)

        pkgs = [pkg for pkg in reader.iter_packages()]
        assert len(pkgs) == 1

    def test_parse_packages(self):
        reader = cr.RepositoryReader.from_path(REPO_01_PATH)
        pkgs, warnings = reader.parse_packages()
        assert list(pkgs.keys()) == ["152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf"]
        pkgs, warnings = reader.parse_packages(only_primary=True)
        assert list(pkgs.keys()) == ["152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf"]

    def test_advisories(self):
        reader = cr.RepositoryReader.from_path(REPO_01_PATH)
        assert len(list(reader.advisories())) == 0

        reader = cr.RepositoryReader.from_path(REPO_WITH_ADDITIONAL_METADATA)
        assert len(list(reader.advisories())) == 1

    def test_count_packages(self):
        reader = cr.RepositoryReader.from_path(REPO_01_PATH)

        assert reader.package_count() == 1

class TestCaseRepositoryWriter(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_empty_repo_create(self):
        """Test that a repository can be created with no packages"""
        with cr.RepositoryWriter(self.tmpdir) as writer:
            pass

        assert os.path.exists(writer.path)
        assert os.path.exists(writer.repodata_dir)

        # assert we see the metadata files we expect to see
        # also: that we don't see updateinfo because none were added
        assert {record.type for record in writer.repomd.records} == {"primary", "filelists", "other"}

        for record in writer.repomd.records:
            # test that the metadata files were created on disk
            record_path = os.path.join(self.tmpdir, record.location_href)
            assert os.path.exists(record_path)

        # no packages
        assert len(list(cr.RepositoryReader.from_path(self.tmpdir).iter_packages())) == 0

    def test_defaults(self):
        """Test that the default options behave as intended"""
        with cr.RepositoryWriter(self.tmpdir) as writer:
            writer.set_num_of_pkgs(3)
            writer.add_pkg_from_file(PKG_ARCHER_PATH)
            writer.add_pkg_from_file(PKG_EMPTY_PATH)
            writer.add_pkg_from_file(PKG_SUPER_KERNEL_PATH)
            writer.add_update_record(TEST_UPDATERECORD_UPDATE1)
            writer.add_repomd_metadata("group", TEST_COMPS_00)

        assert os.path.exists(writer.path)
        assert os.path.exists(writer.repodata_dir)
        assert {record.type for record in writer.repomd.records} == {"primary", "filelists", "other", "updateinfo", "group"}

        for record in writer.repomd.records:
            assert record.checksum_type == "sha256"
            assert record.checksum_open_type == "sha256"

            # test that unique_md_filenames=True prepends a checksum to the metadata filename
            # test that the compression suffix is correct
            if record.type == "group": # the record type isn't the filename..
                assert record.location_href == f"repodata/{record.checksum}-comps_00.xml.zst"
            else:
                assert record.location_href == f"repodata/{record.checksum}-{record.type}.xml.zst"

            # test that the metadata files were created on disk
            record_path = os.path.join(self.tmpdir, record.location_href)
            assert os.path.exists(record_path)

            reader = cr.RepositoryReader.from_path(self.tmpdir)
            for pkg in reader.iter_packages():
                # test that the package files are present where expected
                pkg_path = os.path.join(self.tmpdir, pkg.location_href)
                assert os.path.exists(pkg_path)
                # test that the package checksum type is correct
                assert pkg.checksum_type == "sha256"

    def test_options(self):
        """Test that overriding default options works as intended"""
        with cr.RepositoryWriter(
            self.tmpdir,
            num_packages=3,
            unique_md_filenames=False,
            changelog_limit=1,
            checksum_type=cr.SHA512,
            compression=cr.XZ_COMPRESSION,
        ) as writer:
            writer.add_pkg_from_file(PKG_ARCHER_PATH)
            writer.add_pkg_from_file(PKG_EMPTY_PATH)
            writer.add_pkg_from_file(PKG_SUPER_KERNEL_PATH)
            writer.add_update_record(TEST_UPDATERECORD_UPDATE1)
            writer.add_repomd_metadata("group", TEST_COMPS_00)

        assert os.path.exists(writer.path)
        assert os.path.exists(writer.repodata_dir)
        assert {record.type for record in writer.repomd.records} == {"primary", "filelists", "other", "updateinfo", "group"}

        for record in writer.repomd.records:
            assert record.checksum_type == "sha512"
            assert record.checksum_open_type == "sha512"

            # test that unique_md_filenames=False does not prepend a checksum to the metadata filename
            # test that the compression suffix is correct
            if record.type == "group": # the record type isn't the filename..
                assert record.location_href == f"repodata/comps_00.xml.xz"
            else:
                assert record.location_href == f"repodata/{record.type}.xml.xz"

            # test that the metadata files were created on disk
            record_path = os.path.join(self.tmpdir, record.location_href)
            assert os.path.exists(record_path)

            reader = cr.RepositoryReader.from_path(self.tmpdir)
            for pkg in reader.iter_packages():
                # test that the package files are present where expected
                pkg_path = os.path.join(self.tmpdir, pkg.location_href)
                assert os.path.exists(pkg_path)
                # test that changelog_limit works
                assert len(pkg.changelogs) <= 1
                # test that the package checksum type is correct
                assert pkg.checksum_type == "sha512"

    def test_add_repo_metadata(self):
        """Test adding an additional repo metadata file to the repository."""
        basename = os.path.basename(TEST_COMPS_00)

        with cr.RepositoryWriter(self.tmpdir) as writer:
            writer.add_repomd_metadata("group", TEST_COMPS_00, use_compression=False)

        assert os.path.exists(writer.path)
        assert os.path.exists(writer.repodata_dir)

        # test that the metadata file was added to repomd
        record = [record for record in writer.repomd.records if record.type == "group"][0]
        md_path_relative = f"repodata/{record.checksum}-{basename}"
        assert record.location_href == md_path_relative

        # and that the file exists in repodata/
        md_path = os.path.join(writer.path, md_path_relative)
        assert os.path.exists(md_path)

        # test it with re-compression enabled
        with cr.RepositoryWriter(self.tmpdir) as writer:
            writer.add_repomd_metadata("group", TEST_COMPS_00, use_compression=True)

        assert os.path.exists(writer.path)
        assert os.path.exists(writer.repodata_dir)

        # test that the metadata file was added to repomd
        record = [record for record in writer.repomd.records if record.type == "group"][0]
        md_path_relative = f"repodata/{record.checksum}-{basename}.zst"
        assert record.location_href == md_path_relative

        # and that the file exists in repodata/
        md_path = os.path.join(writer.path, md_path_relative)
        assert os.path.exists(md_path)

        # test that when you add the same metadata twice, it appears only once (types must be unique)
        with cr.RepositoryWriter(self.tmpdir) as writer:
            writer.add_repomd_metadata("group", TEST_COMPS_00)
            writer.add_repomd_metadata("group", TEST_COMPS_00)
        assert len([record.type for record in writer.repomd.records if record.type == "group"]) == 1

        # test that adding a file already in the repodata/ directory works (outside of repository already tested)
        with cr.RepositoryWriter(self.tmpdir) as writer:
            md_path = shutil.copy2(TEST_COMPS_00, writer.repodata_dir)
            writer.add_repomd_metadata("group", md_path, use_compression=True)

        record = [record for record in writer.repomd.records if record.type == "group"][0]
        md_path_relative = f"repodata/{record.checksum}-{basename}.zst"
        assert record.location_href == md_path_relative

        # same as the above test, but without compression enabled
        with cr.RepositoryWriter(self.tmpdir) as writer:
            md_path = shutil.copy2(TEST_COMPS_00, writer.repodata_dir)
            writer.add_repomd_metadata("group", md_path, use_compression=False)

        record = [record for record in writer.repomd.records if record.type == "group"][0]
        md_path_relative = f"repodata/{record.checksum}-{basename}"
        assert record.location_href == md_path_relative

    def test_add_package(self):
        """Test adding a package to the repository."""
        # test that adding a package file already in the repository works (outside of repository already tested)
        with cr.RepositoryWriter(self.tmpdir, num_packages=1) as writer:
            pkg_path = shutil.copy2(PKG_ARCHER_PATH, writer.path)
            writer.add_pkg_from_file(pkg_path)

        reader = cr.RepositoryReader.from_path(self.tmpdir)
        for pkg in reader.iter_packages():
            # test that the location_href points to the root of the directory
            assert pkg.location_href == os.path.basename(PKG_ARCHER_PATH)
            # test that the package files are present where expected
            pkg_path = os.path.join(self.tmpdir, pkg.location_href)
            assert os.path.exists(pkg_path)

        # test that using a custom output_dir works properly
        packages_dir = "Packages"
        with cr.RepositoryWriter(self.tmpdir, num_packages=3) as writer:
            writer.add_pkg_from_file(PKG_ARCHER_PATH, output_dir=packages_dir)
            writer.add_pkg_from_file(PKG_EMPTY_PATH, output_dir=packages_dir)
            writer.add_pkg_from_file(PKG_SUPER_KERNEL_PATH, output_dir=packages_dir)

        reader = cr.RepositoryReader.from_path(self.tmpdir)
        for pkg in reader.iter_packages():
            # test that the packages were relocated to package_dir
            assert pkg.location_href.startswith("Packages/")
            # test that the package files are present where expected
            pkg_path = os.path.join(self.tmpdir, pkg.location_href)
            assert os.path.exists(pkg_path)