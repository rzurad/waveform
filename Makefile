waveform:
	gcc -I/usr/local/include/ffmpeg-devel -L/usr/local/lib/ffmpeg-devel -I/usr/local/include -L/usr/local/lib -o waveform2 newmain.c -Wall -g -O3 -lavcodec-devel -lavutil-devel -lavformat-devel -lpng
	gcc -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -O3 -lsndfile -lmpg123 -lz -lpng

clean:
	rm -f waveform
	rm -f waveform2
