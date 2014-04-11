#!/usr/bin/env bash

if [ ! -f ../waveform ]
then
    echo "Run this from within the test directory after making the waveform file"
    exit
fi

# delete previous run's output
rm data/*.png

# inject the padding and the start of the array for the test page,
echo "injectWaveforms([" > images.js

# iterate through all files in the data/ folder and pass them
# into the waveform, generating a 1600x400 png file wth the same name,
# including the original audio file extension.
FILES=data/*
file=
for f in $FILES
do
    echo "generating waveforms for $f..."
    #../waveform -i "$f" -o "$f.png" -h 400 -w 1600
    ../waveform -i "$f" -o "$f.png" -t 400 -w 1600
    #../waveform -i "$f" -o "$f.png" -h 400 -w 1600 -m

    if [ $? -eq 0 ];
    then
        echo "'$f.png'," >> images.js
        file=$f
    else
        # tell the images.js file that this file failed by
        # tweaking the filename
        echo "'$f.png.FAILED'," >> images.js
    fi
done

# generate different sizes of thumbnails to show how the waveform changes
# with the quantization resolution
if [ ! -z $file ]
then
    echo "Generating WD thumbnail sizes"
    ../waveform -i "$file" -o "$file.TINY.png" -t 20 -w 80
    ../waveform -i "$file" -o "$file.SMALL.png" -t 45 -w 180
    ../waveform -i "$file" -o "$file.LARGE.png" -t 160 -w 640
    ../waveform -i "$file" -o "$file.MAX.png" -t 400 -w 1600
    echo "'$file.TINY.png'," >> images.js
    echo "'$file.SMALL.png'," >> images.js
    echo "'$file.LARGE.png'," >> images.js
    echo "'$file.MAX.png'," >> images.js
fi

# end the padding for the test page array
# leaving a trailing comma because why the hell are you viewing it 
# in a browser that can't ignore trailing commas in arrays?
echo "]);" >> images.js
