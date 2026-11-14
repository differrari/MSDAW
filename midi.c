#include "midi.h"
#include "input_keycodes.h"
#define MOG_MODULE "MIDI"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"
#include "playback.h"
#include "draw/draw.h"
#include "synth.h"
#include "syscalls/syscalls.h"
#define MOUSE_HELPER_NONSTATIC
#include "mouse_helper.h"
#include "alloc/alloc.h"

int midi_notes[] = {
    KEY_A,
    KEY_W,
    KEY_S,
    KEY_E,
    KEY_D,
    KEY_F,
    KEY_T,
    KEY_G,
    KEY_Y,
    KEY_H,
    KEY_U,
    KEY_J,
    KEY_K,
    KEY_O,
    KEY_L,
    KEY_P,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_RIGHTBRACE,
    KEY_BACKSLASH,
};

int get_note(int key){
    for (int i = 0; i < N_ARR(midi_notes); i++)
        if (midi_notes[i] == key) return i;
    return -1;
}

const int grid_size = 50;
int current_step = -1;

void draw_step_sequencer(draw_ctx *ctx, sequencer_t *sequencer, mouse_data mouse){
    for (int x = 0; x < sequencer->note_size*note_per_bar; x++){
        for (int y = 0; y < sequencer->channels; y++){
            int x_sec = x/note_per_bar;
            int x_int = x % note_per_bar;
            int index = (y*sequencer->note_size*note_per_bar) + x;
            gpu_rect r = {
                20 + (x_sec * ((note_per_bar*(grid_size + 5))+10)) + (x_int*(grid_size + 5)), 100 + (y * (grid_size + 5)), grid_size, grid_size
            };
            if (click_status == click_start && mouse_in_rect(r, mouse.position)){
                sequencer->sequences[index] = !sequencer->sequences[index];
            }
            fb_fill_rect(ctx, r.point.x, r.point.y, r.size.width, r.size.height, (argbcolor){
                .alpha = 0xff,
                .red = 0x55+((float)x/(sequencer->note_size*note_per_bar))*0x77,
                .blue = 0x55+((float)y/sequencer->channels)*0x77,
                .green = sequencer->sequences[index] ? 0x77 : 0x00,
            }.color);
            if (current_step == x){
                fb_outline_rect(ctx, r.point.x, r.point.y, r.size.width, r.size.height, 1, 0xFFFFFFFF);
            }
        }
    }
}

void update_step(sequencer_t *sequencer, u64 delta){
    if (!play) {
        resync = true;
        return;
    }
    if (resync){ 
        play_start_time = get_time();
        resync = false;
    }
    int new_step = (get_time()-play_start_time)/(1000.f/((float)bpm/60*sequencer->notes_per_bar));
    new_step %= sequencer->note_size*sequencer->notes_per_bar;
    if (new_step != current_step){
        current_step = new_step;
        for (int y = 0; y < sequencer->channels; y++){
            int index = (y*sequencer->note_size*sequencer->notes_per_bar) + current_step;
            if (sequencer->sequences[index]){
                play_midi(sequencer->audio, sequencer->channels-y, sequencer->octave);
            }
        }
    }
}

sequencer_t* new_sequencer(int octave,int channel, int notes_per_bar, int note_size){
    sequencer_t *s = new(sequencer_t);
    *s = (sequencer_t){
        .audio = make_standard_sample(),
        .channels = 12,
        .current_step = -1,
        .octave = octave,
        .notes_per_bar = notes_per_bar,
        .note_size = note_size,
        .sequences = zalloc(notes_per_bar*note_size*channel)
    };
    return s;
}