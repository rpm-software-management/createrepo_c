#!/bin/bash

# /usr/share/man/man8/createrepo_c.8.gz

EXPECTED_ARGS=3
if [ $# -ne $EXPECTED_ARGS ]
then
    echo "Usage: `basename $0` <createrepo_input_file> <mergerepo_input_file> <outputdir>"
    echo
    echo "Example: `basename $0` src/cmd_parser.c src/mergerepo_c.c doc/"
    exit 1
fi

MY_DIR=`dirname $0`
MY_DIR="$MY_DIR/"

python $MY_DIR/gen_rst.py $1 | rst2man | gzip > $3/createrepo_c.8.gz
python $MY_DIR/gen_rst.py $2 --mergerepo | rst2man | gzip > $3/mergerepo_c.8.gz
