#!/bin/bash

rm -rfv deltarepo-*-*
rm -rfv deltarepos.xml.xz
../../managedeltarepos.py ../repos/repo1/ ../repos/repo2/
../../managedeltarepos.py ../repos/repo2/ ../repos/repo3/

