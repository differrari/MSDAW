#include "syscalls/syscalls.h"
#include "audio/tone.h"
#include "audio/mixer.h"
#include "keyboard_input.h"
#include "input_keycodes.h"
#include "math/math.h"
#include "kbd_helper.h"

static i8 mixin[MIXER_INPUTS] = { };
static audio_samples audio[MIXER_INPUTS];

int midi_notes[] = {
    [KEY_A] = 0,
    [KEY_W] = 1,
    [KEY_S] = 2,
    [KEY_E] = 3,
    [KEY_D] = 4,
    [KEY_F] = 5,
    [KEY_T] = 6,
    [KEY_G] = 7,
    [KEY_Y] = 8,
    [KEY_H] = 9,
    [KEY_U] = 10,
    [KEY_J] = 11,
    [KEY_K] = 12,
    [KEY_O] = 13,
    [KEY_L] = 14,
    [KEY_P] = 15,
    [KEY_SEMICOLON]  = 16,
    [KEY_APOSTROPHE]  = 17,
    [KEY_RIGHTBRACE] = 18,
    [KEY_BACKSLASH]  = 19,
};

draw_ctx ctx = {};

int main(int argc, char *argv[]){
    print("Ni hao fine shyt");
    
    request_draw_ctx(&ctx);
    
    // for (int i = 0; i < MIXER_INPUTS; ++i) mixin[i] = -1;
    mixer_master_level(AUDIO_LEVEL_MAX * 0.75f);
    int octave = 2;
    while (!should_close_ctx()){
        kbd_event ev = {};
        fb_clear(&ctx, 0xFF666999);
        if (read_event(&ev)){
            if (handle_modifier(&ev)) continue;
            if (ev.type == KEY_PRESS){
                int midi = midi_notes[ev.key];
                midi += 12 * (octave + 1);
                if (midi){
                    float freq = powfloat(2, (midi-69)/12.f) * 440.f;
                    print("F %f",freq);
                    if (ev.key == KEY_ESC && is_mod_pressed(KEY_LEFTALT, true)) halt(0);
                    if (ev.key == KEY_Z) octave--;
                    if (ev.key == KEY_X) octave++;
                    octave = clamp(octave, -1, 8);
                    if (freq > 0){
                        envelope_defn env = { ENV_ASD, 0.025, 0.03 };
                        sound_defn s0 = { WAVE_TRIG, freq, freq };
                        sound_create(.2f, &s0, &audio[0]);
                        sound_shape(&env, &audio[0]);
                        audio_play_async(&audio[0], 0, AUDIO_ONESHOT_FREE, AUDIO_LEVEL_MAX / 16, PAN_CENTER);
                    }
                }
            }
        }
        commit_draw_ctx(&ctx);
    }
    return 0;
}