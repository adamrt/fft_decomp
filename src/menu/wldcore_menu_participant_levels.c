/* Active proposition list and participant list/detail levels. */

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Restore the active-proposition list's palettes and cursor animation. */
void wldcore_window_clear_layout_states(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->proposition_list.cursor_window].palette
        = g_wldcore_window_records[level->proposition_list.title_window].palette
        = g_wldcore_window_records[level->proposition_list.count_window].palette
        = g_wldcore_window_records[level->proposition_list.days_window].palette
        = g_wldcore_window_render_records[level->proposition_list.render_index].palette = 0;
    g_wldcore_window_records[level->proposition_list.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->proposition_list.cursor_window].frame_index
        = g_wldcore_window_records[level->proposition_list.cursor_window].anim_counter = 0;
}

/* Select the palettes for the active-proposition list and its four windows. */
void wldcore_window_set_layout_states(wldcore_menu_proposition_list_level_t* level) {
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->title_window].palette = g_wldcore_window_records[level->count_window].palette
        = g_wldcore_window_records[level->days_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
}

/* Input handler for the proposition list level (type 0x26).
 *
 * The level's first five 32-bit values are the window/render indices that
 * wldcore_window_set_layout_states consumes. The wrap-around move is spelled as
 * a subtraction of a negative step: that keeps the window's y operand first
 * in the addition, as the target's register assignment requires. */
void wldcore_list_handle_active_propositions_input(wldcore_menu_proposition_list_level_t* level) {
    u32 buttons;
    s32 count;
    s32 window;

    if ((g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) != 0) {
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if ((buttons & PSX_PAD_SELECT) != 0) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        wldcore_window_set_layout_states(level);
        wldcore_menu_push_message_level(0x105A, 1);
        return;
    }
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_active_propositions_saved_cursor[0] = 0;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_active_propositions_saved_cursor[0] = level->cursor;
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_push_participant_list_level(level->entries[level->cursor]);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->entry_count >= 2) {
            if (level->cursor != 0) {
                g_wldcore_window_records[level->cursor_window].position.y -= 0x10;
                level->cursor = level->cursor - 1;
            } else {
                g_wldcore_window_records[level->cursor_window].position.y -= (level->entry_count - 1) * -0x10;
                level->cursor = level->entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->cursor < count) {
                g_wldcore_window_records[level->cursor_window].position.y += 0x10;
                level->cursor = level->cursor + 1;
            } else {
                g_wldcore_window_records[level->cursor_window].position.y -= count * 0x10;
                level->cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}

/* Pushes the proposition participant list level (type 0x27) for
 * g_main_active_propositions[proposition].
 *
 * The 0x40 frame needs 32 bytes of locals the body never uses. The `- -`
 * spellings keep the target's operand order (render-record base first) that a
 * plain `+` of a loaded field loses to expand_binop's commutative swap. */
void wldcore_menu_push_participant_list_level(s32 proposition) {
    /* Target frame evidence establishes 32 bytes of otherwise unused locals. */
    u8 stack_scratch[32];
    s32 depth;
    s32 window;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_render.window_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    window = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant_list.cursor_window = window;
    g_wldcore_window_records[window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[window].priority = 8;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    wldcore_list_build_proposition_detail_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth], proposition, 1);
    g_text_substitution_values[0]
        = g_main_active_propositions[proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.x
        + 0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].participant_list.cursor_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].participant_list.render_index].base.y
        - -(g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index * 16)
        - -(g_wldcore_menu_stack_records_next[depth].participant_list.panel_rows * 16 + 0xE);
    g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        = g_main_active_propositions[proposition].participant_count;
    if (g_wldcore_menu_stack_records_next[depth].participant_list.entry_count
        < g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index + 1) {
        g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index = 0;
    }
    g_wldcore_menu_stack_records_next[depth].participant_list.cursor
        = g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST].selected_index;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].participant_list.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_LIST;
}

/* Input step of the proposition participant-chooser level.
 *
 * Nothing happens while the render record is hidden (flag 0x100). Once WORLD
 * thread 14's closing animation has finished, the level hands the selected
 * row to wldcore_menu_push_participant_detail_level. Cancel (0x40) tears the level down and returns to the
 * proposition list; confirm (0x20) records the row in g_wldcore_participant_list_saved_cursor and starts
 * thread 14 instead. Both drop one window record, one entry of g_wldcore_window_render_record_count and
 * two render objects. Up/down wrap the cursor and slide the cursor window by
 * one 16-pixel row, and do nothing when there are fewer than two rows.
 *
 * The two directional blocks mirror wldcore_list_handle_active_propositions_input: the up wrap is spelled as
 * a subtraction of a negative step so the window's y stays the first addend,
 * and the down block decrements its own count in place. */
