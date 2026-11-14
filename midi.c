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
#include "math/math.h"

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

void realloc_sequencer(sequencer_t *s){
    if (s->sequences) release(s->sequences);//TODO: if we can get the old stats for the sequencer before the modification we can smartly move them into the new one
    s->sequences = zalloc(s->notes_per_bar*s->note_size*s->channels);
    s->current_step = -1;
}

const int grid_size = 50;
const int grid_idist = 5;
const int grid_odist = 10;
const int grid_margin = 10;
int current_step = -1;

void draw_step_sequencer(draw_ctx *ctx, sequencer_t *sequencer, mouse_data *mouse){
    gpu_point p = {20-grid_margin,100};
    fb_fill_rect(ctx, p.x, p.y, (sequencer->notes_per_bar*sequencer->note_size*(grid_size+grid_idist))-grid_idist+((sequencer->notes_per_bar-1)*grid_odist)+(grid_margin*2), 50 + ((grid_size + grid_idist) * sequencer->channels) + grid_margin, 0xFF676767);
    buffer buf = buffer_create(0x256, buffer_can_grow);
    
    p.x += 20 + grid_margin;
    p.y += 5;
    
    gpu_size s = {};
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "%i",sequencer->notes_per_bar), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->notes_per_bar = clamp(sequencer->notes_per_bar + mouse->raw.scroll, 1, 8);
        realloc_sequencer(sequencer);
        mouse->raw.scroll = 0;
    }
    p.x += s.width;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "/"), p.x, p.y, 2, 0xff343434);
    p.x += s.width;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "%i",sequencer->note_size), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->note_size = clamp(sequencer->note_size + mouse->raw.scroll, 1, 16);
        realloc_sequencer(sequencer);
        mouse->raw.scroll = 0;
    }
    p.x += s.width + 10;
    // p.y += s.height + 5;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "%i channels",sequencer->channels), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->channels = clamp(sequencer->channels + mouse->raw.scroll, 1, 16);
        mouse->raw.scroll = 0;
    }
    p.x += s.width + 10;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "octave %i",sequencer->octave), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->octave = clamp(sequencer->octave + mouse->raw.scroll, -1, 8);
        mouse->raw.scroll = 0;
    }
    
    for (int x = 0; x < sequencer->note_size*sequencer->notes_per_bar; x++){
        for (int y = 0; y < sequencer->channels; y++){
            int x_sec = x/sequencer->note_size;
            int x_int = x % sequencer->note_size;
            int index = (y*sequencer->note_size*sequencer->notes_per_bar) + x;
            gpu_rect r = {
                20 + (x_sec * ((sequencer->note_size*(grid_size + grid_idist))+grid_odist)) + (x_int*(grid_size + grid_idist)), 150 + (y * (grid_size + grid_idist)), grid_size, grid_size
            };
            if (click_status == click_start && mouse_in_rect(r, mouse->position)){
                sequencer->sequences[index] = !sequencer->sequences[index];
            }
            fb_fill_rect(ctx, r.point.x, r.point.y, r.size.width, r.size.height, (argbcolor){
                .alpha = 0xff,
                .red = 0x55+((float)x/(sequencer->note_size*sequencer->notes_per_bar))*0x77,
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

sequencer_t* new_sequencer(int octave,int channels, int notes_per_bar, int note_size){
    sequencer_t *s = new(sequencer_t);
    *s = (sequencer_t){
        .audio = make_standard_sample(),
        .channels = channels,
        .current_step = -1,
        .octave = octave,
        .notes_per_bar = notes_per_bar,
        .note_size = note_size,
    };
    realloc_sequencer(s);
    return s;
}