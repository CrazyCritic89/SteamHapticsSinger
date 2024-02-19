#!/bin/sh

if [ "$EUID" != 0 ]; then
    sudo "$0" "$@"
    exit $?
fi

cd cfg/usb_gadget/g2
echo "" > UDC

rm configs/c.1/midi.shs

rmdir configs/c.1/strings/0x409

rmdir configs/c.1

rmdir functions/ffs.mtp

rmdir strings/0x409

cd ..
rmdir g2

cd ../../
umount cfg
rmdir cfg

modprobe -r usb_f_midi
