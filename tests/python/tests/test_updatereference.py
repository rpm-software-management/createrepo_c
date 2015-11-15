import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateReference(unittest.TestCase):

    def test_updatereference_setters(self):
        ref = cr.UpdateReference()
        self.assertTrue(ref)

        self.assertEqual(ref.href, None)
        self.assertEqual(ref.id, None)
        self.assertEqual(ref.type, None)
        self.assertEqual(ref.title, None)

        ref.href = "href"
        ref.id = "id"
        ref.type = "type"
        ref.title = "title"

        self.assertEqual(ref.href, "href")
        self.assertEqual(ref.id, "id")
        self.assertEqual(ref.type, "type")
        self.assertEqual(ref.title, "title")
