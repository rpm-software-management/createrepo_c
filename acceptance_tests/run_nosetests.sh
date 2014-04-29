#!/bin/bash

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILDDIR="$( cd "$CURDIR/../build" && pwd )"

PATH="$BUILDDIR/src/:/home/tmlcoch/git/repodiff/:$PATH" LD_LIBRARY_PATH=$BUILDDIR/src/ PYTHONPATH=$BUILDDIR/src/python/ nosetests -s -v ./tests/
