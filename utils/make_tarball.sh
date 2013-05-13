#!/usr/bin/bash

TARGETDIR=`pwd -P`
SCRIPTDIR=$( cd "$(dirname "$0")" ; pwd -P )
PROJECTROOTDIR=`realpath "$SCRIPTDIR/../"`

if [ "$#" -eq "1" ] && [ "$1" = "-h" ]; then
    echo "Usage: `basename $0` [git revision]"
    exit 0
fi

if [ "$#" -eq "0" ]; then
    GITREV=`git rev-parse --short HEAD`
    SUFFIX=`$SCRIPTDIR/get_version.py $PROJECTROOTDIR`
else
    GITREV=$1
    SUFFIX=$GITREV
fi

echo "Generate tarball for revision/version: $SUFFIX (project root dir: $PROJECTROOTDIR)"

cd ${PROJECTROOTDIR}
git archive ${GITREV} --prefix="createrepo_c-${SUFFIX}/" | xz > $TARGETDIR/createrepo_c-${SUFFIX}.tar.xz
