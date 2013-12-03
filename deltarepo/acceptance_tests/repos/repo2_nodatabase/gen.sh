#!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --no-database --revision foorevisionbar --distro cpe:/o:fedoraproject:fedora:17,foo --repo abc --content plm .
popd
