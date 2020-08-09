#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <pthread.h>

#define DEBUG 0

#define TUNER_MIN_FPS 2
#define TUNER_MAX_FPS 60
#define TUNER_MIN_WIDTH 10
#define TUNER_MAX_WIDTH 100
#define TUNER_MIN_HEIGHT 5
#define TUNER_MAX_HEIGHT 50

struct {
    int freq;
    char *buffer;
    int buffer_size;
    snd_pcm_uframes_t frames; // number of frames per period
    int direction; // the PCM direction (-1, 0 or 1)
    char device_name[32];
    int baud_rate;
} audio_info;

struct {
    int width;
    int height;
    long delay;
    int *fr_src;
} gui_info;

// General functions
void transform_tone(int freq, char *dest);
void wait_next_frame();
void clear_term();
void configure_pcm();
// GUI functions
void tuner_gui_set_width(int w);
void tuner_gui_set_height(int h);
void tuner_gui_set_freq_source(int *src);
void tuner_gui_set_fps(int fps);
void tuner_gui_set_active(int a);
// Audio functions
void tuner_audio_set_baud_rate(int rate);
void tuner_audio_set_device(char *dev_name);
void tuner_audio_set_buffer_size(int size);
void tuner_audio_set_frames_per_period(int frames);
// ALSA constructors
snd_pcm_t *pcm_handler = NULL;
snd_pcm_hw_params_t *pcm_params = NULL;
// Threads IDs
pthread_t gui_thd;
pthread_t audio_thd;
pthread_t mic_thd;
// Updates routines
void tuner_mic_update();
void tuner_gui_update();
void tuner_audio_update();
void *tuner_mic_routine(void*);
void *tuner_gui_routine(void*);
void *tuner_audio_routine(void*);
void tuner_mic_start();
void tuner_gui_start();
void tuner_audio_start();

void tuner_gui_update() {
    static char tone[128];
    static char const *screen_format = "\nTermTuner v0.1\n\nTone: %s\n\n==================\n";
    transform_tone(*(gui_info.fr_src), &(tone[0]));
    printf(screen_format, tone);
}

void tuner_gui_set_width(int w) {
    if (w < TUNER_MIN_WIDTH) w = TUNER_MIN_WIDTH;
    if (w > TUNER_MAX_WIDTH) w = TUNER_MAX_WIDTH;
    gui_info.width = w;
}

void tuner_gui_set_height(int h) {
    if (h < TUNER_MIN_HEIGHT) h = TUNER_MIN_HEIGHT;
    if (h > TUNER_MAX_HEIGHT) h = TUNER_MAX_HEIGHT;
    gui_info.height = h;
}

void tuner_gui_set_freq_source(int *src) {
    gui_info.fr_src = src;
}

void tuner_gui_set_fps(int fps) {
    if (fps < TUNER_MIN_FPS) fps = TUNER_MIN_FPS;
    if (fps > TUNER_MAX_FPS) fps = TUNER_MAX_FPS;
    gui_info.delay = (long) CLOCKS_PER_SEC / fps;
}

void *tuner_gui_routine(void * args) {
    while (1) {
        clear_term();
        if (DEBUG) {
            printf("clock: %ld\n", clock());
            if (pcm_handler != NULL) {
                int rate, dir;
                snd_pcm_format_t format;
                snd_pcm_hw_params_get_rate(pcm_params, &rate, &dir);
                snd_pcm_hw_params_get_format(pcm_params, &format);
                printf("pcm rate: %d\n", rate);
                printf("pcm direction: %d\n", dir);
                printf("pcm format: %s\n", snd_pcm_format_name(format));
            }
        }
        tuner_gui_update();
        wait_next_frame();
    }
}

void wait_next_frame() {
    usleep(gui_info.delay * 1000000 / CLOCKS_PER_SEC);
}

void tuner_gui_start() {
    int err = pthread_create(&gui_thd, NULL, tuner_gui_routine, NULL);
    if (err != 0) {
        fprintf(stderr, "Error on starting tuner GUI\n");
    }
}

void tuner_audio_update() {
    audio_info.freq = *((int*) audio_info.buffer);
}

void *tuner_audio_routine(void *args) {
    while (1) {
        tuner_audio_update();
        usleep(10000);
    }
}

void tuner_audio_start() {
    configure_pcm();
    tuner_mic_start();
    int err = pthread_create(&audio_thd, NULL, tuner_audio_routine, NULL);
    if (err != 0) {
        fprintf(stderr, "Error on starting tuner audio\n");
    }
}

void transform_tone(int freq, char *tone) {
    sprintf(tone, "%d", freq);
}

void clear_term() {
    system("clear");
}

void tuner_audio_set_buffer_size(int size) {
    audio_info.buffer_size = size;
    if (audio_info.buffer == NULL) {
        audio_info.buffer = (char*) malloc(size);
    } else {
        audio_info.buffer = (char*) realloc((void*) audio_info.buffer, size);
    }
}

void tuner_audio_set_frames_per_period(int frames) {
    audio_info.frames = (snd_pcm_uframes_t) frames;
}

void tuner_audio_set_device(char *dev_name) {
    strcpy(audio_info.device_name, dev_name);
}

void tuner_audio_set_baud_rate(int rate) {
    audio_info.baud_rate = rate;
}

void configure_pcm() {
    int err;
    // Open default audio interface for capture
    err = snd_pcm_open(&pcm_handler, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(err));
        exit(1);
    }
    // Allocate PCM parameters
    snd_pcm_hw_params_alloca(&pcm_params);
    snd_pcm_hw_params_any(pcm_handler, pcm_params);
    snd_pcm_hw_params_set_access(pcm_handler, pcm_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handler, pcm_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handler, pcm_params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handler, pcm_params, &(audio_info.baud_rate), &(audio_info.direction));
    snd_pcm_hw_params_set_period_size_near(pcm_handler, pcm_params, &(audio_info.frames), &(audio_info.direction));
    // Write paramaters in PCM handware
    err = snd_pcm_hw_params(pcm_handler, pcm_params);
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(1);
    }
}

void tuner_mic_update() {
    int err = snd_pcm_readi(pcm_handler, audio_info.buffer, audio_info.frames);
    if (err == -EPIPE) {
        fprintf(stderr, "underrun occured\n");
        snd_pcm_prepare(pcm_handler);
    } else if (err < 0) {
        fprintf(stderr, "error from read: %s\n", snd_strerror(err));
    }
}

void *tuner_mic_routine(void *args) {
    while (1) {
        tuner_mic_update();
    }
}

void tuner_mic_start() {
    int err = pthread_create(&mic_thd, NULL, tuner_mic_routine, NULL);
    if (err != 0) {
        printf("Error creating mic thread.\n");
    }
}

int main(int argc, char const *argv[]) {
    tuner_audio_set_baud_rate(44100);
    tuner_audio_set_frames_per_period(32);
    tuner_audio_set_buffer_size(256);
    tuner_audio_set_device("default");
    tuner_gui_set_width(40);
    tuner_gui_set_height(20);
    tuner_gui_set_fps(15);
    tuner_gui_set_freq_source(&(audio_info.freq));
    tuner_audio_start();
    tuner_gui_start();
    while (getchar() != 'q');
    free(audio_info.buffer);
    return 0;
}
