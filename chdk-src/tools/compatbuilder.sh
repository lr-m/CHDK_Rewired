#!/bin/sh

# Build the pid_leds[] array (loader/generic/compat_table.h) for the loader compatibility check
#
# Information gathered from all ports listed in camera_list.csv (or the file passed as first argument),
# specifically:
# - PLATFORMID address: port-specific stubs_entry.S or makefile.inc
# - Dancingbits encoding (NEED_ENCODED_DISKBOOT): port-specific makefile.inc
# - PLATFORMID: port-specific makefile.inc
# - BLINK_LED_CONTROL (see loader/generic/check_compat.c): platform/(camera)/makefile.inc only
# - BLINK_LED_GPIO (see loader/generic/check_compat.c): platform/(camera)/makefile.inc only

OUTFILE=loader/generic/compat_table.h

if [ "$1" = "" ]; then
INFILE=camera_list.csv
else
INFILE=$1
fi

PREV_CAM=0

rm -f $OUTFILE

while IFS=, read cam fw state srcfw skip
do
  if [ "${cam}" != "CAMERA" ] && ( [ "${srcfw}" = "" ] || [ "${srcfw}" = "${fw}" ] ) && [ "${cam}" != "${PREV_CAM}" ]; then
    var1=`grep -E -h -s "PLATFORMID.+@" platform/${cam}/makefile.inc platform/${cam}/sub/${fw}/makefile.inc platform/${cam}/sub/${fw}/stubs_entry.S`
    if [ "$var1" != "" ]; then
      var1=`echo ${var1} | sed -re 's/(.+@ *)([0-9xXa-fA-F]+)(.*)/\2/'`
      var2=`grep -E -h -s "NEED_ENCODED_DISKBOOT *=" platform/${cam}/makefile.inc platform/${cam}/sub/${fw}/makefile.inc`
      var3=`grep -E -h -s "^ *PLATFORMID *=" platform/${cam}/makefile.inc platform/${cam}/sub/${fw}/makefile.inc`
      var5=`grep -E -h -s "BLINK_LED_CONTROL *=" platform/${cam}/makefile.inc`
      var6=`grep -E -h -s "BLINK_LED_GPIO *=" platform/${cam}/makefile.inc`
      if [ "$var2" = "" ]; then
        var4=`grep -E -h -s "^ *override +PLATFORM *=" platform/${cam}/makefile.inc platform/${cam}/sub/${fw}/makefile.inc`
        if [ "$var4" != "" ]; then
          var4=`echo ${var4} | sed -re 's/(.+= *)(.+)/\2/'`
          var2=`grep -E -h -s "NEED_ENCODED_DISKBOOT *=" platform/${var4}/makefile.inc platform/${var4}/sub/${srcfw}/makefile.inc`
        fi
      fi
      if [ "$var2" != "" ]; then
        var2=`echo ${var2} | sed -re 's/(.+= *)([0-9]+)(.*)/\2/'`
      else
        var2=0
      fi
      if [ "$var3" = "" ]; then
        var3=`grep -E -h -s "^ *TARGET_PID *=" platform/${cam}/makefile.inc platform/${cam}/sub/${fw}/makefile.inc`
      fi
      if [ "$var3" != "" ]; then
        var3=`echo ${var3} | sed -re 's/(.+= *)([0-9xXa-fA-F]+)(.*)/\2/'`
        var3=`printf %d ${var3}`
      else
        var3=0
      fi
      if [ "$var5" != "" ] && [ `echo ${var5} | sed -re 's/([^=]+)(=.*)/\2/'` != "=" ]; then
        ledctrl=`echo ${var5} | sed -re 's/(.+= *)([0-9]+)(.*)/\2/'`
      else
        ledctrl=0
      fi
      if [ "$var6" != "" ] && [ `echo ${var6} | sed -re 's/([^=]+)(=.*)/\2/'` != "=" ]; then
        ledaddr=`echo ${var6} | sed -re 's/(.+= *)([0-9xXa-fA-F]+)(.*)/\2/'`
      else
        ledaddr=0
      fi
      PREV_CAM=${cam}
      echo "${var2},${var3},${cam},${fw},${var1},${ledctrl},${ledaddr}" >> $OUTFILE
    fi
  fi
done < ${INFILE}

cat $OUTFILE | sort > $OUTFILE.tmp

awk -F',' 'BEGIN {print "// !!! THIS FILE IS GENERATED. DO NOT EDIT. !!!"; dance=-1} {if(dance!=$1) {dance=$1; if(dance!=0) el="el"; else el=""; 
  printf("#%sif (NEED_ENCODED_DISKBOOT == %i)\n",el,dance)} printf("{ %s,%s,%s,%s }, // %s\n",$2,$6,$7,$5,$3) } END {print "#endif"}' $OUTFILE.tmp > $OUTFILE

rm -f $OUTFILE.tmp

