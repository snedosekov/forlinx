#!/bin/sh

if test -e /dev/fb0; then
width=`fbset | grep 'geometry' | awk '{print $2}'`
height=`fbset | grep 'geometry' | awk '{print $3}'`

let height=height-38
geo=`echo $width\x$height+0+0`
fi

TYPE=`cat /etc/t2m`

pidof matrix_gui > /dev/null 2>&1
if [ $? == 0 ]
then
        if [ $TYPE = "T" ]
        then
                echo "M" > /etc/t2m
        else
                echo "T" > /etc/t2m
        fi

        killall matrix_gui
        /etc/init.d/qt.sh

fi
