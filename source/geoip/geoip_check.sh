#!/bin/sh

# this script is used by Makefile to check support for `libmaxminddb`

CXX=$*

# try compiling test file
$CXX -o /dev/null ./geoip/geoip_test.cpp -lmaxminddb 2>/dev/null

# if it succeeded, report maxmind support
if [ $? -eq 0 ]
then
    echo " -DHAVE_MAXMINDDB"
else
    echo
fi

unset CXX