import unittest
import createrepo_c as cr

from . import fixtures

class TestCaseVersion(unittest.TestCase):
    def test_version(self):
        self.assertTrue(isinstance(cr.VERSION_MAJOR, int));
        self.assertTrue(isinstance(cr.VERSION_MINOR, int));
        self.assertTrue(isinstance(cr.VERSION_PATCH, int));
