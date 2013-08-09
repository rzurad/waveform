#!/usr/bin/env bash
FILES=test/*
for f in $FILES
do
    echo "converting $f..."
    ./waveform "$f" "$f.png"
done
