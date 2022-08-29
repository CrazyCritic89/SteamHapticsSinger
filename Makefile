steam-haptics-singer : main.cpp midifile/midifile.c
	g++ -o steamcontrollersinger main.cpp midifile/midifile.c -fpermissive `pkg-config --libs --cflags libusb-1.0`
