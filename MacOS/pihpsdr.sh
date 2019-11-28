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

#
# If the things below fail for some reason, stay in $HOME
#
cd $HOME

#
# This is a standard MacOS location
#
localdir="$HOME/Library/Application Support/piHPSDR"

mkdir -p "$localdir"
cd       "$localdir"

#
# Copy HPSDR icon to local work directory
#
cp $this/../Resources/hpsdr.png .

#
# Finally, start the program
#
exec $this/pihpsdr-bin
