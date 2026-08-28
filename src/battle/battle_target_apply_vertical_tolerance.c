#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/* Keeps, per map position, only the terrain layer nearest a reference height.
 *
 * Heights are in half units (height * 2 + slope half-height + depth * 2). A
 * blocked tile clears its panel and counts as height -255. With single_layer
 * set, only the higher layer is considered. The nearer layer survives when it
 * lies within tolerance * 2 of the reference; the other panel is cleared.
 *
 * The upper layer index is a separate variable (j): writing [i + 0x100] lets
 * the front end fold the 0x100 into the symbol, and the target instead
 * strength-reduces 5 * j and 8 * j against hoisted table bases. Block-scoped
 * depth temporaries keep that value out of global allocation. */
void battle_target_apply_vertical_tolerance(u8 ref_height, u8 tolerance, s32 single_layer) {
    s32 max_delta;
    s32 ref;
    s32 i;
    s32 j;
    s32 lower;
    s32 upper;
    s32 lower_delta;
    s32 upper_delta;
    map_tile_t* tile;
    targeting_panel_entry_t* lower_panel;
    targeting_panel_entry_t* upper_panel;

    max_delta = tolerance * 2;
    ref = ref_height;
    for (i = 0; i < 0x100; i++) {
        tile = &g_battle_map_tile_data[i];
        lower_panel = &g_battle_target_panel_data[i];
        if (!tile->flags_06.bits.blocked) {
            u32 depth = tile->depth_half_height;
            lower = tile->height * 2 + (depth & MAP_TILE_HALF_HEIGHT_MASK) + (depth >> MAP_TILE_DEPTH_SHIFT) * 2;
        } else {
            lower = -0xFF;
            lower_panel->a = 0;
            lower_panel->b = 0;
        }
        j = i + 0x100;
        tile = &g_battle_map_tile_data[j];
        upper_panel = &g_battle_target_panel_data[j];
        if (!tile->flags_06.bits.blocked) {
            u32 depth = tile->depth_half_height;
            upper = tile->height * 2 + (depth & MAP_TILE_HALF_HEIGHT_MASK) + (depth >> MAP_TILE_DEPTH_SHIFT) * 2;
        } else {
            upper = -0xFF;
            upper_panel->a = 0;
            upper_panel->b = 0;
        }
        if (single_layer != 0) {
            if (lower >= upper) {
                upper = -0xFF;
            } else {
                lower = -0xFF;
            }
        }
        if (lower >= ref) {
            lower_delta = lower - ref;
        } else {
            lower_delta = ref - lower;
        }
        if (upper >= ref) {
            upper_delta = upper - ref;
        } else {
            upper_delta = ref - upper;
        }
        if (upper_delta < lower_delta) {
            lower_panel->a = 0;
            lower_panel->b = 0;
            if (max_delta < upper_delta) {
                upper_panel->a = 0;
                upper_panel->b = 0;
            }
        } else {
            upper_panel->a = 0;
            upper_panel->b = 0;
            if (max_delta < lower_delta) {
                lower_panel->a = 0;
                lower_panel->b = 0;
            }
        }
    }
}
