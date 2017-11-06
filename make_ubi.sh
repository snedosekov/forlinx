#!/bin/bash

SD_DIR="./sd_card"
KERNEL_DIR="./kernel_src"
TOOLS_DIR="./tools"
UIMAGE="${KERNEL_DIR}/arch/arm/boot/uImage"
UBIFS_IMAGE=${TOOLS_DIR}/ubifs.img
UBI_IMAGE=${TOOLS_DIR}/ubi.img
UBOOT_IMAGE=${TOOLS_DIR}/u-boot.img
MLO=${TOOLS_DIR}/MLO 

echo "Build kernel .. "
make  CROSS_COMPILE=arm-arago-linux-gnueabi- ARCH=arm uImage -C ${KERNEL_DIR}
echo "OK"

echo "build ubx image .. "
cd  ${TOOLS_DIR}
./pack-ubi-256m.sh
cd ../

echo "OK"

echo "Copy binary to ${SD_DIR} .. "
mkdir ${SD_DIR} -p 
cp ${UIMAGE} ${SD_DIR}/
cp ${UBI_IMAGE} ${SD_DIR}/
cp ${UBOOT_IMAGE} ${SD_DIR}/
cp ${MLO} ${SD_DIR}/
echo "OK"

echo "Clean .. "
rm ${UBIFS_IMAGE}
rm ${UBI_IMAGE}
echo "OK"
