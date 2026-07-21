# hidapi ships as "hidapi-hidraw" on Linux, but plain "hidapi" on macOS and MSYS2
UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
	HIDAPI := hidapi-hidraw
else
	HIDAPI := hidapi
endif

steam-haptics-singer : main.cpp midifile/midifile.c
	g++ -o steam-haptics-singer main.cpp -x c midifile/midifile.c -fpermissive `pkg-config --libs --cflags libusb-1.0 $(HIDAPI)`
