#include "playback.h"
#include "audio/cuatro.h"
#define MOG_MODULE "Playback"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"

int bpm = 120;
int note_per_bar = 4;
int note_size = 4;
u64 play_start_time = 0;
bool resync = false;
bool play = true;

i8 mixin[MIXER_INPUTS] = { };

void prepare_playback(){
    mixer_master_level(AUDIO_LEVEL_MAX * 0.75f);
}