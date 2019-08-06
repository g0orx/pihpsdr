#!/bin/sh

#
# Launch the exe file from a shell.
# This is the only way to make a GTK app bundle
# It still relies on a working GTK installation
#
# A full-fledged wrapper here would set dozens of
# environment variables.
#
#
# Use $HOME/.pihpsdr as the working dir,
# copy hpsdr.png to that location

this=`dirname $0` 

#
# If the things below fail for some reason, stay in $HOME
#
cd $HOME

localdir="$HOME/Library/Application Support/piHPSDR"

echo $localdir > $HOME/test.out

mkdir -p "$localdir"
cd       "$localdir"

cp $this/../Resources/hpsdr.png .
exec $this/pihpsdr-bin
