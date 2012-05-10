#!/bin/bash

# /usr/share/man/man8/createrepo_c.8.gz

EXPECTED_ARGS=2
if [ $# -ne $EXPECTED_ARGS ]
then
    echo "Usage: `basename $0` <input_file> <outputdir>"
    echo
    echo "Example: `basename $0` ../src/cmd_parser.c ../doc/"
    exit 1
fi

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

python $MY_DIR/gen_rst.py $1 | rst2man | gzip > $2/createrepo_c.8.gz
