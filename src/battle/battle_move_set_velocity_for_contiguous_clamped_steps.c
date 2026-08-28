#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/types.h"

enum {
    BATTLE_MOVE_STEP_WIDTH = 28,
    BATTLE_MOVE_STEP_HALF_WIDTH = 14,
    BATTLE_MOVE_STEP_NEAR_EDGE = -7,
    BATTLE_MOVE_STEP_FAR_EDGE = 35,
};

/*
 * Builds the per-frame velocity for a run of identical movement steps: walks
 * the remaining path while the steps keep the same direction quadrant and the
 * clamped ground height stays at the unit's current height, then hands the
 * accumulated edge-to-edge delta to the velocity solver.  The 0x80069744 twin
 * does the same for the final step, where it reads the tile directly.
 *
 * The case-3 form first preserves the prior y coordinate in `next`, then reuses
 * y for the path byte before forming the new coordinate. This is equivalent to
 * the direct expression and gives GCC the target's two-load lifetime, without
 * a separate `li v0,1`.
 */
void battle_move_set_velocity_for_contiguous_clamped_steps(
    battle_unit_misc_data_t* unit, const u8* path, const u8* step) {
    VECTOR direction;
    const u8* current;
    s32 start_x;
    s32 start_y;
    s32 start_z;
    s32 index;
    s32 x;
    s32 y;
    s32 end_x;
    s32 end_y;
    s32 end_z;
    s32 next;

    battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    x = unit->map_x;
    y = unit->map_y;
    index = unit->movement_path_offset - 1;
    start_x = unit->screen.vx;
    start_y = unit->screen.vz;
    start_z = unit->screen.vy;

    while (index < *path) {
        current = &path[index + 1];
        {
            /* Pin: the direction mask takes $v1 (unpinned it swaps with the step mask's $v0). */
            register u8 current_for_direction __asm__("$3");

            current_for_direction = *current;
            current_for_direction &= 0xc0;
            if (current_for_direction != (*step & 0xc0)) {
                break;
            }
        }
        switch (*current >> 6) {
        case 0:
            next = x + 1;
            x = next + (*current & 3);
            end_x = (x * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_NEAR_EDGE;
            end_y = (y * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_HALF_WIDTH;
            break;
        case 1:
            next = x - 1;
            x = next - (*current & 3);
            end_x = (x * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_FAR_EDGE;
            end_y = (y * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_HALF_WIDTH;
            break;
        case 2:
            next = y - 1;
            y = next - (*current & 3);
            end_x = (x * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_HALF_WIDTH;
            end_y = (y * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_FAR_EDGE;
            break;
        case 3:
            next = y + 1;
            y = *current;
            y = next + (y & 3);
            end_x = (x * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_HALF_WIDTH;
            end_y = (y * BATTLE_MOVE_STEP_WIDTH) + BATTLE_MOVE_STEP_NEAR_EDGE;
            break;
        }
        end_z = battle_move_clamp_z_to_tile_headroom(start_z, x, y, (*current >> 5) & 1);
        if (end_z != start_z) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_y - start_y;
    direction.vy = end_z - start_z;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_animation_speed;
    unit->velocity.vy = unit->velocity.vy * g_animation_speed;
    unit->velocity.vz = unit->velocity.vz * g_animation_speed;
}
