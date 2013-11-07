#!/bin/bash

DELTAREPO="../deltarepo.py"

MY_DIR=`dirname $0`

pushd $MY_DIR > /dev/null

# Prepare outdir

DATETIME=`date +"%Y%m%d-%H%M%S"`
OUTDIR_TEMPLATE="deltarepo-test-$DATETIME-XXX"
TEST_OUTDIR=`mktemp -d "$OUTDIR_TEMPLATE"`

# Repos paths

REPO1="repos/repo1"
REPO2="repos/repo2"
REPO2_INCOMPLETE="repos/repo2_incomplete"
REPO3="repos/repo3"
REPO3_MD5="repos/repo3_md5"

# Helper functions

function testcase_outdir {
    mktemp -d "$TEST_OUTDIR/testcase-$1-XXX"
}

function compare_repos {
    # Arguments are: repo1 repo2

    echo "Comparing: $1/repodata $2/repodata"
    echo

    diff -r $1/repodata $2/repodata \
        -I "<timestamp>[0-9]*</timestamp>" \
        -I "<repoid .*>.*</repoid>"
    echo

    return $?
}

# Test cases

TESTCASEID=0

function testcase01 {
    # Arguments are: REPO_old REPO_new

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: $FUNCNAME $1 $2"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    $DELTAREPO -o $DELTADIR $1 $2
    $DELTAREPO -a -o $FINALDIR $1 $DELTADIR

    compare_repos $2 $FINALDIR
}

testcase01 $REPO1 $REPO2
testcase01 $REPO1 $REPO2_INCOMPLETE
testcase01 $REPO1 $REPO3
testcase01 $REPO1 $REPO3_MD5
testcase01 $REPO2 $REPO1
testcase01 $REPO2 $REPO2_INCOMPLETE
testcase01 $REPO2 $REPO3
testcase01 $REPO2 $REPO3_MD5
testcase01 $REPO3 $REPO1
testcase01 $REPO3 $REPO2
testcase01 $REPO3 $REPO2_INCOMPLETE
testcase01 $REPO3 $REPO3_MD5
testcase01 $REPO3_MD5 $REPO1
testcase01 $REPO3_MD5 $REPO2
testcase01 $REPO3_MD5 $REPO2_INCOMPLETE
testcase01 $REPO3_MD5 $REPO3

popd > /dev/null
