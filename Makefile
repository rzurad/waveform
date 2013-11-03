# I don't know how to use Make, so this is probably horrible
waveform:
	gcc -I/usr/local/include/ffmpeg-devel -L/usr/local/lib/ffmpeg-devel -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -O3 -lavcodec-devel -lavutil-devel -lavformat-devel -lpng

debug:
	gcc -I/usr/local/include/ffmpeg-devel -L/usr/local/lib/ffmpeg-devel -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -lavcodec-devel -lavutil-devel -lavformat-devel -lpng

clean:
	rm -f waveform
