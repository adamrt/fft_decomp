/* The name entry keyboard, including frames, the key list, confirmed keys, the cursor, key application and the
 * confirmation menu. */

#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void world_name_apply_entry_key(s16 key);
static void world_name_init_entry(const u8* name);
static void world_name_update_entry_cursor(void);

u8* world_name_run_entry_keyboard_frame(const u8* list, s32 unused_1) {
    point16_t cursor;
    s32 busy;

    if (g_world_name_entry_started == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_name_keyboard_last_state = -1;
            world_name_init_entry(list);
            g_world_name_entry_started = 1;
        }
        return 0;
    }
    if (g_world_name_entry_state != 0xFF) {
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_menu_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_DELETE);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        cursor.x = g_world_name_entry_cursor * 6 + 0x54;
        gfx_set_otag_index(0x28);
        menu_update_and_draw_bouncing_cursor_variant(&cursor, &g_world_name_keyboard_text_cursor_anim, busy);
        menu_run_script_with_palette_mode_variant(&g_world_name_entry_display_script, 0, busy);
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50, g_menu_input_primary_repeat & PSX_PAD_L1, busy, MAIN_SFX_PAGE_SWITCH);
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50, g_menu_input_primary_repeat & PSX_PAD_R1, busy, MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode_variant(
            &g_world_name_entry_keyboard_script, g_menu_input_primary_repeat, busy);
        if (g_world_name_entry_state != g_world_name_keyboard_last_state) {
            g_world_name_keyboard_last_state = g_world_name_entry_state;
        }
        return 0;
    }
    g_world_name_entry_started = 0;
    return g_world_name_entry_text;
}

/*
 * Run the WORLD name-entry screen until its state reaches 0xFF.
 *
 * Binds both packet buffers onto a stack-resident pool block, then each frame
 * presents the previous ordering table, redraws the keyboard image, steps the
 * current name-entry handler, draws the cursor and key prompts, and dispatches
 * the queued sound effect. Returns the encoded name.
 *
 * value is one local reused for the cursor x, the drawn-state compare and the
 * sound id; the shared pseudo conflicts with $a0 and keeps all three in $v1.
 */
u8* world_name_run_entry_screen(const u8* list, u32* image) {
    point16_t cursor;
    POLY_FT4 textured_quads[2 * 220];
    TILE tiles[2 * 4];
    LINE_F2 flat_lines[2 * 4];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 10];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    DRAWENV env;
    u32* otag;
    s32 busy;
    s32 value;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0xDC;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0;
    g_world_gfx_textured_gradient_quad_capacity = 0;
    g_world_gfx_flat_line_capacity = 4;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 4;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 30;
    g_gfx_draw_mode_capacity = 30;
    g_world_gfx_draw_area_capacity = 10;
    world_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, 0, 0, flat_lines, 0, 0, 0, 0, 0,
        tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    g_world_menu_context_id = WORLD_MENU_CONTEXT_NAME_ENTRY;
    world_formation_init_menu_display();
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
    world_name_init_entry(list);
    g_world_name_entry_last_state = -1;
    world_gfx_set_fade_disabled(1);
    menu_input_update_with_message_state_variant();
    while (g_world_name_entry_state != 0xFF) {
        world_gfx_update_fade_in_tile();
        main_system_frame_stub();
        g_gfx_poly_f3_count = 0;
        g_gfx_poly_ft3_count = 0;
        g_world_gfx_poly_f4_count = 0;
        g_menu_gfx_poly_ft4_count = 0;
        g_gfx_poly_g3_count = 0;
        g_gfx_poly_gt3_count = 0;
        g_gfx_poly_g4_count = 0;
        g_menu_gfx_poly_gt4_count = 0;
        g_world_gfx_flat_line_count = 0;
        g_gfx_line_f3_count = 0;
        g_gfx_line_f4_count = 0;
        g_world_gfx_gradient_line_count = 0;
        g_gfx_line_g3_count = 0;
        g_gfx_line_g4_count = 0;
        g_gfx_tile_count = 0;
        g_gfx_tile_1_count = 0;
        g_gfx_tile_8_count = 0;
        g_gfx_tile_16_count = 0;
        g_gfx_sprite_count = 0;
        g_gfx_sprite_8_count = 0;
        g_gfx_sprite_16_count = 0;
        g_menu_gfx_draw_move_count = 0;
        g_world_gfx_draw_area_count = 0;
        otag = g_menu_gfx_context->otag;
        GetDrawEnv(&env);
        g_world_gfx_draw_buffer_clip_y = env.clip.y;
        while (DrawSync(1) != 0) { }
        VSync(g_menu_event_speed);
        {
            menu_gfx_context_t* context = g_world_gfx_packet_buffers;

            if (g_menu_gfx_context == context) {
                context++;
            }
            g_menu_gfx_context = context;
            PutDispEnv(&context->disp_env);
        }
        PutDrawEnv(&g_menu_gfx_context->draw_env);
        g_world_name_entry_screen_image_rect.y = g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0;
        gfx_load_image_and_wait(&g_world_name_entry_screen_image_rect, image);
        DrawOTag(otag);
        ClearOTag(g_menu_gfx_context->otag, g_world_ot_length);
        menu_input_update_with_message_state_variant();
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_menu_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_DELETE);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        value = g_world_name_entry_cursor * 6 + 0x54;
        cursor.x = value;
        gfx_set_otag_index(0x28);
        menu_update_and_draw_bouncing_cursor_variant(&cursor, &g_world_name_entry_text_cursor_anim, busy);
        menu_run_script_with_palette_mode_variant(&g_world_name_entry_display_script, 0, busy);
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50, g_menu_input_primary_repeat & PSX_PAD_L1, busy, MAIN_SFX_PAGE_SWITCH);
        world_menu_draw_pressable_button(
            MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50, g_menu_input_primary_repeat & PSX_PAD_R1, busy, MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode_variant(
            &g_world_name_entry_keyboard_script, g_menu_input_primary_repeat, busy);
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames_variant();
        menu_input_update_event_state_variant(
            g_menu_gfx_context->otag + 63, PadRead(0), g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        value = g_world_name_entry_state;
        if (value != g_world_name_entry_last_state) {
            g_world_name_entry_last_state = value;
        }
        if (g_sound_pending_sfx_id == 2 || g_sound_pending_sfx_id == 3) {
            g_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_NONE;
        }
        value = g_sound_pending_sfx_id;
        if (value == MAIN_SFX_NONE) {
            value = g_sound_queued_effect_id;
        }
        if (value == MAIN_SFX_TEXT_GLYPH) {
            main_sound_play_sfx_find_channel(MAIN_SFX_TEXT_GLYPH);
        } else if (value != MAIN_SFX_NONE) {
            main_sound_play_sfx(value);
        }
        g_sound_queued_effect_id = MAIN_SFX_NONE;
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    }
    menu_set_vsync_mode_and_event_speed(2);
    world_formation_reset_menu_context();
    world_gfx_set_fade_disabled(0);
    return g_world_name_entry_text;
}