void wldcore_menu_handle_participant_list_input(wldcore_menu_stack_record_t* level) {
    s32 count;

    if (g_wldcore_window_render_records[level->participant_list.render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_wldcore_menu_stack_depth--;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_menu_push_participant_detail_level(level->participant_list.proposition, level->participant_list.cursor);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_participant_list_saved_cursor = 0;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_active_propositions();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_window_record_count--;
        g_wldcore_participant_list_saved_cursor = level->participant_list.cursor;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->participant_list.entry_count >= 2) {
            if (level->participant_list.cursor != 0) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= 0x10;
                level->participant_list.cursor = level->participant_list.cursor - 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y
                    -= (level->participant_list.entry_count - 1) * -0x10;
                level->participant_list.cursor = level->participant_list.entry_count - 1;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        count = level->participant_list.entry_count;
        if (count >= 2) {
            count = count - 1;
            if (level->participant_list.cursor < count) {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y += 0x10;
                level->participant_list.cursor = level->participant_list.cursor + 1;
            } else {
                g_wldcore_window_records[level->participant_list.cursor_window].position.y -= count * 0x10;
                level->participant_list.cursor = 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        }
    }
}

/* Push the proposition participant detail level (type 0x28).
 *
 * Copies the 0x40x0x100 VRAM block at (0x180, 0) to (0x200, 0x100), appends
 * the two frame windows, draws the selected participant and restarts the
 * three core background threads. Each window has its own pos/window pointer:
 * a single-set pointer keeps its address computation beside its first use.
 */
void wldcore_menu_push_participant_detail_level(s32 proposition, s32 participant) {
    RECT rect;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    point32_t* pos1;
    point32_t* pos2;
    s32 index;
    s32 depth;

    rect.x = 0x180;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x200, 0x100);
    DrawSync(0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.proposition = proposition;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.participant = participant;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.left_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x74;
    pos1->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.right_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x60;
    pos2->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    wldcore_unit_build_status_panel_data(g_main_active_propositions[proposition].participant_indices[participant]);

    g_wldcore_thread8_params.x = 0;
    g_wldcore_thread8_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread9_params.y = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_wldcore_thread8_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].participant._unknown_08 = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Loads party member party_index into the scratch unit at 0x800d467c and
 * fills the status-panel records the participant detail level draws.
 *
 * Empty hand slots fall back to the shield; empty equipment reads as -1 and
 * a zero ability id is decremented to -1. Monsters show their primary
 * skillset's first five abilities instead of their equipped ability set. */
void wldcore_unit_build_status_panel_data(s32 party_index) {
    unit_stats_t* unit;
    menu_unit_status_record_t* gauges;
    menu_unit_status_identity_t* identity;
    menu_status_panel_slot_state_t* detail;
    u16* abilities;
    s32 i;
    u8 flags;
    u16 maximum_hp;
    u16 maximum_mp;

    if (main_unit_init_job_data(&g_wldcore_status_unit, party_index, 1) == -1) {
        return;
    }
    unit = &g_wldcore_status_unit;
    gauges = &g_menu_selected_unit_stat_summary;
    identity = &g_world_selected_unit_identity;
    detail = &g_world_selected_unit_stat_detail.state;
    gauges->team_kind = 0;
    gauges->level = unit->level;
    flags = unit->team_flags;
    if (flags & BATTLE_TEAM_MASK) {
        gauges->team_kind = 1;
    }
    if (!(flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        gauges->team_kind = 2;
    }
    gauges->list_index = 0;
    gauges->unit_count = 0;
    gauges->experience = unit->experience;
    maximum_hp = unit->max_hp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->hp;
    maximum_mp = unit->max_mp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->mp;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = unit->ct;
    identity->job_id = unit->job_id;
    identity->brave = unit->brave;
    gauges->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    gauges->unit_index = party_index;
    identity->unit_index = party_index;
    identity->roster_slot = party_index;
    identity->faith = unit->faith;
    identity->zodiac = unit->birthday.zodiac;
    detail->move = unit->move;
    detail->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    detail->jump = unit->jump;
    detail->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    detail->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    detail->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    detail->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    detail->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    detail->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE]) {
        detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    }
    detail->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    detail->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    detail->magical_class_evade = 0;
    detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE]) {
        detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    }
    detail->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    detail->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]
        = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    detail->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND]
        = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    detail->equipment[WORLD_EQUIPMENT_SLOT_HEAD] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    detail->equipment[WORLD_EQUIPMENT_SLOT_BODY] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    detail->equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        if (detail->equipment[i] == 0 || detail->equipment[i] == ITEM_ID_NONE) {
            detail->equipment[i] = -1;
        }
    }
    detail->generic_monster = unit->unit_flags & UNIT_FLAG_MONSTER;
    if (detail->generic_monster != 0) {
        abilities = main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        for (i = 0; i < 5; i++) {
            detail->ability_ids[i] = *abilities++;
        }
    } else {
        detail->ability_ids[0] = unit->primary_skillset;
        detail->ability_ids[1] = unit->secondary_skillset;
        detail->ability_ids[2] = unit->abilities.reaction;
        detail->ability_ids[3] = unit->abilities.support;
        detail->ability_ids[4] = unit->abilities.movement;
    }
    for (i = 0; i < 5; i++) {
        if (detail->ability_ids[i] == 0) {
            detail->ability_ids[i]--;
        }
    }
}

