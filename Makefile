steam-haptics-singer : main.cpp
	g++ -o steam-haptics-singer main.cpp -std=c++20 -DLIBREMIDI_ALSA=1 -DLIBREMIDI_HEADER_ONLY=1 -I ./include -lasound -pthread `pkg-config --libs --cflags libusb-1.0 hidapi-hidraw`
