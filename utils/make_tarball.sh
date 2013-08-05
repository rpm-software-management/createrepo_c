TARGET_DIR="./"

if [ "$#" -eq "0" ]; then
    GITREV=`git rev-parse --short HEAD`
else
    GITREV=$1
fi

echo "Generate tarball for revision: $GITREV"

git archive ${GITREV} --prefix=createrepo_c/ | xz > $TARGET_DIR/createrepo_c-${GITREV}.tar.xz
