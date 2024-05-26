#! /bin/bash

folder=/opt/unitedffmpeg

if [ ! -d $folder ] 
then
	mkdir $folder
fi 

make -f Makefile.ffmpeg
