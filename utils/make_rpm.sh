#!/bin/bash

RPMBUILD_DIR="${HOME}/rpmbuild/"
BUILD_DIR="$RPMBUILD_DIR/BUILD"
TARGETDIR=`pwd -P`
SCRIPTDIR=$( cd "$(dirname "$0")" ; pwd -P )
PROJECTROOTDIR=`realpath "$SCRIPTDIR/../"`
GITREV=""

echo "Cleaning $BUILD_DIR"
rm -rf $BUILD_DIR
echo "Removing $RPMBUILD_DIR/createrepo_c.spec"
rm -f $RPMBUILD_DIR/createrepo_c.spec

if [ $# -gt "1" ] && [ "$1" = "-h" ]; then
    echo "Usage: `basename $0` [git revision]"
    exit 0
fi

if [ $# -eq "1" ]; then
    GITREV="$1"
fi

if [ ! -d "$RPMBUILD_DIR" ]; then
    echo "rpmbuild dir $RPMBUILD_DIR doesn't exist!"
    echo "init rpmbuild dir with command: rpmdev-setuptree"
    echo "(Hint: Package group @development-tools and package fedora-packager)"
    exit 1
fi

echo "> Making tarball .."
$SCRIPTDIR/make_tarball.sh $GITREV

if [ ! $? == "0" ]; then
    echo "Error while making tarball"
    exit 1
fi
echo "Tarball done"

echo "> Copying tarball and .spec file into the $RPMBUILD_DIR .."
cp createrepo_c-*.tar.xz $RPMBUILD_DIR/SOURCES/
if [ ! $? == "0" ]; then
    echo "Error while: cp createrepo_c-*.tar.xz $RPMBUILD_DIR/SOURCES/"
    exit 1
fi

cp $PROJECTROOTDIR/createrepo_c.spec $RPMBUILD_DIR/SPECS/
if [ ! $? == "0" ]; then
    echo "Error while: cp $PROJECTROOTDIR/createrepo_c*.spec $RPMBUILD_DIR/SPECS/"
    exit 1
fi
echo "Copying done"

echo "> Starting rpmbuild createrepo_c.."
rpmbuild -ba $RPMBUILD_DIR/SPECS/createrepo_c.spec
if [ ! $? == "0" ]; then
    echo "Error while: rpmbuild -ba $RPMBUILD_DIR/SPECS/createrepo_c.spec"
    exit 1
fi
echo "rpmbuild done"

echo "> Cleanup .."
rpmbuild --clean $RPMBUILD_DIR/SPECS/createrepo_c.spec
echo "Cleanup done"

echo "> Moving rpms and srpm .."
mv --verbose $RPMBUILD_DIR/SRPMS/createrepo_c-*.src.rpm $TARGETDIR/.
mv --verbose $RPMBUILD_DIR/RPMS/*/createrepo_c-*.rpm $TARGETDIR/.
mv --verbose $RPMBUILD_DIR/RPMS/*/python-createrepo_c-*.rpm $TARGETDIR/.
echo "Moving done"

echo "All done!"
