#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/option.h"
#include "fft/require.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define REQUIRE_MOVE_PARTY_PORTRAIT(party_index, portrait_rect, portrait_destination, palette_rect)                    \
    do {                                                                                                               \
        (portrait_rect)->x = ((party_index) % 7) * 8 + 0x100;                                                          \
        (portrait_rect)->y = ((party_index) / 7) * 0x30 + 0x28;                                                        \
        MoveImage((portrait_rect), (portrait_destination)[0], (portrait_destination)[1]);                              \
        (palette_rect)->x = ((party_index) % 3) * 0x10 + 0x100;                                                        \
        (palette_rect)->y = (party_index) / 3;                                                                         \
        MoveImage((palette_rect), g_require_party_palette_destination, g_require_party_palette_destination_y);         \
    } while (0)

/* Let the player pick a party member to dismiss when the roster is full.

   Returns 1 when the opening prompt answers 1 or -1 (no unit removed) and 0
   after main_party_remove_unit.
   Matching notes: should_restore also serves as the loop's constant 1 (the
   target keeps 1 in that variable's register and materialises it before the
   pointer copies); the portrait pointers are assigned inside the loop so
   loop.c hoists them after the thread constants; the empty do-while gives
   the goto loop the loop note reorg needs to fill its delay slot from the
   target without letting loop.c hoist the 0x14 bound; the formation search
   is a goto loop so the 3 bound stays in place. */
