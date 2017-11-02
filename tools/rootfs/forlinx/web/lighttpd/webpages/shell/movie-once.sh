#!/bin/sh

if test -e /dev/fb0; then
width=`fbset | grep 'geometry' | awk '{print $2}'`
height=`fbset | grep 'geometry' | awk '{print $3}'`

let height=height-20
geo=`echo $width\x$height+0+0`
fi

FB_SIZE=$(cat /sys/class/graphics/fb0/virtual_size)

pidof matrix_gui > /dev/null 2>&1
if [ $? == 0 ]
then
	BOARDNAME=`cat /proc/boardname`
	if [ $BOARDNAME != "OK335xS2" ]; then
		if [ $FB_SIZE = "480,544" ]; then
			/forlinx/bin/mplayer -fs /forlinx/video/xm-4.mp4 -geometry $geo $*
		else
			/forlinx/bin/mplayer -fs /forlinx/video/xm.mp4 -geometry $geo $*
		fi
        else
		if [ $FB_SIZE = "480,544" ]; then
			/forlinx/bin/mplayer -fs -nosound /forlinx/video/xm-4.mp4 -geometry $geo $*
		else
			/forlinx/bin/mplayer -fs -nosound /forlinx/video/xm.mp4 -geometry $geo $*
		fi
        fi
else
export TSLIB_TSDEVICE=/dev/input/touchscreen0
export QWS_MOUSE_PROTO="Tslib:/dev/input/touchscreen0 Intellimouse:/dev/input/mice"

		if [ $FB_SIZE = "480,544" ]; then
			/forlinx/bin/mplayer -qws -fs /forlinx/video/xm-4.mp4 -geometry $geo $*
		else
			/forlinx/bin/mplayer -qws -fs /forlinx/video/xm.mp4 -geometry $geo $*
		fi
fi
