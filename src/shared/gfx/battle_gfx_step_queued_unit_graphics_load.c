#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_battle_gfx_unit_graphics_load_state holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. SHP/SEQ ids below 5, or equal to the one already resident, and
 * an already loaded spritesheet skip the read. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary. */
s32 battle_gfx_step_queued_unit_graphics_load(void) {
    s32 result;
    s32 id;

    result = 1;
    switch (g_battle_gfx_unit_graphics_load_state.phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .shp_id;
            if (id < 5 || id == g_battle_gfx_last_loaded_shp_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_shp_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_shp_files[id].sector,
                    g_battle_gfx_sprite_shp_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .seq_id;
            if (id < BATTLE_SEQ_RUKA || id == g_battle_gfx_last_loaded_seq_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_seq_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_seq_files[id].sector,
                    g_battle_gfx_sprite_seq_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            if (g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id
                >= 0xa0) {
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
            }
            if (battle_gfx_is_spritesheet_id_loaded(
                    g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                        .spritesheet_id)
                != 0) {
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL;
                result = 2;
                break;
            }
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(
                g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                    .size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .sector,
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .size,
                    g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (!main_file_is_still_loading()) {
            g_battle_gfx_unit_graphics_load_state.phase++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_fallback_shp_frame_data;
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_boss_shp_frame_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_seq_data_cursor = g_battle_gfx_fallback_seq_data;
            battle_gfx_unpack_unit_seq_data(
                &g_battle_gfx_boss_seq_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            battle_unit_init(
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_x,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_y,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_level,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].facing,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].palette_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].runtime_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].stats,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].flags,
                g_battle_gfx_unit_graphics_load_state.buffer);
            battle_unit_update_and_animate_units_three_times();
            break;
        }
        g_battle_gfx_unit_graphics_load_state.phase++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        if (g_battle_gfx_unit_graphics_load_state.buffer != 0) {
            main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        }
        g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_unit_graphics_load_state.file_kind++;
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_QUEUED_LOAD_SHP;
            g_battle_gfx_unit_graphics_load_state.index++;
            break;
        }
        result = 2;
        break;
    }
    return (g_battle_gfx_unit_graphics_load_state.index < g_main_gfx_unit_graphics_load_queue.count) ? result : 0;
}
