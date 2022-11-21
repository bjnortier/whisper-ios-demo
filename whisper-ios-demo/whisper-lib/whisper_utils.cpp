#include "whisper_utils.hpp"
#include "whisper.hpp"


#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <stdio.h>
#include <fstream>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

//  500 -> 00:05.000
// 6000 -> 01:00.000
std::string to_timestamp(int64_t t) {
    int64_t msec = t * 10;
    int64_t hr = msec / (1000 * 60 * 60);
    msec = msec - hr * (1000 * 60 * 60);
    int64_t min = msec / (1000 * 60);
    msec = msec - min * (1000 * 60);
    int64_t sec = msec / 1000;
    msec = msec - sec * 1000;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", (int) hr, (int) min, (int) sec, (int) msec);

    return std::string(buf);
}

struct whisper_params {
    int32_t seed      = -1; // RNG seed, not used currently
    int32_t n_threads = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t offset_ms = 0;

    bool verbose              = false;
    bool translate            = false;
    bool output_txt           = false;
    bool output_vtt           = false;
    bool output_srt           = false;
    bool print_special_tokens = false;
    bool no_timestamps        = false;

    std::string language  = "en";
    std::string model     = "resources/ggml-small.en.bin";

    std::vector<std::string> fname_inp = {};
};


int read_wav(const char* model_filename, const char* wav_filename, int (*progress_callback)(const char*)) {
    progress_callback("Initializing Whisper...");

    whisper_params params;
    params.model = model_filename;
    struct whisper_context * ctx = whisper_init(params.model.c_str());

    drwav wav;
    std::vector<float> pcmf32;
    if (!drwav_init_file(&wav, wav_filename, NULL)) {
        fprintf(stderr, "failed to open WAV file - check your input\n");
        return 3;
    }
    if (wav.channels != 1 && wav.channels != 2) {
        fprintf(stderr, "WAV file must be mono or stereo\n");
        return 4;
    }
    if (wav.sampleRate != WHISPER_SAMPLE_RATE) {
        fprintf(stderr, "WAV file must be 16 kHz\n");
        return 5;
    }
    if (wav.bitsPerSample != 16) {
        fprintf(stderr, "WAV file must be 16-bit\n");
        return 6;
    }

    drwav_uint64 n = wav.totalPCMFrameCount;

    std::vector<int16_t> pcm16;
    pcm16.resize(n*wav.channels);
    drwav_read_pcm_frames_s16(&wav, n, pcm16.data());
    drwav_uninit(&wav);

    // convert to mono, float
    pcmf32.resize(n);
    if (wav.channels == 1) {
        for (int i = 0; i < n; i++) {
            pcmf32[i] = float(pcm16[i])/32768.0f;
        }
    } else {
        for (int i = 0; i < n; i++) {
            pcmf32[i] = float(pcm16[2*i] + pcm16[2*i + 1])/65536.0f;
        }
    }

    char buffer[512];
    snprintf(buffer, 512, "opened WAV. size: %lu", pcmf32.size());
    progress_callback(buffer);


    // print some info about the processing
    {
        fprintf(stderr, "\n");
        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        fprintf(stderr, "%s: processing '%s' (%d samples, %.1f sec), %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__, wav_filename, int(pcmf32.size()), float(pcmf32.size())/WHISPER_SAMPLE_RATE, params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);

        fprintf(stderr, "\n");
    }

    {
        whisper_full_params wparams = whisper_full_default_params(WHISPER_DECODE_GREEDY);
        wparams.n_threads = 8;

        wparams.print_realtime       = true;
        wparams.print_progress       = false;
        wparams.print_timestamps     = !params.no_timestamps;
        wparams.print_special_tokens = params.print_special_tokens;
        wparams.translate            = params.translate;
        wparams.language             = params.language.c_str();
        wparams.n_threads            = params.n_threads;
        wparams.offset_ms            = params.offset_ms;

        snprintf(buffer, 512, "processing audio...");
        progress_callback(buffer);
        if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
            fprintf(stderr, "%s: failed to process audio\n", wav_filename);
            return 7;
        }
        snprintf(buffer, 512, "audio processed");
        progress_callback(buffer);

        // print result
        if (!wparams.print_realtime) {
            printf("\n");

            const int n_segments = whisper_full_n_segments(ctx);
            for (int i = 0; i < n_segments; ++i) {
                const char * text = whisper_full_get_segment_text(ctx, i);

                if (params.no_timestamps) {
                    progress_callback(text);
//                    printf("%s", text);
//                    fflush(stdout);
                } else {
                    const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
                    const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

                    snprintf(buffer, 512, "[%s --> %s]  %s\n", to_timestamp(t0).c_str(), to_timestamp(t1).c_str(), text);
                    progress_callback(buffer);
                }
            }
        }

        printf("\n");
    }
    whisper_free(ctx);
    return 0;
}
