#!/bin/sh

# Read the 'camera_list.csv' file ($3) and run the
# CHDK build action ($2) using the designated
# make program ($1)
# ($4) = -noskip: ignore the SKIP_AUTOBUILD column
# - also see main Makefile

# -noskip operations will only execute on 'real' ports
# the firzipsub and firzipsubcomplete actions will be transformed into firzipsubcopy and firzipsubcompletecopy
# when the srcfw column is not empty

while IFS=, read cam fw state srcfw skip
do
  if [ ${cam} != "CAMERA" ] && ( [ "$4" = "-noskip" ] || [ "${skip}" = "" ] ); then
    srcfwo=${srcfw}
    if [ "${state}" != "" ]; then state=_${state}; fi
    if [ "${srcfw}" = "" ]; then srcfw=${fw}; fi
    if [ "$4" = "-noskip" ] && [ ${srcfw} != ${fw} ] && [ "$2" != "os-camera-list-entry" ] && [ "$2" != "cam-info" ]; then jmp=1; else jmp=0; fi
    if ( [ "$2" = "firzipsub" ] || [ "$2" = "firzipsubcomplete" ] ) && [ ${srcfw} != ${fw} ] ; then cpy="copy"; else cpy=""; fi
    if [ ${jmp} -eq 0 ] ; then
      $1 -s --no-print-directory TARGET_FW=${fw} PLATFORM=${cam} PLATFORMSUB=${srcfw} STATE=${state} SRCFW=${srcfwo} SKIP_AUTOBUILD=${skip} ${2}${cpy} || exit 1
    fi
  fi
done < $3
