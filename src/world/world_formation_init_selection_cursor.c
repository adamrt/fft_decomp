#include "fft/world.h"

/* g_world_formation_cursor_position.y, bound separately: as a member store GCC
 * addresses it from the .x store's base and the code changes. */
extern s16 g_world_formation_cursor_position_y;

/*
 * Initialize the selected formation unit's stat callbacks and cursor position.
 *
 * The scroll update must report 10 stationary iterations before the cursor
 * coordinates are finalized. The four-column layout uses signed coordinates.
 */
void world_formation_init_selection_cursor(s16 formation_index) {
    s32 count;
    /* Pin: unpinned, the table base takes $v0 and the stored values $v1. */
    register world_formation_stat_callback_t* callbacks __asm__("$3");
    s16 x;
    s32 y;
    u8 offset;

    world_menu_init_scrollable_list_core(0, 0, 0);
    callbacks = (world_formation_stat_callback_t*)g_world_menu_script_callbacks;
    /* Keeps the table base in a register instead of eight absolute stores. */
    __asm__("" : "=r"(callbacks) : "0"(callbacks));
    callbacks[0].wide = world_get_formation_unit_stat_by_mode;
    callbacks[1].wide = world_get_formation_unit_stat_by_mode;
    callbacks[2].wide = world_formation_get_unit_egg_hatch_progress;
    callbacks[3].wide = world_formation_get_unit_egg_hatch_total;
    callbacks[4].halfword = world_formation_get_unit_level;
    callbacks[5].halfword = world_formation_get_unit_experience;
    callbacks[6].halfword = world_formation_get_unit_brave;
    callbacks[7].halfword = world_formation_get_unit_faith;
    g_world_formation_scroll_velocity = 0;
    D_801C833C = 0;
    g_world_formation_selected_unit_index = formation_index;
    count = 0;
    world_formation_stage_selected_unit();
    do {
        world_formation_step_scroll_velocity(&g_world_formation_scroll_velocity, &g_world_formation_scroll_position);
        if (g_world_formation_scroll_velocity == 0) {
            count++;
        }
    } while (count < 10);
    g_world_formation_scroll_enabled = world_formation_can_scroll_slots(g_world_formation_scroll_position);
    x = formation_index % 4;
    g_world_formation_cursor_position.x = x * 62 + 34;
    offset = g_world_formation_scroll_position;
    /* A second use keeps the lbu + sll/sra pair; combine otherwise folds it into lb. */
    __asm__("" : : "r"(offset));
    y = (s8)offset + (formation_index >> 2) * 60 + 61;
    g_world_formation_cursor_position_y = y;
    g_world_formation_selection_cursor_ready = 1;
    /* The target passes y in a0 to this argument-less callee. */
    ((void (*)(s32))world_formation_reset_slots_and_frame_tpages)(y);
}
