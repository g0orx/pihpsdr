#!/bin/sh

#
# Launch the exe file from a shell.
# This is the only way to make a GTK app bundle
# It still relies on a working GTK installation
#
# A full-fledged wrapper here would set dozens of
# environment variables.
#
this=`dirname $0` 
cd $this/../Resources

exec $this/pihpsdr-bin
