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
     *
     * NOTE: This is NOT the size in 8 bit ints. The buffer itself is defined as pointers to uint8_t,
     * but the contents can actually be any number type specified by AVSampleFormat. The actual
     * `sizeof` each sample is determined by the `bps` property. Use `size` together with
     * `bps` and `format` to determine how to pull samples out of the `samples` buffer---`bps` telling
     * use the size of the pointer to use (1, 2, or 4 bytes) and `format` telling you the type
     * of number to use (int, float, double)
     */
    int size;

    /*
     * Number of bytes per sample. Use together with `size` and `format` to pull data from
     * the `samples` buffer
     */
    size_t bps;

    /*
     * Tells us the format of the audio file as identified by ffmpeg:
     * https://www.ffmpeg.org/doxygen/2.1/samplefmt_8h.html#af9a51ca15301871723577c730b5865c5
     *
     * TODO: AVSampleFormat contains different values for planar vs interleaved, but when
     * we actually care about the `format` property, we have already normalized to interleaved,
     * meaning the planar options of AVSampleFormat are redundant and just add cruft to branching
     * logic. See about refactoring to a planar/interleaved agnostic enum to identify format
     */
    enum AVSampleFormat format;

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
    ret.pRows = (png_bytep *) malloc(sizeof(png_bytep) * ret.height);

    int y = 0;
    for (; y < ret.height; ++y) {
        png_bytep row = (png_bytep) malloc(ret.width * 4);
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
// the given AudioData struct
void draw_png(WaveformPNG *png, AudioData *data) {
    int center_y = png->height / 2;
    int image_bound_y = png->height - 1;

    png_byte color_bg[4] = {0, 0, 0, 255};
    png_byte color_center[4] = {255, 255, 255, 255};
    png_byte color_outer[4] = {255, 255, 255, 255};
    png_bytep color_at_pixel = (png_bytep) malloc(sizeof(png_byte) * png->height * 4);

    //compute the foreground color at each y pixel
    int y;
    for (y = 0; y < png->height; y++) {
        int i;
        for (i = 0; i < 4; i++) {
            float fAmount = abs(y - center_y) / (float) center_y;

            color_at_pixel[4 * y + i] = (1 - fAmount) * color_center[i] + fAmount * color_outer[i];
        }
    }

    // figure out the min and max ranges of samples, based on bit depth.
    // these come out to be the min/max values of various signed number sizes:
    // -128/127, -32,768/32,767, etc
    int sample_min = pow(2, data->bps * 8) / -2;
    int sample_max = pow(2, data->bps * 8) / 2 - 1;

    // unless we're dealing with floats, then we're dealing with ranges from 1.0 to -1.0
    if (data->format == AV_SAMPLE_FMT_FLT || data->format == AV_SAMPLE_FMT_FLTP) {
        sample_min = -1;
        sample_max = 1;
    }

    int sample_range = sample_max - sample_min;

    int sample_count = data->size / data->bps;
    int samples_per_pixel = sample_count / png->width;
    double average_multiplier = 1.0 / samples_per_pixel;
    double channel_average_multiplier = 1.0 / data->channels;

    // for each column of pixels in the final output image
    int x;
    for (x = 0; x < png->width; ++x) {
        // find the average sample value, the minimum sample value, and the maximum
        // sample value within the the range of samples that fit within this column of pixels
        double average = 0;
        double min = sample_max;
        double max = sample_min;

        //for each "sample", which is really a sample for each channel,
        //reduce the samples * channels value to a single value that is
        //the average of the samples for each channel.
        int i;
        for (i = 0; i < samples_per_pixel; ++i) {
            double value = 0;

            int c;
            for (c = 0; c < data->channels; ++c) {
                switch (data->format) {
                    case AV_SAMPLE_FMT_U8:
                    case AV_SAMPLE_FMT_U8P:
                        value += data->samples[(x * samples_per_pixel) + i + c] * channel_average_multiplier;
                        break;
                    case AV_SAMPLE_FMT_S16:
                    case AV_SAMPLE_FMT_S16P:
                        value += ((int16_t *) data->samples)[(x * samples_per_pixel) + i + c] * channel_average_multiplier;
                        break;
                    case AV_SAMPLE_FMT_S32:
                    case AV_SAMPLE_FMT_S32P:
                        value += ((int32_t *) data->samples)[(x * samples_per_pixel) + i + c] * channel_average_multiplier;
                        break;
                    case AV_SAMPLE_FMT_FLT:
                    case AV_SAMPLE_FMT_FLTP:
                        value += ((float *) data->samples)[(x * samples_per_pixel) + i + c] * channel_average_multiplier;

                        if (value < -1.0) {
                            value = -1.0;
                        }

                        if (value > 1.0) {
                            value = 1.0;
                        }

                        break;
                    case AV_SAMPLE_FMT_DBL:
                    case AV_SAMPLE_FMT_DBLP:
                        value += ((double *) data->samples)[(x * samples_per_pixel) + i + c] * channel_average_multiplier;

                        if (value < -1.0) {
                            value = -1.0;
                        }

                        if (value > 1.0) {
                            value = 1.0;
                        }

                        break;
                    default:
                        fprintf(stderr, "Encountered float/double format and freaked out.\n");
                        exit(1);
                }
            }

            average += value * average_multiplier;

            if (value < min) {
                min = value;
            }

            if (value > max) {
                max = value;
            }
        }

        // calculate the y pixel values that represent the waveform for this column of pixels.
        // they are subtracted from image_bound_y to flip the waveform image, putting positive
        // numbers above the center of waveform and negative numbers below.
        int y_max = image_bound_y - ((min - sample_min) * image_bound_y / sample_range);
        int y_min = image_bound_y - ((max - sample_min) * image_bound_y / sample_range);

        y = image_bound_y;

        // draw the bottom background
        for (; y > y_max; --y) {
            memcpy(png->pRows[y] + x * 4, color_bg, 4);
        }

        // draw the waveform from the bottom to top
        for (; y >= y_min; --y) {
            memcpy(png->pRows[y] + x * 4, color_at_pixel + 4 * y, 4);
        }

        // draw the bottom background
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
    fprintf(stdout, "   -v                          Enable verbose mode. Don't expect valid PNGs\n");
    fprintf(stdout, "                               in stdout when this is enabled.\n");
    fprintf(stdout, "   -w [width]                  The desired width of the output PNG. Defaults to 256\n");
    fprintf(stdout, "   -h [height]                 The desired height of the output PNG. Defaults to 64\n\n");
    exit(1);
}



// Takes a given AVFormatContext and AVCodecContext from ffmpeg, extracts the raw sample
// information and dumps it into the returned AudioData struct
AudioData get_audio_data(AVFormatContext *pFormatContext, AVCodecContext *pDecoderContext) {
    // Packets will contain chucks of compressed audio data read from the audio file.
    AVPacket packet;

    // Frames will contain the raw uncompressed audio data from a given Packet
    AVFrame *pFrame = NULL;

    // how long in seconds is the audio file?
    double duration = pFormatContext->duration / (double) AV_TIME_BASE;

    // how many bits per second is the audio file?
    // TODO: bit_rate is only used to guess the buffer size, and it's sometimes a bad guess.
    // Perhaps find a more accurate way that doesn't rely on bit_rate?
    int bit_rate = pFormatContext->bit_rate;
    int is_planar = av_sample_fmt_is_planar(pDecoderContext->sample_fmt);

    // running total of how much data has been converted to raw and copied into the AudioData
    // `samples` buffer. This will eventually be `data->size`
    int total_data_size = 0;

    // Make the AudioData object we'll be returning
    AudioData data;
    data.format = pDecoderContext->sample_fmt;
    data.bps = av_get_bytes_per_sample(pDecoderContext->sample_fmt); // *byte* depth
    data.channels = pDecoderContext->channels;

    // guess how much memory we'll need for samples
    int allocated_buffer_size = bit_rate * (int) ceil(duration) * data.bps * data.channels;
    data.samples = malloc(sizeof(uint8_t) * allocated_buffer_size);
    av_init_packet(&packet);

    if (!(pFrame = avcodec_alloc_frame())) {
        avcodec_close(pDecoderContext);
        avformat_close_input(&pFormatContext);
        exit(1);
    }

    // Loop through the entire audio file by reading a compressed frame of the stream
    // into the packet struct and copy it into a buffer.
    // It's important to remember that in this context, even though the actual format might
    // be 16 bit or 24 bit with x number of channels, while we're copying things,
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
            av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
        }

        // did we get an entire raw frame from the packet?
        if (frame_finished) {
            // Find the size of all pFrame->extended_data in bytes. Remember, this will be:
            // data_size = pFrame->nb_samples * pFrame->channels * bytes_per_sample
            int data_size = av_samples_get_buffer_size(
                is_planar ? &pFrame->linesize[0] : NULL,
                data.channels,
                pFrame->nb_samples,
                data.format,
                1
            );

            // if we don't have enough space in our copy buffer, expand it
            if (total_data_size + data_size > allocated_buffer_size) {
                allocated_buffer_size = allocated_buffer_size * 1.25;
                data.samples = realloc(data.samples, allocated_buffer_size);
            }

            if (is_planar) {
                // normalize all planes into the interleaved sample buffer
                int i = 0;
                int c = 0;

                // data_size is total data overall for all planes.
                // iterate through extended_data and copy each sample into `samples` while
                // interleaving each channel (copy sample one from left, then right. copy sample
                // two from left, then right, etc.)
                for (; i < data_size / data->channels; i += data.bps) {
                    // we only care about the first two channels.

                    // TODO: This means data->channels lies. The source file may have had 5 channels,
                    // but `samples` will only contain the first two. (CAUTION: DRAGONS!)
                    // FIX THIS so that all the data is there. Let the drawing algorithm decide
                    // to ignore all but the first two channels
                    for (c = 0; c < 1; c++) {
                        memcpy(data.samples + total_data_size, pFrame->extended_data[c] + i, data.bps);
                        total_data_size += data.bps;
                    }
                }
            } else {
                // source file is already interleaved. just copy the raw data from the frame into
                // the `samples` buffer.
                memcpy(data.samples + total_data_size, pFrame->extended_data[0], data_size);
                total_data_size += data_size;
            }
        }

        // Packets must be freed, otherwise you'll have a fix a hole where the rain gets in
        // (and keep your mind from wandering...)
        av_free_packet(&packet);
    }

    if (total_data_size == 0) {
        // not a single packet could be read. Quit.
        fprintf(stderr, "Did not read any audio data.\n");
        exit(1);
    }

    data.size = total_data_size;

    return data;
}



