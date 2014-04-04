#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <png.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

typedef struct waveform_png {
    int width;
    int height;
    int quality;
    FILE *pPNGFile;
    png_structp png;
    png_infop png_info;
    png_bytep *pRows;
} WaveformPNG;

typedef struct audio_data {
    uint8_t *samples;
    int size;
    size_t bps;
    enum AVSampleFormat format;
    int channels;
} AudioData;

WaveformPNG init_png(const char *pOutFile, int width, int height) {
    WaveformPNG ret;

    ret.width = width;
    ret.height = height;
    ret.quality = 100;

    if (pOutFile) {
        ret.pPNGFile = fopen(pOutFile, "wb");
    } else {
        ret.pPNGFile = stdout;
    }

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

void write_png(WaveformPNG *pWaveformPNG) {
    png_write_info(pWaveformPNG->png, pWaveformPNG->png_info);
    png_write_image(pWaveformPNG->png, pWaveformPNG->pRows);
    png_write_end(pWaveformPNG->png, pWaveformPNG->png_info);
}

void close_png(WaveformPNG *pWaveformPNG) {
    png_destroy_write_struct(&(pWaveformPNG->png), &(pWaveformPNG->png_info));
    fclose(pWaveformPNG->pPNGFile);
}

void draw_png(WaveformPNG *png, AudioData *data) {
    int center_y = png->height / 2;
    int image_bound_y = png->height - 1;
    int c, i, x, y; //loop counters

    png_byte color_bg[4] = {0, 0, 0, 255};
    png_byte color_center[4] = {255, 255, 255, 255};
    png_byte color_outer[4] = {255, 255, 255, 255};
    png_bytep color_at_pixel = (png_bytep) malloc(sizeof(png_byte) * png->height * 4);

    //compute the foreground color at each y pixel
    for (y = 0; y < png->height; y++) {
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
    int sample_range = sample_max - sample_min;

    int sample_count = data->size / data->bps;
    int samples_per_pixel = sample_count / png->width;
    float average_multiplier = 1.0 / samples_per_pixel;
    float channel_average_multiplier = 1.0 / data->channels;

    // for each column of pixels in the final output image
    for (x = 0; x < png->width; ++x) {
        // find the average sample value, the minimum sample value, and the maximum
        // sample value within the the range of samples that fit within this column of pixels
        float average = 0;
        int min = sample_max;
        int max = sample_min;

        //for each "sample", which is really a sample for each channel,
        //reduce the samples * channels value to a single value that is
        //the average of the samples for each channel.
        for (i = 0; i < samples_per_pixel; ++i) {
            float value = 0;

            for (c = 0; c < data->channels; ++c) {
                switch (data->bps) {
                    case 1: value += data->samples[(x * samples_per_pixel) + i + c] * channel_average_multiplier;
                            break;
                    case 2: value += ((int16_t *) data->samples)[(x * samples_per_pixel) + i + c] * channel_average_multiplier;
                            break;
                    default:
                        fprintf(stderr, "Encountered bit depth of %i and freaked out.\n", (int) data->bps * 8);
                        exit(1);
                }
            }

            average += value * average_multiplier;

            if (value < min) {
                min = (int) value;
            }

            if (value > max) {
                max = (int) value;
            }
        }

        int y_min = (min - sample_min) * image_bound_y / sample_range;
        int y_max = (max - sample_min) * image_bound_y / sample_range;

        y = 0;

        // draw the top background
        for (; y < y_min; ++y) {
            memcpy(png->pRows[y] + x * 4, color_bg, 4);
        }

        // draw the waveform from the top to bottom
        for (; y <= y_max; ++y) {
            memcpy(png->pRows[y] + x * 4, color_at_pixel + 4 * y, 4);
        }

        // draw the bottom background
        for (; y < png->height; ++y) {
            memcpy(png->pRows[y] + x * 4, color_bg, 4);
        }
    }
}

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

int main(int argc, char *argv[]) {
    int c;
    int verbose = 0;
    int width = 256;
    int height = 64;
    const char *pFilePath = NULL;
    const char *pOutFile = NULL;

    if (argc < 1) {
        help();
    }

    // command line arg parsing
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

    // The FormatContext is the container for the audio file
    AVFormatContext *pFormatContext = NULL;
    AVCodecContext *pDecoderContext = NULL;
    AVCodec *pDecoder = NULL; // decoder for the audio stream
    int stream_index = 0; // which stream from the file we care about

    // open the audio file
    if (avformat_open_input(&pFormatContext, pFilePath, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        exit(1);
    }

    // Tell ffmpeg to read the file header and scan some of the data to determine
    // everything it can about the format of the file
    // `avformat_find_stream_info` is what writes out the max_analyze_duration warnings.
    if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        exit(1);
    }

    // find the audio stream we probably care about.
    // For audio files, there will probably only ever be one stream.
    // When it comes to video files, you'll probably have to regularly plan for it
    // if you're going to do anything with its audio.
    stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pDecoder, 0);

    if (stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in file\n");
        exit(1);
    }

    // get the decoder context (really just a wrapper struct for the decoder
    // struct that will convert the compressed audio data in to raw audio data
    pDecoderContext = pFormatContext->streams[stream_index]->codec;

    //TODO:  is this needed?
    av_opt_set_int(pDecoderContext, "refcounted_frames", 1, 0);

    // open the decoder for this audio stream
    if (avcodec_open2(pDecoderContext, pDecoder, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        exit(1);
    }

    /**
     * Try reading some frames
     */
    AVPacket packet; // Packets will contain compressed audio data
    AVFrame *pFrame = NULL; // Frames will contain the raw audio data retrieved from Packets via the Codec
    double duration = pFormatContext->duration / (double) AV_TIME_BASE; //how long in seconds is the audio file?
    int bit_rate = pFormatContext->bit_rate; // how many bits per second
    int is_planar = av_sample_fmt_is_planar(pDecoderContext->sample_fmt);
    int total_data_size = 0;

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
    // we are only dealing with an array of 8 bit integers. We'll deal with how the
    // actual data is stored later on.
    while (av_read_frame(pFormatContext, &packet) == 0) {
        // some audio formats might not contain an entire raw frame in a single compressed packet.
        // If this is the case, then decode_audio4 will tell us that it didn't get all of the
        // raw frame via this out argument.
        int frame_finished = 0;

        // Use the decoder to populate the raw frame with data from the compressed packet.
        if (avcodec_decode_audio4(pDecoderContext, pFrame, &frame_finished, &packet) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
            exit(1);
        }

        // did we get an entire raw frame from the packet?
        if (frame_finished) {
            // Find the size of all pFrame->extended_data in bytes. Remember, this will be:
            //      data_size = pFrame->nb_samples * pFrame->channels * bytes_per_sample
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
                //normalize all planes into the interleaved sample buffer
                int i = 0;
                int c = 0;

                for (; i < data_size / 2; i += data.bps) {
                    for (c = 0; c < 1; c++) {
                        memcpy(data.samples + total_data_size, pFrame->extended_data[c] + i, data.bps);
                        total_data_size += data.bps;
                    }
                }
            } else {
                memcpy(data.samples + total_data_size, pFrame->extended_data[0], data_size);
                total_data_size += data_size;
            }
        }

        // Packets must be freed, otherwise you'll have a fix a hole where the rain gets in
        // (and keep your mind from wandering...)
        av_free_packet(&packet);
    }

    data.size = total_data_size;

    WaveformPNG png = init_png(pOutFile, width, height);
    draw_png(&png, &data);
    write_png(&png);
    close_png(&png);

    /*
    if (verbose) {
        av_dump_format(pFormatContext, 0, pFilePath, 0);
        fprintf(stdout, "format: %s | ", av_get_sample_fmt_name(pDecoderContext->sample_fmt));
        fprintf(stdout, "filename: %s\n", pFilePath);
        fprintf(stdout, "sample_size: %i\n", (int) bytes_per_sample);
        fprintf(stdout, "is_planar: %i\n", av_sample_fmt_is_planar(pDecoderContext->sample_fmt));
        fprintf(stdout, "allocated_buffer_size: %i\n", allocated_buffer_size);
        fprintf(stdout, "sample rate: %i\n", pDecoderContext->sample_rate);
        fprintf(stdout, "channels: %i\n", channel_count);
        fprintf(stdout, "frame size: %i\n", pDecoderContext->frame_size);
        fprintf(stdout, "delay: %i\n", pDecoderContext->delay);
        fprintf(stdout, "bit rate: %i\n", pDecoderContext->bit_rate);
    }
    */

    // clean-up before exit
    avformat_close_input(&pFormatContext);
    avcodec_close(pDecoderContext);

    return 0;
}
