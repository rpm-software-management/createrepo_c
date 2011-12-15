#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
Run tests in current directory.
(This file is 'stolen' from kobo)
"""

import os
import sys


# prepare environment for tests
PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.environ["PYTHONPATH"] = PROJECT_DIR
sys.path.insert(0, PROJECT_DIR)


from kobo.shortcuts import run


if __name__ == '__main__':
    failed = False
    for test in sorted(os.listdir(os.path.dirname(__file__))):
        # run all tests that match the 'test_*.py" pattern
        if not test.startswith("test_"):
            continue
        if not test.endswith(".py"):
            continue

        print "Executing tests in %-40s" % test,
        retcode, output = run("python %s" % test, can_fail=True)

        if retcode == 0:
            print "[   OK   ]"
            print output
        else:
            failed = True
            print "[ FAILED ]"
            print output

    if failed:
        sys.exit(1)
    sys.exit(0)
