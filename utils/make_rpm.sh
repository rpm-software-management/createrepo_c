#!/bin/bash

RPMBUILD_DIR="${HOME}/rpmbuild/"
BUILD_DIR="$RPMBUILD_DIR/BUILD"
GITREV=`git rev-parse --short HEAD`
PREFIX=""   # Root project dir
MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

if [ $# -lt "1"  -o $# -gt "2" ]
then
    echo "Usage: `basename $0` <root_project_dir> [revision]"
    exit 1
fi

PREFIX="$1/"

if [ ! -d "$RPMBUILD_DIR" ]; then
    echo "rpmbuild dir $RPMBUILD_DIR doesn't exist!"
    echo "init rpmbuild dir with command: rpmdev-setuptree"
    echo "(Hint: Package group @development-tools and package fedora-packager)"
    exit 1
fi

echo "Generating rpm for $GITREV"

echo "Cleaning $BUILD_DIR"
rm -rf $BUILD_DIR
echo "Removing $RPMBUILD_DIR/createrepo_c.spec"
rm -f $RPMBUILD_DIR/createrepo_c.spec

echo "> Making tarball .."
$MY_DIR/make_tarball.sh $GITREV
if [ ! $? == "0" ]; then
    echo "Error while making tarball"
    exit 1
fi
echo "Tarball done"

echo "> Copying tarball and .spec file into the $RPMBUILD_DIR .."
cp $PREFIX/createrepo_c-$GITREV.tar.xz $RPMBUILD_DIR/SOURCES/
if [ ! $? == "0" ]; then
    echo "Error while: cp createrepo_c-*.tar.xz $RPMBUILD_DIR/SOURCES/"
    exit 1
fi

# Copy via sed
sed -i "s/%global gitrev .*/%global gitrev $GITREV/g" $PREFIX/createrepo_c.spec
sed "s/%global gitrev .*/%global gitrev $GITREV/g" $PREFIX/createrepo_c.spec > $RPMBUILD_DIR/SPECS/createrepo_c.spec
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
mv --verbose $RPMBUILD_DIR/SRPMS/createrepo_c-*.src.rpm $PREFIX/.
mv --verbose $RPMBUILD_DIR/RPMS/*/createrepo_c-*.rpm $PREFIX/.
mv --verbose $RPMBUILD_DIR/RPMS/*/python-createrepo_c-*.rpm $PREFIX/.
mv --verbose $RPMBUILD_DIR/RPMS/*/python-deltarepo-*.rpm $PREFIX/.
mv --verbose $RPMBUILD_DIR/RPMS/*/deltarepo-*.rpm $PREFIX/.
echo "Moving done"

echo "All done!"
