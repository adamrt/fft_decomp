#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/* Rotate a tile offset by the caller's facing and select its terrain layer.
 *
 * Returns -1 when the rotated offset leaves the map or lands on a blocked
 * lower tile whose upper tile is blocked too; otherwise 0 with `*out_flag`
 * selecting the upper (1) or lower (0) terrain layer.  Bit 8 of `flags` picks
 * the destination by absolute height instead of by proximity to the reference
 * tile's height.
 */
s32 battle_map_resolve_rotated_offset_layer(
    battle_stats_t* unit, s32 dx, s32 dy, s32 flags, s32* out_x, s32* out_y, s32* out_flag) {
    map_tile_t* current;
    map_tile_t* lower;
    /* Pins: unpinned, upper and upper_blocked swap $a2 and $a3. */
    register map_tile_t* upper __asm__("$6");
    s32 index;
    register s32 upper_index __asm__("$3");
    s32 lower_distance;
    s32 upper_distance;
    s32 previous;
    u32 upper_blocked;
    u8 current_height;
    u8 lower_height;
    u8 upper_height;

    switch (flags & 3) {
    case 1:
        previous = dx;
        dx = dy;
        dy = -previous;
        break;
    case 2:
        dx = -dx;
        dy = -dy;
        break;
    case 3:
        previous = dx;
        dx = -dy;
        dy = previous;
        break;
    }
    *out_x = unit->x + dx;
    *out_y = unit->position.bits.y + dy;
    if (*out_x < 0 || *out_x >= g_map_max_x) {
        return -1;
    }
    if (*out_y < 0 || *out_y >= g_map_max_y) {
        return -1;
    }
    current = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
    index = g_map_max_x * *out_y + *out_x;
    upper_index = index + 0x100;
    /* Builds index + 0x100 in $v1 before scaling instead of in $a2. */
    __asm__("" : "=r"(upper_index) : "0"(upper_index));
    upper = &g_battle_map_tile_data[upper_index];
    lower = &g_battle_map_tile_data[index];
    upper_blocked = upper->flags_06.value & 1;
    if ((lower->flags_06.value & 1) != 0) {
        /* Hides upper_blocked's 0/1 range so this byte test keeps its andi 0xff. */
        __asm__("" : "=r"(upper_blocked) : "0"(upper_blocked));
        if ((u8)upper_blocked != 0) {
            return -1;
        }
        *out_flag = 1;
        return 0;
    }
    if (upper_blocked != 0) {
        *out_flag = 0;
        return 0;
    }
    upper_height = upper->height;
    current_height = current->height;
    lower_height = lower->height;
    if ((flags & 0x100) != 0) {
        if (upper_height < lower_height) {
            *out_flag = 0;
            return 0;
        }
        *out_flag = 1;
        return 0;
    }
    if (lower_height > current_height) {
        lower_distance = lower_height - current_height;
    } else {
        lower_distance = current_height - lower_height;
    }
    if (upper_height > current_height) {
        upper_distance = upper_height - current_height;
    } else {
        upper_distance = current_height - upper_height;
    }
    if (upper_distance < lower_distance) {
        *out_flag = 1;
        return 0;
    }
    if (lower_distance < upper_distance) {
        *out_flag = 0;
        return 0;
    }
    if (upper_height < lower_height) {
        *out_flag = 0;
    } else {
        *out_flag = 1;
    }
    return 0;
}