/* Open the name-entry key list: identity entry ids 0..8 plus a -1 terminator. */
static void world_menu_open_name_entry_key_list(void) {
    s32 count;
    s32 i;

    count = 0;
    for (i = 8; i >= 0; i--) {
        g_world_name_entry_key_ids[i] = i;
        count++;
    }
    g_world_name_entry_key_ids[count] = -1;
    world_menu_init_and_load_scrollable_list(g_world_name_entry_key_ids, g_menu_list_selected_index,
        world_text_get_resident_data(WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD), &g_world_name_entry_keyboard_script);
}

/*
 * Handle confirmed name-entry keys after the initial 10-update delay.
 *
 * Row 0 selects special keys; other rows decode a character from the key
 * table. Encoded 0xfe is submitted as 0xfa. State and sound requests are
 * left for the surrounding menu handlers.
 */
void world_name_handle_entry_confirmed_keys(void) {
    u8 previous;
    s32 character;

    if (g_world_name_entry_confirm_message_shown == 0) {
        g_world_name_entry_input_delay = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
        g_world_name_entry_confirm_message_shown = 1;
    }
    if (g_world_name_entry_input_delay < 10) {
        g_world_name_entry_input_delay++;
    }
    world_name_update_entry_cursor();
    g_world_name_entry_confirm_message_shown = 0;
    if ((g_menu_input_primary_repeat & (PSX_PAD_CROSS | PSX_PAD_START)) && g_world_name_entry_input_delay >= 10) {
        previous = g_world_name_entry_state;
        g_world_name_entry_state = 1;
        g_world_name_entry_saved_state = previous;
    } else {
        g_world_name_entry_confirm_message_shown = 1;
    }
    if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) && g_world_name_entry_input_delay >= 10) {
        g_sound_queued_effect_id = MAIN_SFX_SELECT;
        if (g_world_name_entry_list_cursor == 0) {
            if (g_world_name_entry_cursor_index == 5) {
                previous = g_world_name_entry_state;
                g_world_name_entry_state = 1;
                g_world_name_entry_confirm_message_shown = 0;
                g_world_name_entry_saved_state = previous;
            } else {
                world_name_apply_entry_key(g_world_name_entry_key_codes[g_world_name_entry_cursor_index]);
            }
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            return;
        }
        character = world_text_get_encoded_char_at_position(g_world_name_entry_character_rows,
            g_world_name_entry_key_ids[g_world_name_entry_list_cursor], g_world_name_entry_cursor_index);
        if (character == TEXT_END_WAIT_FOR_CONFIRM) {
            character = TEXT_SPACE;
        }
        world_name_apply_entry_key(character);
    }
}

