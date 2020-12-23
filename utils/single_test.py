#!/usr/bin/env python3
"""
Convert a single line from test output to run single test command.

E.g:
    "2: test_download_package_via_metalink (tests.test_yum_package_downloading.TestCaseYumPackageDownloading) ... ok"

To:
    tests/python/tests/test_yum_repo_downloading.py:TestCaseYumRepoDownloading.test_download_and_update_repo_01
"""

import os
import sys
import argparse

LIBPATH = "./build/src/python/"
COMMAND = "PYTHONPATH=`readlink -f {libpath}` nosetests -s -v {testpath}"
TEST_PATH_PREFIX = "tests/python"

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Convert a single line from '\
            'python unittesttest output to run command for this single test')

    parser.add_argument('test_out_line', metavar='TESTOUTPUTLINE',
                        type=str,
                        help='A single line from python unittesttest output')
    args = parser.parse_args()

    test_out_line = args.test_out_line

    # Remove suffix "... ok" or "... FAIL"
    test_out_line = test_out_line.split(" ... ")[0]

    # Remove prefix "test_number: "
    res = test_out_line = test_out_line.split(": ")
    test_out_line = res[-1]

    # Get test name
    res = test_out_line.split(" ")
    if len(res) != 2:
        print("Bad input line format")
        sys.exit(1)

    test_name, test_out_line = res

    # Get test case
    test_out_line = test_out_line.strip().lstrip("(").rstrip(")")
    res = test_out_line.rsplit(".", 1)
    if len(res) != 2:
        print("Bad input line format")
        sys.exit(1)

    test_out_line, test_case = res

    # Get path
    test_path = test_out_line.replace(".", "/") + ".py"

    full_path = os.path.join(TEST_PATH_PREFIX, test_path)

    testpath = "{0}:{1}.{2}".format(full_path, test_case, test_name)
    libpath = LIBPATH

    command = COMMAND.format(libpath=libpath, testpath=testpath)

    print(command)

