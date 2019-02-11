#!/bin/bash

PACKAGE="createrepo_c"
RPMBUILD_DIR="${HOME}/rpmbuild/"
BUILD_DIR="$RPMBUILD_DIR/BUILD"
GITREV=`git rev-parse --short HEAD`
PREFIX=""   # Root project dir
MY_DIR=`dirname "$0"`

if [ $# -lt "1"  -o $# -gt "2" ]
then
    echo "Usage: `basename "$0"` <root_project_dir> [revision]"
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
rm -rf "$BUILD_DIR"
echo "Removing $RPMBUILD_DIR/SPECS/$PACKAGE.spec"
rm -f "$RPMBUILD_DIR/SPECS/$PACKAGE.spec"

echo "> Making tarball .."
"$MY_DIR/make_tarball.sh" "$GITREV"
if [ ! $? == "0" ]; then
    echo "Error while making tarball"
    exit 1
fi
echo "Tarball done"

echo "> Copying tarball and .spec file into the $RPMBUILD_DIR .."
cp "$PREFIX/$PACKAGE-$GITREV.tar.xz" "$RPMBUILD_DIR/SOURCES/"
if [ ! $? == "0" ]; then
    echo "Error while: cp $PREFIX/$PACKAGE-$GITREV.tar.xz $RPMBUILD_DIR/SOURCES/"
    exit 1
fi

# Copy via sed
sed -i "s/%global gitrev .*/%global gitrev $GITREV/g" "$PREFIX/$PACKAGE.spec"
sed "s/%global gitrev .*/%global gitrev $GITREV/g" "$PREFIX/$PACKAGE.spec" > "$RPMBUILD_DIR/SPECS/$PACKAGE.spec"
if [ ! $? == "0" ]; then
    echo "Error while: cp $PREFIX/$PACKAGE.spec $RPMBUILD_DIR/SPECS/"
    exit 1
fi
echo "Copying done"

echo "> Starting rpmbuild $PACKAGE.."
rpmbuild -ba "$RPMBUILD_DIR/SPECS/$PACKAGE.spec"
if [ ! $? == "0" ]; then
    echo "Error while: rpmbuild -ba $RPMBUILD_DIR/SPECS/$PACKAGE.spec"
    exit 1
fi
echo "rpmbuild done"

echo "> Cleanup .."
rpmbuild --clean "$RPMBUILD_DIR/SPECS/$PACKAGE.spec"
echo "Cleanup done"

echo "> Moving rpms and srpm .."
mv --verbose "$RPMBUILD_DIR"/SRPMS/"$PACKAGE"-*.src.rpm "$PREFIX/."
mv --verbose "$RPMBUILD_DIR"/RPMS/*/"$PACKAGE"-*.rpm "$PREFIX/."
mv --verbose "$RPMBUILD_DIR"/RPMS/*/python*-"$PACKAGE"-*.rpm "$PREFIX/."
echo "Moving done"

echo "All done!"
