#!/bin/bash

rm -rf test/
mkdir test/
cp -r repos/repo1 test/
cp -r repos/repo3 test/

#../repoupdater.py test/repo1/ --repo file://`pwd`/test/repo3/ --drmirror file://`pwd`/deltarepos/
../repoupdater.py test/repo1/ --verbose --repo file:///home/tmlcoch/git/createrepo_c_lib/deltarepo/tempdirvoe/test/repo3/ --drmirror file:///home/tmlcoch/git/createrepo_c_lib/deltarepo/tempdirvoe/deltarepos/
