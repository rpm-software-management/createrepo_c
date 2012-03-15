#!/bin/bash

NAME="createrepo_c"
VER="0.1.0"

if [ $# -ne "1" ]
then
    echo "Usage: `basename $0` <root_project_dir>"
    exit 1
fi

PREFIX="$1/"
DIRECTORY="./$NAME-$VER"

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
    cp --verbose COPYING.lib $DIRECTORY

    cp --verbose --parents cmake/Modules/* $DIRECTORY

    cp --verbose --parents doc/CMakeLists.txt $DIRECTORY
    cp --verbose --parents doc/createrepo_c.8.gz $DIRECTORY

    cp --verbose --parents src/CMakeLists.txt $DIRECTORY
    cp --verbose --parents src/*.c $DIRECTORY
    cp --verbose --parents src/*.h $DIRECTORY

    tar -cvjf "$DIRECTORY.tar.bz2" "$DIRECTORY"
)

