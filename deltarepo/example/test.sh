#!/bin/bash

rm -rf test/
mkdir test/
cp -r repos/repo1 test/
cp -r repos/repo3 test/

../repoupdater.py test/repo1/ --verbose --repo file://`pwd`/test/repo3/ --drmirror file://`pwd`/deltarepos/
