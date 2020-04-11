#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define DEBUG 1

struct {
    int width;
    int height;
    long delay;
    int *fr_src;
} gui_info;

pthread_t gui_thd;

void update_gui();
void *update_gui_runner(void*);
void transform_tone(int freq, char *dest);
void wait_next_frame();
void clear_term();

void tuner_set_width(int w);
void tuner_set_height(int h);
void tuner_set_freq_source(int *src);
void tuner_set_fps(int fps);
void tuner_set_active(int a);
void tuner_start_gui();

void update_gui() {
    static char tone[10];
    printf("TermTuner v0.1\n\n");
    transform_tone(*(gui_info.fr_src), &(tone[0]));
    printf("Tone: %s\n\n", tone);
    printf("==============\n");
}

void tuner_set_width(int w) {
    gui_info.width = w;
}

void tuner_set_height(int h) {
    gui_info.height = h;
}

void tuner_set_freq_source(int *src) {
    gui_info.fr_src = src;
}

void tuner_set_fps(int fps) {
    gui_info.delay = (long) CLOCKS_PER_SEC / fps;
}

void *update_gui_runner(void * args) {
    while (1) {
        clear_term();
        if (DEBUG) {
            printf("clock: %ld\n", clock());
        }
        update_gui();
        wait_next_frame();
    }
}

void wait_next_frame() {
    usleep(gui_info.delay * 100000 / CLOCKS_PER_SEC);
}

void tuner_start_gui() {
    int err = pthread_create(&gui_thd, NULL, update_gui_runner, NULL);
    if (err != 0) {
        fprintf(stderr, "Error on starting tuner GUI\n");
    }
}

void transform_tone(int freq, char *tone) {
    strcpy(tone, "NONE");
}

void clear_term() {
    system("clear");
}

int main(int argc, char const *argv) {
    int freq = 45;
    tuner_set_width(40);
    tuner_set_height(20);
    tuner_set_fps(30);
    tuner_set_freq_source(&freq);
    tuner_start_gui();
    pthread_join(gui_thd, NULL);
    return 0;
}