int main(int argc, char *argv[]) {
    int verbose = 0; // print out ffmpeg file information?
    int width = 256; // default width of the generated png image
    int height = 64; // default height of the generated png image
    const char *pFilePath = NULL; // audio input file path
    const char *pOutFile = NULL; // image output file path. `NULL` means stdout

    if (argc < 1) {
        help();
    }

    // command line arg parsing
    int c;
    while ((c = getopt(argc, argv, "i:o:vw:h:")) != -1) {
        switch (c) {
            case 'i': pFilePath = optarg; break;
            case 'o': pOutFile = optarg; break;
            case 'v': verbose = 1; break;
            case 'w': width = atol(optarg); break;
            case 'h': height = atol(optarg); break;
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
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        exit(1);
    }

    // Tell ffmpeg to read the file header and scan some of the data to determine
    // everything it can about the format of the file
    if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        exit(1);
    }

    // find the audio stream we probably care about.
    // For audio files, there will most likely be only one stream.
    stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pDecoder, 0);

    if (stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in file\n");
        exit(1);
    }

    // now that we have a stream, get the codec for the given stream
    pDecoderContext = pFormatContext->streams[stream_index]->codec;

    //TODO:  is this needed?
    av_opt_set_int(pDecoderContext, "refcounted_frames", 1, 0);

    // open the decoder for this audio stream
    if (avcodec_open2(pDecoderContext, pDecoder, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        exit(1);
    }

    // we have a stream that looks valid, so get all the raw samples from it
    AudioData data = get_audio_data(pFormatContext, pDecoderContext);

    // initialize the png we'll be drawing
    WaveformPNG png = init_png(pOutFile, width, height);

    draw_png(&png, &data);
    write_png(&png);
    close_png(&png);

    if (verbose) {
        av_dump_format(pFormatContext, 0, pFilePath, 0);
    }

    // clean-up before exit
    avformat_close_input(&pFormatContext);
    avcodec_close(pDecoderContext);

    return 0;
}
