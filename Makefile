ifeq ($(OS),Windows_NT)
	CFLAGS = -std=c++20 -DLIBREMIDI_WINMM=1 -DLIBREMIDI_HEADER_ONLY=1 -I ./include -lwinmm -pthread `pkg-config --libs --cflags libusb-1.0 hidapi`
else
	UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
	CFLAGS = -std=c++20 -DLIBREMIDI_ALSA=1 -DLIBREMIDI_HEADER_ONLY=1 -I ./include -lasound -pthread `pkg-config --libs --cflags libusb-1.0 hidapi-hidraw`
	else
	$(error Can not build for this OS, or OS could not be detected!)
	endif
endif

steam-haptics-singer : main.cpp
	g++ -o steam-haptics-singer main.cpp $(CFLAGS)
