#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* CD extent of one file: start sector and byte size. */
typedef struct battle_file_extent {
    s32 sector;
    s32 size;
} battle_file_extent_t;

extern battle_file_extent_t g_battle_gfx_sprite_seq_files[];
extern battle_file_extent_t g_battle_gfx_sprite_shp_files[];
extern battle_file_extent_t g_battle_gfx_spritesheet_files[];

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_battle_gfx_state_words holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. SHP/SEQ ids below 5, or equal to the one already resident, and
 * an already loaded spritesheet skip the read. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary.
 *
 * The phase word is read through a pointer in the shared read block: its
 * address register is what the target reuses there, while the other phase
 * accesses rematerialise the address. */
s32 battle_gfx_step_queued_unit_graphics_load(void) {
    s32 result;
    s32* phase;
    s32 id;
    s32 sector;
    s32 size;
    void* data;

    result = 1;
    switch (g_battle_gfx_state_words[1]) {
    case 0:
        switch (g_battle_gfx_state_words[0]) {
        case 0:
            id = g_battle_gfx_spritesheet_data[g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]]
                                                   .portrait_id]
                     .shp_id;
            if (id < 5 || id == g_battle_gfx_last_loaded_shp_id) {
                result = 2;
                g_battle_gfx_state_words[3] = 0;
                g_battle_gfx_state_words[1] = 3;
                break;
            }
            g_battle_gfx_last_loaded_shp_id = id;
            g_battle_gfx_state_words[3] = (s32)main_heap_alloc(g_battle_gfx_sprite_shp_files[id].size);
            if (g_battle_gfx_state_words[3] == 0) {
                break;
            }
            data = (void*)g_battle_gfx_state_words[3];
            sector = g_battle_gfx_sprite_shp_files[id].sector;
            size = g_battle_gfx_sprite_shp_files[id].size;
            phase = &g_battle_gfx_state_words[1];
            goto load;
        case 1:
            id = g_battle_gfx_spritesheet_data[g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]]
                                                   .portrait_id]
                     .seq_id;
            if (id < 5 || id == g_battle_gfx_last_loaded_seq_id) {
                result = 2;
                g_battle_gfx_state_words[3] = 0;
                g_battle_gfx_state_words[1] = 3;
                break;
            }
            g_battle_gfx_last_loaded_seq_id = id;
            g_battle_gfx_state_words[3] = (s32)main_heap_alloc(g_battle_gfx_sprite_seq_files[id].size);
            if (g_battle_gfx_state_words[3] == 0) {
                break;
            }
            data = (void*)g_battle_gfx_state_words[3];
            sector = g_battle_gfx_sprite_seq_files[id].sector;
            size = g_battle_gfx_sprite_seq_files[id].size;
            phase = &g_battle_gfx_state_words[1];
            goto load;
        case 2:
            if (g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].portrait_id >= 0xa0) {
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].portrait_id = 1;
            }
            if (battle_gfx_is_spritesheet_id_loaded(
                    g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].portrait_id)
                != 0) {
                g_battle_gfx_state_words[3] = 0;
                g_battle_gfx_state_words[1] = 2;
                result = 2;
                break;
            }
            data = main_heap_alloc(
                g_battle_gfx_spritesheet_files[g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]]
                                                   .portrait_id]
                    .size);
            g_battle_gfx_state_words[3] = (s32)data;
            if (data == 0) {
                break;
            }
            sector = g_battle_gfx_spritesheet_files[g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]]
                                                        .portrait_id]
                         .sector;
            size = g_battle_gfx_spritesheet_files[g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]]
                                                      .portrait_id]
                       .size;
            phase = &g_battle_gfx_state_words[1];
        load:
            if (main_file_call_build_header(sector, size, data) != 0) {
                main_heap_call_free((void*)g_battle_gfx_state_words[3]);
            } else {
                (*phase)++;
            }
            break;
        }
        break;
    case 1:
        if (!main_file_is_still_loading()) {
            g_battle_gfx_state_words[1]++;
        }
        break;
    case 2:
        switch (g_battle_gfx_state_words[0]) {
        case 0:
            g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_fallback_shp_frame_data;
            battle_gfx_unpack_unit_shp_data((battle_gfx_unit_shp_frame_tables_t*)g_battle_gfx_spritesheet_fallback,
                (u8*)g_battle_gfx_state_words[3]);
            break;
        case 1:
            g_battle_gfx_load_data_cursor = g_battle_gfx_fallback_seq_data;
            battle_load_seq_data((u32*)g_battle_gfx_spritesheet_alt_fallback, (u8*)g_battle_gfx_state_words[3]);
            break;
        case 2:
            battle_unit_init_misc_data(g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].map_x,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].map_y,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].map_level,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].map_height,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].portrait_id,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].palette_id,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].misc_id,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].battle_stats,
                g_unit_graphics_load_descriptors[g_battle_gfx_state_words[2]].flags, (u8*)g_battle_gfx_state_words[3]);
            battle_unit_update_and_animate_units_three_times();
            break;
        }
        g_battle_gfx_state_words[1]++;
        break;
    case 3:
        if (g_battle_gfx_state_words[3] != 0) {
            main_heap_call_free((void*)g_battle_gfx_state_words[3]);
        }
        g_battle_gfx_state_words[1] = 0;
        switch (g_battle_gfx_state_words[0]) {
        case 0:
        case 1:
            g_battle_gfx_state_words[0]++;
            break;
        case 2:
            g_battle_gfx_state_words[0] = 0;
            g_battle_gfx_state_words[2]++;
            break;
        }
        result = 2;
        break;
    }
    return (g_battle_gfx_state_words[2] < g_unit_graphics_load_count) ? result : 0;
}
