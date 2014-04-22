#!/usr/bin/env bash

if [ ! -f ../waveform ]
then
    echo "Run this from within the test directory after making the waveform file"
    exit
fi

# delete previous run's output
rm data/*.png

all=$1
FILES=data/*
file=
result=

# executes the waveform program and dumps the exit code into `result`
#   Paremeter 1: the audio file to test
#   Parameter 2: the image file to output
#   Parameter 3: string of additional arguments to send to the waveform program
run () {
    # eval result=../waveform -i $1 -o $2 $3
    ../waveform -i "$1" -o "$2" $3

    if [ $? -eq 0 ];
    then
        # echo the location of the produced png into the images.js file
        echo "'$2'," >> images.js
        file=$1
    else
        # could not generate waveform.
        # echo the png file name, but append FAILED to it
        echo "'$2.FAILED'," >> images.js
    fi
}

# inject the padding and the start of the array for the test page,
echo "injectWaveforms([" > images.js

# iterate through all files in the data/ folder
for f in $FILES
do
    echo "generating waveforms for $f..."

    run "$f" "$f.max_height.png" "-h 800 -t 600 -w 1600"

    if [ "$all" == "all" ];
    then
        run "$f" "$f.monofied.png" "-h 600 -w 1600 -m"
        run "$f" "$f.png" "-t 400 -w 1600"
        run "$f" "$f.fixed.png" "-h 800 -w 1600"
    fi
done

# generate different sizes of thumbnails to show how the waveform changes
# with the quantization resolution
if [ ! -z $file ]
then
    echo "generating WD thumbnail sizes..."

    run "$file" "$file.TINY.png" "-h 40 -w 80"
    run "$file" "$file.SMALL.png" "-h 90 -w 180"
    run "$file" "$file.LARGE.png" "-h 320 -w 640"
    run "$file" "$file.MAX.png" "-h 800 -w 1600"
fi

# end the padding for the test page array
# leaving a trailing comma because why the hell are you viewing it 
# in a browser that can't ignore trailing commas in arrays?
echo "]);" >> images.js
