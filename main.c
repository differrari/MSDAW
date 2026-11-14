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

i8 mixin[MIXER_INPUTS] = { };
audio_samples audio[MIXER_INPUTS];

int bpm = 120;
int note_per_bar = 4;
int note_size = 4;
int channels = 12;

bool play = true;

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

draw_ctx ctx = {
    .width = 1080,
    .height = 720
};

int get_note(int key){
    for (int i = 0; i < N_ARR(midi_notes); i++)
        if (midi_notes[i] == key) return i;
    return -1;
}

bool *sequences = 0;

const int grid_size = 50;

int current_step = -1;
int octave = 2;
u64 play_start_time = 0;
u64 accumulator = 0;

void step_sequencer(mouse_data mouse){
    for (int x = 0; x < note_size*note_per_bar; x++){
        for (int y = 0; y < channels; y++){
            int x_sec = x/note_per_bar;
            int x_int = x % note_per_bar;
            int index = (y*note_size*note_per_bar) + x;
            gpu_rect r = {
                20 + (x_sec * ((note_per_bar*(grid_size + 5))+10)) + (x_int*(grid_size + 5)), 100 + (y * (grid_size + 5)), grid_size, grid_size
            };
            if (click_status == click_start && mouse_in_rect(r, mouse.position)){
                sequences[index] = !sequences[index];
            }
            fb_fill_rect(&ctx, r.point.x, r.point.y, r.size.width, r.size.height, (argbcolor){
                .alpha = 0xff,
                .red = 0x55+((float)x/(note_size*note_per_bar))*0x77,
                .blue = 0x55+((float)y/channels)*0x77,
                .green = sequences[index] ? 0x77 : 0x00,
            }.color);
            if (current_step == x){
                fb_outline_rect(&ctx, r.point.x, r.point.y, r.size.width, r.size.height, 1, 0xFFFFFFFF);
            }
        }
    }
}

void play_midi(int midi, int octave){
    if (midi >= 0){
        midi += 12 * (octave + 1);
        float freq = powf(2, (midi-69)/12.f) * 440.f;
        if (freq > 0){
            mog(VERBOSE,"MIDI %i, FREQ %f",midi,freq);
            envelope_defn env = { ENV_ASD, 0.025, 0.03 };
            sound_defn s0 = { WAVE_SINE, freq, freq };
            audio_samples *current_audio = &audio[0];
            wave_generate(&s0, (audio_sample_t*)current_audio->samples.ptr, current_audio->smpls_per_channel);
            sound_shape(&env, current_audio);
            audio_play_async(current_audio, 0, AUDIO_ONESHOT_FREE, AUDIO_LEVEL_MAX / 16, PAN_CENTER);
        }
    }
}

bool resync = false;

void update_step(u64 delta){
    if (!play) {
        resync = true;
        return;
    }
    if (resync){ 
        play_start_time = get_time();
        resync = false;
    }
    int new_step = (get_time()-play_start_time)/(1000.f/((float)bpm/60*note_per_bar));
    new_step %= note_size*note_per_bar;
    if (new_step != current_step){
        current_step = new_step;
        for (int y = 0; y < channels; y++){
            int index = (y*note_size*note_per_bar) + current_step;
            if (sequences[index]){
                play_midi(channels-y, octave);
            }
        }
    }
}

int main(int argc, char *argv[]){
    print("Ni hao fine shyt");
    
    request_draw_ctx(&ctx);
    
    play_start_time = get_time();
    
    // for (int i = 0; i < MIXER_INPUTS; ++i) mixin[i] = -1;
    mixer_master_level(AUDIO_LEVEL_MAX * 0.75f);
    
    sound_alloc((1.f/(bpm/60.f*note_per_bar)), &audio[0]);
    
    sequences = zalloc(note_per_bar*note_size*channels);
    
    while (!should_close_ctx()){
        
        update_step(profiler_delta());
        
        kbd_event ev = {};
        fb_clear(&ctx, 0xFF666999);
        
        for (int i = 0; i < min(audio[0].smpls_per_channel,ctx.width); i += sizeof(i16)){
            fb_draw_pixel(&ctx, i, 50+((float)((i16*)audio[0].samples.ptr)[i]/INT16_MAX)*30, 0xffb4dd13);
        }
        
        mouse_data mouse = {};
        get_mouse_status(&mouse);
        process_mouse(mouse);
        step_sequencer(mouse);
        
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
                play_midi(midi,octave);
            }
        }
        commit_draw_ctx(&ctx);
    }
    return 0;
}