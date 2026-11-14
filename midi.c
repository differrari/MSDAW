#include "midi.h"
#include "input_keycodes.h"
#define MOG_MODULE "MIDI"
#define MOG_LEVEL MOG_VERBOSE
#include "utils/mog.h"
#include "playback.h"
#include "draw/draw.h"
#include "synth.h"
#include "syscalls/syscalls.h"
#include "mouse_helper.h"
#include "alloc/alloc.h"
#include "math/math.h"
#define INFCANVAS_NONSTATIC
#include "header_utils/infcanvas.h"
#include "draw/pathdraw.h"

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

static const int seq_grid_size = 50;
static const int seq_grid_idist = 5;
static const int seq_grid_odist = 10;
static const int seq_grid_margin = 10;
int current_step = -1;

void draw_step_sequencer(draw_ctx *ctx, sequencer_t *sequencer, mouse_data *mouse){
    gpu_point p = {pan.x+sequencer->hdr.render_loc.x+20-seq_grid_margin,pan.y+sequencer->hdr.render_loc.y+100};
    fb_fill_rect(ctx, p.x, p.y, (sequencer->notes_per_bar*sequencer->note_size*(seq_grid_size+seq_grid_idist))-seq_grid_idist+((sequencer->notes_per_bar-1)*seq_grid_odist)+(seq_grid_margin*2), 50 + ((seq_grid_size + seq_grid_idist) * sequencer->channels) + seq_grid_margin, 0xFF676767);
    buffer buf = buffer_create(0x256, buffer_can_grow);
    
    p.x += 20 + seq_grid_margin;
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
                pan.x + 20 + (x_sec * ((sequencer->note_size*(seq_grid_size + seq_grid_idist))+seq_grid_odist)) + (x_int*(seq_grid_size + seq_grid_idist)), pan.y + 150 + (y * (seq_grid_size + seq_grid_idist)), seq_grid_size, seq_grid_size
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

const int circuit_node_size = 25;

void circuit_calc_graph(circuit_t *circuit){
    circuit->graph.num_points = linked_list_count(circuit->points);
    if (!circuit->point_buffer.buffer) circuit->point_buffer = buffer_create(0x100, buffer_can_grow);
    else buffer_wipe(&circuit->point_buffer);
    for (linked_list_node_t *node = circuit->points->head; node; node = node->next){
        vector_point *p = node->data;
        if (!p) continue;
        point_entry entry = { .pos = p->location, .on_curve = !p->control };
        buffer_write_lim(&circuit->point_buffer, (char*)&entry, sizeof(point_entry));
    }
    circuit->slice = (point_graph_slice){
      .start = 0,
      .count = circuit->graph.num_points,
      .max_curve_points = 1,
      .curve_type = point_graph_curve_quad_only,
      .close = true,
    };
    circuit->graph.graph = circuit->point_buffer.buffer;
    circuit->graph.num_slices = 1;
    circuit->graph.slices = &circuit->slice;
}

void draw_circuit(draw_ctx *ctx, circuit_t *circuit, mouse_data *mouse){
    gpu_point loc = mouse->position;
    loc.x -= pan.x;
    loc.y -= pan.y;
    loc.x /= zoom;
    loc.y /= zoom;
    loc.x = round_to_int((float)loc.x/grid_size) * grid_size;
    loc.y = round_to_int((float)loc.y/grid_size) * grid_size;
    int handle_size = circuit_node_size/zoom;
    if (mouse_down()){
        vec2 v = {loc.x,loc.y};
        linked_list_node_t *existing = circuit->selected ?: find_vector_point(circuit->points, v, handle_size);
        if (existing){
            if (click_status == click_start){
                if (is_mod_pressed(KEY_LEFTCTRL, true)){
                    linked_list_remove(circuit->points, existing);
                    circuit_calc_graph(circuit);
                } else {
                    circuit->selected = existing;
                }
            } else {
                vector_point *p = existing->data;
                if (p){
                    p->location = v;
                    circuit_calc_graph(circuit);
                }
            }
        } else if (click_status == click_start){
            vector_point *p = zalloc(sizeof(vector_point));
            *p = (vector_point){.location = v};
            if (is_mod_pressed(KEY_LEFTCTRL, true)){
                p->control = true;
            }
            insert_vector_point(circuit->points, p, -1);
            circuit_calc_graph(circuit);
        }
    } else circuit->selected = 0;
    fb_fill_rect(ctx, loc.x-handle_size/2.f, loc.y-handle_size/2.f, handle_size, handle_size, is_mod_pressed(KEY_LEFTCTRL, true) ? 0x66b41313 : 0x66d1ff31);
    for (linked_list_node_t *node = circuit->points->head; node; node = node->next){
        vector_point *p = node->data;
        if (!p) continue;
        fb_fill_rect(ctx, p->location.x-handle_size/2.f, p->location.y-handle_size/2.f, handle_size, handle_size, p->control ? 0xffb41313 : 0xffd1ff31);
    }
    fb_draw_path(ctx, circuit->hdr.render_loc, circuit->graph, (path_render_options){
        .outline_color = 0xffd1ff31,
        .scale = 1,
        .sizing_policy = path_render_sizing_none
    });
}

void draw_midi(draw_ctx *ctx, midi_hdr *midi, mouse_data *mouse){
    if (!midi) return;
    switch (midi->type){
        case midi_sequencer: draw_step_sequencer(ctx, (sequencer_t*)midi, mouse); break;
        case midi_circuit: draw_circuit(ctx, (circuit_t*)midi, mouse); break;
        case midi_roll: break;
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
        .hdr = { .type = midi_sequencer },
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

circuit_t* new_circuit(int octave, int notes_per_bar, int note_size){
    circuit_t *c = new(circuit_t);
    *c = (circuit_t){
        .hdr = { .type = midi_circuit },
        .points = linked_list_create()
    };
    return c;
}