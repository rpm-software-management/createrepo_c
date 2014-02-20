#!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

if [[ -z "$MODIFYREPO" ]]
then
    MODIFYREPO="modifyrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --groupfile group.xml --revision "1st repo" --content "A content tag" .
$MODIFYREPO --mdtype="foobar"  foobar-1 repodata/
popd
