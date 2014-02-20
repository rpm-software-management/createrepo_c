#!/bin/bash

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

# Note:
# If you wanna use different createrepo tool, set the CREATEREPO
# environ variable
# E.g.:
# $ CREATEREPO="createrepo" ./regenrepos.sh

for dir in $MY_DIR/repo*/
do
    echo "### Regeneration of $dir"
    pushd $dir
    ./gen.sh
    popd
    sleep 1
    echo
done

echo "" > pkgs_per_repo
for dir in ${MY_DIR}repo*/
do
    echo "[ $dir ]" >> pkgs_per_repo
    cat $dir/pkglist | awk '{ split($0,a,"/"); print a[3] }' >> pkgs_per_repo
    echo "" >> pkgs_per_repo
done

