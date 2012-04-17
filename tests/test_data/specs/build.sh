#!/bin/sh

for spec in fake-*.spec; do
    rpmbuild -ba $spec
done
