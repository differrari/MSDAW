#include "syscalls/syscalls.h"
#include "audio/tone.h"
#include "audio/mixer.h"
#include "keyboard_input.h"
#include "input_keycodes.h"
#include "math/math.h"
#include "kbd_helper.h"
#include "audio/tone.h"
#define MOUSE_HELPER_NONSTATIC
#include "mouse_helper.h"
#include "debug/profiler.h"
#define MOG_MODULE "MSDAW"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"
#include "playback.h"
#include "synth.h"
#include "midi.h"

int channels = 12;

draw_ctx ctx = {
    .width = 1080,
    .height = 720
};

int octave = 2;

int main(int argc, char *argv[]){
    
    request_draw_ctx(&ctx);
    
    play_start_time = get_time();
    
    // for (int i = 0; i < MIXER_INPUTS; ++i) mixin[i] = -1;
    
    audio_samples *sample = make_standard_sample();
    
    sequencer_t *seq = new_sequencer(octave, 12, 5, note_size);
    
    while (!should_close_ctx()){
        
        update_step(seq, profiler_delta());
        
        kbd_event ev = {};
        fb_clear(&ctx, 0xFF666999);
        
        draw_waveform(&ctx, sample);
        
        mouse_data mouse = {};
        get_mouse_status(&mouse);
        process_mouse(mouse);
        draw_step_sequencer(&ctx, seq, &mouse);
        
        if (mouse.raw.scroll){
            bpm += 10 * mouse.raw.scroll;
            resync = true;
        }
                
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