./mkfs.ubifs -F -r rootfs -m 2048 -e 126976 -c 1866 -o ubifs.img
./ubinize -o ubi.img -m 2048 -p 128KiB -s 2048 -O 2048 ubinize-256M.cfg