waveform:
	gcc -I/usr/local/include -L/usr/local/lib -o waveform main.c -Wall -g -O3 -lsndfile -lmpg123 -lz -lpng

clean:
	rm -f waveform
