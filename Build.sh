#!/bin/sh
# This shell script is still here for compatibility reasons.
# make forced

# custom
DEST=/media/bdamour/3538-6661/apps/NintendontDEBUG
make -C loader/source/ppc/PADReadGC/ 1>/dev/null && make -C loader 1>/dev/null && make 1>/dev/null
if [ $? -eq 0 ] ; then
	echo
	echo 'success!'
	echo "shipping to $DEST"
	wiiload loader/loader.dol
	# cp ../bkup/NintendontDEBUG/meta.xml $DEST/meta.xml
	# if [ $? -eq 0 ] ; then
	# 	cp loader/loader.dol $DEST/boot.dol
	# 	echo 'unmounting'
	# 	umount /media/bdamour/3538-6661
	# 	echo 'done.'
	# fi
fi

