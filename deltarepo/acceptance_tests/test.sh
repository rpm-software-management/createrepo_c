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
REPO1_ONLY_PRI="repos/repo1_only_pri"
REPO1_ONLY_PRI_FIL="repos/repo1_only_pri_fil"
REPO2="repos/repo2"
REPO2_INCOMPLETE="repos/repo2_incomplete"
REPO2_INCOMPLETE_2="repos/repo2_incomplete_2"
REPO2_ONLY_PRI="repos/repo2_only_pri"
REPO2_NODATABASE="repos/repo2_nodatabase"
REPO3="repos/repo3"
REPO3_MD5="repos/repo3_md5"


# Check if repos are available (if not regenrepos.sh must be run)

if [ ! -d "$REPO1/repodata" ]; then
  echo "It seems that the test repositories doesn't have metadata created."
  echo "Running metadata regeneration script.."
  echo
  repos/regenrepos.sh
  echo
  echo "Metadata regenerated"
fi


# Some ugly global variables

DELTAREPO_QUIET=0
COMPAREREPOS_IGNORE_REPOMD_CONTENT=0


# Helper functions

function testcase_outdir {
    mktemp -d "$TEST_OUTDIR/testcase-$1-XXX"
}

function compare_repos {
    # Arguments are: repo1 repo2

    echo "Comparing: $1/repodata $2/repodata"
    echo

    OTHERARGUMENTS=""

    if [ "$COMPAREREPOS_IGNORE_REPOMD_CONTENT" = 1 ]; then
        OTHERARGUMENTS=" --exclude repomd.xml "
    fi

    diff -r $1/repodata $2/repodata \
        -I "<timestamp>[0-9]*</timestamp>" \
        -I "<repoid .*>.*</repoid>" \
        $OTHERARGUMENTS
    echo

    return $?
}

# Test cases

TESTCASEID=0

function testcase01 {
    # Arguments are: REPO_old REPO_new

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: $FUNCNAME $1 -> $2 applied on: $1"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet -o $DELTADIR $1 $2
        $DELTAREPO --quiet -a -o $FINALDIR $1 $DELTADIR
    else
        $DELTAREPO -o $DELTADIR $1 $2
        $DELTAREPO -a -o $FINALDIR $1 $DELTADIR
    fi


    compare_repos $2 $FINALDIR
}


# Misuse cases

testcase01 $REPO1 $REPO1
testcase01 $REPO2 $REPO2
testcase01 $REPO2_INCOMPLETE $REPO2_INCOMPLETE
testcase01 $REPO2_INCOMPLETE_2 $REPO2_INCOMPLETE_2
testcase01 $REPO2_NODATABASE $REPO2_NODATABASE
testcase01 $REPO3 $REPO3
testcase01 $REPO3_MD5 $REPO3_MD5


# Regular cases

testcase01 $REPO1 $REPO2
testcase01 $REPO1 $REPO2_INCOMPLETE
testcase01 $REPO1 $REPO2_INCOMPLETE_2
testcase01 $REPO1 $REPO2_NODATABASE
testcase01 $REPO1 $REPO3
testcase01 $REPO1 $REPO3_MD5

testcase01 $REPO1_ONLY_PRI_FIL $REPO2
testcase01 $REPO1_ONLY_PRI_FIL $REPO2_INCOMPLETE
testcase01 $REPO1_ONLY_PRI_FIL $REPO2_INCOMPLETE_2
testcase01 $REPO1_ONLY_PRI_FIL $REPO2_NODATABASE
testcase01 $REPO1_ONLY_PRI_FIL $REPO3
testcase01 $REPO1_ONLY_PRI_FIL $REPO3_MD5

testcase01 $REPO2 $REPO1
testcase01 $REPO2 $REPO2_INCOMPLETE
testcase01 $REPO2 $REPO2_INCOMPLETE_2
testcase01 $REPO2 $REPO2_NODATABASE
testcase01 $REPO2 $REPO3
testcase01 $REPO2 $REPO3_MD5

testcase01 $REPO2_INCOMPLETE $REPO1
testcase01 $REPO2_INCOMPLETE $REPO2
testcase01 $REPO2_INCOMPLETE $REPO2_INCOMPLETE_2
testcase01 $REPO2_INCOMPLETE $REPO3
testcase01 $REPO2_INCOMPLETE $REPO3_MD5

testcase01 $REPO2_INCOMPLETE_2 $REPO1
testcase01 $REPO2_INCOMPLETE_2 $REPO2
testcase01 $REPO2_INCOMPLETE_2 $REPO2_INCOMPLETE
testcase01 $REPO2_INCOMPLETE_2 $REPO3
testcase01 $REPO2_INCOMPLETE_2 $REPO3_MD5

testcase01 $REPO2_NODATABASE $REPO1
testcase01 $REPO2_NODATABASE $REPO2
testcase01 $REPO2_NODATABASE $REPO2_INCOMPLETE
testcase01 $REPO2_NODATABASE $REPO2_INCOMPLETE_2
testcase01 $REPO2_NODATABASE $REPO3
testcase01 $REPO2_NODATABASE $REPO3_MD5

testcase01 $REPO3 $REPO1
testcase01 $REPO3 $REPO2
testcase01 $REPO3 $REPO2_INCOMPLETE
testcase01 $REPO3 $REPO2_INCOMPLETE_2
testcase01 $REPO3 $REPO3_MD5

testcase01 $REPO3_MD5 $REPO1
testcase01 $REPO3_MD5 $REPO2
testcase01 $REPO3_MD5 $REPO2_INCOMPLETE
testcase01 $REPO3_MD5 $REPO2_INCOMPLETE_2
testcase01 $REPO3_MD5 $REPO3

