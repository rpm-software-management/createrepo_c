#!/bin/bash

REPONAME="createrepo_c"
GITREPO="https://github.com/Tojaj/createrepo_c.git"

MY_DIR="$( cd "$(dirname "$0")" ; pwd -P )"

pushd "$MY_DIR"
echo "Generating doc into $MY_DIR"

echo "Pulling changes"
git pull

mkdir -p "python/"  # In the most scenarios this dir would already exist
mkdir -p "c/"  # In the most scenarios this dir would already exist

TMP_DIR=$( mktemp -d )
echo "Using temporary directory: $TMP_DIR"

pushd "$TMP_DIR"
git clone --depth 1 --branch master --single-branch "$GITREPO"
cd "$REPONAME"
mkdir build
cd build
cmake ..
make
make doc
echo "Copying python doc..."
cp doc/python/html/*.html "$MY_DIR"/python
cp doc/python/html/*.js "$MY_DIR"/python
cp -r doc/python/html/_static "$MY_DIR"/python
echo "Copying C doc..."
cp -r doc/html/* "$MY_DIR"/c
popd
echo "Removing: $TMP_DIR"
rm -rf "$TMP_DIR"

echo "Successfully finished"
echo "To push updated doc - commit changes and push them:"
echo "git commit -a -m \"Documentation update\""
echo "git push origin gh-pages"

popd
