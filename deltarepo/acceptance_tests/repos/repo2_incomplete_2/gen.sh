#!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

if [[ -z "$MODIFYREPO" ]]
then
    CREATEREPO="modifyrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --revision foorevisionbar --distro cpe:/o:fedoraproject:fedora:17,foo --repo abc --content plm .
$MODIFYREPO --remove primary_db repodata/
$MODIFYREPO --remove filelists repodata/
$MODIFYREPO --remove filelists_db repodata/
$MODIFYREPO --remove other repodata/
$MODIFYREPO --remove other_db repodata/
popd