# Cases where repomd.xml will differ

COMPAREREPOS_IGNORE_REPOMD_CONTENT=1
DELTAREPO_QUIET=1

testcase01 $REPO1_ONLY_PRI_FIL $REPO1_ONLY_PRI_FIL
testcase01 $REPO1 $REPO1_ONLY_PRI_FIL
testcase01 $REPO2 $REPO1_ONLY_PRI_FIL
testcase01 $REPO2_INCOMPLETE $REPO1_ONLY_PRI_FIL
testcase01 $REPO2_INCOMPLETE_2 $REPO1_ONLY_PRI_FIL
testcase01 $REPO3 $REPO1_ONLY_PRI_FIL
testcase01 $REPO3_MD5 $REPO1_ONLY_PRI_FIL


# 2nd test case
#
#   Scenario:
# We have a regular delta from R1 -> R2 but our R1 is incomplete
# and some metadata files are missing.
#   Expected result:
# Deltarepo should update the available files and print warning
# about the missing ones.

function testcase02 {
    # Arguments are: REPO_old REPO_new

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: $FUNCNAME $1 -> $2 applied on: $3"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet -o $DELTADIR $1 $2
        $DELTAREPO --quiet -a -o $FINALDIR $3 $DELTADIR
    else
        $DELTAREPO -o $DELTADIR $1 $2
        $DELTAREPO -a -o $FINALDIR $3 $DELTADIR
    fi


    compare_repos $2 $FINALDIR
}


COMPAREREPOS_IGNORE_REPOMD_CONTENT=1
DELTAREPO_QUIET=1

testcase02 $REPO1 $REPO2 $REPO1_ONLY_PRI_FIL
testcase02 $REPO1 $REPO3 $REPO1_ONLY_PRI_FIL
testcase02 $REPO1 $REPO2 $REPO1_ONLY_PRI
testcase02 $REPO1 $REPO3 $REPO1_ONLY_PRI
testcase02 $REPO2 $REPO1 $REPO2_ONLY_PRI
testcase02 $REPO2 $REPO3 $REPO2_ONLY_PRI


# 3th test case
#
#   Scenario:
# We have incomplete delta for R1 -> R2. And incomplete R1.
# The delta contains only deltas for the files contained by our R1.
#   Expected result:
# Available deltas are applicated on available metadata.

function testcase03 {
    # Arguments are: REPO_old REPO_new

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: incomplete delta $FUNCNAME $1 -> $2 applied on: $3"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    # Gen delta

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet -o $DELTADIR $1 $2
    else
        $DELTAREPO -o $DELTADIR $1 $2
    fi

    # Remove some metadata from delta

    rm -f $DELTADIR/repodata/*filelists.sqlite*
    rm -f $DELTADIR/repodata/*other*
    rm -f $DELTADIR/repodata/*comps*
    rm -f $DELTADIR/repodata/*foobar*

    # Apply this delta to incomplete repo

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet -a -o $FINALDIR $3 $DELTADIR
    else
        $DELTAREPO -a -o $FINALDIR $3 $DELTADIR
    fi

    compare_repos $2 $FINALDIR
}

COMPAREREPOS_IGNORE_REPOMD_CONTENT=1
DELTAREPO_QUIET=1

testcase03 $REPO2 $REPO3 $REPO2_ONLY_PRI


# 4th test case
#
#   Scenario:
# We have delta where databases should not be generated.
# We want the databases.
#   Expected result:
# deltarepo --apply with --database argument should generate repo
# with databases

function testcase04 {
    # Arguments are: REPO_old REPO_new_nodbs REPO_new_dbs

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: $FUNCNAME $1 -> $2 applied on: $1"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet -o $DELTADIR $1 $2
        $DELTAREPO --quiet --database -a -o $FINALDIR $1 $DELTADIR
    else
        $DELTAREPO -o $DELTADIR $1 $2
        $DELTAREPO --database -a -o $FINALDIR $1 $DELTADIR
    fi

    compare_repos $3 $FINALDIR
}

testcase04 $REPO1 $REPO2_NODATABASE $REPO2


# 5th test case
#
#   Scenario:
# We want create delta where destination repo doesn't have a databases
# But we want the databases. We use deltarepo with --database argument
# during delta repo generation
#   Expected result:
# deltarepo --apply even WITHOUT --database argument should generate repo
# with databases.

function testcase05 {
    # Arguments are: REPO_old REPO_new_nodbs REPO_new_dbs

    IDSTR=$(printf "%02d\n" $TESTCASEID)
    TESTCASEID=$[$TESTCASEID+1]

    TCNAME="$IDSTR: $FUNCNAME $1 -> $2 applied on: $1"
    TCDIR=$(testcase_outdir "$IDSTR-$FUNCNAME")

    echo "==============================================="
    echo "$TCNAME ($TCDIR)";
    echo "==============================================="

    DELTADIR="$TCDIR/delta"
    FINALDIR="$TCDIR/final"
    mkdir $DELTADIR
    mkdir $FINALDIR

    if [ "$DELTAREPO_QUIET" = 1 ]; then
        $DELTAREPO --quiet --database -o $DELTADIR $1 $2
        $DELTAREPO --quiet -a -o $FINALDIR $1 $DELTADIR
    else
        $DELTAREPO --database -o $DELTADIR $1 $2
        $DELTAREPO -a -o $FINALDIR $1 $DELTADIR
    fi

    compare_repos $3 $FINALDIR
}

testcase05 $REPO1 $REPO2_NODATABASE $REPO2

popd > /dev/null
