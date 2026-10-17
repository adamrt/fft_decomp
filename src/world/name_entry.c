/* The name entry keyboard, including frames, the key list, confirmed keys, the cursor, key application and the
 * confirmation menu. */

#include "fft/name_entry.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/message_box.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_screen_text_name_entry_keys_data[];

/* screen */
typedef enum name_entry_state {
    NAME_ENTRY_STATE_EDITING = 0,
    NAME_ENTRY_STATE_CONFIRMATION = 1,
    NAME_ENTRY_STATE_FINISHED = 0xff
} name_entry_state_e;

extern void (*g_name_entry_state_handlers[])(void);
extern u8 g_name_entry_state;       /* name_entry_state_e */
extern u8 g_name_entry_saved_state; /* name_entry_state_e */
extern s8 g_name_entry_started;
extern s8 g_name_entry_last_state;          /* last drawn name-entry state */
extern s8 g_name_entry_keyboard_last_state; /* last drawn state */
extern u8 g_name_entry_input_delay;
extern const u8* g_name_entry_original_name;
extern u8 g_name_entry_used_original_name;
extern RECT g_name_entry_screen_image_rect;
extern RECT g_name_entry_text_vram_rect;

/* keyboard */
/* Editing keys of name_entry_apply_key; any other key is the character
 * written at the cursor. */
typedef enum name_entry_key {
    NAME_ENTRY_KEY_BACKSPACE = -1,
    NAME_ENTRY_KEY_DELETE = -2,
    NAME_ENTRY_KEY_INSERT_SPACE = -3,
    NAME_ENTRY_KEY_CURSOR_LEFT = -4,
    NAME_ENTRY_KEY_CURSOR_RIGHT = -5
} name_entry_key_e;

extern s16 g_name_entry_text_row_ids[];
extern u8 g_name_entry_character_rows[];
extern s16 g_name_entry_key_codes[];
extern s16 g_name_entry_key_ids[];
extern u8 g_name_entry_char_to_special_column[];
extern u8 g_name_entry_special_to_char_column[];
extern u8 g_name_entry_cursor_x_positions[];
extern u8 g_name_entry_cursor;
extern u8 g_name_entry_cursor_index;
extern u8 g_name_entry_list_cursor;
extern u16 g_name_entry_key_cursor_anim;
extern u16 g_name_entry_text_cursor_anim;
extern u16 g_name_entry_keyboard_text_cursor_anim;

/* confirmation */
extern menu_entry_t g_name_entry_confirm_menu;
extern u8 g_name_entry_confirm_initialized;
extern u8 g_name_entry_confirm_finished;
extern s16 g_name_entry_confirm_result;
extern u8 g_name_entry_confirm_message_shown;

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_name_entry_display_script[38];
extern u8 g_name_entry_keyboard_script[41];

static u8 g_name_entry_text[17] = { 0 };

static void name_entry_apply_key(s16 key);
static void name_entry_init(const u8* name);
static void name_entry_update_cursor(void);

/* One frame of the name-entry keyboard inside another screen (the shop's rename and hire
 * steps). The first call, once the message box is idle, starts editing the name at `list`;
 * later calls apply L1/R1 (cursor) and Square (delete), run the current entry state's handler
 * and draw the name, its cursor, the shoulder buttons and the keyboard. Returns 0 while
 * editing and the encoded name once the state reaches NAME_ENTRY_STATE_FINISHED. */
u8* name_entry_keyboard_frame(const u8* list, s32 unused_1) {
    point16_t cursor;
    s32 busy;

    if (g_name_entry_started == 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_name_entry_keyboard_last_state = -1;
            name_entry_init(list);
            g_name_entry_started = 1;
        }
        return 0;
    }
    if (g_name_entry_state != NAME_ENTRY_STATE_FINISHED) {
        busy = screen_is_busy();
        if (busy == 0) {
            if (g_menu_input_primary_repeat & PSX_PAD_L1) {
                name_entry_apply_key(NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_R1) {
                name_entry_apply_key(NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_SQUARE) {
                name_entry_apply_key(NAME_ENTRY_KEY_DELETE);
            }
        }
        g_name_entry_state_handlers[g_name_entry_state]();
        cursor.y = 0x3B;
        cursor.x = g_name_entry_cursor * 6 + 0x54;
        gfx_set_otag_index(0x28);
        menu_update_and_draw_bouncing_cursor(&cursor, &g_name_entry_keyboard_text_cursor_anim, busy);
        menu_run_script_with_palette_mode(g_name_entry_display_script, 0, busy);
        menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50, g_menu_input_primary_repeat & PSX_PAD_L1, busy,
            SOUND_SFX_PAGE_SWITCH);
        menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50, g_menu_input_primary_repeat & PSX_PAD_R1, busy,
            SOUND_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(g_name_entry_keyboard_script, g_menu_input_primary_repeat, busy);
        if (g_name_entry_state != g_name_entry_keyboard_last_state) {
            g_name_entry_keyboard_last_state = g_name_entry_state;
        }
        return 0;
    }
    g_name_entry_started = 0;
    return g_name_entry_text;
}

