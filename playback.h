#pragma once

#include "types.h"

extern int bpm;
extern int note_per_bar;
extern int note_size;
extern u64 play_start_time;
extern bool resync;
extern bool play;

void prepare_playback();