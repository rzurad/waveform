#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <png.h>
#include <stdio.h>

void die(const char* message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

typedef struct waveform_png {
    int width;
    int height;
    int quality;
    FILE *pPNGFile;
    png_structp png;
    png_infop png_info;
    png_bytep *pRows;
} WaveformPNG;

WaveformPNG init_png(char *pFilePath) {
    WaveformPNG ret;

    ret.width = 256;
    ret.height = 256;
    ret.quality = 100;

    if (pFilePath) {
        ret.pPNGFile = fopen(pFilePath, "wb");
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
    int y;

    for (y = 0; y < ret.height; ++y) {
        png_bytep row = (png_bytep) malloc(ret.width * 4);
        ret.pRows[y] = row;
    }

    return ret;
}

void write_and_close_png(WaveformPNG *pWaveformPNG) {
    png_write_info(pWaveformPNG->png, pWaveformPNG->png_info);
    png_write_image(pWaveformPNG->png, pWaveformPNG->pRows);
    png_write_end(pWaveformPNG->png, pWaveformPNG->png_info);

    png_destroy_write_struct(&(pWaveformPNG->png), &(pWaveformPNG->png_info));
    fclose(pWaveformPNG->pPNGFile);
}

// 8bit_index = channel_count * byte_depth * 16bit_index
// 
uint8_t extract_sample_8bit(int index, uint8_t *samples, int channel_count) {
    uint8_t raw = 0;
    float channel_average_multiplier = 1 / (float) channel_count;
    int c;

    for (c = 0; c < channel_count; ++c) {
        raw += samples[index + c] * channel_average_multiplier;
    }

    return raw;
}

uint16_t extract_sample_16bit(int index, uint8_t *samples, int channel_count) {
    uint16_t raw = 0;
    float channel_average_multiplier = 1 / (float) channel_count;
    int c;

    for (c = 0; c < channel_count; ++c) {
        raw += ((uint16_t *) samples)[index + c] * channel_average_multiplier;
    }

    return raw;
}

void draw_png(WaveformPNG *png,
              uint8_t *samples,
              int data_size, //the length of samples
              size_t bytes_per_sample,
              int channel_count
        ) {

    int center_y = png->height / 2;
    int sample_count = data_size / bytes_per_sample;
    int samples_per_pixel = sample_count / png->width;
    int i, x, y; //loop counters

    png_byte color_bg[4] = {0, 0, 0, 0};
    png_byte color_center[4] = {0, 0, 0, 255};
    png_byte color_outer[4] = {0, 0, 0, 255};
    png_bytep color_at_pixel = (png_bytep) malloc(sizeof(png_byte) * png->height * 4);

    //compute the foreground color at each y pixel
    for (y = 0; y < png->height; y++) {
        for (i = 0; i < 4; i++) {
            float fAmount = abs(y - center_y) / (float) center_y;

            color_at_pixel[4 * y + i] = (1 - fAmount) * color_center[i] + fAmount * color_outer[i];
        }
    }

    // for each column of pixels in the final output image
    for (x = 0; x < png->width; ++x) {
        float average = 0;
        float average_multiplier = 1.0 / png->width;
        int min = 0;
        int max = 0;

        //for each "sample", which is really a sample for each channel,
        //reduce the samples * channels value to a single value that is
        //the average of the samples for each channel.
        for (i = 0; i < samples_per_pixel; ++i) {
            int value = 0;

            switch (bytes_per_sample) {
                // 8-bit depth
                case 1:
                    value = (int) extract_sample_8bit(i, samples, channel_count);
                    break;

                // 16-bit depth
                case 2:
                    value = (int) extract_sample_16bit(i, samples, channel_count);
                    //fprintf(stdout, "value: %i\n", value);
                    break;

                // 24-bit depth
                default:
                    fprintf(
                        stderr,
                        "Encountered file with %i-bit depth. I don't know what to do.",
                        (int) bytes_per_sample * 8
                    );
            }

            if (value < min) min = value;
            if (value > max) max = value;

            average += value * average_multiplier;
        }

        fprintf(stdout, "Pixel Column %i: min %i, max %i, average %f\n", x, min, max, average);
    }
    /*
    int image_bound_y = image_height - 1;
    float channel_count_multiplier = 1 / (float) channel_count;

    //range of frames that fit in this pixel
    int start = 0;
    int mstart = 0;
    int mstart_delta = frames_per_pixel * channel_count;

    int x;
    for (x = 0; x < image_width; ++x, start += frames_per_pixel, mstart += mstart_delta) {
        //TODO: DRAW SHIT
    }
    */
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        die("Please provide the file path as the first argument");
    }

    // We could be fed any number of types of audio containers with any number of
    // encodings. These functions tell ffmpeg to load every library it knows about.
    // This way we don't need to explicity tell ffmpeg which libraries to load.

    // Register all availible muxers/demuxers/protocols. We could be fed any
    // http://ffmpeg.org/doxygen/trunk/group__lavf__core.html#ga917265caec45ef5a0646356ed1a507e3
    av_register_all(); 

    // register all codecs/parsers/bitstream-filters
    // http://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gaf1a2bb4e7c7611c131bb6212bf0fa639
    avcodec_register_all();

    // "It's important for this to be aligned correctly..."
    // Yeah, I wish you fucking told me why...
    // http://ffmpeg.org/doxygen/trunk/structAVFormatContext.html
    AVFormatContext *pFormatContext __attribute__ ((aligned (16))) = 0;
    AVCodec *pDecoder; // http://ffmpeg.org/doxygen/trunk/structAVCodec.html
    AVCodecContext *pDecoderContext; // http://ffmpeg.org/doxygen/trunk/structAVCodecContext.html
    int stream_index; // which stream from the file we care about
    const char *pFilePath = argv[1]; // filename command line arg

    // open the audio file
    // http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga10a404346c646e4ab58f4ed798baca32
    if (avformat_open_input(&pFormatContext, pFilePath, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        exit(1);
    }

    // http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#gad42172e27cddafb81096939783b157bb
    // `avformat_find_stream_info is what writes out the max_analyze_duration warnings.
    // keep an eye on it.
    if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        exit(1);
    }

    // find the audio stream we probably care about
    // http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#gaa6fa468c922ff5c60a6021dcac09aff9
    stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &pDecoder, 0);

    if (stream_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in file\n");
        exit(1);
    }

    pDecoderContext = pFormatContext->streams[stream_index]->codec;

    // http://ffmpeg.org/doxygen/trunk/group__opt__set__funcs.html#ga3adf7185c21cc080890a5ec02c2e56b2
    av_opt_set_int(pDecoderContext, "refcounted_frames", 1, 0);

    // open the decoder for this audio stream
    // http://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
    if (avcodec_open2(pDecoderContext, pDecoder, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        exit(1);
    }

    /**
     * Try reading some frames
     */
    //debugger vars
    int frame_count = 0;

    WaveformPNG png = init_png(NULL);
    double duration = pFormatContext->duration / (double)AV_TIME_BASE;
    size_t bytes_per_sample = av_get_bytes_per_sample(pDecoderContext->sample_fmt);
    int bit_rate = pFormatContext->bit_rate;
    int channel_count = pDecoderContext->channels;
    int total_data_size = 0;
    int sample_count = 0;
    int approx_buffer_size = bit_rate * (int)floor(duration) / 2;
    int allocated_buffer_size = approx_buffer_size;
    uint8_t *samples = malloc(allocated_buffer_size);

    AVPacket packet; //http://ffmpeg.org/doxygen/trunk/structAVPacket.html

    av_init_packet(&packet); //http://ffmpeg.org/doxygen/trunk/group__lavc__packet.html#gac9cb9756175b96e7441575803757fb73

    AVFrame *pFrame = NULL;

    // Not entirely sure why this is necessary. Wont pFrame always be !pFrame?
    if (!pFrame) {
        //http://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gad5f9212dec34c9fff0124171fa684a18
        if (!(pFrame = avcodec_alloc_frame())) {
            avcodec_close(pDecoderContext);
            avformat_close_input(&pFormatContext);
        }
    } else {
        //http://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga1cb4e0fd7b8eb2f56d977ff96973479d
        avcodec_get_frame_defaults(pFrame);
    }

    //http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga4fdb3084415a82e3810de6ee60e46a61
    while (av_read_frame(pFormatContext, &packet) == 0) {
        int frame_finished = 0;
        int plane_size;

        //http://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga834bb1b062fbcc2de4cf7fb93f154a3e
        avcodec_decode_audio4(pDecoderContext, pFrame, &frame_finished, &packet);

        //we need to get a copy of the array of sample data and pass it to a function that
        //will draw the png
        if (frame_finished) {
            //http://ffmpeg.org/doxygen/trunk/samplefmt_8c.html#aa7368bc4e3a366b688e81938ed55eb06
            int data_size = av_samples_get_buffer_size(
                &plane_size,
                pDecoderContext->channels,
                pFrame->nb_samples,
                pDecoderContext->sample_fmt,
                1
            );

            fwrite(pFrame->data[0], 1, data_size, stdout);
            if (total_data_size + data_size > allocated_buffer_size) {
                allocated_buffer_size = allocated_buffer_size * 1.25;
                samples = realloc(samples, allocated_buffer_size);
            }

            memcpy(samples + total_data_size, pFrame->data[0], data_size);
            total_data_size += data_size;

            //NOTE: nb_samples is the number of samples PER CHANNEL!
            //this makes total_data_size = samples per frame * bytes per sample * number of channels
            sample_count += pFrame->nb_samples;
        }

        //plug those leaks! (DRESS REHEARSAL FOR HELL, BOYS!!!!!)
        av_free_packet(&packet);
    }

    return 0;
    /** DEBUG **/
    av_dump_format(pFormatContext, 0, pFilePath, 0);
    fprintf(stdout, "sample_size: %i\n", (int) bytes_per_sample);
    fprintf(stdout, "is_planar: %i\n", av_sample_fmt_is_planar(pDecoderContext->sample_fmt));
    fprintf(stdout, "allocated_buffer_size: %i\n", allocated_buffer_size);
    fprintf(stdout, "total_data_size: %i\n", total_data_size);
    fprintf(stdout, "sample_count: %i\n", sample_count);
    fprintf(stdout, "sample rate: %i\n", pDecoderContext->sample_rate);
    fprintf(stdout, "channels: %i\n", channel_count);
    fprintf(stdout, "frame size: %i\n", pDecoderContext->frame_size);
    fprintf(stdout, "delay: %i\n", pDecoderContext->delay);
    fprintf(stdout, "bit rate: %i\n", pDecoderContext->bit_rate);
    fprintf(stdout, "frame count: %i\n", frame_count);
    /** END DEBUG **/

    draw_png(&png, samples, total_data_size, bytes_per_sample, channel_count);
    write_and_close_png(&png);

    // clean-up before exit
    avformat_close_input(&pFormatContext);
    avcodec_close(pDecoderContext);

    return 0;
}