s32 require_select_party_unit_to_remove(void) {
    s32 party_index;
    u8* text;
    s32 selected_party_index;
    s32 result;
    s32 should_restore;
    s32 message_id;
    s32 formation_index;
    party_data_t* party_unit;
    battle_stats_t* selected_battle_unit;
    RECT* portrait_rect;
    s16* portrait_destination;
    RECT* palette_rect;

    require_prepare_party_portrait_textures();
    text = g_battle_text_section_pointers[8];
    require_panel_set_transition_value(1);
    {
        s32 name_character_index;

        for (party_index = 0; party_index < 0x14; party_index++) {
            party_unit = main_party_get_data_pointer(party_index);
            if (party_unit->party_id == 0xff || party_index >= 0x10 || party_unit->proposition_status != 0
                || (party_unit->gender_flags & 4) != 0) {
                g_require_party_removable_flags[party_index] = 0;
                *text = 0xfe;
                text++;
            } else {
                g_require_party_removable_flags[party_index] = 1;
                for (name_character_index = 0; name_character_index < 0x10; name_character_index++) {
                    *text = party_unit->name[name_character_index];
                    if ((*text & 0xfe) == 0xfe) {
                        text++;
                        break;
                    }
                    text++;
                    if (name_character_index == 0xf)
                        *text++ = 0xfe;
                }
            }
        }
    }

    for (party_index = 0; party_index < 0x14; party_index++) {
        if (g_require_party_removable_flags[party_index] == 0)
            continue;
        require_load_party_unit_into_editor(0, party_index);
        selected_party_index = party_index;
        break;
    }
    battle_action_copy_active_turn_data_from(
        g_require_panel_active_unit_banner, g_require_active_unit_data, g_require_panel_billboard_data);
    g_require_status_display_redraw_request = 1;
    g_require_numeric_editor_redraw_request = 1;
    g_require_character_status_redraw_request = 1;
    battle_thread_yield();
    battle_thread_start(10, require_gfx_run_suspended_thread_transition);
    battle_thread_set_parameters(10, 0, 0, 0);
    g_battle_screen_fade = 0xff;
    do {
        battle_thread_yield();
        g_battle_screen_fade -= 0x10;
    } while (g_battle_screen_fade > 0);
    g_battle_screen_fade = 0;
    {
        s16* destination;

        destination = &g_require_party_portrait_destination;
        g_require_party_portrait_move_rect.x = (selected_party_index % 7) * 8 + 0x100;
        g_require_party_portrait_move_rect.y = (selected_party_index / 7) * 0x30 + 0x28;
        MoveImage(&g_require_party_portrait_move_rect, destination[0], destination[1]);
        g_require_party_palette_move_rect.x = (selected_party_index % 3) * 0x10 + 0x100;
        g_require_party_palette_move_rect.y = selected_party_index / 3;
        MoveImage(&g_require_party_palette_move_rect, g_require_party_palette_destination,
            g_require_party_palette_destination_y);
    }

    should_restore = 1;

    for (;;) {
        g_require_party_removal_result = 0xff;
        battle_thread_start(7, battle_menu_icon_linked_entry_thread);
        battle_thread_set_parameters(7, (s32)&g_require_party_removal_menus[2], 0, 0);
        portrait_rect = &g_require_party_portrait_move_rect;
        portrait_destination = &g_require_party_portrait_destination;
        palette_rect = &g_require_party_palette_move_rect;
        battle_thread_wait_until_inactive(7);
        battle_thread_wait_until_inactive(6);
        if (g_require_party_removal_result == should_restore || g_require_party_removal_result == -1) {
            require_panel_set_transition_value(0);
            should_restore = 0;
            result = 1;
            break;
        }
        /* The target keeps the story-unit message branch here, out of line
           at the loop head; written at its test it compiles in place. */
        goto selection_menu;

    show_unit_message:
        battle_thread_start(7, battle_text_character_handling_thread);
        battle_thread_set_parameters_4(7, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_battle_threads[7].task_words[3] = 0x18;
        battle_thread_wait_until_inactive(7);
        continue;

    selection_menu:
        battle_thread_start(5, battle_menu_icon_linked_entry_thread);
        battle_thread_set_parameters(5, (s32)&g_require_party_removal_menus[4], 0, 0);
        battle_thread_wait_until_inactive(5);
        require_panel_set_transition_value(0);
        for (;;) {
            s32 helpmenu_thread;

            battle_thread_yield();
            g_require_input_controller = battle_script_get_controller_input_pointer(0);
            if (*g_require_input_controller & PSX_PAD_SELECT) {
                helpmenu_thread = battle_thread_resolve_id(0x10);
                battle_thread_start(helpmenu_thread, require_overlay_run_helpmenu);
                battle_thread_wait_until_inactive(helpmenu_thread);
                continue;
            }
            if (*g_require_input_controller & PSX_PAD_R1) {
                g_require_panel_status_animation[1] = 7;
                do {
                } while (0); /* loop note only; see function comment */
            next_party:
                selected_party_index++;
                if (selected_party_index == 0x14)
                    selected_party_index = 0;
                if (g_require_party_removable_flags[selected_party_index] == 0)
                    goto next_party;
                g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
                g_require_status_display_redraw_request = should_restore;
                g_require_numeric_editor_redraw_request = should_restore;
                g_require_character_status_redraw_request = should_restore;
                require_load_party_unit_into_editor(1, selected_party_index);
                REQUIRE_MOVE_PARTY_PORTRAIT(selected_party_index, portrait_rect, portrait_destination, palette_rect);
                battle_action_copy_active_turn_data_from(
                    g_require_panel_active_unit_banner, g_require_active_unit_data, g_require_panel_billboard_data);
            } else if (*g_require_input_controller & PSX_PAD_L1) {
                g_require_panel_status_animation[0] = 7;
                do {
                    selected_party_index--;
                    if (selected_party_index == -1)
                        selected_party_index = 0x13;
                } while (g_require_party_removable_flags[selected_party_index] == 0);
                g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
                g_require_status_display_redraw_request = 2;
                g_require_numeric_editor_redraw_request = should_restore;
                g_require_character_status_redraw_request = should_restore;
                require_load_party_unit_into_editor(1, selected_party_index);
                REQUIRE_MOVE_PARTY_PORTRAIT(selected_party_index, portrait_rect, portrait_destination, palette_rect);
                battle_action_copy_active_turn_data_from(
                    g_require_panel_active_unit_banner, g_require_active_unit_data, g_require_panel_billboard_data);
            }
            if (*g_require_input_controller & PSX_PAD_CIRCLE)
                break;
        }

        {
            battle_stats_t* battle_unit;

            for (party_index = 0; party_index < 0x15; party_index++) {
                battle_unit = (battle_stats_t*)battle_unit_get_stats_from_battle_id(party_index);
                selected_battle_unit
                    = battle_find_unit_data_pointer_for_entd_unit_id(battle_unit->unit_id, &message_id);
                if (message_id >= 0 && party_index == message_id
                    && battle_unit_get_misc_id_by_battle_id(party_index) != -1)
                    break;
            }
        }
        party_unit = main_party_get_data_pointer(selected_party_index);
        formation_index = 0;
        if (party_unit->sprite_set & 0x80) {
            /* The target also passes a1 = 0 to this one-argument callee. */
            message_id = ((party_unit->sprite_set - 0x80) * 8)
                + (((s32 (*)(s32, s32))battle_script_get_variable)(0x21, 0) & 7) + 0x1800;
        } else {
            s32 sprite_column;
            u8* sprite_group;
            u32 sprite_set;

            sprite_set = party_unit->sprite_set;
            sprite_group = g_require_gfx_formation_sprite_groups[0];
        next_formation:
            for (sprite_column = 0; sprite_column < 3; sprite_column++) {
                if (sprite_set == sprite_group[sprite_column])
                    break;
            }
            if (sprite_column == 3) {
                formation_index++;
                sprite_group += 3;
                if (formation_index < 0x49)
                    goto next_formation;
            }
            if (formation_index == 0x49)
                formation_index = 0;
            if (formation_index == 0x47)
                formation_index = 0x48;
            if (sprite_set != 0 && sprite_set < 4) {
                message_id = formation_index + 0x1862;
            } else {
                message_id = formation_index + 0x1818;
            }
        }

        require_panel_set_transition_value(1);
        g_battle_text_substitution_values[0] = selected_party_index;
        if (party_unit->sprite_set < 4)
            goto show_unit_message;
        g_require_menu_selection_result = 0xff;
        battle_thread_start(7, battle_text_character_handling_thread);
        battle_thread_set_parameters_4(7, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_battle_threads[7].task_words[3] = 0x18;
        battle_thread_start(5, battle_menu_run_icon_selection_loop);
        battle_thread_set_parameters(5, (s32)&g_require_party_removal_menus[0], 0, 0);
        require_menu_wait_selection_threads();
        if (g_require_menu_selection_result != 0)
            continue;
        should_restore = require_party_restore_item_counts(selected_party_index);
        result = 0;
        main_party_remove_unit(selected_party_index);
        require_party_update_bio_variable_for_unit_class(party_unit->sprite_set, 0xc);
        break;
    }

    {
        s32 name_character_index;

        text = g_battle_text_section_pointers[8];
        for (party_index = 0; party_index < 0x15; party_index++) {
            selected_battle_unit = (battle_stats_t*)battle_unit_get_stats_from_battle_id(party_index);
            for (name_character_index = 0; name_character_index < 0x10; name_character_index++) {
                *text = selected_battle_unit->name[name_character_index];
                if ((*text & 0xfe) == 0xfe) {
                    text++;
                    break;
                }
                text++;
                if (name_character_index == 0xf)
                    *text++ = 0xfe;
            }
        }
    }
    if (should_restore != 0) {
        battle_thread_start(5, battle_menu_icon_linked_entry_thread);
        battle_thread_set_parameters(5, (s32)&g_require_party_removal_menus[5], 0, 0);
        battle_thread_wait_until_inactive(5);
    }
    require_panel_set_transition_value(0);
    g_battle_screen_fade = 0;
    do {
        battle_thread_yield();
        g_battle_screen_fade += 0x10;
    } while (g_battle_screen_fade < 0x100);
    g_battle_screen_fade = 0xff;
    battle_thread_set_parameters(10, 0, 0, 1);
    return result;
}
