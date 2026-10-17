/* Menu background tiles, texture-page and palette commands, blend mode, color and draw priority. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* BUNIT and JOBSTTS use fixed background tiles; EQUIP selects a rectangle source below. */
#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_JOBSTTS)
/* Fill a background rectangle with 16-pixel textured tiles.
 *
 * Command bytes 5 and 6 encode the full-tile counts in their high nibbles and
 * the remaining width and height in their low nibbles.
 */
menu_instruction_t* menu_opcode_11_draw_background_tiles(menu_instruction_t* desc) {
    RECT rect;
    s32 full_columns;
    s32 origin_x;
    s32 origin_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index;
    u16 clut_id;
    u32 packed_height = desc->window.height;
    u32 packed_width = desc->window.width;
    s32 row;
    s32 column;

    otag_index = g_menu_gfx_otag_index;
    clut_id = g_menu_palette_clut;
    full_columns = packed_width >> 4;
    partial_width = packed_width & 0xF;
    full_rows = packed_height >> 4;
    partial_height = packed_height & 0xF;
    origin_x = desc->window.x;
    origin_y = desc->window.y;

    for (row = 0; row < full_rows; row++) {
        rect.x = origin_x;
        rect.y = origin_y + (row * 16);
        rect.w = partial_width;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut_id, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = origin_x + partial_width + (column * 16);
            rect.y = origin_y + (row * 16);
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut_id, otag_index);
        }
    }

    if (partial_height != 0) {
        rect.x = origin_x;
        rect.y = origin_y + (row * 16);
        rect.w = partial_width;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut_id, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = origin_x + partial_width + (column * 16);
            rect.y = origin_y + (row * 16);
            rect.w = 16;
            rect.h = partial_height;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut_id, otag_index);
        }
    }

    return menu_next_instruction(desc);
}
#endif

#ifdef FFT_EVENT_EQUIP
/* Tile the command rectangle, or the shared rectangle when source is 2. */
menu_instruction_t* menu_opcode_11_draw_tiled_rectangle(menu_instruction_t* command) {
    RECT rect;
    s32 base_x;
    s32 base_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index = g_menu_gfx_otag_index;
    u16 clut = g_menu_palette_clut;
    s32 full_columns;
    s32 row;
    s32 column;

    if (command->window.rectangle_source == 2) {
        base_x = g_gfx_tiled_rect.x;
        base_y = g_gfx_tiled_rect.y;
        full_columns = g_gfx_tiled_rect.w / 16;
        partial_width = g_gfx_tiled_rect.w % 16;
        full_rows = g_gfx_tiled_rect.h / 16;
        partial_height = g_gfx_tiled_rect.h % 16;
    } else {
        u8 packed_width;
        u8 packed_height;

        base_x = command->window.x;
        base_y = command->window.y;
        packed_width = command->window.width;
        packed_height = command->window.height;
        full_columns = packed_width >> 4;
        partial_width = packed_width & 15;
        full_rows = packed_height >> 4;
        partial_height = packed_height & 15;
    }

    for (row = 0; row < full_rows; row++) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    if (partial_height != 0) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = partial_height;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    command = (menu_instruction_t*)((u8*)command + command->header.length);
    return command;
}
#endif

/* Menu script command: set the sprite CLUT from the command's VRAM coordinates and return the next
 * command. */
menu_instruction_t* menu_opcode_09_set_fixed_clut(menu_instruction_t* data) {
    g_menu_gfx_clut_id
        = GetClut(data->fixed_clut.x_div_16 << 4, data->fixed_clut.y_low | (data->fixed_clut.y_high << 8));
    return menu_next_instruction(data);
}

/* Set the sprite CLUT from the command's normal or compact-layout coordinates
 * and return the next command. */
menu_instruction_t* menu_opcode_0a_set_clut(menu_instruction_t* command) {
    if (g_menu_use_alternate_palette != 0) {
        g_menu_gfx_clut_id = GetClut(command->clut.alternate_x_div_16 << 4,
            command->clut.alternate_y_low | (command->clut.alternate_y_high << 8));
    } else {
        g_menu_gfx_clut_id = GetClut(
            command->clut.normal_x_div_16 << 4, command->clut.normal_y_low | (command->clut.normal_y_high << 8));
    }
    return menu_next_instruction(command);
}

/* Select the texture page and blend rate. */
menu_instruction_t* menu_opcode_0c_set_texture_page(menu_instruction_t* data) {
    g_menu_gfx_texture_page = GetTPage(data->texture_page.depth, data->texture_page.y_blend >> 4,
        data->texture_page.x_div_16 << 4, data->texture_page.y_blend << 8);
    return menu_next_instruction(data);
}
/* JOBSTTS links translucency with primitive drawing and has a guarded priority setter below. */
#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_EQUIP)
/* Menu script command: turn semi-transparency on or off for later primitives and continue with the
 * next command. */
menu_instruction_t* menu_opcode_0e_set_semi_trans(menu_instruction_t* data) {
    g_menu_gfx_semitransparency = data->draw_state.value;
    return menu_next_instruction(data);
}

/* Queue a draw mode with the requested blend rate at the current OT entry. */
menu_instruction_t* menu_opcode_18_add_blend_draw_mode(menu_instruction_t* data) {
    menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, data->blend.blend_rate, 0x100, 0), 0, g_menu_gfx_otag_index);
    return menu_next_instruction(data);
}

/* Select the normal or alternate sprite color. */
menu_instruction_t* menu_opcode_0b_set_draw_color(menu_instruction_t* data) {
    u8 blue;
    if (g_menu_use_alternate_palette != 0) {
        g_menu_text_color.r = data->sprite_color.alternate.r;
        g_menu_text_color.g = data->sprite_color.alternate.g;
        blue = data->sprite_color.alternate.b;
    } else {
        g_menu_text_color.r = data->sprite_color.normal.r;
        g_menu_text_color.g = data->sprite_color.normal.g;
        blue = data->sprite_color.normal.b;
    }
    g_menu_text_color.b = blue;
    return menu_next_instruction(data);
}

/* Menu script command: select the ordering-table slot for later primitives and continue with the
 * next command. */
menu_instruction_t* menu_opcode_0f_set_draw_priority(menu_instruction_t* data) {
    g_menu_gfx_otag_index = data->draw_state.value;
    return menu_next_instruction(data);
}
#endif

#ifdef FFT_EVENT_JOBSTTS
/* Menu script command: select the ordering-table slot for later primitives unless the slot is
 * locked, and continue with the next command. */
menu_instruction_t* menu_opcode_0f_set_draw_priority_if_unlocked(menu_instruction_t* data) {
    if (g_gfx_otag_index_locked == 0) {
        g_menu_gfx_otag_index = data->draw_state.value;
    }
    return menu_next_instruction(data);
}
#endif