/* Moves and draws the name-entry keyboard cursor.
 *
 * Row 0 is the six special keys; character rows have 11 columns. Crossing
 * between them remaps the column through
 * g_world_name_entry_special_to_char_column/g_world_name_entry_char_to_special_column. Left/right wrap within the row,
 * and a column change requests cursor sound 3. */
static void world_name_update_entry_cursor(void) {
    point16_t cursor;
    s32 previous;
    u32 input;

    previous = g_world_name_entry_cursor_index;
    if (g_menu_list_selected_index > 0) {
        if (g_world_name_entry_list_cursor == 0) {
            g_world_name_entry_cursor_index = g_world_name_entry_special_to_char_column[previous];
            previous = g_world_name_entry_cursor_index;
        }
        if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
            if (g_world_name_entry_cursor_index == 0) {
                g_world_name_entry_cursor_index = 10;
            } else {
                g_world_name_entry_cursor_index--;
            }
        }
        if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
            g_world_name_entry_cursor_index = (g_world_name_entry_cursor_index + 1) % 11;
        }
        g_world_name_entry_list_cursor = g_menu_list_selected_index;
        cursor.x = g_world_name_entry_cursor_index * 20 + 2;
        cursor.y = (g_world_name_entry_list_cursor - g_menu_scroll_base_index) * 16 + 0x80;
    } else if (g_menu_list_selected_index == 0) {
        if (g_world_name_entry_list_cursor != 0) {
            g_world_name_entry_cursor_index = g_world_name_entry_char_to_special_column[previous];
            previous = g_world_name_entry_cursor_index;
        }
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_RIGHT) {
            g_world_name_entry_cursor_index = (g_world_name_entry_cursor_index + 1) % 6;
        }
        if (input & PSX_PAD_LEFT) {
            if (g_world_name_entry_cursor_index == 0) {
                g_world_name_entry_cursor_index = 5;
            } else {
                g_world_name_entry_cursor_index--;
            }
        }
        cursor.x = g_world_name_entry_cursor_x_positions[g_world_name_entry_cursor_index];
        g_world_name_entry_list_cursor = 0;
        cursor.y = 0x80;
    }
    if (g_world_name_entry_cursor_index != previous) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    gfx_set_otag_index(0x28);
    menu_update_and_draw_sliding_cursor_variant(&cursor, &g_world_name_entry_key_cursor_anim, g_menu_input_lock_mode);
}

/* Applies a name-entry key to the encoded name at g_world_name_entry_text and re-kerns it.
 *
 * The name is expanded to 14 character slots (0xD0-0xDF lead bytes form
 * two-byte characters, 0xFA is a space). Key -1 is backspace, -2 deletes at
 * the cursor, -3 inserts a space, -4/-5 move the cursor left/right, and any
 * other key overwrites the slot at the cursor and advances it. The slots are
 * re-encoded with trailing spaces trimmed before the terminator. */
