#!/bin/bash

# Global variables

REPO=""             # Path to repo
CLEAR_CACHE=true    # Clear cache?

# Param check

if [ $# -lt "1" -o $# -gt "2" ]; then
    echo "Usage: `basename $0` <repository> [--cache]"
    exit 1
fi

if [ $1 == "-h" ]; then
    echo "Tool for comparsion of speed between createrepo and createrepo_c."
    echo "WARNING! This tool changes (removes) repodata if exits!"
    echo "Usage: `basename $0` <repository> [--cache]"
    echo "Options:"
    echo "  --cache   Skip cleaning of disk cache"
    exit 0
fi

if [ $# -eq "2" ]; then
    if [ $2 != "--cache" ]; then
        echo "Unknown param $2"
        exit 1
    else
        CLEAR_CACHE=false
    fi
fi

if $CLEAR_CACHE; then
    if [ `id --user` != "0" ]; then
        echo "Note:"
        echo "You are not root!"
        echo "For cleaning disk caches you have to have a root permissions."
        echo "You will be asked for sudo password."
        echo "(Maybe even several times)"
        sudo bash -c "echo \"OK\""
        echo
    fi
fi

REPO=$1

if [ ! -d "$REPO" ]; then
    echo "Directory $REPO doesn't exists"
    exit 1
fi

# Main

function clear_cache {
    # Clear cache if CLEAR_CACHE is true

    if ! $CLEAR_CACHE; then
        return
    fi

    if [ `id --user` != "0" ]; then
        sudo bash -c "echo 3 > /proc/sys/vm/drop_caches"
    else
        echo 3 > /proc/sys/vm/drop_caches
    fi
}

function run {
    # Run - entire metadata from scratch

    rm -rf "$REPO"/.repodata # Just in case previous run of createrepo_c failed

    rm -rf "$REPO"/repodata
    echo -e "\n\$ createrepo_c $1 $REPO"
    clear_cache
    (time createrepo_c $1 "$REPO") 2>&1

    rm -rf "$REPO"/repodata
    echo -e "\n\$ createrepo $1 $REPO"
    clear_cache
    (time createrepo $1   "$REPO") 2>&1

    echo
}

function dirty_run {
    # Run - repodata already exists in place

    rm -rf "$REPO"/.repodata # Just in case previous run of createrepo_c failed

    # Prepare metadata
    rm -rf "$REPO"/repodata
    createrepo --quiet --database "$REPO" > /dev/null

    echo -e "\n\$ createrepo_c $1 $REPO"
    clear_cache
    (time createrepo_c $1 "$REPO") 2>&1

    # Prepare metadata
    rm -rf "$REPO"/repodata
    createrepo --quiet --database "$REPO" > /dev/null

    echo -e "\n\$ createrepo $1 $REPO"
    clear_cache
    (time createrepo $1   "$REPO") 2>&1

    echo
}

echo "Test setup"
echo "+---------------------------------------------------------------+"
echo "System:"
uname --operating-system --kernel-release
if [ -e /etc/issue ]; then
    head -q -n 1 /etc/issue
fi
grep "model name" /proc/cpuinfo
uname --processor
grep "MemTotal" /proc/meminfo
echo
echo "Package versions:"
rpm -qa|grep createrepo
echo
echo "Test repo:"
echo "$REPO"
echo
echo "Case-1: generating entire metadata from scratch"
echo "+---------------------------------------------------------------+"
echo "+ With sqlite DB"
echo "+----------------------+"
run "--database"
echo "+ Without sqlite DB"
echo "+----------------------+"
run "--no-database"

echo "Case-2: re-generating metadata (with existing repodata in place)"
echo "+---------------------------------------------------------------+"
echo "+ With sqlite DB"
echo "+----------------------+"
dirty_run "--update --database"
echo "+ Without sqlite DB"
echo "+----------------------+"
dirty_run "--update --no-database"

# Final clean up

rm -rf "$REPO"/repodata
rm -rf "$REPO"/.repodata
