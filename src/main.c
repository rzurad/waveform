/******************************************************************************
 *                  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                          Version 2, December 2004
 *
 *       Copyright (C) 2014 Richard Zurad <rzurad@gmail.com>
 *
 *       Everyone is permitted to copy and distribute verbatim or modified
 *       copies of this license document, and changing it is allowed as long
 *       as the name is changed.
 *
 *                  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *         TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *        0. You just DO WHAT THE FUCK YOU WANT TO.
 *****************************************************************************/
/**
    C Program to draw png images of the waveform of a given audio file.
    Uses ffmpeg 2.1 to read the audio file and libpng to draw the image.

    Distributed under the WTFPL: http://www.wtfpl.net/faq/

    If you find this program useful or modify it, I encourage you to drop me a line.
    I'd love to hear about it :)

    http://github.com/rzurad/waveform
 */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <png.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>



// struct for creating PNG images.
typedef struct WaveformPNG {
    int width;
    int height;
    int quality;
    FILE *pPNGFile; // pointer to the file being written (or stdout)
    png_structp png; // internal struct for dealing with png images (libpng)
    png_infop png_info; // struct for information about the png file being generated (libpng)
    png_bytep *pRows; // pointer to all the rows of pixels in the image
} WaveformPNG;

// normalized version of the AVSampleFormat enum that doesn't care about planar vs interleaved
enum SampleFormat {
    SAMPLE_FORMAT_UINT8,
    SAMPLE_FORMAT_INT16,
    SAMPLE_FORMAT_INT32,
    SAMPLE_FORMAT_FLOAT,
    SAMPLE_FORMAT_DOUBLE
};

// struct to store the raw important data of an audio file pulled from ffmpeg
typedef struct AudioData {
    /*
     * The `samples` buffer is an interleaved buffer of all the raw samples from the audio file.
     *
     * Recall that audio data can be either planar (one buffer or "plane" for each channel) or
     * interleaved (one buffer for all channels: in a stereo file, the first sample for the left
     * channel is at index 0 with the first sample for the right channel at index 1, the second
     * sample for the left channel is at index 2 with the second sample for the right channel at
     * index 3, etc.).
     *
     * To make things easier, data read from ffmpeg is normalized to an interleaved buffer and
     * pointed to by `samples`.
     */
    uint8_t *samples;

    /*
     * The size of the `samples` buffer.
     */
    int size;

    /*
     * Number of bytes per sample. Use together with `size` and `format` to pull data from
     * the `samples` buffer
     */
    int sample_size;

    /*
     * Tells us the number format of the audio file
     */
    enum SampleFormat format;

    // how many channels does the audio file have? 1 (mono)? 2 (stereo)
    int channels;
} AudioData;



