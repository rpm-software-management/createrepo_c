#!/bin/bash

rm -rf test/
mkdir test/
cp -r repos/repo1 test/
cp -r repos/repo3 test/

../repoupdater.py test/repo1/ $@ --repo file://`pwd`/test/repo3/ --drmirror file://`pwd`/deltarepos/

rm -rf test2/
mkdir test2/
cp -r repos/repo1 test2/
cp -r repos/repo3 test2/
rm -f test2/repo1/repodata/*sqlite*
rm -f test2/repo1/repodata/*other*
rm -f test2/repo1/repodata/*foobar*

../repoupdater.py test2/repo1/ $@ --repo file://`pwd`/test/repo3/ --drmirror file://`pwd`/deltarepos/
