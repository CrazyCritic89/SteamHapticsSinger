steam-haptics-singer : main.cpp midifile/midifile.c rtmidi/RtMidi.cpp
	g++ -D__LINUX_ALSA__ -o steam-haptics-singer main.cpp midifile/midifile.c rtmidi/RtMidi.cpp -lasound -lpthread -fpermissive `pkg-config --libs --cflags libusb-1.0`
