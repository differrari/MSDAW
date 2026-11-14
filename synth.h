#pragma once

#include "types.h"
#include "audio/cuatro.h"
#include "playback.h"
#include "graphic_types.h"

audio_samples* make_sample(float duration); 

static inline audio_samples* make_standard_sample(){
    return make_sample(1.f/(bpm/60.f*note_per_bar));
}

void draw_waveform(draw_ctx *ctx, audio_samples *sample);

void play_midi(audio_samples* sample, int midi, int octave);