// initialize all the structs necessary to start writing png images with libpng
WaveformPNG init_png(const char *pOutFile, int width, int height) {
    WaveformPNG ret;

    ret.width = width;
    ret.height = height;
    ret.quality = 100;

    // default to stdout if no output file is given
    if (pOutFile) {
        ret.pPNGFile = fopen(pOutFile, "wb");
    } else {
        ret.pPNGFile = stdout;
    }

    // libpng homework
    ret.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    ret.png_info = png_create_info_struct(ret.png);

    png_init_io(ret.png, ret.pPNGFile);

    png_set_IHDR(
        ret.png,
        ret.png_info,
        ret.width,
        ret.height,
        8, //bit depth
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    //allocate memory for each row of pixels we will be drawing to
    ret.pRows = malloc(sizeof(png_bytep) * ret.height);

    int y = 0;
    for (; y < ret.height; ++y) {
        png_bytep row = malloc(sizeof(png_bytep) * ret.width);
        ret.pRows[y] = row;
    }

    return ret;
}



// write the data in the given WaveformPNG struct to an actual output file (or stdout)
void write_png(WaveformPNG *pWaveformPNG) {
    png_write_info(pWaveformPNG->png, pWaveformPNG->png_info);
    png_write_image(pWaveformPNG->png, pWaveformPNG->pRows);
    png_write_end(pWaveformPNG->png, pWaveformPNG->png_info);
}



// close and destroy all the png structs we were using to draw png images
void close_png(WaveformPNG *pWaveformPNG) {
    png_destroy_write_struct(&(pWaveformPNG->png), &(pWaveformPNG->png_info));
    fclose(pWaveformPNG->pPNGFile);
}



// take the given WaveformPNG struct and draw an audio waveform based on the data from
// the given AudioData struct. This function will combine all channels into a single waveform
// by averaging them.
void draw_waveform(WaveformPNG *png, AudioData *data) {
    png_byte color_bg[4] = {0, 0, 0, 255};
    png_byte color_waveform[4] = {255, 255, 255, 255};

    // figure out the min and max ranges of samples, based on bit depth and format
    int sample_min;
    int sample_max;

    // figure out the range of sample values we're dealing with
    switch (data->format) {
        case SAMPLE_FORMAT_FLOAT:
        case SAMPLE_FORMAT_DOUBLE:
            // floats and doubles have a range of -1.0 to 1.0
            // NOTE: It is entirely possible for a sample to go beyond this range. Any value outside
            // is considered beyond full volume. Be aware of this when doing math with sample values.
            sample_min = -1;
            sample_max = 1;

            break;
        case SAMPLE_FORMAT_UINT8:
            sample_min = 0;
            sample_max = 255;

            break;
        default:
            // we're dealing with integers, so the range of samples is going to be the min/max values
            // of signed integers of either 16 or 32 bit (24 bit formats get converted to 32 bit at
            // the AVFrame level):
            //  -32,768/32,767, or -2,147,483,648/2,147,483,647
            sample_min = pow(2, data->sample_size * 8) / -2;
            sample_max = pow(2, data->sample_size * 8) / 2 - 1;
    }

    uint32_t sample_range = sample_max - sample_min; // total range of values a sample can have
    int sample_count = data->size / data->sample_size; // samples per channel
    int samples_per_pixel = sample_count / png->width; // how many samples fit in a column of pixels?

    // for each column of pixels in the final output image
    int c;
    for (c = 0; c < data->channels; ++c) {
        //fprintf(stdout, "CHANNEL %i\n", c);
        int x;
        for (x = 0; x < png->width; ++x) {
            // find the average sample value, the minimum sample value, and the maximum
            // sample value within the the range of samples that fit within this column of pixels
            double min = sample_max;
            double max = sample_min;

            //for each "sample", which is really a sample for each channel,
            //reduce the samples * channels value to a single value that is
            //the average of the samples for each channel.
            int i;
            for (i = 0; i < samples_per_pixel; i++) {
                double value = 0;
                int index = x * samples_per_pixel + i + c;

                switch (data->format) {
                    case SAMPLE_FORMAT_UINT8:
                        value += data->samples[index];
                        break;
                    case SAMPLE_FORMAT_INT16:
                        value += ((int16_t *) data->samples)[index];
                        break;
                    case SAMPLE_FORMAT_INT32:
                        value += ((int32_t *) data->samples)[index];
                        break;
                    case SAMPLE_FORMAT_FLOAT:
                        value += ((float *) data->samples)[index];
                        break;
                    case SAMPLE_FORMAT_DOUBLE:
                        value += ((double *) data->samples)[index];
                        break;
                }

                // if the value is over or under the floating point range (which it perfectly fine
                // according to ffmpeg), we need to truncate it to still be within our range of
                // -1.0 to 1.0, otherwise some of our latter math will have a bad case of
                // the segfault sads.
                if (data->format == SAMPLE_FORMAT_DOUBLE || data->format == SAMPLE_FORMAT_FLOAT) {
                    if (value < -1.0) {
                        value = -1.0;
                    } else if (value > 1.0) {
                        value = 1.0;
                    }
                }

                if (value < min) {
                    min = value;
                }

                if (value > max) {
                    max = value;
                }
            }

            //fprintf(stdout, "min: %f, max: %f\n", min, max);
            int start_y = (png->height / data->channels) * c;
            int end_y = start_y + (png->height / data->channels) - 1;
            int channel_height = end_y - start_y;

            // calculate the y pixel values that represent the waveform for this column of pixels.
            // they are subtracted from last_y to flip the waveform image, putting positive
            // numbers above the center of waveform and negative numbers below.
            int y_max = start_y + channel_height - ((min - sample_min) * channel_height / sample_range);
            int y_min = start_y + channel_height - ((max - sample_min) * channel_height / sample_range);

            // start drawing from the bottom
            int y = end_y;

            // draw the bottom background
            for (; y > y_max; --y) {
                memcpy(png->pRows[y] + x * 4, color_bg, 4);
            }

            // draw the waveform from the bottom to top
            for (; y >= y_min; --y) {
                memcpy(png->pRows[y] + x * 4, color_waveform, 4);
            }

            // draw the top background
            for (; y >= start_y; --y) {
                memcpy(png->pRows[y] + x * 4, color_bg, 4);
            }
        }
    }
}



// take the given WaveformPNG struct and draw an audio waveform based on the data from
// the given AudioData struct. This function will combine all channels into a single waveform
// by averaging them.
void draw_combined_waveform(WaveformPNG *png, AudioData *data) {
    int last_y = png->height - 1; // count of pixels in height starting from 0

    png_byte color_bg[4] = {0, 0, 0, 255};
    png_byte color_waveform[4] = {255, 255, 255, 255};

    // figure out the min and max ranges of samples, based on bit depth and format
    int sample_min;
    int sample_max;

    // figure out the range of sample values we're dealing with
    switch (data->format) {
        case SAMPLE_FORMAT_FLOAT:
        case SAMPLE_FORMAT_DOUBLE:
            // floats and doubles have a range of -1.0 to 1.0
            // NOTE: It is entirely possible for a sample to go beyond this range. Any value outside
            // is considered beyond full volume. Be aware of this when doing math with sample values.
            sample_min = -1;
            sample_max = 1;

            break;
        case SAMPLE_FORMAT_UINT8:
            sample_min = 0;
            sample_max = 255;

            break;
        default:
            // we're dealing with integers, so the range of samples is going to be the min/max values
            // of signed integers of either 16 or 32 bit (24 bit formats get converted to 32 bit at
            // the AVFrame level):
            //  -32,768/32,767, or -2,147,483,648/2,147,483,647
            sample_min = pow(2, data->sample_size * 8) / -2;
            sample_max = pow(2, data->sample_size * 8) / 2 - 1;
    }

    uint32_t sample_range = sample_max - sample_min; // total range of values a sample can have
    int sample_count = data->size / data->sample_size; // how many samples are there total?
    int samples_per_pixel = sample_count / png->width; // how many samples fit in a column of pixels?

    // multipliers used to produce averages while iterating through samples.
    double channel_average_multiplier = 1.0 / data->channels;

    // for each column of pixels in the final output image
    int x;
    for (x = 0; x < png->width; ++x) {
        // find the average sample value, the minimum sample value, and the maximum
        // sample value within the the range of samples that fit within this column of pixels
        double min = sample_max;
        double max = sample_min;

        //for each "sample", which is really a sample for each channel,
        //reduce the samples * channels value to a single value that is
        //the average of the samples for each channel.
        int i;
        for (i = 0; i < samples_per_pixel; i += data->channels) {
            double value = 0;

            int c;
            for (c = 0; c < data->channels; ++c) {
                int index = x * samples_per_pixel + i + c;

                switch (data->format) {
                    case SAMPLE_FORMAT_UINT8:
                        value += data->samples[index] * channel_average_multiplier;
                        break;
                    case SAMPLE_FORMAT_INT16:
                        value += ((int16_t *) data->samples)[index] * channel_average_multiplier;
                        break;
                    case SAMPLE_FORMAT_INT32:
                        value += ((int32_t *) data->samples)[index] * channel_average_multiplier;
                        break;
                    case SAMPLE_FORMAT_FLOAT:
                        value += ((float *) data->samples)[index] * channel_average_multiplier;
                        break;
                    case SAMPLE_FORMAT_DOUBLE:
                        value += ((double *) data->samples)[index] * channel_average_multiplier;
                        break;
                }
            }

            // if the value is over or under the floating point range (which it perfectly fine
            // according to ffmpeg), we need to truncate it to still be within our range of
            // -1.0 to 1.0, otherwise some of our latter math will have a bad case of
            // the segfault sads.
            if (data->format == SAMPLE_FORMAT_DOUBLE || data->format == SAMPLE_FORMAT_FLOAT) {
                if (value < -1.0) {
                    value = -1.0;
                } else if (value > 1.0) {
                    value = 1.0;
                }
            }

            if (value < min) {
                min = value;
            }

            if (value > max) {
                max = value;
            }
        }

        // calculate the y pixel values that represent the waveform for this column of pixels.
        // they are subtracted from last_y to flip the waveform image, putting positive
        // numbers above the center of waveform and negative numbers below.
        int y_max = last_y - ((min - sample_min) * last_y / sample_range);
        int y_min = last_y - ((max - sample_min) * last_y / sample_range);

        // start drawing from the bottom
        int y = last_y;

        // draw the bottom background
        for (; y > y_max; --y) {
            memcpy(png->pRows[y] + x * 4, color_bg, 4);
        }

        // draw the waveform from the bottom to top
        for (; y >= y_min; --y) {
            memcpy(png->pRows[y] + x * 4, color_waveform, 4);
        }

        // draw the top background
        for (; y >= 0; --y) {
            memcpy(png->pRows[y] + x * 4, color_bg, 4);
        }
    }
}



// print out help text saying how to use this program and exit
void help() {
    fprintf(stdout, "\n\nGenerate waveform images from audio files.\n\n");
    fprintf(stdout, "   -i [input audio file]       [REQUIRED] Path to input audio file to process.\n");
    fprintf(stdout, "   -o [output png file]        Output PNG file. Will default to stdout if not specified\n");
    fprintf(stdout, "   -w [width]                  The desired width of the output PNG. Defaults to 256\n");
    fprintf(stdout, "   -h [height]                 The desired height of the output PNG. Defaults to 64\n\n");
    exit(1);
}



// Takes a given AVFormatContext and AVCodecContext from ffmpeg, extracts the raw sample
// information and dumps it into the returned AudioData struct
AudioData *get_audio_data(AVFormatContext *pFormatContext, AVCodecContext *pDecoderContext) {
    // Packets will contain chucks of compressed audio data read from the audio file.
    AVPacket packet;

    // Frames will contain the raw uncompressed audio data read from a packet
    AVFrame *pFrame = NULL;

    // how long in seconds is the audio file?
    double duration = pFormatContext->duration / (double) AV_TIME_BASE;

    // is the audio interleaved or planar?
    int is_planar = av_sample_fmt_is_planar(pDecoderContext->sample_fmt);

    // running total of how much data has been converted to raw and copied into the AudioData
    // `samples` buffer. This will eventually be `data->size`
    int total_size = 0;

    // Make the AudioData object we'll be returning
    AudioData *data = malloc(sizeof(AudioData));
    data->format = pDecoderContext->sample_fmt;
    data->sample_size = (int) av_get_bytes_per_sample(pDecoderContext->sample_fmt); // *byte* depth
    data->channels = pDecoderContext->channels;

    // normalize the sample format to an enum that's less verbose than AVSampleFormat.
    // We won't care about planar/interleaved
    switch (pDecoderContext->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            data->format = SAMPLE_FORMAT_UINT8;
            break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            data->format = SAMPLE_FORMAT_INT16;
            break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            data->format = SAMPLE_FORMAT_INT32;
            break;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            data->format = SAMPLE_FORMAT_FLOAT;
            break;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            data->format = SAMPLE_FORMAT_DOUBLE;
            break;
        default:
            fprintf(stderr, "Bad format: %s\n", av_get_sample_fmt_name(pDecoderContext->sample_fmt));
            return NULL;
    }

    // guess how much memory we'll need for samples.
    int allocated_buffer_size = (pFormatContext->bit_rate / 8) * duration;

    data->samples = malloc(sizeof(uint8_t) * allocated_buffer_size);
    av_init_packet(&packet);

    if (!(pFrame = avcodec_alloc_frame())) {
        fprintf(stderr, "Could not allocate AVFrame\n");
        return NULL;
    }

    // Loop through the entire audio file by reading a compressed packet of the stream
    // into the uncomrpressed frame struct and copy it into a buffer.
    // It's important to remember that in this context, even though the actual format might
    // be 16 bit or 24 bit or float with x number of channels, while we're copying things,
    // we are only dealing with an array of 8 bit integers. 
    //
    // It's up to anything using the AudioData struct to know how to properly read the data
    // inside `samples`
    while (av_read_frame(pFormatContext, &packet) == 0) {
        // some audio formats might not contain an entire raw frame in a single compressed packet.
        // If this is the case, then decode_audio4 will tell us that it didn't get all of the
        // raw frame via this out argument.
        int frame_finished = 0;

        // Use the decoder to populate the raw frame with data from the compressed packet.
        if (avcodec_decode_audio4(pDecoderContext, pFrame, &frame_finished, &packet) < 0) {
            // unable to decode this packet. continue on to the next packet
            continue;
        }

        // did we get an entire raw frame from the packet?
        if (frame_finished) {
            // Find the size of all pFrame->extended_data in bytes. Remember, this will be:
            // data_size = pFrame->nb_samples * pFrame->channels * bytes_per_sample
            int data_size = av_samples_get_buffer_size(
                is_planar ? &pFrame->linesize[0] : NULL,
                data->channels,
                pFrame->nb_samples,
                data->format,
                1
            );

            // if we don't have enough space in our copy buffer, expand it
            if (total_size + data_size > allocated_buffer_size) {
                allocated_buffer_size = allocated_buffer_size * 1.25;
                data->samples = realloc(data->samples, allocated_buffer_size);
            }

            if (is_planar) {
                // normalize all planes into the interleaved sample buffer
                int i = 0;
                int c = 0;

                // data_size is total data overall for all planes.
                // iterate through extended_data and copy each sample into `samples` while
                // interleaving each channel (copy sample one from left, then right. copy sample
                // two from left, then right, etc.)
                for (; i < data_size / data->channels; i += data->sample_size) {
                    for (c = 0; c < data->channels; c++) {
                        memcpy(data->samples + total_size, pFrame->extended_data[c] + i, data->sample_size);
                        total_size += data->sample_size;
                    }
                }
            } else {
                // source file is already interleaved. just copy the raw data from the frame into
                // the `samples` buffer.
                memcpy(data->samples + total_size, pFrame->extended_data[0], data_size);
                total_size += data_size;
            }
        }

        // Packets must be freed, otherwise you'll have a fix a hole where the rain gets in
        // (and keep your mind from wandering...)
        av_free_packet(&packet);
    }

    if (total_size == 0) {
        // not a single packet could be read.
        return NULL;
    }

    /*
    int q = 0;
    for (; q < total_size / 4; q++) {
        fprintf(stdout, "%i, %i: %i\n", q, q % 8, ((int32_t *) data->samples)[q]);
    }
    */

    data->size = total_size;

    return data;
}



// close and free ffmpeg structs
void cleanup(AVFormatContext *pFormatContext, AVCodecContext *pDecoderContext) {
    avformat_close_input(&pFormatContext);
    avcodec_close(pDecoderContext);
}



int main(int argc, char *argv[]) {
    int width = 256; // default width of the generated png image
    int height = 64; // default height of the generated png image
    int monofy = 0; // should we reduce everything into one waveform
    int fixed_height = 0; // should the height of the image be constrained to the -h argument
    const char *pFilePath = NULL; // audio input file path
    const char *pOutFile = NULL; // image output file path. `NULL` means stdout

    if (argc < 1) {
        help();
    }

    // command line arg parsing
    int c;
    while ((c = getopt(argc, argv, "i:o:vmw:h:t:")) != -1) {
        switch (c) {
            case 'i': pFilePath = optarg; break;
            case 'm': monofy = 1; break;
            case 'o': pOutFile = optarg; break;
            case 'w': width = atol(optarg); break;
            case 't': height = atol(optarg); break;
            case 'h':
                height = atol(optarg);
                fixed_height = 1;
                break;
            default:
                fprintf(stderr, "WARNING: Don't know what to do with argument %c\n", (char) c);
                help();
        }
    }

    if (!pFilePath) {
        fprintf(stderr, "ERROR: Please provide an input file through argument -i\n");
        help();
    }

    // We could be fed any number of types of audio containers with any number of
    // encodings. These functions tell ffmpeg to load every library it knows about.
    // This way we don't need to explicity tell ffmpeg which libraries to load.
    //
    // Register all availible muxers/demuxers/protocols. We could be fed anything.
    av_register_all(); 

    // register all codecs/parsers/bitstream-filters
    avcodec_register_all();

    AVFormatContext *pFormatContext = NULL; // Container for the audio file
    AVCodecContext *pDecoderContext = NULL; // Container for the stream's codec
    AVCodec *pDecoder = NULL; // actual codec for the stream
    int stream_index = 0; // which audio stream should be looked at

    // open the audio file
    if (avformat_open_input(&pFormatContext, pFilePath, NULL, NULL) < 0) {
        fprintf(stderr, "Cannot open input file.\n");
        goto ERROR;
    }

    // Tell ffmpeg to read the file header and scan some of the data to determine
    // everything it can about the format of the file
    if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
        fprintf(stderr, "Cannot find stream information.\n");
        goto ERROR;
    }

    // find the audio stream we probably care about.
    // For audio files, there will most likely be only one stream.
    stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pDecoder, 0);

    if (stream_index < 0) {
        fprintf(stderr, "Unable to find audio stream in file.\n");
        goto ERROR;
    }

    // now that we have a stream, get the codec for the given stream
    pDecoderContext = pFormatContext->streams[stream_index]->codec;

    // open the decoder for this audio stream
    if (avcodec_open2(pDecoderContext, pDecoder, NULL) < 0) {
        fprintf(stderr, "Cannot open audio decoder.\n");
        goto ERROR;
    }

    // we have a stream that looks valid, so get all the raw samples from it
    AudioData *data = get_audio_data(pFormatContext, pDecoderContext);
    if (data == NULL) {
        goto ERROR;
    }

    // if the height is not specified to be fixed (`t` option instead of `h`), adjust the
    // height of the image based on the number of channels we found in the audio stream
    if (!fixed_height && !monofy) {
        height = data->channels * height;
    }

    // init the png struct so we can start drawing
    WaveformPNG png = init_png(pOutFile, width, height);

    if (monofy) {
        // if specified, call the drawing function that reduces all channels into a single
        // waveform

        draw_combined_waveform(&png, data);
    } else {
        // otherwise, draw them all stacked individually
        draw_waveform(&png, data);
    }

    write_png(&png);
    close_png(&png);

    cleanup(pFormatContext, pDecoderContext);
    return 0;

ERROR:
    cleanup(pFormatContext, pDecoderContext);
    return 1;
}
