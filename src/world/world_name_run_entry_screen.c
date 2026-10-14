#include "fft/gfx.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

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
    g_world_gfx_poly_f3_capacity = 0;
    g_world_gfx_poly_ft3_capacity = 0;
    g_world_gfx_poly_f4_capacity = 0;
    g_world_gfx_poly_g3_capacity = 0;
    g_world_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0;
    g_world_gfx_textured_gradient_quad_capacity = 0;
    g_world_gfx_flat_line_capacity = 4;
    g_world_gfx_line_f3_capacity = 0;
    g_world_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_world_gfx_line_g4_capacity = 0;
    g_world_gfx_tile_capacity = 4;
    g_world_gfx_tile_1_capacity = 0;
    g_world_gfx_tile_8_capacity = 0;
    g_world_gfx_tile_16_capacity = 0;
    g_world_gfx_sprite_capacity = 0;
    g_world_gfx_sprite_8_capacity = 0;
    g_world_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 30;
    g_world_gfx_draw_mode_capacity = 30;
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
    world_input_update_with_message_state();
    while (g_world_name_entry_state != 0xFF) {
        world_gfx_update_fade_in_tile();
        main_system_frame_stub();
        g_world_gfx_poly_f3_count = 0;
        g_world_gfx_poly_ft3_count = 0;
        g_world_gfx_poly_f4_count = 0;
        g_menu_gfx_poly_ft4_count = 0;
        g_world_gfx_poly_g3_count = 0;
        g_world_gfx_poly_gt3_count = 0;
        g_world_gfx_gradient_quad_count = 0;
        g_menu_gfx_poly_gt4_count = 0;
        g_world_gfx_flat_line_count = 0;
        g_world_gfx_line_f3_count = 0;
        g_world_gfx_line_f4_count = 0;
        g_world_gfx_gradient_line_count = 0;
        g_world_gfx_line_g3_count = 0;
        g_world_gfx_line_g4_count = 0;
        g_world_gfx_tile_count = 0;
        g_world_gfx_tile_1_count = 0;
        g_world_gfx_tile_8_count = 0;
        g_world_gfx_tile_16_count = 0;
        g_world_gfx_sprite_count = 0;
        g_world_gfx_sprite_8_count = 0;
        g_world_gfx_sprite_16_count = 0;
        g_menu_gfx_draw_move_count = 0;
        g_world_gfx_draw_area_count = 0;
        otag = g_menu_gfx_context->otag;
        GetDrawEnv(&env);
        g_world_gfx_draw_buffer_clip_y = env.clip.y;
        while (DrawSync(1) != 0) { }
        VSync(g_world_gfx_vsync_mode);
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
        world_input_update_with_message_state();
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_world_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
            }
            if (g_world_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT);
            }
            if (g_world_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_DELETE);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        value = g_world_name_entry_cursor * 6 + 0x54;
        cursor.x = value;
        world_menu_set_draw_priority(0x28);
        world_menu_draw_animated_vertical_cursor(&cursor, &g_world_name_entry_text_cursor_anim, busy);
        menu_run_script_with_palette_mode(&g_world_name_entry_display_script, 0, busy);
        world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_L1, 0x37, 0x50,
            g_world_input_primary_repeat & PSX_PAD_L1, busy, MAIN_SFX_PAGE_SWITCH);
        world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_R1, 0xB1, 0x50,
            g_world_input_primary_repeat & PSX_PAD_R1, busy, MAIN_SFX_PAGE_SWITCH);
        menu_run_script_with_palette_mode(&g_world_name_entry_keyboard_script, g_world_input_primary_repeat, busy);
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        world_input_update_event_state(
            g_menu_gfx_context->otag + 63, PadRead(0), g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        value = g_world_name_entry_state;
        if (value != g_world_name_entry_last_state) {
            g_world_name_entry_last_state = value;
        }
        if (g_sound_pending_sfx_id == 2 || g_sound_pending_sfx_id == 3) {
            g_world_menu_sound_effect_id = g_sound_pending_sfx_id;
        }
        if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_NONE;
        }
        value = g_sound_pending_sfx_id;
        if (value == MAIN_SFX_NONE) {
            value = g_world_menu_sound_effect_id;
        }
        if (value == MAIN_SFX_TEXT_GLYPH) {
            main_sound_play_sfx_find_channel(MAIN_SFX_TEXT_GLYPH);
        } else if (value != MAIN_SFX_NONE) {
            main_sound_play_sfx(value);
        }
        g_world_menu_sound_effect_id = MAIN_SFX_NONE;
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    }
    world_script_set_vsync_mode_and_event_speed(2);
    world_formation_reset_menu_context();
    world_gfx_set_fade_disabled(0);
    return g_world_name_entry_text;
}
