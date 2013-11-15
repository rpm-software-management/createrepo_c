!/bin/bash

MY_DIR=`dirname $0`

if [[ -z "$CREATEREPO" ]]
then
    CREATEREPO="createrepo_c"
fi

pushd "$MY_DIR"
$CREATEREPO $EXTRAARGS --pkglist pkglist --revision foorevisionbar --distro cpe:/o:fedoraproject:fedora:17,foo --repo abc --content plm .
rm repodata/*filelists.sqlite*
rm repodata/*other.sqlite*
rm repodata/*filelists.xml*
rm repodata/*other.xml*
rm repodata/*primary.sqlite*
popd
