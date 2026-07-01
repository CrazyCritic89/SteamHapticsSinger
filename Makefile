MP3_SRCS = midifile/mp3/Mp3Decoder.cpp \
           midifile/mp3/PitchDetector.cpp \
           midifile/mp3/MidiExporter.cpp \
           midifile/mp3/Mp3ToMidi.cpp \
           midifile/mp3/Yin.cpp \
           midifile/mp3/Pyin.cpp \
           midifile/mp3/Mpm.cpp \
           midifile/mp3/Autocorr.cpp \
           midifile/mp3/Amdf.cpp \
           midifile/mp3/Hps.cpp \
           midifile/mp3/Cepstrum.cpp \
           midifile/mp3/FftPeak.cpp \
           midifile/mp3/Cqt.cpp \
           midifile/mp3/Stft.cpp

# Main build — MP3 pipeline is compiled directly into steam-haptics-singer
steam-haptics-singer : midifile/mp3/minimp3.h main.cpp midifile/midifile.c $(MP3_SRCS)
	g++ -o steam-haptics-singer main.cpp midifile/midifile.c $(MP3_SRCS) \
	    -fpermissive -std=c++14 \
	    `pkg-config --libs --cflags libusb-1.0 hidapi-hidraw`

# Download minimp3 header if not already present (required by Mp3Decoder.cpp)
midifile/mp3/minimp3.h :
	@echo "Downloading minimp3.h..."
	@curl -fsSL -o midifile/mp3/minimp3.h \
	    https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h \
	  || wget -q -O midifile/mp3/minimp3.h \
	    https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h \
	  || (echo "ERROR: Cannot download minimp3.h. Get it from https://github.com/lieff/minimp3" \
	      && exit 1)

.PHONY : all clean
all : steam-haptics-singer

clean :
	rm -f steam-haptics-singer
