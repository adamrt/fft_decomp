#include "fft/battle_gfx.h"

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_battle_gfx_unit_graphics_load_state.file_kind selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_battle_gfx_unit_graphics_load_state.phase the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_battle_gfx_unit_graphics_load_state.index the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_battle_gfx_unit_graphics_load_state.file_kind load above the counter store, as the target does. */
s32 battle_gfx_step_load_unit_shp_seq_data(void) {
    s32* phase;
    s32* index;
    void** slot;
    void* buffer;

    phase = &g_battle_gfx_unit_graphics_load_state.phase;
    switch (*phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_shp_files[*index].sector, g_battle_gfx_sprite_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_seq_files[*index].sector, g_battle_gfx_sprite_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_shp_files[*index].sector, g_battle_gfx_weapon_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_seq_files[*index].sector, g_battle_gfx_weapon_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_shp_files[*index].sector, g_battle_gfx_effect_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_seq_files[*index].sector, g_battle_gfx_effect_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_WEP_SPR, 0x15000, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_OTHER_SPR, 0x8800, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (main_file_is_still_loading() != 0) {
            break;
        }
        (*phase)++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_unit_shp_frame_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            battle_gfx_unpack_unit_seq_data(&g_battle_gfx_unit_seq_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            battle_gfx_load_wep_spr_data(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            battle_gfx_copy_other_spr_palette_data_to_ram(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        }
        {
            s32* next_phase = &g_battle_gfx_unit_graphics_load_state.phase;

            (*next_phase)++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        *phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 2) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 1) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR: {
            s32* kind = &g_battle_gfx_unit_graphics_load_state.file_kind;

            g_battle_gfx_unit_graphics_load_state.index = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }
    return g_battle_gfx_unit_graphics_load_state.file_kind;
}
