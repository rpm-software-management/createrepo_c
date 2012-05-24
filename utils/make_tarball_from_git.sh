#!/bin/bash

NAME="createrepo_c"
VER="0.0.0"

if [ $# -ne "0" ]
then
    echo "Usage: `basename $0`"
    exit 1
fi

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

DIRECTORY="./$NAME-$VER"
TARBALL="$DIRECTORY.tar.xz"

TMP_DIR=`mktemp -d`
echo "Using TMP_DIR: $TMP_DIR"

echo "Using root projekt dir: $PREFIX"

(
    echo "cd $PREFIX"
    cd $PREFIX

    echo "Cloning git repo..."
    git clone git://fedorahosted.org/git/createrepo_c.git $TMP_DIR
)

# Get actual version
VER=`$MY_DIR/get_version.py $TMP_DIR`
echo "Detected version $VER"

echo "Making tarball from git..."
$MY_DIR/make_tarball.sh $TMP_DIR

echo "Copy tarball"
cp $TMP_DIR/$TARBALL $MY_DIR/../

echo "Rm temp..."
echo "rm -rf $TMP_DIR"
rm -rf $TMP_DIR

echo "DONE"


