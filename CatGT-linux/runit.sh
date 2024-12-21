#!/bin/sh

# You can't call CatGT directly, rather, call it via runit.sh.
# You can call runit.sh two ways:
#
# 1) > runit.sh 'cmd-line-parameters'
# 2a) Edit parameters in runit.sh, then call it ...
# 2b) > runit.sh
#
# This script effectively says:
# "If there are no parameters sent to runit.sh, call CatGT
# with the parameters hard coded here, else, pass all of the
# parameters through to CatGT."
#
# Shell notes:
# - This script is provided for "sh" shell. If you need to use
# a different shell feel free to edit the script as required.
# Remember to change the first line to invoke that shell, for
# example, replace /bin/sh with /bin/bash
#
# - In most environments $0 returns the path and name of this
# script, but that is not guaranteed to be true. If using the
# bash shell, it is more reliable to define RUN_DIR like this:
# RUN_DIR=$(dirname $(readlink -f BASH_SOURCE[0]))
#
# - Enclosing whole parameter list in quotes is required to suppress
# linux curly brace expansion, but using quotes is recommended in general:
#
#    > runit.sh 'cmd-line-parameters'
#

if [ -z "$1" ]
then
    SRC=/groups/apig/apig/Austin_Graves/CatGT_C_Waves_test/SC_artifact_test_data
    DST=$SRC/SC011_OUT
    ARGS="-dir=$SRC -run=SC011_022319 -g=0 -t=0,1 -prb_fld -prb=3"
    ARGS="$ARGS -ap -apfilter=butter,12,300,9000 -gblcar -gfix=0,0.1,0.02"
    ARGS="$ARGS -dest=$DST -out_prb_fld"
else
    ARGS=$@
fi

RUN_DIR=$(dirname $(readlink -f $0))
export LD_LIBRARY_PATH=$RUN_DIR/links
$LD_LIBRARY_PATH/ld-linux-x86-64.so.2 --library-path $LD_LIBRARY_PATH $RUN_DIR/CatGT $ARGS

