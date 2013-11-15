#!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --groupfile group.xml --revision "1st repo" --content "A content tag" .
rm repodata/*filelists.sqlite*
rm repodata/*other.sqlite*
rm repodata/*filelists.xml*
rm repodata/*other.xml*
rm repodata/*group.xml*
rm repodata/*primary.sqlite*
popd
