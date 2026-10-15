#include "fft/battle_gfx.h"

/* Appends one unit to the graphics load queue that
 * battle_gfx_step_queued_unit_graphics_load works through.
 *
 * A spritesheet id of BATTLE_SPRITESHEET_ID_COUNT or more is replaced by
 * Ramza's chapter 1 sheet. The count stops at 16, the size of the
 * queue, so a seventeenth call writes the record one past its end. */
void battle_gfx_append_unit_graphics_load_descriptor(s32 map_x, s32 map_y, s32 map_level, s16 facing,
    s16 spritesheet_id, s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags) {
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_x = map_x;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_y = map_y;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_level = map_level;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].facing = facing;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
        = spritesheet_id;
    if (spritesheet_id >= BATTLE_SPRITESHEET_ID_COUNT) {
        g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
            = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
    }
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].palette_id = palette_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].runtime_id = runtime_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].stats = unit;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].flags = flags;
    if (g_main_gfx_unit_graphics_load_queue.count < BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        g_main_gfx_unit_graphics_load_queue.count++;
    }
}
