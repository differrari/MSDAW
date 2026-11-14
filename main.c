#include "syscalls/syscalls.h"
#include "audio/tone.h"
#include "audio/mixer.h"
#include "keyboard_input.h"
#include "input_keycodes.h"
#include "math/math.h"
#include "kbd_helper.h"
#include "audio/tone.h"
#include "mouse_helper.h"
#include "debug/profiler.h"
#define MOG_MODULE "MSDAW"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"
#include "playback.h"
#include "synth.h"
#include "midi.h"
#define INFCANVAS_NONSTATIC
#include "header_utils/infcanvas.h"

int channels = 12;

draw_ctx ctx = {
    .width = 1080,
    .height = 720
};

int octave = 2;

float zoom = 1;
float max_zoom = 1;
float min_zoom = 1;
float zoom_speed = 1;
int max_grid = 1;
int min_grid = 1;
int grid_snap = 1;
gpu_point pan = {};
int grid_size = 1;
void (*on_canvas_pan)(gpu_point point) = 0;
void (*on_canvas_zoom)(float zoom) = 0;

int main(int argc, char *argv[]){
    
    request_draw_ctx(&ctx);
    
    play_start_time = get_time();
    
    // for (int i = 0; i < MIXER_INPUTS; ++i) mixin[i] = -1;
    
    audio_samples *sample = make_standard_sample();
    
    sequencer_t *seq = new_sequencer(octave, 12, note_per_bar, note_size);
    
    circuit_t *cir = new_circuit(octave, note_per_bar, note_size);
    
    profiler_init();
    
    while (!should_close_ctx()){
        
        update_step(seq, profiler_delta());

        canvas_background(&ctx, 0xFF666999, 0);
        
        draw_waveform(&ctx, sample);
        
        mouse_data mouse = {};
        get_mouse_status(&mouse);
        process_mouse(mouse);
        draw_midi(&ctx, &seq->hdr, &mouse);
        draw_midi(&ctx, &cir->hdr, &mouse);
        
        if (mouse.raw.scroll){
            bpm += 10 * mouse.raw.scroll;
            resync = true;
            mouse.raw.scroll = 0;
        }
        
        canvas_mouse(mouse, profiler_delta()/1000.f);
                
        kbd_event ev = {};
        if (read_event(&ev)){
            if (handle_modifier(&ev)) continue;
            if (ev.type == KEY_PRESS){
                if (ev.key == KEY_ESC && is_mod_pressed(KEY_LEFTALT, true)) halt(0);
                int note_played = ev.key;
                if (ev.key == KEY_Z){ octave--; note_played = KEY_A; }
                if (ev.key == KEY_X){ octave++; note_played = KEY_A; }
                if (ev.key == KEY_SPACE) play = !play;
                octave = clamp(octave, -1, 8);
                int midi = get_note(note_played);
                play_midi(sample, midi,octave);
            }
        }
        commit_draw_ctx(&ctx);
    }
    return 0;
}