/* Restore the participant detail windows' palettes. */
void wldcore_window_clear_pair_state(wldcore_menu_stack_record_t* level) {
    s32 second = level->participant.left_window;

    g_wldcore_window_records[level->participant.right_window].palette = 0;
    g_wldcore_window_records[second].palette = 0;
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL: nothing to publish. */
void wldcore_menu_publish_participant_detail_level(void) {
}

/*
 * Input step of the proposition participant detail level (the unit status
 * panels of one unit sent on a job).
 *
 * Circle or Cross asks the three panel threads to close and blocks input;
 * once the status banner thread has ended the level pops, the saved screen
 * strip is moved back, line 0xB849 is shown with the proposition's name and
 * the participant list level is pushed again. Select opens the layout window.
 * R1 and L1 step to the next or previous participant with wrap-around and
 * request a redraw of the panels (banner direction 1 or 2); with a single
 * participant they only play the page sound. The L1/R1 prompts show their
 * pressed sequence while held, and one button is ignored while the other's
 * pressed sequence is showing.
 */
void wldcore_menu_handle_participant_detail_input(wldcore_menu_participant_level_t* level) {
    RECT rect;
    u32 buttons;
    s32 count;
    s32 direction;
    s32 participant;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) == 0) {
        rect.x = 0x200;
        rect.y = 0x100;
        rect.w = 0x40;
        rect.h = 0x100;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        g_wldcore_menu_stack_depth--;
        MoveImage(&rect, 0x180, 0);
        DrawSync(0);
        g_text_substitution_values[0]
            = g_main_active_propositions[level->proposition].proposition_id | TEXT_ID_PROPOSITION_NAME_BASE;
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
        wldcore_menu_push_participant_list_level(level->proposition);
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_object_count -= 2;
        thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
            = 0xA;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_menu_push_layout_window_level();
        return;
    }
    if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter
            = g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
    }
    if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
        && g_wldcore_window_records[level->left_window].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
        g_wldcore_window_records[level->right_window].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
        g_wldcore_window_records[level->right_window].frame_index
            = g_wldcore_window_records[level->right_window].anim_counter = 0;
        count = g_main_active_propositions[level->proposition].participant_count;
        if (count == 1) {
            /* Shared tail: both single-participant cases branch to the closing
             * sound effect; nesting the redraw under `count != 1` changes the
             * register allocation. */
            goto done;
        }
        participant = level->participant + 1;
        if (participant >= count) {
            level->participant = 0;
        } else {
            level->participant = participant;
        }
        direction = 1;
    } else {
        if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
            || g_wldcore_window_records[level->right_window].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
            return;
        }
        g_wldcore_window_records[level->left_window].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
        g_wldcore_window_records[level->left_window].frame_index
            = g_wldcore_window_records[level->left_window].anim_counter = 0;
        if (g_main_active_propositions[level->proposition].participant_count == 1) {
            goto done;
        }
        if (level->participant != 0) {
            level->participant--;
        } else {
            level->participant = g_main_active_propositions[level->proposition].participant_count - 1;
        }
        direction = 2;
    }
    g_wldcore_thread8_params.redraw_request = direction;
    wldcore_unit_build_status_panel_data(
        g_main_active_propositions[level->proposition].participant_indices[level->participant]);
    g_wldcore_threadc_params.redraw_request = 1;
    g_wldcore_thread9_params.redraw_request = 1;
done:
    wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
}
