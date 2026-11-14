#pragma once

#include "types.h"
#include "mouse_input.h"
#include "audio/cuatro.h"
#include "data/struct/linked_list.h"
#include "math/vector.h"
#include "header_utils/vector_point.h"
#include "draw/point_graph.h"
#include "files/buffer.h"

extern int midi_notes[];

int get_note(int key);

typedef enum { midi_sequencer, midi_circuit, midi_roll } midi_types;

typedef struct {
    midi_types type;
    gpu_point render_loc;
    audio_samples *audio;
    int octave;
    int notes_per_bar;
    int note_size;
} midi_hdr;

typedef struct {
    midi_hdr hdr;
    bool *sequences;
    int current_step;
    int channels;
} sequencer_t;

void draw_midi(draw_ctx *ctx, midi_hdr *midi, mouse_data *mouse);
void update_midi(midi_hdr *midi, u64 delta);

sequencer_t* new_sequencer(int octave, int channels, int notes_per_bar, int note_size);

typedef struct {
    midi_hdr hdr;
    linked_list_t *points;
    vec2 size;
    point_graph graph;
    buffer point_buffer;
    point_graph_slice slice;
    linked_list_node_t *selected;
    linked_list_node_t *interp_current;
    float interp;
    float interp_goal;
    float interp_total_len;
} circuit_t;

circuit_t* new_circuit(int octave, int notes_per_bar, int note_size);