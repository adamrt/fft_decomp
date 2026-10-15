/* Proposition detail view push, its palette helpers and the accept input step. */

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Declared with an s32 value here: the target passes the entry value
 * without narrowing it. */

/* Opens the detail view for proposition entry `value`: shows message
 * 0xb879 + value with a chance from its save-data state byte (10, 20 or 40
 * percent), otherwise message 0xb80c with substitution value 0xd000 + value;
 * unpacks the proposition row, builds a render record with text 0x8800 +
 * value and pushes it as a type-9 level, hiding the parent level's windows.
 *
 * The target stores base_x/base_y through one pointer to the pair. */
void wldcore_proposition_push_detail_level(s32 value) {
    s32 chance;
    s32 index;
    s32 depth;
    u8 state;
    RECT bounds;
    point32_t* base;

    state = g_main_proposition_states[value];
    chance = (state & WLDCORE_PROPOSITION_STATE_FAILURE) ? 0x14 : 0;
    if (state & WLDCORE_PROPOSITION_STATE_BAD_FAILURE) {
        chance = 0x28;
    }
    if (!(state & (WLDCORE_PROPOSITION_STATE_FAILURE | WLDCORE_PROPOSITION_STATE_BAD_FAILURE))) {
        chance = 0xA;
    }
    if ((rand() * 100 >> 15) < chance) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, value + WLDCORE_TEXT_ID_PROPOSITION_HINT_FIRST, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PROPOSITION_NAME_BASE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_PROPOSITION_ABOUT, 0);
    }
    wldcore_unpack_proposition_row(g_wldcore_selected_proposition_row, value);
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_render.render_index = index;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = 0x80;
    bounds.x = 0;
    window_init_text_record_variant(
        index, bounds, 0, value + TEXT_ID_WORLD_MESSAGE_BASE, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    g_wldcore_window_render_records[index].priority = 9;
    depth = g_wldcore_menu_stack_depth;
    base->x = -0x7A;
    base->y = -0x28;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_types_next[depth] = WLDCORE_MENU_LEVEL_PROPOSITION_DETAIL;
}

/* Restore the proposition detail renderer's palette. */
void wldcore_window_restore_proposition_detail_palette(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_render_records[level->window_render.render_index].palette = 0;
}

/* Select palette 2 for the window's render record. */
void wldcore_window_set_render_palette_2(wldcore_window_render_pair_t* pair) {
    g_wldcore_window_render_records[pair->render_index].palette = 2;
}

/* Menu input step: cancel (0x40) closes this level and returns through
 * 0x80072a18; confirm (0x20) stores the 0x8009f2f4 + 0x8009f2f6 total, shows
 * message 0xb810 (with the total substituted) or 0xb80f when it is zero,
 * opens the yes/no panel at (0x48, 0x20), and pushes a level tagged 0xa. */
void wldcore_proposition_handle_accept_input(wldcore_window_render_pair_t* owner) {
    s32 total;
    point32_t origin;

    if (g_wldcore_window_render_records[owner->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_available_propositions();
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        total = g_wldcore_selected_proposition_row[0].fee_a + g_wldcore_selected_proposition_row[0].fee_b;
        g_wldcore_proposition_gil_amount = total;
        if (total != 0) {
            g_text_substitution_values[0] = total;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOB_FEE_PROMPT, 0);
        } else {
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_ACCEPT_JOB_PROMPT, 0);
        }
        wldcore_window_set_render_palette_2(owner);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT;
        g_wldcore_menu_stack_depth++;
    }
}
