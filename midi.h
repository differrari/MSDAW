#pragma once

#include "types.h"
#include "mouse_input.h"
#include "audio/cuatro.h"

extern int midi_notes[];

int get_note(int key);

typedef struct {
    gpu_point render_loc;
    bool *sequences;
    int current_step;
    int channels;
    int octave;
    int notes_per_bar;
    int note_size;
    audio_samples *audio;
} sequencer_t;

void draw_step_sequencer(draw_ctx *ctx, sequencer_t *sequencer, mouse_data *mouse);
void update_step(sequencer_t *sequencer, u64 delta);

sequencer_t* new_sequencer(int octave, int channel, int notes_per_bar, int note_size);