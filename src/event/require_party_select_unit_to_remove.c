#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define REQUIRE_MOVE_PARTY_PORTRAIT(party_index, portrait_rect, portrait_destination, palette_rect)                    \
    do {                                                                                                               \
        portrait_rect->x = (party_index % 7) * 8 + 0x100;                                                              \
        portrait_rect->y = (party_index / 7) * 0x30 + 0x28;                                                            \
        MoveImage(portrait_rect, portrait_destination[0], portrait_destination[1]);                                    \
        palette_rect->x = (party_index % 3) * 0x10 + 0x100;                                                            \
        palette_rect->y = party_index / 3;                                                                             \
        MoveImage(palette_rect, g_require_party_palette_destination, g_require_party_palette_destination_y);           \
    } while (0)

/* Let the player pick a party member to dismiss when the roster is full.

   Returns 1 when the opening prompt answers 1 or -1 (no unit removed) and 0
   after main_party_remove_unit.
   Matching notes: should_restore also serves as the loop's constant 1 (the
   target keeps 1 in that variable's register and materialises it before the
   pointer copies); the portrait pointers are assigned inside the loop so
   loop.c hoists them after the thread constants; the R1 search loop holds
   its whole switch-unit block (a loop that large keeps the 0x14 bound
   inside it), while the L1 search stays a do-while (with both blocks inside
   their loops should_restore outranks selected_battle_unit for s4); the
   formation search is a goto loop so the 3 bound stays in place. */
