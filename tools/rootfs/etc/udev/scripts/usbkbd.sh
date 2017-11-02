#!/bin/sh

 
if 
/sbin/udevadm info --query=all --name=/dev/input/$1|grep board >/dev/null
then
	echo "$1 is a keyboard"
	rm /dev/input/usbkbd 
	cd /dev/input
	ln -s $1 usbkbd
else  
	echo "$1 is not a keyboard"
fi
