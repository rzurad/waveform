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
    ./waveform2 -i "$f" -o "$f.new.png" -v
    echo "================================================================"
done