static void world_name_apply_entry_key(s16 key) {
    u16 chars[14];
    s32 i;
    s32 j;
    s32 slot;

    slot = 0xFA;
    for (i = 13; i >= 0; i--) {
        chars[i] = slot;
    }
    j = 0;
    slot = 0;
    while (g_world_name_entry_text[j] != TEXT_END_WAIT_FOR_CONFIRM) {
        if (g_world_name_entry_text[j] < 0xd0 || g_world_name_entry_text[j] > 0xdf) {
            chars[slot] = g_world_name_entry_text[j++];
        } else {
            chars[slot] = (g_world_name_entry_text[j] << 8) + g_world_name_entry_text[j + 1];
            j += 2;
        }
        slot++;
    }
    if (key == WORLD_NAME_ENTRY_KEY_BACKSPACE) {
        if (g_world_name_entry_cursor == 0) {
            return;
        }
        g_world_name_entry_cursor--;
        for (i = g_world_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == WORLD_NAME_ENTRY_KEY_DELETE) {
        for (i = g_world_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == WORLD_NAME_ENTRY_KEY_INSERT_SPACE) {
        for (i = 13; g_world_name_entry_cursor < i; i--) {
            chars[i] = chars[i - 1];
        }
        chars[g_world_name_entry_cursor] = TEXT_SPACE;
    } else if (key == WORLD_NAME_ENTRY_KEY_CURSOR_LEFT) {
        g_world_name_entry_cursor = g_world_name_entry_cursor != 0 ? g_world_name_entry_cursor - 1 : 0;
    } else if (key == WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT) {
        g_world_name_entry_cursor = g_world_name_entry_cursor < 13 ? g_world_name_entry_cursor + 1 : 13;
    } else {
        chars[g_world_name_entry_cursor] = key;
        g_world_name_entry_cursor = g_world_name_entry_cursor < 13 ? g_world_name_entry_cursor + 1 : 13;
    }
    for (i = 0, j = 0; i < 14; i++) {
        if (chars[i] <= 0xCFFF) {
            g_world_name_entry_text[j++] = chars[i];
        } else {
            g_world_name_entry_text[j] = chars[i] >> 8;
            g_world_name_entry_text[j + 1] = chars[i];
            j += 2;
        }
    }
    g_world_name_entry_text[j] = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 0; g_world_name_entry_text[i] != TEXT_END_WAIT_FOR_CONFIRM; i++) { }
    for (i--; g_world_name_entry_text[i] == TEXT_SPACE; i--) { }
    g_world_name_entry_text[i + 1] = TEXT_END_WAIT_FOR_CONFIRM;
    world_text_kern_into_command_buffer(
        g_world_name_entry_text, g_world_name_entry_text_row_ids, &g_world_name_entry_text_vram_rect, 0);
}

/*
 * Confirm a name, using the original name when the edit buffer is empty.
 *
 * Names use 0xfe termination and contain at most 16 encoded bytes. Canceling
 * restores the empty-buffer marker if the original name was substituted.
 * Confirmation waits for the prompt thread to finish before reporting 0xff.
 */
void world_name_run_confirmation_menu(void) {
    s32 count;
    u8 value;
    if (g_world_name_confirm_initialized == 0) {
        g_world_name_confirm_menu.selected_index = 0;
        if (g_world_name_entry_text[0] == TEXT_END_WAIT_FOR_CONFIRM) {
            /* Sharing the flag/index temporary preserves the target's
             * initial register allocation without an explicit constraint. */
            count = 1;
            g_world_name_used_original_name = count;
            value = g_world_name_entry_original_name[0];
            count = 0;
            g_world_name_entry_text[0] = value;
            while (value != TEXT_END_WAIT_FOR_CONFIRM) {
                if (count >= 16) {
                    g_world_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
                    break;
                }
                count++;
                value = g_world_name_entry_original_name[count];
                g_world_name_entry_text[count] = value;
            }
        } else {
            g_world_name_used_original_name = 0;
        }
        g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_world_name_entry_text;
        g_text_substitution_values[0] = 0xc800;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xc01a, 0);
        g_world_name_confirm_initialized = 1;
        g_world_name_confirm_finished = 0;
        g_world_name_confirm_result = -1;
        g_world_menu_prompt_active = 1;
    }
    if (g_world_name_confirm_finished != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_world_name_confirm_initialized = 0;
            g_world_name_entry_state = 0xff;
            g_world_menu_prompt_active = 0;
        }
    } else if (menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_name_confirm_menu) == 0) {
        if (g_world_name_confirm_result == 0) {
            g_world_name_confirm_finished = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, -1, 0);
            return;
        }
        g_world_name_entry_state = g_world_name_entry_saved_state;
        if (g_world_name_used_original_name != 0) {
            g_world_name_entry_text[0] = TEXT_END_WAIT_FOR_CONFIRM;
        }
        g_world_name_confirm_initialized = 0;
    }
}

/* Seed the editable name and open its keyboard and message window. */
static void world_name_init_entry(const u8* name) {
    s32 i;

    g_world_name_entry_original_name = name;
    g_world_name_entry_cursor_index = 0;
    g_world_name_entry_list_cursor = 1;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
    menu_init_scrollable_list_core_variant(0, 0, 0);
    g_menu_list_selected_index = 1;
    world_text_bind_section_data(WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD);
    g_text_section_pointers[WORLD_TEXT_SECTION_NAME_ENTRY_KEYS] = g_world_text_name_entry_keys_data;
    g_world_name_entry_cursor = 0;
    i = 0;
    while ((g_world_name_entry_text[i] = name[i]) != TEXT_END_WAIT_FOR_CONFIRM) {
        if (i >= 16) {
            g_world_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
            break;
        }
        i++;
    }
    world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
    g_sound_queued_effect_id = MAIN_SFX_NONE;
    g_world_name_entry_state = 0;
    world_menu_open_name_entry_key_list();
    menu_set_vsync_mode_and_event_speed(2);
}