s32 require_party_select_unit_to_remove(void) {
    s32 party_index;
    u8* text;
    s32 selected_party_index;
    s32 result;
    s32 should_restore;
    s32 message_id;
    s32 formation_index;
    party_unit_t* party_unit;
    unit_stats_t* selected_battle_unit;
    RECT* portrait_rect;
    s16* portrait_destination;
    RECT* palette_rect;

    require_gfx_prepare_party_portrait_textures();
    text = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    require_panel_set_transition_value(1);
    {
        s32 name_character_index;

        for (party_index = 0; party_index < 0x14; party_index++) {
            party_unit = main_party_get_unit(party_index);
            if (party_unit->party_id == PARTY_ID_NONE || party_index >= 0x10 || party_unit->proposition_status != 0
                || (party_unit->gender_flags & UNIT_FLAG_EGG) != 0) {
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
        require_editor_load_party_unit(0, party_index);
        selected_party_index = party_index;
        break;
    }
    battle_menu_copy_billboard_data_in(
        &g_require_panel_selected_billboard, &g_require_active_unit_data, &g_require_editor_unit_fields);
    g_require_status_display_redraw_request = 1;
    g_require_numeric_editor_redraw_request = 1;
    g_require_character_status_redraw_request = 1;
    thread_yield();
    thread_start(REQUIRE_THREAD_SLOT_TRANSITION, require_gfx_run_suspended_thread_transition);
    thread_set_parameters(REQUIRE_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_gfx_screen_fade = 0xff;
    do {
        thread_yield();
        g_gfx_screen_fade -= 0x10;
    } while (g_gfx_screen_fade > 0);
    g_gfx_screen_fade = 0;
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
        thread_start(REQUIRE_THREAD_SLOT_MESSAGE, battle_menu_icon_linked_entry_thread);
        thread_set_parameters(REQUIRE_THREAD_SLOT_MESSAGE, (s32)&g_require_party_removal_menus[2], 0, 0);
        portrait_rect = &g_require_party_portrait_move_rect;
        portrait_destination = &g_require_party_portrait_destination;
        palette_rect = &g_require_party_palette_move_rect;
        thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MESSAGE);
        thread_wait_until_inactive(6);
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
        thread_start(REQUIRE_THREAD_SLOT_MESSAGE, battle_text_run_dialog_thread);
        thread_set_four_parameters(
            REQUIRE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[REQUIRE_THREAD_SLOT_MESSAGE].task_words[3] = 0x18;
        thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MESSAGE);
        continue;

    selection_menu:
        thread_start(REQUIRE_THREAD_SLOT_MENU, battle_menu_icon_linked_entry_thread);
        thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, (s32)&g_require_party_removal_menus[4], 0, 0);
        thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MENU);
        require_panel_set_transition_value(0);
        for (;;) {
            s32 helpmenu_thread;

            thread_yield();
            g_menu_input_controller = script_get_controller_input_pointer(0);
            if (*g_menu_input_controller & PSX_PAD_SELECT) {
                helpmenu_thread = battle_thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                thread_start(helpmenu_thread, require_overlay_run_helpmenu);
                thread_wait_until_inactive(helpmenu_thread);
                continue;
            }
            if (*g_menu_input_controller & PSX_PAD_R1) {
                g_menu_panel_status_animation[1] = 7;
                for (;;) {
                    selected_party_index++;
                    if (selected_party_index == 0x14)
                        selected_party_index = 0;
                    if (g_require_party_removable_flags[selected_party_index] != 0) {
                        g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
                        g_require_status_display_redraw_request = should_restore;
                        g_require_numeric_editor_redraw_request = should_restore;
                        g_require_character_status_redraw_request = should_restore;
                        require_editor_load_party_unit(1, selected_party_index);
                        REQUIRE_MOVE_PARTY_PORTRAIT(
                            selected_party_index, portrait_rect, portrait_destination, palette_rect);
                        battle_menu_copy_billboard_data_in(&g_require_panel_selected_billboard,
                            &g_require_active_unit_data, &g_require_editor_unit_fields);
                        break;
                    }
                }
            } else if (*g_menu_input_controller & PSX_PAD_L1) {
                g_menu_panel_status_animation[0] = 7;
                do {
                    selected_party_index--;
                    if (selected_party_index == -1)
                        selected_party_index = 0x13;
                } while (g_require_party_removable_flags[selected_party_index] == 0);
                g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
                g_require_status_display_redraw_request = 2;
                g_require_numeric_editor_redraw_request = should_restore;
                g_require_character_status_redraw_request = should_restore;
                require_editor_load_party_unit(1, selected_party_index);
                REQUIRE_MOVE_PARTY_PORTRAIT(selected_party_index, portrait_rect, portrait_destination, palette_rect);
                battle_menu_copy_billboard_data_in(
                    &g_require_panel_selected_billboard, &g_require_active_unit_data, &g_require_editor_unit_fields);
            }
            if (*g_menu_input_controller & PSX_PAD_CIRCLE)
                break;
        }

        {
            unit_stats_t* battle_unit;

            for (party_index = 0; party_index < 0x15; party_index++) {
                battle_unit = battle_unit_get_stats_from_battle_id(party_index);
                selected_battle_unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &message_id);
                if (message_id >= 0 && party_index == message_id
                    && battle_unit_get_runtime_id_by_battle_id(party_index) != -1)
                    break;
            }
        }
        party_unit = main_party_get_unit(selected_party_index);
        formation_index = 0;
        if (party_unit->sprite_set & CHARACTER_IDENTITY_HIGH_BIT_MASK) {
            message_id = ((party_unit->sprite_set - CHARACTER_IDENTITY_SELECTOR_FIRST) * 8)
                + (script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 7) + 0x1800;
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
            if (sprite_set != CHARACTER_IDENTITY_ENTD_NONE && sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
                message_id = formation_index + 0x1862;
            } else {
                message_id = formation_index + 0x1818;
            }
        }

        require_panel_set_transition_value(1);
        g_text_substitution_values[0] = selected_party_index;
        if (party_unit->sprite_set < CHARACTER_IDENTITY_RAMZA_END)
            goto show_unit_message;
        g_require_menu_selection_result = 0xff;
        thread_start(REQUIRE_THREAD_SLOT_MESSAGE, battle_text_run_dialog_thread);
        thread_set_four_parameters(
            REQUIRE_THREAD_SLOT_MESSAGE, 0x19, message_id, selected_battle_unit->unit_id, should_restore);
        g_threads[REQUIRE_THREAD_SLOT_MESSAGE].task_words[3] = 0x18;
        thread_start(REQUIRE_THREAD_SLOT_MENU, battle_menu_run_icon_selection_loop);
        thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, (s32)&g_require_party_removal_menus[0], 0, 0);
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

        text = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
        for (party_index = 0; party_index < 0x15; party_index++) {
            selected_battle_unit = battle_unit_get_stats_from_battle_id(party_index);
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
        thread_start(REQUIRE_THREAD_SLOT_MENU, battle_menu_icon_linked_entry_thread);
        thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, (s32)&g_require_party_removal_menus[5], 0, 0);
        thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MENU);
    }
    require_panel_set_transition_value(0);
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 0x10;
    } while (g_gfx_screen_fade < 0x100);
    g_gfx_screen_fade = 0xff;
    thread_set_parameters(REQUIRE_THREAD_SLOT_TRANSITION, 0, 0, 1);
    return result;
}
