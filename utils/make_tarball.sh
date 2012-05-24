#!/bin/bash

NAME="createrepo_c"
VER="0.0.0"

if [ $# -ne "1" ]
then
    echo "Usage: `basename $0` <root_project_dir>"
    exit 1
fi

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

# Get actual version
VER=`$MY_DIR/get_version.py $1`

PREFIX="$1/"
DIRECTORY="./$NAME-$VER"
TARBALL="$DIRECTORY.tar.xz"

echo "Using root projekt dir: $PREFIX"

(
    echo "cd $PREFIX"
    cd $PREFIX

    if [ -d "$DIRECTORY" ]; then
        echo "Removing old tarball directory: $DIRECTORY"
        rm -rf $DIRECTORY
    fi

    make clean

    mkdir $DIRECTORY

    cp --verbose CMakeLists.txt $DIRECTORY
    cp --verbose AUTHORS $DIRECTORY
    cp --verbose README $DIRECTORY
    cp --verbose COPYING $DIRECTORY

    cp --verbose --parents cmake/Modules/* $DIRECTORY

    cp --verbose --parents doc/CMakeLists.txt $DIRECTORY
    cp --verbose --parents doc/createrepo_c.8.* $DIRECTORY
    cp --verbose --parents doc/mergerepo_c.8.* $DIRECTORY

    cp --verbose --parents src/CMakeLists.txt $DIRECTORY
    cp --verbose --parents src/*.c $DIRECTORY
    cp --verbose --parents src/*.h $DIRECTORY

    cp --verbose --parents tests/CMakeLists.txt $DIRECTORY
    cp --verbose --parents tests/*.c $DIRECTORY
    cp --recursive --verbose --parents tests/test_data $DIRECTORY

    tar -cvJf "$TARBALL" "$DIRECTORY"
)

