#!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --groupfile group.xml --revision "1st repo" --content "A content tag" .
popd
