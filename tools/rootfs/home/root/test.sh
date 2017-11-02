#!/bin/sh
export QWS_MOUSE_PROTO="Tslib:/dev/input/touchscreen0 Intellimouse:/dev/input/mice"
base=matrix_gui
pid=`/bin/pidof $base`
if [ -n "$pid" ]; then
    kill -9  $pid
fi


if test -e /dev/fb0; then
width=`fbset | grep 'geometry' | awk '{print $2}'`
height=`fbset | grep 'geometry' | awk '{print $3}'`

let height=height-38
geo=`echo $width\x$height+0+0`
fi


FB_SIZE=$(cat /sys/class/graphics/fb0/virtual_size)
if [ $FB_SIZE = "480,544"  ]
then
 /forlinx/qt/bin/IntegrateTest43 -font unifont  -geometry $geo $*  -qws
else
 /forlinx/qt/bin/IntegrateTest -font unifont  -geometry $geo $*  -qws
fi

