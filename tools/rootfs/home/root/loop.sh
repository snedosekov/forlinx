#!/bin/sh

base=matrix_gui
pid=`/bin/pidof $base`
if [ -n "$pid" ]; then
    kill -9  $pid
fi

FB_SIZE=$(cat /sys/class/graphics/fb0/virtual_size)

if [ $FB_SIZE = "480,544" ]; then
/forlinx/bin/mplayer -fs -loop 0 /forlinx/video/xm-4.mp4
else
/forlinx/bin/mplayer -fs -loop 0 /forlinx/video/xm.mp4
fi

