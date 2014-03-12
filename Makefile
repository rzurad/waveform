# I don't know how to use Make, so this is probably horrible
waveform:
	gcc47 -I/usr/local/include/ffmpeg -L/usr/local/lib/ffmpeg -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -O3 -lavcodec -lavutil -lavformat -lpng -lm

debug:
	gcc47 -I/usr/local/include/ffmpeg -L/usr/local/lib/ffmpeg -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -lavcodec -lavutil -lavformat -lpng -lm

clean:
	rm -f waveform
