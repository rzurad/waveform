Audio waveform image generator
====

    ./waveform -i 1hz-10khz-sweep.flac -h 400 -w 1600 -m -c 89d1f3ff -b 474b50ff
![](test/examples/1hz-10khz-sweep.png)

    ./waveform -i dialup.wav -h 400 -w 1600 -c ffffffff -b 000000ff
![](test/examples/dialup.png)

How to use
===

**Name**

    waveform - generates a png image of the waveform of a given audio file.

**Synopsis**

    waveform [options]

**Description**

    Waveform uses ffmpeg and libpng to read an audio file and output a png
    image of the waveform representing the audio file's contents. Any audio
    container/codec combination that can be read by your build of ffmpeg
    should be supported.

    The fidelity of the produced waveform will be determined by the
    dimensions of the output png. Larger images will have more waveform
    detail than smaller images. To preserve waveform fidelity, you can
    have this program output a large image that is then rescaled using
    another program, such as ImageMagick.

    By default, the image will render a waveform for each channel of the
    audio file with the height of the image determined by the number of
    channels in the input file.

    Waveform can also be used to get accurate data about the given input file
    (more accurate than ffprobe can be depending on the input format) via
    the -d option.

**Options**

    -b HEX [default ffffffff]
            Set the background color of the image. Color is specified in hex
            format: RRGGBBAA or 0xRRGGBBAA.

    -c HEX [default 595959ff]
            Set the color of the waveform. Color is specified in hex format:
            RRGGBBAA or 0xRRGGBBAA

    -d
            Do not generate an image, but instead print out file metadata to
            standard out. This is mostly useful to find the actual duration
            of an input file, since ffprobe can occasionally be inaccurate in
            its prediction of duration.

    -h NUM
            Height of output image. The height of each channel will be
            constrained so that all channels can fit within the specified
            height.

            If used with the -t option, -h defines the maximum height the
            generated image can have.

            If all tracks can have a height of -t with the final image being
            below the height defined by -h, the output image will have a
            height of -t multiplied by the number of channels in the input
            file. If not, the output image will have a height of -h.

    -i FILE
            Input file to parse. Can be any format/codec that can be read by
            the installed ffmpeg.

    -o FILE
            Output file for PNG. If -o is omitted, the png will be written
            to stdout.

    -m
            Produce a single channel waveform. Each channel will be averaged
            together to produce the final channel. The -h and -t options
            behave as they would when supplied a monaural file.

    -t NUM [default 64]
            Height of each track in the output image. The final height of the
            output png will be this value multiplied by the number of channels
            in the audio stream.

            If you use the -t option together with the -h option, the final
            output will use -t if all tracks can fit within the height
            constraint defined by the -h option. If they can not, the track
            height will be adjusted to fit within the -h option.

    -w NUM [default 256]
            Width of output PNG image

Dependencies:
====

    ffmpeg 2.1.1
        - libavformat 55.19.104
        - libavcodec 55.39.101
        - libavutil 52.48.101
    libpng 1.5.18

Examples:
====

Fixed height per channel
-------
    ./waveform -i drumpan.wav -t 200 -w 1600 -b f3f3f3ff

This example takes a four-channel audio file of a drum beat panning between all channels and renders it giving each channel a dedicated 200 pixels of height.

![](test/examples/pan_t.png)

Reduce to monaural
----
    ./waveform -i drumpan.wav -h 400 -w 1600 -m -b f3f3f3ff

Here we take the same four-channel file from above and reduce all channels into a single waveform by averaging the values of all the samples across all channels.

![](test/examples/pan_m.png)

Fixed height image
----
    ./waveform -i drumpan.wav -h 600 -w 1600 -b f3f3f3ff

Again, the four-channel file, but rendered to constrain the entire output image to 600 pixels high, giving about 150 pixels of height to each channel.

![](test/examples/pan_h.png)

Specify max image height
----
If you're not sure how many channels will be in the input file and want to make sure that no image generated will exceed a certain height, you can combine the -t and -h options to achieve this. if there is enough height defined by -h to draw each channel with the height specified by -t, each channel will be -t tall with the overall image being less than -h. However, if there are more channels than would fit within -h, -h will be used and each channel will be scaled down to fit inside an image of height -h.

Calling waveform with the following options on a stereo file produces the following:

    ./waveform -i parachute.mp3 -h 800 -t 600 -w 1600 -b f3f3f3ff
![](test/examples/parachute.png)

Notice that since this was a stereo file and 600 * 2 > 800, the final image size is restricted to 800 pixels. However, if we make the same call supplying a mono mix of the same file, the output image has a height of 600, since 600 * 1 < 800.

    ./waveform -i parachute_mono.mp3 -h 800 -t 600 -w 1600 -b f3f3f3ff
![](test/examples/parachute_mono.png)

Print file info/metadata
----
Ffmpeg may sometimes not be able to accurately guess the duration of an input file for a variety of reasons, which can lead to some discrepencies if ffprobe's duration is used to gague how much time the output image represents. Since waveform needs to uncompress all samples to do its thing anyway, this allows it to accurately determine how much sample data is actually in the audio file, regardless of what its header or ffmpeg's prediction says.

For example, let's send a problematic mp3 into ffprobe to see its duration:

    ffprobe -i midnight_city.mp3

which outputs:

    [mp3 @ 0x80b02f420] Estimating duration from bitrate, this may be inaccurate
    Input #0, mp3, from 'midnight-city.mp3':
      Duration: 00:04:18.75, start: 0.000000, bitrate: 255 kb/s
        Stream #0:0: Audio: mp3, 44100 Hz, stereo, s16p, 256 kb/s

Notice that ffprobe reports the duration as 4 minutes, **18.75 seconds**. But if we tell ffmpeg to attempt a transcode, we see that this is not the real duration at all:

    ffmpeg -i midnight_city.mp3 -f null -

which outputs many things, but of primary interest is:

    [null @ 0x80b03ea20] Encoder did not produce proper pts, making some up.
    frame=    1 fps=0.0 q=0.0 Lsize=N/A time=00:04:02.72 bitrate=N/A

which reveals the actual duration of the file: 4 minutes, **2.72 seconds**. Since Waveform reads all of the raw data anyway, it is in a position to know that the actual duration is 4:02. Calling waveform with the -d option gives us this information:

    ./waveform -i test/data/midnight-city.mp3 -d
    [mp3 @ 0x807419420] Estimating duration from bitrate, this may be inaccurate
    [mp3 @ 0x807441120] Header missing
        Duration       : 242.729796 seconds
        Compression    : mp3
        Sample rate    : 44100 Hz
        Channels       : 2
        Bit rate       : 255999 b/s

Waveform shows a duration of 242.73 seconds, which comes out to 4 minutes, 2.73 seconds.
