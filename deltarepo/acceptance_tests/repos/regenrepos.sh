#!/bin/bash

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

# Note:
# If you wanna use different createrepo tool, set the CREATEREPO
# environ variable
# E.g.:
# $ CREATEREPO="createrepo" ./regenrepos.sh

for dir in $MY_DIR/*/
do
    echo "### Regeneration of $dir"
    $dir/gen.sh
    echo
done
