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
#include "math/bezier.h"

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
    s->sequences = zalloc(s->hdr.notes_per_bar*s->hdr.note_size*s->channels);
    s->current_step = -1;
}

static const int seq_grid_size = 50;
static const int seq_grid_idist = 5;
static const int seq_grid_odist = 10;
static const int seq_grid_margin = 10;
int current_step = -1;

void draw_step_sequencer(draw_ctx *ctx, sequencer_t *sequencer, mouse_data *mouse){
    gpu_point p = {pan.x+sequencer->hdr.render_loc.x+20-seq_grid_margin,pan.y+sequencer->hdr.render_loc.y+100};
    fb_fill_rect(ctx, p.x, p.y, (sequencer->hdr.notes_per_bar*sequencer->hdr.note_size*(seq_grid_size+seq_grid_idist))-seq_grid_idist+((sequencer->hdr.notes_per_bar-1)*seq_grid_odist)+(seq_grid_margin*2), 50 + ((seq_grid_size + seq_grid_idist) * sequencer->channels) + seq_grid_margin, 0xFF676767);
    buffer buf = buffer_create(0x256, buffer_can_grow);
    
    p.x += 20 + seq_grid_margin;
    p.y += 5;
    
    gpu_size s = {};
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "%i",sequencer->hdr.notes_per_bar), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->hdr.notes_per_bar = clamp(sequencer->hdr.notes_per_bar + mouse->raw.scroll, 1, 8);
        realloc_sequencer(sequencer);
        mouse->raw.scroll = 0;
    }
    p.x += s.width;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "/"), p.x, p.y, 2, 0xff343434);
    p.x += s.width;
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "%i",sequencer->hdr.note_size), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->hdr.note_size = clamp(sequencer->hdr.note_size + mouse->raw.scroll, 1, 16);
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
    s = fb_draw_slice(ctx, buffer_write_get_slice(&buf, "octave %i",sequencer->hdr.octave), p.x, p.y, 3, 0xff343434);
    if (mouse_in_rect((gpu_rect){p,s}, mouse->position) && mouse->raw.scroll){
        sequencer->hdr.octave = clamp(sequencer->hdr.octave + mouse->raw.scroll, -1, 8);
        mouse->raw.scroll = 0;
    }
    
    for (int x = 0; x < sequencer->hdr.note_size*sequencer->hdr.notes_per_bar; x++){
        for (int y = 0; y < sequencer->channels; y++){
            int x_sec = x/sequencer->hdr.note_size;
            int x_int = x % sequencer->hdr.note_size;
            int index = (y*sequencer->hdr.note_size*sequencer->hdr.notes_per_bar) + x;
            gpu_rect r = {
                pan.x + 20 + (x_sec * ((sequencer->hdr.note_size*(seq_grid_size + seq_grid_idist))+seq_grid_odist)) + (x_int*(seq_grid_size + seq_grid_idist)), pan.y + 150 + (y * (seq_grid_size + seq_grid_idist)), seq_grid_size, seq_grid_size
            };
            if (!click_handled && click_status == click_start && mouse_in_rect(r, mouse->position)){
                sequencer->sequences[index] = !sequencer->sequences[index];
                click_handled = true;
            }
            fb_fill_rect(ctx, r.point.x, r.point.y, r.size.width, r.size.height, (argbcolor){
                .alpha = 0xff,
                .red = 0x55+((float)x/(sequencer->hdr.note_size*sequencer->hdr.notes_per_bar))*0x77,
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
    circuit->interp_total_len = 0;
    circuit->interp = 0;
    circuit->interp_goal = 0;
    circuit->interp_current = 0;
    for (linked_list_node_t *node = circuit->points->head; node; node = node->next){
        vector_point *a = node->data;
        linked_list_node_t *bn = node->next ?: circuit->points->head;
        vector_point *b = bn->data;
        vector_point *c = 0;
        if (b->control){
            c = b;
            b = (bn->next ?: circuit->points->head)->data;
        }
        if (a && b){
            circuit->interp_total_len += vector2_magnitude(vector2_sub(a->location, b->location));
        }
    }
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
    if (mouse_down() && !click_handled){
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
            // if (is_mod_pressed(KEY_LEFTCTRL, true)){
            //     p->control = true;
            // }
            insert_vector_point(circuit->points, p, -1);
            circuit_calc_graph(circuit);
        }
        click_handled = true;
    } else circuit->selected = 0;
    fb_fill_rect(ctx, pan.x + loc.x-handle_size/2.f, pan.y + loc.y-handle_size/2.f, handle_size, handle_size, is_mod_pressed(KEY_LEFTCTRL, true) ? 0x66b41313 : 0x66d1ff31);
    int count = 0;
    if (circuit->interp_current){
        vector_point *a = circuit->interp_current->data;
        linked_list_node_t *bn = circuit->interp_current->next ?: circuit->points->head;
        vector_point *b = bn->data;
        vector_point *c = 0;
        if (b->control){
            c = b;
            b = (bn->next ?: circuit->points->head)->data;
        }
        if (a && b && circuit->interp_goal > 0){
            vec2 interp = c ? vec2_quad_bezier(a->location, c->location, b->location, circuit->interp/circuit->interp_goal) : vector2_lerp(a->location, b->location, circuit->interp/circuit->interp_goal);
            fb_fill_rect(ctx, interp.x, interp.y, handle_size/2, handle_size/2, 0x66b41313);
        }
    }
    fb_draw_path(ctx, (gpu_point){pan.x + circuit->hdr.render_loc.x, pan.y + circuit->hdr.render_loc.y}, circuit->graph, (path_render_options){
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
    int new_step = (get_time()-play_start_time)/(1000.f/((float)bpm/60*sequencer->hdr.notes_per_bar));
    new_step %= sequencer->hdr.note_size*sequencer->hdr.notes_per_bar;
    if (new_step != current_step){
        current_step = new_step;
        for (int y = 0; y < sequencer->channels; y++){
            int index = (y*sequencer->hdr.note_size*sequencer->hdr.notes_per_bar) + current_step;
            if (sequencer->sequences[index]){
                play_midi(sequencer->hdr.audio, sequencer->channels-y, sequencer->hdr.octave);
            }
        }
    }
}

midi_hdr make_midi_hdr(midi_types type, int octave, int notes_per_bar, int note_size){
    return (midi_hdr){ 
        .type = type, 
        .audio = make_standard_sample(),
        .octave = octave,
        .notes_per_bar = notes_per_bar,
        .note_size = note_size,
    };
}

sequencer_t* new_sequencer(int octave,int channels, int notes_per_bar, int note_size){
    sequencer_t *s = new(sequencer_t);
    *s = (sequencer_t){
        .hdr = make_midi_hdr(midi_sequencer, octave, notes_per_bar, note_size),
        .channels = channels,
        .current_step = -1,
    };
    realloc_sequencer(s);
    return s;
}

circuit_t* new_circuit(int octave, int notes_per_bar, int note_size){
    circuit_t *c = new(circuit_t);
    *c = (circuit_t){
        .hdr = make_midi_hdr(midi_circuit, octave, notes_per_bar, note_size),
        .points = linked_list_create(),
        .interp = 0,
        .interp_total_len = -1,
    };
    return c;
}

void update_circuit(circuit_t *circuit, u64 delta){
    if (!circuit->interp_current) circuit->interp_current = circuit->points->head;
    if (!circuit->interp_current) return;
    circuit->interp += delta/1000.f;
    if (circuit->interp >= circuit->interp_goal){
        play_midi(circuit->hdr.audio, 3, 1);
        circuit->interp_current = circuit->interp_current->next ?: circuit->points->head;
        circuit->interp = 0;
        vector_point *a = circuit->interp_current->data;
        linked_list_node_t *bn = circuit->interp_current->next ?: circuit->points->head;
        vector_point *b = bn->data;
        vector_point *c = 0;
        if (b->control){
            c = b;
            b = (bn->next ?: circuit->points->head)->data;
        }
        if (a && b){
            circuit->interp_goal = (vector2_magnitude(vector2_sub(a->location, b->location))/grid_size) * 1.f/((float)bpm/60*circuit->hdr.notes_per_bar);
        }
    }
}

void update_midi(midi_hdr *midi, u64 delta){
    if (!play) {
        resync = true;
        return;
    }
    if (resync){ 
        play_start_time = get_time();
        if (midi->type == midi_circuit){ ((circuit_t*)midi)->interp = 0;  ((circuit_t*)midi)->interp_current = 0; }
        resync = false;
    }
    if (!midi) return;
    switch (midi->type){
        case midi_sequencer: update_step((sequencer_t*)midi, delta); break;
        case midi_circuit: update_circuit((circuit_t*)midi, delta); break;
        case midi_roll: break;
    }
}