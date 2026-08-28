#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"

/* CD extent of one file: start sector and byte size. */
typedef struct battle_file_extent {
    s32 sector;
    s32 size;
} battle_file_extent_t;

extern battle_file_extent_t g_battle_gfx_sprite_seq_files[]; /* sprite type SEQ */
extern battle_file_extent_t g_battle_gfx_sprite_shp_files[]; /* sprite type SHP */
extern battle_file_extent_t g_battle_gfx_weapon_seq_files[]; /* WEP SEQ */
extern battle_file_extent_t g_battle_gfx_weapon_shp_files[]; /* WEP SHP */
extern battle_file_extent_t g_battle_gfx_effect_seq_files[]; /* EFF SEQ */
extern battle_file_extent_t g_battle_gfx_effect_shp_files[]; /* EFF SHP */

void battle_gfx_copy_shp_data_to_ram();
void battle_gfx_resolve_packed_offset_table();

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_battle_gfx_state_words[0] selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_battle_gfx_state_words[1] the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_battle_gfx_state_words[2] the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_battle_gfx_state_words[0] load above the counter store, as the target does. */
s32 battle_gfx_step_load_unit_shp_seq_data(void) {
    s32* phase;
    s32* index;
    s32* slot;
    s32 buffer;

    phase = &g_battle_gfx_state_words[1];
    switch (*phase) {
    case 0:
        switch (g_battle_gfx_state_words[0]) {
        case 0:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_sprite_shp_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_shp_files[*index].sector,
                    g_battle_gfx_sprite_shp_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 1:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_sprite_seq_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_seq_files[*index].sector,
                    g_battle_gfx_sprite_seq_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 2:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_weapon_shp_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_weapon_shp_files[*index].sector,
                    g_battle_gfx_weapon_shp_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 3:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_weapon_seq_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_weapon_seq_files[*index].sector,
                    g_battle_gfx_weapon_seq_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 4:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_effect_shp_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_effect_shp_files[*index].sector,
                    g_battle_gfx_effect_shp_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 5:
            index = &g_battle_gfx_state_words[2];
            buffer = (s32)main_heap_alloc(g_battle_gfx_effect_seq_files[*index].size);
            g_battle_gfx_state_words[3] = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_effect_seq_files[*index].sector,
                    g_battle_gfx_effect_seq_files[*index].size, (void*)buffer)
                == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            }
            break;
        case 6:
            *(slot = &g_battle_gfx_state_words[3]) = (s32)main_heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(0xdefa, 0x15000, (void*)*slot) == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        case 7:
            *(slot = &g_battle_gfx_state_words[3]) = (s32)main_heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(0xdf24, 0x8800, (void*)*slot) == 0) {
                g_battle_gfx_state_words[1]++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        }
        break;
    case 1:
        if (main_file_is_still_loading() != 0) {
            break;
        }
        (*phase)++;
        break;
    case 2:
        switch (g_battle_gfx_state_words[0]) {
        case 0:
            battle_gfx_unpack_unit_shp_data(g_battle_gfx_spritesheet_record_data + g_battle_gfx_state_words[2] * 0x688,
                g_battle_gfx_state_words[3]);
            break;
        case 1:
            battle_load_seq_data(
                g_battle_gfx_spritesheet_alt_data + g_battle_gfx_state_words[2] * 0x408, g_battle_gfx_state_words[3]);
            break;
        case 2:
            battle_gfx_copy_shp_data_to_ram(
                g_battle_gfx_spritesheet_shp_data + g_battle_gfx_state_words[2] * 0xc40, g_battle_gfx_state_words[3]);
            break;
        case 3:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_spritesheet_seq_data + g_battle_gfx_state_words[2] * 0xc40, g_battle_gfx_state_words[3]);
            break;
        case 4:
            battle_gfx_copy_shp_data_to_ram(
                g_battle_gfx_body_part_sprite_data + g_battle_gfx_state_words[2] * 0xc40, g_battle_gfx_state_words[3]);
            break;
        case 5:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_body_part_seq_data + g_battle_gfx_state_words[2] * 0xc40, g_battle_gfx_state_words[3]);
            break;
        case 6:
            battle_gfx_load_wep_spr_data((u8*)g_battle_gfx_state_words[3]);
            break;
        case 7:
            battle_gfx_copy_other_spr_palette_data_to_ram((u8*)g_battle_gfx_state_words[3]);
            break;
        }
        {
            s32* next_phase = &g_battle_gfx_state_words[1];

            (*next_phase)++;
        }
        break;
    case 3:
        main_heap_call_free((void*)g_battle_gfx_state_words[3]);
        *phase = 0;
        switch (g_battle_gfx_state_words[0]) {
        case 0: {
            s32* counter = &g_battle_gfx_state_words[2];

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_state_words[0]++;
            }
            break;
        }
        case 1: {
            s32* counter = &g_battle_gfx_state_words[2];

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_state_words[0]++;
            }
            break;
        }
        case 2:
        case 3: {
            s32* counter = &g_battle_gfx_state_words[2];

            if (++*counter >= 2) {
                *counter = 0;
                g_battle_gfx_state_words[0]++;
            }
            break;
        }
        case 4:
        case 5: {
            s32* counter = &g_battle_gfx_state_words[2];

            if (++*counter >= 1) {
                *counter = 0;
                g_battle_gfx_state_words[0]++;
            }
            break;
        }
        case 6:
        case 7: {
            s32* kind = &g_battle_gfx_state_words[0];

            g_battle_gfx_state_words[2] = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }
    return g_battle_gfx_state_words[0];
}
