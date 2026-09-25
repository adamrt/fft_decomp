#include "fft/world.h"
#include "psx/etc.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef void (*world_name_entry_handler_t)(void);

/* Stack-resident primitive pools for both WORLD packet buffers; each pool
 * holds two frames of the per-type counts this routine publishes. */
typedef struct world_name_entry_screen_packets {
    POLY_FT4 textured_quads[2 * 220];     /* 0x0000: g_world_gfx_textured_quad_capacity = 220 */
    TILE tiles[2 * 4];                    /* 0x44c0: g_world_gfx_tile_capacity = 4 */
    TILE tiles_24[2 * 4];                 /* 0x4540: g_world_gfx_tiles_24_capacity = 4 */
    DR_MOVE draw_moves[2 * 30];           /* 0x45c0: g_world_gfx_draw_move_capacity = 30 */
    DR_MODE draw_modes[2 * 30];           /* 0x4b60: g_world_gfx_draw_mode_capacity = 30 */
    DR_AREA draw_areas[2 * 10];           /* 0x4e30: g_world_gfx_draw_area_capacity = 10 */
    u32 otags[2 * 64];                    /* 0x4f20: g_world_ot_length = 64 */
    world_gfx_packet_buffer_t buffers[2]; /* 0x5120 */
} world_name_entry_screen_packets_t;

extern world_name_entry_handler_t g_world_name_entry_state_handlers[];

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
u8* world_name_run_entry_screen(u8* list, u32* image) {
    world_menu_point_t cursor;
    world_name_entry_screen_packets_t packets;
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
    g_world_gfx_tiles_24_capacity = 4;
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
    world_gfx_init_double_packet_buffers(packets.buffers, packets.otags, 0, 0, 0, packets.textured_quads, 0, 0, 0, 0,
        packets.tiles_24, 0, 0, 0, 0, 0, packets.tiles, 0, 0, 0, 0, 0, 0, packets.draw_moves, packets.draw_areas,
        packets.draw_modes);
    g_world_shop_id = -2;
    world_formation_init_menu_display();
    world_thread_start(2, world_text_message_box_thread);
    world_thread_set_parameters(2, 0x21, 0xC015, 0);
    world_menu_open_scrollable_list(list);
    g_world_name_entry_last_state = -1;
    world_gfx_set_fade_disabled(1);
    world_thread_update_task_state();
    while (g_world_name_entry_state != 0xFF) {
        world_gfx_update_fade_in_tile();
        main_noop_800449ec();
        g_world_gfx_poly_f3_count = 0;
        g_world_gfx_poly_ft3_count = 0;
        g_world_gfx_poly_f4_count = 0;
        g_world_gfx_textured_quad_count = 0;
        g_world_gfx_poly_g3_count = 0;
        g_world_gfx_poly_gt3_count = 0;
        g_world_gfx_gradient_quad_count = 0;
        g_world_gfx_textured_gradient_quad_count = 0;
        g_world_gfx_tiles_24_count = 0;
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
        g_world_gfx_draw_move_count = 0;
        g_world_gfx_draw_area_count = 0;
        otag = g_world_gfx_active_packet_buffer->otag;
        GetDrawEnv(&env);
        g_world_gfx_draw_buffer_clip_y = env.clip.y;
        while (DrawSync(1) != 0) { }
        VSync(g_world_gfx_vsync_mode);
        {
            world_gfx_packet_buffer_t* context = g_world_gfx_packet_buffers;

            if (g_world_gfx_active_packet_buffer == context) {
                context++;
            }
            g_world_gfx_active_packet_buffer = context;
            PutDispEnv(&context->disp_env);
        }
        PutDrawEnv(&g_world_gfx_active_packet_buffer->draw_env);
        g_world_name_entry_screen_image_rect.y = g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0;
        world_gfx_load_image_sync(&g_world_name_entry_screen_image_rect, image);
        DrawOTag(otag);
        ClearOTag(g_world_gfx_active_packet_buffer->otag, g_world_ot_length);
        world_thread_update_task_state();
        busy = world_map_is_busy();
        if (busy == 0) {
            if (g_world_input_primary_repeat & PSX_PAD_L1) {
                world_name_apply_entry_key(-4);
            }
            if (g_world_input_primary_repeat & PSX_PAD_R1) {
                world_name_apply_entry_key(-5);
            }
            if (g_world_input_primary_repeat & PSX_PAD_SQUARE) {
                world_name_apply_entry_key(-2);
            }
        }
        g_world_name_entry_state_handlers[g_world_name_entry_state]();
        cursor.y = 0x3B;
        value = g_world_name_entry_cursor * 6 + 0x54;
        cursor.x = value;
        world_menu_set_draw_priority(0x28);
        world_menu_draw_animated_vertical_cursor(&cursor, &g_world_name_entry_text_cursor_anim, busy);
        world_menu_run_script_with_palette_mode(g_world_name_entry_display_script, 0, busy);
        /* The definition's s16/u16 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
            0, 0x37, 0x50, (u16)g_world_input_primary_repeat & PSX_PAD_L1, busy, 6);
        /* The definition's s16/u16 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
            1, 0xB1, 0x50, (u16)g_world_input_primary_repeat & PSX_PAD_R1, busy, 6);
        world_menu_run_script_with_palette_mode(g_world_name_entry_keyboard_script, g_world_input_primary_repeat, busy);
        g_world_gfx_active_otag_entries = g_world_gfx_active_packet_buffer->otag + 63;
        world_menu_draw_active_window_frames();
        world_script_update_event_frame_input(
            g_world_gfx_active_packet_buffer->otag + 63, PadRead(0), g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        value = g_world_name_entry_state;
        if (value != g_world_name_entry_last_state) {
            g_world_name_entry_last_state = value;
        }
        if ((u32)(g_world_sound_effect_id_to_play - 2) < 2) {
            g_world_menu_sound_effect_id = g_world_sound_effect_id_to_play;
        }
        if (g_world_sound_effect_id_to_play < 0 || g_world_menu_sound_muted != 0) {
            g_world_sound_effect_id_to_play = 0;
        }
        value = g_world_sound_effect_id_to_play;
        if (value == 0) {
            value = g_world_menu_sound_effect_id;
        }
        if (value == MAIN_SFX_TEXT_GLYPH) {
            main_sound_play_sfx_find_channel(MAIN_SFX_TEXT_GLYPH);
        } else if (value != 0) {
            main_sound_play_sfx(value);
        }
        g_world_menu_sound_effect_id = 0;
        g_world_sound_effect_id_to_play = 0;
    }
    world_script_set_vsync_mode_and_event_speed(2);
    world_formation_reset_menu_context();
    world_gfx_set_fade_disabled(0);
    return g_world_name_entry_text;
}
