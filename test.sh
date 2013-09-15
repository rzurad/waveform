#!/usr/bin/env bash
FILES=test/*
for f in $FILES
do
    extension="${f##*.}"

    if [ "$extension" == "png" ];
    then
        continue;
    fi

    echo "converting $f..."
    #./waveform "$f" "$f.png" --verbose
    #ffmpeg -i "$f"
    ./waveform2 "$f"
    echo "================================================================"
done
