#!/bin/sh

# Read the 'camera_list.csv' file ($3) and run the
# CHDK build action ($2) using the designated
# make program ($1)
# ($4) = 'start' (for Windows) or 'echo' (for creating a batch file)
# ($5) = -noskip disable SKIP_AUTOBUILD
# - also see main Makefile
# parallel version - starts each build in a seperate session (Windows only)
while IFS=, read cam fw state srcfw skip
do
  if [ ${cam} != "CAMERA" ] && ( [ "$5" = "-noskip" ] || [ "${skip}" = "" ] ); then
    if [ "${state}" != "" ]; then state=_${state}; fi
# skip copied firmwares
    if [ "${srcfw}" = "" ]; then
    	$4 $1 -s --no-print-directory TARGET_FW=${fw} PLATFORM=${cam} PLATFORMSUB=${fw} STATE=${state} SKIP_AUTOBUILD=${skip} $2 || exit 1;
	fi
  fi
done < $3
