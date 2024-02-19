#!/bin/sh

# DeckMTP used as reference https://github.com/dafta/DeckMTP/blob/main/backend/src/

if [ "$EUID" != 0 ]; then
    sudo "$0" "$@"
    exit $?
fi

export SERIAL_NUMBER=$(dmidecode -s system-serial-number)

modprobe usb_f_midi

mkdir cfg
mount none cfg -t configfs

mkdir cfg/usb_gadget/g2
cd cfg/usb_gadget/g2

echo 0x4000 > idProduct
echo 0x28DE > idVendor

mkdir configs/c.1

mkdir strings/0x409
mkdir configs/c.1/strings/0x409

echo "$SERIAL_NUMBER" > strings/0x409/serialnumber

echo "Valve" > strings/0x409/manufacturer
echo "Steam Deck MIDI" > strings/0x409/product

echo "Conf 1" > configs/c.1/strings/0x409/configuration

mkdir functions/midi.shs

ln -s functions/midi.shs configs/c.1

ls /sys/class/udc/ > UDC
