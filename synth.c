#include "synth.h"
#include "audio/cuatro.h"
#include "audio/tone.h"
#include "math/math.h"
#include "playback.h"
#define MOG_MODULE "SYNTH"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"
#include "draw/draw.h"

audio_samples audio[MIXER_INPUTS];

i64 sample_count = 0;

audio_samples* make_sample(float duration){
    if (sample_count >= MIXER_INPUTS) return 0;
    sound_alloc(duration, &audio[sample_count]);
    return &audio[sample_count++];
}

void play_midi(audio_samples* sample, int midi, int octave){
    if (midi < 0) return;
    midi += 12 * (octave + 1);
    float freq = powf(2, (midi-69)/12.f) * 440.f;
    if (freq > 0){
        mog(VERBOSE,"KEY %i => FREQ %f",midi,freq);
        envelope_defn env = { ENV_ASD, 0.025, 0.03 };
        sound_defn s0 = { WAVE_SINE, freq, freq };
        wave_generate(&s0, (audio_sample_t*)sample->samples.ptr, sample->smpls_per_channel);
        sound_shape(&env, sample);
        audio_play_async(sample, 0, AUDIO_ONESHOT_FREE, AUDIO_LEVEL_MAX / 16, PAN_CENTER);
    }
}


void draw_waveform(draw_ctx *ctx, audio_samples *sample){
    for (int i = 0; i < min(sample->smpls_per_channel,ctx->width); i += sizeof(i16)){
        fb_draw_pixel(ctx, i, 50+((float)((i16*)sample->samples.ptr)[i]/INT16_MAX)*30, 0xffb4dd13);
    }
}