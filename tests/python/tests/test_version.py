import unittest
import createrepo_c as cr

import fixtures

class TestCaseVersion(unittest.TestCase):
    def test_version(self):
        self.assertIsInstance(cr.VERSION_MAJOR, int);
        self.assertIsInstance(cr.VERSION_MINOR, int);
        self.assertIsInstance(cr.VERSION_PATCH, int);
