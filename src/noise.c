#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <unistd.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

typedef struct NoiseState {
    float amplitude;
    ma_uint32 channels;
    uint32_t seed;
} NoiseState;

static inline float frand_signed(uint32_t* s) {
    *s = (*s * 1664525u) + 1013904223u; // LCG
    float v = (float)(*s & 0x00FFFFFF) / (float)0x01000000; // [0,1)
    return (v * 2.0f) - 1.0f; // [-1,1)
}

static void data_callback(ma_device* device, void* out, const void* in, ma_uint32 frameCount) {
    NoiseState* st = (NoiseState*)device->pUserData;
    float* f32 = (float*)out;
    ma_uint64 total = (ma_uint64)frameCount * st->channels;
    for (ma_uint64 i = 0; i < total; ++i) {
        f32[i] = frand_signed(&st->seed) * st->amplitude;
    }
    (void)in;
}

static void print_usage(const char* exe) {
    fprintf(stderr, "Usage: %s [--rate N] [--channels N] [--duration S] [--amp A]\n", exe);
    fprintf(stderr, "  --rate: sample rate in Hz (default 48000)\n");
    fprintf(stderr, "  --channels: 1 or 2 (default 2)\n");
    fprintf(stderr, "  --duration: seconds to play (default 5)\n");
    fprintf(stderr, "  --amp: amplitude 0..1 (default 0.2)\n");
}

int main(int argc, char** argv) {
    ma_uint32 sampleRate = 48000;
    ma_uint32 channels = 2;
    int durationSec = 5;
    float amplitude = 0.2f;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            sampleRate = (ma_uint32)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--channels") == 0 && i + 1 < argc) {
            channels = (ma_uint32)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            durationSec = (int)strtol(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--amp") == 0 && i + 1 < argc) {
            amplitude = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (channels == 0 || channels > 8) channels = 2;
    if (sampleRate < 8000) sampleRate = 8000;
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    if (durationSec <= 0) durationSec = 1;

    NoiseState state;
    state.amplitude = amplitude;
    state.channels = channels;
    state.seed = (uint32_t)time(NULL);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sampleRate;
    config.dataCallback = data_callback;
    config.pUserData = &state;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        return 1;
    }

    printf("Playing white noise: rate=%u, channels=%u, duration=%d s, amp=%.2f\n",
           sampleRate, channels, durationSec, amplitude);

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start device.\n");
        ma_device_uninit(&device);
        return 1;
    }

#ifdef __APPLE__
    // Simple sleep loop for duration
    for (int i = 0; i < durationSec * 10; ++i) {
        usleep(100000); // 100ms
    }
#else
    ma_uint32 totalMS = (ma_uint32)(durationSec * 1000);
    ma_uint32 step = 50;
    for (ma_uint32 elapsed = 0; elapsed < totalMS; elapsed += step) {
        ma_sleep(step);
    }
#endif

    ma_device_stop(&device);
    ma_device_uninit(&device);
    printf("Done.\n");
    return 0;
}
