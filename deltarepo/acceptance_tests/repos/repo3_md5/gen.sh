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
$CREATEREPO --checksum "md5" --groupfile comps.xml --revision "3th repo - md5" --content "111" --content "222" --repo "aaa" --repo "bbb" --repo "ccc" --distro="one,foo" --distro="two:bar" .
popd
