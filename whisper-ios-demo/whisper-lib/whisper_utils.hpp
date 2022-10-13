#ifndef WHISPER_UTILS_HPP
#define WHISPER_UTILS_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int read_wav(const char* model_filename, const char* wav_filename);

#ifdef __cplusplus
}
#endif

#endif