/*
 * Run the WORLD name-entry screen until the name is confirmed.
 *
 * Starts editing the name at `list`, binds both packet buffers onto a
 * stack-resident pool block, then each frame presents the previous ordering
 * table, uploads `image` as the backdrop (OPEN, the one caller, passes a copy
 * of its last displayed frame), steps the
 * current name-entry handler, draws the cursor and key prompts, and dispatches
 * the queued sound effect. Returns the encoded name.
 *
 * value is one local reused for the cursor x, the drawn-state compare and the
 * sound id; the shared pseudo conflicts with $a0 and keeps all three in $v1.
 */
u8* name_entry_run_screen(const u8* list, u32* image) {
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

    g_screen_ot_length = 0x40;
    g_screen_textured_quad_capacity = 0xDC;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_screen_gradient_quad_capacity = 0;
    g_screen_textured_gradient_quad_capacity = 0;
    g_screen_flat_line_capacity = 4;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_screen_gradient_line_capacity = 0;
    g_screen_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 4;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_screen_draw_move_capacity = 30;
    g_gfx_draw_mode_capacity = 30;
    g_screen_draw_area_capacity = 10;
    screen_init_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, 0, 0, flat_lines, 0, 0, 0, 0, 0, tiles, 0,
        0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    g_screen_context_id = SCREEN_CONTEXT_NAME_ENTRY;
    formation_init_display();
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
    name_entry_init(list);
    g_name_entry_last_state = -1;
    screen_fade_set_disabled(1);
    menu_input_update_with_message_state();
    while (g_name_entry_state != NAME_ENTRY_STATE_FINISHED) {
        screen_fade_update_in();
        system_frame_stub();
        g_gfx_poly_f3_count = 0;
        g_gfx_poly_ft3_count = 0;
        g_screen_poly_f4_count = 0;
        g_menu_gfx_poly_ft4_count = 0;
        g_gfx_poly_g3_count = 0;
        g_gfx_poly_gt3_count = 0;
        g_gfx_poly_g4_count = 0;
        g_menu_gfx_poly_gt4_count = 0;
        g_gfx_line_f2_count = 0;
        g_gfx_line_f3_count = 0;
        g_gfx_line_f4_count = 0;
        g_screen_gradient_line_count = 0;
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
        g_screen_draw_area_count = 0;
        otag = g_menu_gfx_context->otag;
        GetDrawEnv(&env);
        g_screen_draw_buffer_clip_y = env.clip.y;
        while (DrawSync(1) != 0) { }
        VSync(g_menu_event_speed);
        {
            menu_gfx_context_t* context = g_screen_packet_buffers;

            if (g_menu_gfx_context == context) {
                context++;
            }
            g_menu_gfx_context = context;
            PutDispEnv(&context->disp_env);
        }
        PutDrawEnv(&g_menu_gfx_context->draw_env);
        g_name_entry_screen_image_rect.y = g_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0;
        gfx_load_image_and_wait(&g_name_entry_screen_image_rect, image);
        DrawOTag(otag);
        ClearOTag(g_menu_gfx_context->otag, g_screen_ot_length);
        menu_input_update_with_message_state();
        busy = screen_is_busy();
        if (busy == 0) {
            if (g_menu_input_primary_repeat & PSX_PAD_L1) {
                name_entry_apply_key(NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_R1) {
                name_entry_apply_key(NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_menu_input_primary_repeat & PSX_PAD_SQUARE) {
                name_entry_apply_key(NAME_ENTRY_KEY_DELETE);
            }
        }
        g_name_entry_state_handlers[g_name_entry_state]();
        cursor.y = 0x3B;
        value = g_name_entry_cursor * 6 + 0x54;
        cursor.x = value;
        gfx_set_otag_index(0x28);
        menu_update_and_draw_bouncing_cursor(&cursor, &g_name_entry_text_cursor_anim, busy);
        menu_run_script_with_palette_mode(g_name_entry_display_script, 0, busy);
        menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50, g_menu_input_primary_repeat & PSX_PAD_L1, busy,
            SOUND_SFX_PAGE_SWITCH);
        menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50, g_menu_input_primary_repeat & PSX_PAD_R1, busy,
            SOUND_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(g_name_entry_keyboard_script, g_menu_input_primary_repeat, busy);
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        menu_input_update_event_state(
            g_menu_gfx_context->otag + 63, PadRead(0), g_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        value = g_name_entry_state;
        if (value != g_name_entry_last_state) {
            g_name_entry_last_state = value;
        }
        if (g_sound_pending_sfx_id == SOUND_SFX_CANCEL || g_sound_pending_sfx_id == SOUND_SFX_CURSOR_MOVE) {
            g_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = SOUND_SFX_NONE;
        }
        value = g_sound_pending_sfx_id;
        if (value == SOUND_SFX_NONE) {
            value = g_sound_queued_effect_id;
        }
        if (value == SOUND_SFX_TEXT_GLYPH) {
            sound_play_sfx_on_free_channel(SOUND_SFX_TEXT_GLYPH);
        } else if (value != SOUND_SFX_NONE) {
            sound_play_sfx(value);
        }
        g_sound_queued_effect_id = SOUND_SFX_NONE;
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    }
    menu_set_vsync_mode_and_event_speed(2);
    screen_reset_context();
    screen_fade_set_disabled(0);
    return g_name_entry_text;
}

/* Open the name-entry key list: identity entry ids 0..8 plus a -1 terminator. */
static void name_entry_open_key_list(void) {
    s32 count = 0;
    s32 i;

    for (i = 8; i >= 0; i--) {
        g_name_entry_key_ids[i] = i;
        count++;
    }
    g_name_entry_key_ids[count] = -1;
    screen_init_list(g_name_entry_key_ids, g_menu_list_selected_index,
        screen_text_get_resident(SCREEN_TEXT_DATA_NAME_ENTRY_KEYBOARD),
        (menu_instruction_t*)g_name_entry_keyboard_script);
}

/*
 * Handle confirmed name-entry keys after the initial 10-update delay.
 *
 * Row 0 selects special keys; other rows decode a character from the key
 * table. Encoded 0xfe is submitted as 0xfa. Cross or Start, or special key 5,
 * opens the confirmation menu, remembering the current state to return
 * to. Other state and sound requests are left for the surrounding menu
 * handlers.
 */
void name_entry_handle_keys(void) {
    u8 previous;
    s32 character;

    if (g_name_entry_confirm_message_shown == 0) {
        g_name_entry_input_delay = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
        g_name_entry_confirm_message_shown = 1;
    }
    if (g_name_entry_input_delay < 10) {
        g_name_entry_input_delay++;
    }
    name_entry_update_cursor();
    g_name_entry_confirm_message_shown = 0;
    if ((g_menu_input_primary_repeat & (PSX_PAD_CROSS | PSX_PAD_START)) && g_name_entry_input_delay >= 10) {
        previous = g_name_entry_state;
        g_name_entry_state = NAME_ENTRY_STATE_CONFIRMATION;
        g_name_entry_saved_state = previous;
    } else {
        g_name_entry_confirm_message_shown = 1;
    }
    if ((g_menu_input_primary_repeat & PSX_PAD_CIRCLE) && g_name_entry_input_delay >= 10) {
        g_sound_queued_effect_id = SOUND_SFX_SELECT;
        if (g_name_entry_list_cursor == 0) {
            if (g_name_entry_cursor_index == 5) {
                previous = g_name_entry_state;
                g_name_entry_state = NAME_ENTRY_STATE_CONFIRMATION;
                g_name_entry_confirm_message_shown = 0;
                g_name_entry_saved_state = previous;
            } else {
                name_entry_apply_key(g_name_entry_key_codes[g_name_entry_cursor_index]);
            }
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            return;
        }
        character = screen_text_get_char_at(
            g_name_entry_character_rows, g_name_entry_key_ids[g_name_entry_list_cursor], g_name_entry_cursor_index);
        if (character == TEXT_END_WAIT_FOR_CONFIRM) {
            character = TEXT_SPACE;
        }
        name_entry_apply_key(character);
    }
}

/* Moves and draws the name-entry keyboard cursor.
 *
 * Row 0 is the six special keys; character rows have 11 columns. Crossing
 * between them remaps the column through
 * g_name_entry_special_to_char_column/g_name_entry_char_to_special_column. Left/right wrap within the row,
 * and a column change queues the cursor-move sound. */
static void name_entry_update_cursor(void) {
    point16_t cursor;
    s32 previous = g_name_entry_cursor_index;
    u32 input;

    if (g_menu_list_selected_index > 0) {
        if (g_name_entry_list_cursor == 0) {
            g_name_entry_cursor_index = g_name_entry_special_to_char_column[previous];
            previous = g_name_entry_cursor_index;
        }
        if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
            if (g_name_entry_cursor_index == 0) {
                g_name_entry_cursor_index = 10;
            } else {
                g_name_entry_cursor_index--;
            }
        }
        if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
            g_name_entry_cursor_index = (g_name_entry_cursor_index + 1) % 11;
        }
        g_name_entry_list_cursor = g_menu_list_selected_index;
        cursor.x = g_name_entry_cursor_index * 20 + 2;
        cursor.y = (g_name_entry_list_cursor - g_menu_scroll_base_index) * 16 + 0x80;
    } else if (g_menu_list_selected_index == 0) {
        if (g_name_entry_list_cursor != 0) {
            g_name_entry_cursor_index = g_name_entry_char_to_special_column[previous];
            previous = g_name_entry_cursor_index;
        }
        input = g_menu_input_primary_repeat;
        if (input & PSX_PAD_RIGHT) {
            g_name_entry_cursor_index = (g_name_entry_cursor_index + 1) % 6;
        }
        if (input & PSX_PAD_LEFT) {
            if (g_name_entry_cursor_index == 0) {
                g_name_entry_cursor_index = 5;
            } else {
                g_name_entry_cursor_index--;
            }
        }
        cursor.x = g_name_entry_cursor_x_positions[g_name_entry_cursor_index];
        g_name_entry_list_cursor = 0;
        cursor.y = 0x80;
    }
    if (g_name_entry_cursor_index != previous) {
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
    gfx_set_otag_index(0x28);
    screen_draw_sliding_cursor(&cursor, &g_name_entry_key_cursor_anim, g_menu_input_lock_mode);
}

/* Applies a name-entry key to the encoded name at g_name_entry_text and re-kerns it.
 *
 * The name is expanded to 14 character slots (0xD0-0xDF lead bytes form
 * two-byte characters, 0xFA is a space). Key -1 is backspace, -2 deletes at
 * the cursor, -3 inserts a space, -4/-5 move the cursor left/right, and any
 * other key overwrites the slot at the cursor and advances it. The slots are
 * re-encoded with trailing spaces trimmed before the terminator. */
static void name_entry_apply_key(s16 key) {
    u16 chars[14];
    s32 i;
    s32 j;
    s32 slot;

    slot = TEXT_SPACE;
    for (i = 13; i >= 0; i--) {
        chars[i] = slot;
    }
    j = 0;
    slot = 0;
    while (g_name_entry_text[j] != TEXT_END_WAIT_FOR_CONFIRM) {
        if (g_name_entry_text[j] < TEXT_EXTENDED_GLYPH_PREFIX_FIRST
            || g_name_entry_text[j] > TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
            chars[slot] = g_name_entry_text[j++];
        } else {
            chars[slot] = (g_name_entry_text[j] << 8) + g_name_entry_text[j + 1];
            j += 2;
        }
        slot++;
    }
    if (key == NAME_ENTRY_KEY_BACKSPACE) {
        if (g_name_entry_cursor == 0) {
            return;
        }
        g_name_entry_cursor--;
        for (i = g_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == NAME_ENTRY_KEY_DELETE) {
        for (i = g_name_entry_cursor; i < 13; i++) {
            chars[i] = chars[i + 1];
        }
        chars[13] = TEXT_SPACE;
    } else if (key == NAME_ENTRY_KEY_INSERT_SPACE) {
        for (i = 13; g_name_entry_cursor < i; i--) {
            chars[i] = chars[i - 1];
        }
        chars[g_name_entry_cursor] = TEXT_SPACE;
    } else if (key == NAME_ENTRY_KEY_CURSOR_LEFT) {
        g_name_entry_cursor = g_name_entry_cursor != 0 ? g_name_entry_cursor - 1 : 0;
    } else if (key == NAME_ENTRY_KEY_CURSOR_RIGHT) {
        g_name_entry_cursor = g_name_entry_cursor < 13 ? g_name_entry_cursor + 1 : 13;
    } else {
        chars[g_name_entry_cursor] = key;
        g_name_entry_cursor = g_name_entry_cursor < 13 ? g_name_entry_cursor + 1 : 13;
    }

    /* Re-encode, then trim trailing spaces back to the terminator. */
    for (i = 0, j = 0; i < 14; i++) {
        if (chars[i] <= 0xCFFF) {
            g_name_entry_text[j++] = chars[i];
        } else {
            g_name_entry_text[j] = chars[i] >> 8;
            g_name_entry_text[j + 1] = chars[i];
            j += 2;
        }
    }
    g_name_entry_text[j] = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 0; g_name_entry_text[i] != TEXT_END_WAIT_FOR_CONFIRM; i++) { }
    for (i--; g_name_entry_text[i] == TEXT_SPACE; i--) { }
    g_name_entry_text[i + 1] = TEXT_END_WAIT_FOR_CONFIRM;
    screen_text_kern(g_name_entry_text, g_name_entry_text_row_ids, &g_name_entry_text_vram_rect, 0);
}

/*
 * Confirm a name, using the original name when the edit buffer is empty.
 *
 * Names use 0xfe termination and contain at most 16 encoded bytes. Canceling
 * restores the empty-buffer marker if the original name was substituted.
 * Confirmation waits for the prompt thread to finish before marking entry finished.
 */
void name_entry_confirm_menu(void) {
    s32 count;
    u8 value;

    if (g_name_entry_confirm_initialized == 0) {
        g_name_entry_confirm_menu.selected_index = 0;
        if (g_name_entry_text[0] == TEXT_END_WAIT_FOR_CONFIRM) {
            /* Sharing the flag/index temporary preserves the target's
             * initial register allocation without an explicit constraint. */
            count = 1;
            g_name_entry_used_original_name = count;
            value = g_name_entry_original_name[0];
            count = 0;
            g_name_entry_text[0] = value;
            while (value != TEXT_END_WAIT_FOR_CONFIRM) {
                if (count >= 16) {
                    g_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
                    break;
                }
                count++;
                value = g_name_entry_original_name[count];
                g_name_entry_text[count] = value;
            }
        } else {
            g_name_entry_used_original_name = 0;
        }
        g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_name_entry_text;
        g_text_substitution_values[0] = 0xc800;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xc01a, 0);
        g_name_entry_confirm_initialized = 1;
        g_name_entry_confirm_finished = 0;
        g_name_entry_confirm_result = -1;
        g_screen_prompt_active = 1;
    }
    if (g_name_entry_confirm_finished != 0) {
        if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
            g_name_entry_confirm_initialized = 0;
            g_name_entry_state = NAME_ENTRY_STATE_FINISHED;
            g_screen_prompt_active = 0;
        }
    } else if (menu_start_managed_thread(SCREEN_THREAD_SLOT_UPPER_MENU, &g_name_entry_confirm_menu) == 0) {
        if (g_name_entry_confirm_result == 0) {
            g_name_entry_confirm_finished = 1;
            thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, -1, 0);
            return;
        }
        g_name_entry_state = g_name_entry_saved_state;
        if (g_name_entry_used_original_name != 0) {
            g_name_entry_text[0] = TEXT_END_WAIT_FOR_CONFIRM;
        }
        g_name_entry_confirm_initialized = 0;
    }
}

/* Seed the editable name and open its keyboard and message window. */
static void name_entry_init(const u8* name) {
    s32 i;

    g_name_entry_original_name = name;
    g_name_entry_cursor_index = 0;
    g_name_entry_list_cursor = 1;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
    menu_init_scrollable_list_core(0, 0, 0);
    g_menu_list_selected_index = 1;
    screen_text_bind_section(SCREEN_TEXT_DATA_NAME_ENTRY_KEYBOARD);
    g_text_section_pointers[SCREEN_TEXT_SECTION_NAME_ENTRY_KEYS] = g_screen_text_name_entry_keys_data;
    g_name_entry_cursor = 0;
    i = 0;
    while ((g_name_entry_text[i] = name[i]) != TEXT_END_WAIT_FOR_CONFIRM) {
        if (i >= 16) {
            g_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
            break;
        }
        i++;
    }
    /* A cursor move at slot 0 changes nothing but kerns and uploads the name. */
    name_entry_apply_key(NAME_ENTRY_KEY_CURSOR_LEFT);
    g_sound_queued_effect_id = SOUND_SFX_NONE;
    g_name_entry_state = NAME_ENTRY_STATE_EDITING;
    name_entry_open_key_list();
    menu_set_vsync_mode_and_event_speed(2);
}
