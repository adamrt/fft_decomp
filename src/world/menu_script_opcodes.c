/* Menu script opcodes for conditionals, tiles, sprites, lines, window frames, tiled rectangles, CLUTs,
 * semi-transparency, colors and draw priority, formatted and right-aligned numbers, the sliding and bouncing cursors,
 * instruction groups, scrollable list layouts and bodies, and sprite color. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Dispatch slots before this program's end opcode; zero entries stay unhandled. */
menu_opcode_handler_t g_menu_opcode_handlers[SCREEN_SCRIPT_END] = {
    menu_opcode_00_draw_window_frame,         /* 0x00 */
    menu_opcode_01_if_group,                  /* 0x01 */
    menu_opcode_02_if_else_group,             /* 0x02 */
    menu_opcode_03_draw_tile,                 /* 0x03 */
    menu_opcode_04_draw_textured_quad,        /* 0x04 */
    menu_opcode_04_draw_textured_quad,        /* 0x05 */
    menu_opcode_06_draw_callback_quad,        /* 0x06 */
    menu_opcode_07_draw_line,                 /* 0x07 */
    0,                                        /* 0x08 */
    menu_opcode_09_set_fixed_clut,            /* 0x09 */
    menu_opcode_0a_set_clut,                  /* 0x0a */
    menu_opcode_0b_set_draw_color,            /* 0x0b */
    menu_opcode_0c_set_texture_page,          /* 0x0c */
    menu_opcode_0d_draw_formatted_number,     /* 0x0d */
    menu_opcode_0e_set_semi_trans,            /* 0x0e */
    menu_opcode_0f_set_draw_priority,         /* 0x0f */
    menu_opcode_10_draw_scrollable_list,      /* 0x10 */
    menu_opcode_11_draw_background_tiles,     /* 0x11 */
    menu_opcode_12_draw_opening_window_frame, /* 0x12 */
    menu_opcode_13_clear_temporary_draw_area, /* 0x13 */
    menu_opcode_14_set_draw_area,             /* 0x14 */
    menu_opcode_15_repeat_draw_area,          /* 0x15 */
    menu_opcode_16_advance_window_scale_step, /* 0x16 */
    menu_opcode_17_draw_right_aligned_number, /* 0x17 */
    menu_opcode_18_add_blend_draw_mode,       /* 0x18 */
    menu_opcode_0d_draw_formatted_number,     /* 0x19 */
    menu_opcode_0d_draw_formatted_number,     /* 0x1a */
    menu_opcode_0d_draw_formatted_number,     /* 0x1b */
};

s8 g_gfx_transition_frame = 0;
RECT g_screen_temporary_draw_area = { 0, 0, 256, 240 };

static menu_sprite_command_t g_screen_script_sprite_command = { 5, 9, 1, { 0, 0, 0, 0, 0, 0 } };
static menu_window_command_t g_screen_window_interior_command = { 17, 7, 0, 0, 0, 0, 0 };
static s16 g_screen_window_scale_table[4] = { 10, 60, 90, 95 };
static oriented_quad_t g_screen_number_blank_sprite = { { 0, 216 }, 7, 8, { 208, 40, 7, 8, 0x7ee7, 31 } };
static s8 g_screen_cursor_bob_thresholds[12] = { 16, 0, 18, 1, 22, 2, 28, 3, 36, 2, 46, 1 };
static s8 g_screen_cursor_bob_active_thresholds[12] = { 2, 0, 8, -2, 10, 1, 12, 3, 14, 5, 16, 4 };

/* Menu script command: run the next count commands when the row predicate holds, otherwise skip
 * them. The predicate gets the command's literal value, or the current list row while the list
 * uses its scroll position. */
menu_instruction_t* menu_opcode_01_if_group(menu_instruction_t* script) {
    menu_instruction_t* instruction = script;
    s32 (*predicate)(s32) = g_menu_row_callbacks[instruction->conditional.predicate].value;
    s32 count = instruction->conditional.count;
    s32 arg;

    if (g_screen_use_scroll_position == 0) {
        arg = instruction->conditional.value;
    } else {
        arg = g_menu_scroll_base_index + g_screen_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
        }
    }
    instruction = menu_next_instruction(instruction);
    if (predicate(arg) == 0) {
        count -= 1;
        while (count != -1) {
            instruction = menu_next_instruction(instruction);
            count -= 1;
        }
    } else {
        count -= 1;
        while (count != -1) {
            instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
            count -= 1;
        }
    }
    return instruction;
}

/* Execute the selected branch and skip the other by encoded instruction
 * lengths, using a literal predicate argument or the current scrolling row. */
menu_instruction_t* menu_opcode_02_if_else_group(menu_instruction_t* instruction) {
    s32 (*predicate)(s32) = g_menu_row_callbacks[instruction->if_else.predicate].value;
    s32 count_true = instruction->if_else.count_true;
    s32 count_false = instruction->if_else.count_false;
    s32 arg;

    if (g_screen_use_scroll_position == 0) {
        arg = instruction->if_else.value;
    } else {
        arg = g_menu_scroll_base_index + g_screen_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
        }
    }
    instruction = menu_next_instruction(instruction);
    if (predicate(arg) != 0) {
        count_true -= 1;
        while (count_true != -1) {
            instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            instruction = menu_next_instruction(instruction);
            count_false -= 1;
        }
    } else {
        count_true -= 1;
        while (count_true != -1) {
            instruction = menu_next_instruction(instruction);
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
            count_false -= 1;
        }
    }
    return instruction;
}

/* Menu script command: draw a filled tile (x, y, width, height and color) at
 * the current draw priority and semi-transparency, and return the next
 * command. Inside a list body, y follows the row being drawn. */
menu_instruction_t* menu_opcode_03_draw_tile(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;

    if (g_screen_use_scroll_position == 0) {
        y = instruction->tile.y;
    } else {
        step = g_screen_row_height;
        y = step * g_screen_scroll_row_offset + instruction->tile.y + g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y -= step;
        }
    }
    rect.x = instruction->tile.x;
    rect.y = y;
    rect.w = instruction->tile.width;
    rect.h = instruction->tile.height;
    gfx_enqueue_tile(&rect, &instruction->tile.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Draws the sprite described by a 9-byte command header (x, y, w, h, u, v)
 * with the current texture page and CLUT, tinted by the menu sprite color
 * unless it is the uncolored variant, and returns the next command. Inside a
 * list body, y follows the row being drawn. */
menu_instruction_t* menu_opcode_04_draw_textured_quad(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;
    rgb24_t* color;

    if (g_screen_use_scroll_position == 0) {
        y = instruction->sprite.sprite.y;
    } else {
        step = g_screen_row_height;
        y = step * g_screen_scroll_row_offset + instruction->sprite.sprite.y - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y -= step;
        }
    }
    rect.x = instruction->sprite.sprite.x;
    rect.y = y;
    rect.w = instruction->sprite.sprite.w;
    rect.h = instruction->sprite.sprite.h;
    if (instruction->sprite.opcode == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    } else {
        color = &g_screen_sprite_color;
    }
    menu_gfx_enqueue_textured_quad(&rect, instruction->sprite.sprite.u, instruction->sprite.sprite.v, color,
        g_menu_gfx_semitransparency, g_screen_texture_page, g_screen_clut, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Draw the command's two endpoints with its RGB color, then advance. */
menu_instruction_t* menu_opcode_07_draw_line(menu_instruction_t* instruction) {
    point16_t endpoints[2];

    endpoints[0].x = instruction->line.x0;
    endpoints[0].y = instruction->line.y0;
    endpoints[1].x = instruction->line.x1;
    endpoints[1].y = instruction->line.y1;
    gfx_enqueue_line_f2(endpoints, &instruction->line.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Ask the selected callback for the literal row's icon, or the current
 * scrolling row's icon, then draw it at the command's position. */
menu_instruction_t* menu_opcode_06_draw_callback_quad(menu_instruction_t* instruction) {
    menu_icon_source_t* (*handler)(s32);
    menu_icon_source_t* entry;
    s32 index;

    handler = g_menu_row_callbacks[instruction->callback_sprite.callback_index].icon;
    if (g_screen_use_scroll_position == 0) {
        index = instruction->callback_sprite.row;
    } else {
        index = g_menu_scroll_base_index + g_screen_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            index -= 1;
        }
    }
    entry = handler(index);
    if (entry != 0) {
        g_screen_script_sprite_command.sprite.x = instruction->callback_sprite.x;
        g_screen_script_sprite_command.sprite.y = instruction->callback_sprite.y;
        /* The icon source stores 16-bit values; the menu script copies their low bytes. */
        g_screen_script_sprite_command.sprite.w = entry->w;
        g_screen_script_sprite_command.sprite.h = entry->h;
        g_screen_script_sprite_command.sprite.u = entry->u;
        g_screen_script_sprite_command.sprite.v = entry->v;
        g_screen_clut = entry->clut;
        g_screen_texture_page = entry->tpage;
        menu_opcode_04_draw_textured_quad((menu_instruction_t*)&g_screen_script_sprite_command);
    }
    return menu_next_instruction(instruction);
}

/*
 * Draw a menu window's 5x9/5x7 border pieces around its rectangle and tile
 * the interior through menu_opcode_11_draw_background_tiles.
 *
 * Source 2 takes the signed rectangle from the shared window record; other
 * sources use the command's byte operands. Source 1 selects the alternate
 * border texture coordinates. The copied return statement keeps the command
 * pointer's reference count above the draw priority's, so global allocation
 * places them in s6 and s7 like the target.
 */
menu_instruction_t* menu_opcode_00_draw_window_frame(menu_instruction_t* instruction) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority = g_menu_gfx_otag_index;
    s16 u_left;
    s16 u_middle;
    s16 u_right;
    s16 v_bottom;
    u16 clut = g_menu_palette_clut;
    s16 v_top;
    s32 column;
    s32 row;

    if (instruction->window.rectangle_source == 2) {
        left = g_menu_window_rect_source->rect.x - 5;
        top = g_menu_window_rect_source->rect.y - 9;
        full_columns = g_menu_window_rect_source->rect.w / 16;
        partial_width = g_menu_window_rect_source->rect.w % 16;
        full_rows = g_menu_window_rect_source->rect.h / 16;
        partial_height = g_menu_window_rect_source->rect.h % 16;
    } else {
        u8 width;
        u8 height;
        left = instruction->window.x - 5;
        top = instruction->window.y - 9;
        width = instruction->window.width;
        full_columns = width >> 4;
        partial_width = width & 15;
        height = instruction->window.height;
        full_rows = height >> 4;
        partial_height = height & 15;
    }
    g_screen_window_interior_command.rectangle_source = instruction->window.rectangle_source;
    g_screen_window_right_x = instruction->window.width + left + 5;
    v_top = 2;
    if (instruction->window.rectangle_source == 1) {
        u_left = 0xDA;
        v_top = 3;
        u_middle = 0xDF;
        u_right = 0xF7;
        v_bottom = 0x18;
    } else {
        u_left = 2;
        u_middle = 8;
        u_right = 0x1F;
        v_bottom = 0x1C;
    }

    /* Top edge: the left corner, the partial piece, the full 16-pixel pieces and the right corner. */
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);

    /* Side edges, partial piece first. */
    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(&rect, 2, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    }

    if (instruction->window.rectangle_source == 2) {
        g_screen_window_rect.x = g_menu_window_rect_source->rect.x;
        g_screen_window_rect.y = g_menu_window_rect_source->rect.y;
        g_screen_window_rect.w = g_menu_window_rect_source->rect.w;
        g_screen_window_rect.h = g_menu_window_rect_source->rect.h;
    } else {
        g_screen_window_interior_command.x = instruction->window.x;
        g_screen_window_interior_command.y = instruction->window.y;
        g_screen_window_interior_command.width = instruction->window.width;
        g_screen_window_interior_command.height = instruction->window.height;
    }
    menu_opcode_11_draw_background_tiles((menu_instruction_t*)&g_screen_window_interior_command);

    /* Bottom edge, laid out like the top. */
    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    instruction = menu_next_instruction(instruction);
    return instruction;
}

/*
 * Apply the centered opening-animation clip before processing a window command.
 *
 * Steps 0-3 scale width and height by the signed percentage table. Later
 * steps disable the temporary clip; the downstream command still runs.
 */
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* instruction) {
    RECT clip;
    s32 scale;

    if (g_gfx_transition_frame < 4) {
        if (g_gfx_transition_frame < 3) {
            g_menu_script_input = 0;
        }
        scale = g_screen_window_scale_table[g_gfx_transition_frame];
        clip.w = instruction->window.width * scale / 100;
        clip.h = instruction->window.height * scale / 100;
        clip.x = instruction->window.x + (instruction->window.width >> 1) - (clip.w >> 1);
        clip.y = instruction->window.y + (instruction->window.height >> 1) - (clip.h >> 1);
        screen_add_draw_area(&clip, g_menu_gfx_otag_index - 1);
        g_gfx_otag_index_locked = 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    return menu_opcode_00_draw_window_frame(instruction);
}

/*
 * Tile a menu rectangle with 16-pixel sprites and clipped edge pieces.
 *
 * Source 2 uses the signed shared rectangle; other sources use byte operands.
 */
menu_instruction_t* menu_opcode_11_draw_background_tiles(menu_instruction_t* instruction) {
    RECT rect;
    s32 base_x;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority = g_menu_gfx_otag_index;
    u16 clut = g_menu_palette_clut;
    s32 full_columns;
    s32 row;
    s32 column;
    s32 base_y;

    if (instruction->window.rectangle_source == 2) {
        base_x = g_screen_window_rect.x;
        base_y = g_screen_window_rect.y;
        full_columns = g_screen_window_rect.w / 16;
        partial_width = g_screen_window_rect.w % 16;
        full_rows = g_screen_window_rect.h / 16;
        partial_height = g_screen_window_rect.h % 16;
    } else {
        u8 width;
        u8 height;
        base_x = instruction->window.x;
        base_y = instruction->window.y;
        width = instruction->window.width;
        full_columns = width >> 4;
        partial_width = width & 15;
        height = instruction->window.height;
        full_rows = height >> 4;
        partial_height = height & 15;
    }
    for (row = 0; row < full_rows; row++) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
        }
    }
    if (partial_height == 0) {
        return menu_next_instruction(instruction);
    }
    rect.x = base_x;
    rect.y = base_y + row * 16;
    rect.w = partial_width;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(&rect, 8, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = base_x + partial_width + column * 16;
        rect.y = base_y + row * 16;
        rect.w = 16;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_screen_window_tpage, clut, priority);
    }
    return menu_next_instruction(instruction);
}

/* Menu script opcode: compute a CLUT id from the packed operands and record it.
 * Returns the next script instruction pointer.
 *
 * Built at -O1: the unscheduled prologue and load order are the -O1 signature
 * of the menu-script handlers (see world_menu_script_* in src/world). The
 * reassigned s32 y defeats plus->ior folding (gives addu). */
menu_instruction_t* menu_opcode_09_set_fixed_clut(menu_instruction_t* instruction) {
    s32 x = instruction->fixed_clut.x_div_16;
    s32 y;

    x <<= 4;
    y = instruction->fixed_clut.y_low;
    y += instruction->fixed_clut.y_high << 8;
    g_screen_clut = GetClut(x, y);
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: pick the CLUT x/y operands from bytes 2..4 or,
 * when g_screen_use_alternate_palette selects the alternate palette, bytes 5..7, and record the
 * resulting CLUT id. Advances by the instruction length (byte 1), like
 * menu_opcode_0c_set_texture_page. */
menu_instruction_t* menu_opcode_0a_set_clut(menu_instruction_t* instruction) {
    if (g_screen_use_alternate_palette != 0) {
        g_screen_clut = GetClut(instruction->clut.alternate_x_div_16 << 4,
            instruction->clut.alternate_y_low + (instruction->clut.alternate_y_high << 8));
    } else {
        g_screen_clut = GetClut(instruction->clut.normal_x_div_16 << 4,
            instruction->clut.normal_y_low + (instruction->clut.normal_y_high << 8));
    }
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: set the texture page from the operand bytes and
 * advance by the instruction length (byte 1), like menu_opcode_0e_set_semi_trans. */
menu_instruction_t* menu_opcode_0c_set_texture_page(menu_instruction_t* instruction) {
    g_screen_texture_page = GetTPage(instruction->texture_page.depth, instruction->texture_page.y_blend >> 4,
        instruction->texture_page.x_div_16 << 4, instruction->texture_page.y_blend << 8);
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: byte 3 of the instruction selects the
 * semi-transparency flag used by the window/text primitives. Returns the
 * address of the next instruction (byte 1 is the instruction length). */
menu_instruction_t* menu_opcode_0e_set_semi_trans(menu_instruction_t* instruction) {
    g_menu_gfx_semitransparency = instruction->draw_state.value;
    return menu_next_instruction(instruction);
}

/* Menu script command: queue a draw-mode packet that selects the command's blend rate at the
 * current ordering-table slot, then continue with the next command. */
menu_instruction_t* menu_opcode_18_add_blend_draw_mode(menu_instruction_t* instruction) {
    menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, instruction->blend.blend_rate, 0x100, 0), 0, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Set the current sprite color from a menu-script operand triplet.
 *
 * The palette selector chooses bytes 5..7 instead of the normal bytes 2..4. */
menu_instruction_t* menu_opcode_0b_set_draw_color(menu_instruction_t* instruction) {
    if (g_screen_use_alternate_palette != 0) {
        g_screen_sprite_color.r = instruction->sprite_color.alternate.r,
        g_screen_sprite_color.g = instruction->sprite_color.alternate.g,
        g_screen_sprite_color.b = instruction->sprite_color.alternate.b;
    } else {
        g_screen_sprite_color.r = instruction->sprite_color.normal.r,
        g_screen_sprite_color.g = instruction->sprite_color.normal.g,
        g_screen_sprite_color.b = instruction->sprite_color.normal.b;
    }
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: byte 3 of the instruction sets the menu draw
 * priority. Returns the address of the next instruction. */
menu_instruction_t* menu_opcode_0f_set_draw_priority(menu_instruction_t* instruction) {
    g_menu_gfx_otag_index = instruction->draw_state.value;
    return menu_next_instruction(instruction);
}

/*
 * Render a formatted callback value with the menu's compact number glyphs.
 *
 * The command selects its value source, width, spacing, palette, and one of
 * three glyph layouts. High value bits request a disabled palette or dashes.
 */
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* command) {
    s8 text[16];
    RECT rect;
    RECT glyph;
    RECT minus_rect;
    s32 (*getter)(s32);
    s32 row;
    s16 step;
    s32 y;
    s32 value;
    s32 digit;
    s16 blanks;
    s32 text_index;
    s32 i;
    s8 negative;
    rgb24_t* color;
    u16 clut;

    getter = g_menu_row_callbacks[command->number.getter].value;
    if (g_screen_use_scroll_position == 0) {
        row = command->number.value;
        y = command->number.y;
    } else {
        row = g_menu_scroll_base_index + g_screen_scroll_row_offset;
        step = g_screen_row_height;
        y = step * g_screen_scroll_row_offset + command->number.y - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            row--;
            y -= step;
        }
    }
    negative = 0;
    if (getter != 0) {
        value = getter(row);
    } else {
        value = row;
    }
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_TINTED
        || command->number.opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
        color = &g_screen_sprite_color;
    } else {
        color = 0;
    }
    if (command->number.alternate_palette != 0) {
        clut = g_screen_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_screen_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }
    if (command->number.style == 1) {
        glyph.x = 0x78;
        glyph.y = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (command->number.style == 0) {
        glyph.x = 0x78;
        glyph.y = 0x10;
        glyph.w = 6;
        glyph.h = 0xB;
    } else {
        glyph.x = 0x20;
        glyph.y = 0;
        glyph.w = 0xA;
        glyph.h = 0x10;
    }
    if (value & MENU_NUMBER_VALUE_DASHES) {
        for (value = command->number.width; value > 0; value--) {
            rect.x = command->number.x + (value - 1) * command->number.spacing;
            rect.y = y + 5;
            rect.w = 6;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(&rect, 0xBA, 0x13, color, g_menu_gfx_semitransparency, g_screen_window_tpage,
                clut, g_menu_gfx_otag_index);
        }
    } else {
        blanks = command->number.width - sprintf(text, "%d", value) - negative;
        text_index = 0;
        for (i = 0; i < command->number.width; i++) {
            rect.x = command->number.x + i * command->number.spacing;
            rect.y = y;
            rect.w = glyph.w;
            rect.h = glyph.h;
            if (negative != 0) {
                minus_rect.x = rect.x - 6;
                minus_rect.y = y + 2;
                minus_rect.w = 6;
                minus_rect.h = 0xB;
                if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS) {
                    minus_rect.x = 0xAE;
                }
                menu_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, color, g_menu_gfx_semitransparency,
                    g_screen_window_tpage, clut, g_menu_gfx_otag_index);
                negative = 0;
            } else {
                if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS) {
                    clut = g_menu_palette_clut;
                }
                if (blanks != 0) {
                    digit = 0;
                    blanks--;
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
                        continue;
                    }
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS
                        && i < command->number.width - 1) {
                        clut = g_screen_disabled_number_clut;
                    }
                } else {
                    digit = text[text_index] - '0';
                    text_index++;
                }
                if (command->number.style < 2) {
                    menu_gfx_enqueue_textured_quad(&rect, glyph.x + glyph.w * digit, glyph.y, color,
                        g_menu_gfx_semitransparency, g_screen_window_tpage, clut, g_menu_gfx_otag_index);
                }
            }
        }
    }
    return menu_next_instruction(command);
}

/*
 * Render a plain right-aligned number with the menu's compact glyphs.
 *
 * The printed value and the digit count are one variable, which keeps the
 * value in $s2 across the if/else join. The count is accumulated
 * (`value = width; value -= blanks;`) so the re-read width lands in the
 * destination register as the minuend; written as a single subtraction the
 * sign-extended blanks claims $s2 and the operands swap.
 */
menu_instruction_t* menu_opcode_17_draw_right_aligned_number(menu_instruction_t* instruction) {
    s8 text[16];
    RECT rect;
    s32 value;
    s16 blanks;
    s32 x;
    s32 y;
    s32 i;

    value = g_menu_row_callbacks[instruction->number.getter].value != 0
        ? g_menu_row_callbacks[instruction->number.getter].value(instruction->number.value)
        : instruction->number.value;
    blanks = instruction->number.width - sprintf(text, "%d", value);
    value = instruction->number.width;
    value -= blanks;
    x = instruction->number.x;
    y = instruction->number.y;
    g_screen_number_blank_sprite.position.y = y + 4;
    blanks -= 1;
    while (blanks != -1) {
        g_screen_number_blank_sprite.position.x = x;
        menu_gfx_enqueue_oriented_textured_quad(
            &g_screen_number_blank_sprite, 0, TEXTURE_UV_FLIP_NONE, 0, g_menu_gfx_otag_index);
        blanks -= 1;
        x += 8;
    }
    rect.x = x;
    rect.y = y + 2;
    rect.w = 8;
    rect.h = 14;
    /* The digit glyphs sit 8 pixels apart from u 0xA8 ('0' * 8 - 0xD8). */
    i = 0;
    while (i < value) {
        menu_gfx_enqueue_textured_quad(
            &rect, text[i] * 8 - 0xD8, 0x32, 0, 0, g_menu_cursor_texture_page, 0x7EE7, g_menu_gfx_otag_index);
        i++;
        rect.x += 8;
    }
    return menu_next_instruction(instruction);
}

/*
 * Draw and advance the two-layer cursor used by equipment and item menus.
 *
 * Bit 15 selects the alternate animation. The `state_mode` copies are the
 * target's own `move v0,s2` before each test.
 */
void screen_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode) {
    RECT rect;
    /* Pin required: unpinned, the roles held in $s2/$s3/$s4 rotate. */
    register s32 active __asm__("$20");
    u32 state_mode;
    s32 alternate;
    s32 timer;
    s32 i;
    s8 raw_offset;
    s32 x_offset;
    u16 front_clut;
    u16 shadow_clut;

    active = mode;
    state_mode = *state >> 15;
    alternate = state_mode;
    if (active == 0) {
        state_mode = alternate;
        if (state_mode != 0) {
            *state = 0;
            /* Hides that active is 0 here, so alternate = 0 stays `move s2,zero`
             * instead of reusing $s4. */
            __asm__("" : "=r"(active) : "0"(active));
            alternate = 0;
        }
    }
    state_mode = alternate;
    if (state_mode == 0 && (active & 1)) {
        *state = 0x8000;
        alternate = 1;
    }
    timer = *state;
    timer &= 0x7F;
    timer *= menu_get_event_speed();
    state_mode = alternate;
    if (state_mode != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else if (timer >= 0x2F) {
        timer = 0;
    }
    state_mode = alternate;
    if (state_mode != 0) {
        for (i = 0; g_screen_cursor_bob_active_thresholds[i] < timer; i += 2) { }
        raw_offset = g_screen_cursor_bob_active_thresholds[i + 1];
    } else {
        for (i = 0; g_screen_cursor_bob_thresholds[i] < timer; i += 2) { }
        raw_offset = g_screen_cursor_bob_thresholds[i + 1];
    }
    x_offset = raw_offset;
    timer += menu_get_event_speed();
    state_mode = alternate;
    if (state_mode != 0) {
        if (timer < 0x25) {
            *state += 1;
        }
    } else if (timer < 0x2F) {
        *state += 1;
    } else {
        *state = 0;
    }
    if (active != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }
    rect.x = x_offset + pos->x - 2;
    rect.y = pos->y;
    rect.w = 0x10;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_menu_cursor_texture_page, front_clut, g_menu_gfx_otag_index);
    rect.x += 2;
    rect.y += 2;
    menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, g_menu_gfx_otag_index);
}

/*
 * Draw and advance the vertical two-layer cursor used by menu prompts.
 *
 * This variant offsets a shared textured quad vertically before submitting
 * two `POLY_FT4` layers. Bit 15 selects the alternate animation.
 */
void menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode) {
    RECT rect; /* never used; retail reserves its 8 bytes */
    u32 state_mode = *state >> 15;
    s32 active = state_mode;
    s32 timer;
    s32 i;
    s8 bob;
    s32 y_offset;
    s32 bottom;
    u16 front_clut;
    u16 shadow_clut;

    if (mode == 0) {
        state_mode = active;
        if (state_mode != 0) {
            *state = 0;
            active = 0;
        }
    }
    state_mode = active;
    if (state_mode == 0 && (mode & 1)) {
        *state = 0x8000;
        active = 1;
    }
    timer = *state;
    timer &= 0x7F;
    timer *= menu_get_event_speed();
    state_mode = active;
    if (state_mode != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else if (timer >= 0x2F) {
        timer = 0;
    }
    state_mode = active;
    if (state_mode != 0) {
        for (i = 0; g_screen_cursor_bob_active_thresholds[i] < timer; i += 2) { }
        bob = g_screen_cursor_bob_active_thresholds[i + 1];
    } else {
        for (i = 0; g_screen_cursor_bob_thresholds[i] < timer; i += 2) { }
        bob = g_screen_cursor_bob_thresholds[i + 1];
    }
    y_offset = bob;
    timer += menu_get_event_speed();
    state_mode = active;
    if (state_mode != 0) {
        if (timer < 0x25) {
            *state += 1;
        }
    } else if (timer < 0x2F) {
        *state += 1;
    } else {
        *state = 0;
    }
    if (mode != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    g_screen_vertical_cursor_quad.x0 = pos->x;
    g_screen_vertical_cursor_quad.y0 = y_offset + pos->y;
    g_screen_vertical_cursor_quad.x1 = pos->x + 0x10;
    g_screen_vertical_cursor_quad.y1 = y_offset + pos->y;
    g_screen_vertical_cursor_quad.x2 = pos->x;
    bottom = y_offset + 0x10;
    g_screen_vertical_cursor_quad.y2 = pos->y + bottom;
    g_screen_vertical_cursor_quad.x3 = pos->x + 0x10;
    g_screen_vertical_cursor_quad.y3 = pos->y + bottom;
    g_screen_vertical_cursor_quad.clut = front_clut;
    g_screen_vertical_cursor_quad.tpage = g_menu_cursor_texture_page;
    gfx_enqueue_textured_quad_corners(&g_screen_vertical_cursor_quad, 0, 0, g_menu_gfx_otag_index);

    g_screen_vertical_cursor_quad.x0 = pos->x;
    g_screen_vertical_cursor_quad.y0 = y_offset + pos->y + 2;
    g_screen_vertical_cursor_quad.x1 = pos->x + 0x10;
    g_screen_vertical_cursor_quad.y1 = y_offset + pos->y + 2;
    g_screen_vertical_cursor_quad.x2 = pos->x;
    g_screen_vertical_cursor_quad.y2 = pos->y + bottom + 2;
    g_screen_vertical_cursor_quad.x3 = pos->x + 0x10;
    g_screen_vertical_cursor_quad.y3 = pos->y + bottom + 2;
    g_screen_vertical_cursor_quad.clut = shadow_clut;
    g_screen_vertical_cursor_quad.tpage = 0x5F;
    gfx_enqueue_textured_quad_corners(&g_screen_vertical_cursor_quad, 0, 1, g_menu_gfx_otag_index);
}

/* Walks past the records that follow the header, returning the next one. */
static menu_instruction_t* screen_script_skip_group(menu_instruction_t* instruction) {
    s32 count;

    if (instruction->header.opcode == MENU_SCRIPT_OP_IF_GROUP) {
        count = instruction->conditional.count;
        count += 1;
    } else if (instruction->header.opcode == MENU_SCRIPT_OP_IF_ELSE_GROUP) {
        count = instruction->if_else.count_true;
        count += instruction->if_else.count_false;
        count += 1;
    } else {
        count = 1;
    }
    count -= 1;
    while (count != -1) {
        instruction = menu_next_instruction(instruction);
        count -= 1;
    }
    return instruction;
}

/* Find the scrollable-list command in a menu script, load its row and column layout, clamp the
 * selection and scroll base to the list, and render the visible rows' text into the list image in
 * VRAM. */
void screen_load_list_layout(const menu_instruction_t* script) {
    RECT rect;
    s16 text_ids[20];
    const menu_list_record_t* record;
    const menu_instruction_t* instruction = script;
    s32 i;

    while (instruction->header.opcode != MENU_SCRIPT_OP_SCROLLABLE_LIST) {
        if (instruction->header.opcode == SCREEN_SCRIPT_END) {
            return;
        }
        instruction = menu_next_const_instruction(instruction);
    }
    record = &instruction->list;
    g_screen_list_row_record_count = record->skip_count;
    g_screen_row_height = record->row_height;
    g_screen_list_visible_rows = record->visible_rows;
    g_screen_list_glyph_width = record->width_a;
    g_screen_list_text_columns = record->width_b;
    g_screen_list_image_width = (g_screen_list_glyph_width * g_screen_list_text_columns) >> 2;
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (g_menu_list_selected_index - g_menu_scroll_base_index >= g_screen_list_visible_rows) {
        g_menu_scroll_base_index = g_menu_list_selected_index;
    }
    if (g_menu_list_selected_index < g_menu_scroll_base_index) {
        g_menu_scroll_base_index = g_menu_list_selected_index;
    }
    if (g_menu_list_entry_count < g_screen_list_visible_rows) {
        g_menu_scroll_base_index = 0;
    } else if (g_menu_list_entry_count - g_menu_scroll_base_index < g_screen_list_visible_rows) {
        g_menu_scroll_base_index = g_menu_list_entry_count - g_screen_list_visible_rows;
    }
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (g_screen_list_text_table != 0) {
        rect.x = 0x240;
        rect.y = 0x130;
        rect.w = g_screen_list_image_width;
        rect.h = g_screen_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_screen_list_visible_rows; i++) {
            text_ids[i] = g_screen_list_entry_ids[i + g_menu_scroll_base_index];
        }
        text_ids[i] = -1;
        rect.x = 0x240;
        rect.y = 0x130;
        rect.w = g_screen_list_image_width;
        rect.h = g_screen_row_height;
        screen_text_render_rows(g_screen_list_text_table, text_ids, &rect, 0);
    }
    g_screen_list_scroll_direction = 0;
    g_screen_list_layout_pending = 0;
}

/* Initialize the list state, then load its visible entries from the menu
 * definition. */
void screen_init_list(const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script) {
    menu_init_scrollable_list_core(values, selected_index, text_table);
    screen_load_list_layout(script);
}

/* Built at -O1: the unscheduled prologue (sw ra, sw s0, lw s0) is the -O1 signature shared by the
 * neighboring menu-script handlers. */
void screen_init_and_draw_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list(entries, selected_index, scroll_base_index, text_table);
    screen_load_list_layout(script);
}

/*
 * Menu-script handler for a scrollable list: the WORLD original of the
 * overlays' scrollable-list body commands.
 *
 * Moves the cursor on Up/Down (the held-repeat flags of Up and Down, bits
 * 16 up of the script input, keep a scroll going without moving the cursor)
 * and pages through screen_scroll_list_page. Moving past the first or
 * last visible row starts a one-row scroll: the newly exposed row is
 * rendered into the list's VRAM strip at x 0x240, the pixel offset advances
 * by the slow step until the repeat timer reaches
 * g_menu_scroll_accel_delay and by the fast step after it, and when a
 * full row height is covered the strip is moved one row with draw-move
 * primitives (one per row when scrolling up). Then it draws the list image,
 * the up and down arrows, the scrollbar thumb and the cursor, and replays the
 * per-row record group once per visible row, plus one while a scroll is in
 * progress.
 *
 * An empty list skips the list command and its following row group. Returns
 * the record that follows the list.
 */
menu_instruction_t* menu_opcode_10_draw_scrollable_list(menu_instruction_t* command) {
    RECT rect;
    s32 unused[4];
    point16_t marker;
    u16 ot_index;
    u8 speed;
    s32 i;
    s32 j;
    s32 rows;
    s32 index;
    u16 old_cursor;
    menu_instruction_t* row_commands;

    ot_index = g_menu_gfx_otag_index;
    if (g_menu_list_entry_count <= 0) {
        for (j = command->list.skip_count; j != -1; j--) {
            command = screen_script_skip_group(command);
        }
        return command;
    }
    speed = menu_get_event_speed();
    g_screen_use_scroll_position = 1;
    if (g_screen_list_layout_pending != 0) {
        screen_load_list_layout(command);
    }
    old_cursor = g_menu_list_selected_index;
    if (g_screen_list_scroll_direction == 0) {
        i = menu_input_read_page_scroll_direction();
        if (i != 0) {
            if (i != 2) {
                screen_scroll_list_page(i, command);
            }
        } else if ((g_menu_script_input & (PSX_PAD_UP << 16)) || (g_menu_script_input & PSX_PAD_UP)) {
            index = g_menu_scroll_base_index;
            if (index == g_menu_list_selected_index) {
                if (index > 0) {
                    g_screen_list_scroll_direction = -1;
                    if (g_screen_list_text_table != 0) {
                        g_screen_list_row_text_ids[0] = g_screen_list_entry_ids[index - 1];
                        rect.x = 0x240;
                        rect.y = 0x120;
                        rect.w = g_screen_list_image_width;
                        rect.h = g_screen_row_height;
                        screen_text_render_rows(g_screen_list_text_table, g_screen_list_row_text_ids, &rect, 0);
                    }
                }
            } else if (g_menu_script_input & PSX_PAD_UP) {
                g_menu_list_selected_index--;
            }
        } else if ((g_menu_script_input & (PSX_PAD_DOWN << 16)) || (g_menu_script_input & PSX_PAD_DOWN)) {
            if (g_menu_scroll_base_index + g_screen_list_visible_rows - 1 == g_menu_list_selected_index) {
                if (g_menu_scroll_base_index + g_screen_list_visible_rows - 1 < g_menu_list_entry_count - 1) {
                    g_screen_list_scroll_direction = 1;
                    if (g_screen_list_text_table != 0) {
                        index = g_menu_scroll_base_index + g_screen_list_visible_rows - 1;
                        g_screen_list_row_text_ids[0] = g_screen_list_entry_ids[index + 1];
                        rect.x = 0x240;
                        rect.y = g_screen_list_visible_rows * g_screen_row_height + 0x130;
                        rect.w = g_screen_list_image_width;
                        rect.h = g_screen_row_height;
                        screen_text_render_rows(g_screen_list_text_table, g_screen_list_row_text_ids, &rect, 0);
                    }
                }
            } else {
                if (g_menu_list_selected_index < g_menu_list_entry_count - 1 && (g_menu_script_input & PSX_PAD_DOWN)) {
                    g_menu_list_selected_index++;
                }
            }
        } else {
            g_screen_scroll_repeat_timer = 0;
        }
    }
    if (g_screen_list_scroll_direction < 0) {
        if (g_screen_scroll_repeat_timer >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= g_menu_scroll_fast_step * speed;
        } else {
            g_screen_scroll_repeat_timer += speed;
            g_menu_list_scroll_offset -= g_menu_scroll_slow_step * speed;
        }
        if (g_menu_list_scroll_offset <= -g_screen_row_height) {
            g_menu_list_scroll_offset = 0;
            g_screen_list_scroll_direction = 0;
            g_menu_scroll_base_index = --g_menu_list_selected_index;
            if (g_screen_list_text_table != 0) {
                for (i = 0; i < g_screen_list_visible_rows; i++) {
                    rect.x = 0x240;
                    rect.y = (i - 1) * g_screen_row_height + 0x130;
                    rect.w = g_screen_list_image_width;
                    rect.h = g_screen_row_height;
                    menu_gfx_enqueue_draw_move(&rect, 0x240, g_screen_row_height * i + 0x130, ot_index - 1);
                }
            }
        }
    } else if (g_screen_list_scroll_direction > 0) {
        if (g_screen_scroll_repeat_timer >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += g_menu_scroll_fast_step * speed;
        } else {
            g_screen_scroll_repeat_timer += speed;
            g_menu_list_scroll_offset += g_menu_scroll_slow_step * speed;
        }
        if (g_menu_list_scroll_offset >= g_screen_row_height) {
            g_menu_list_scroll_offset = 0;
            g_screen_list_scroll_direction = 0;
            g_menu_scroll_base_index++;
            g_menu_list_selected_index++;
            if (g_screen_list_text_table != 0) {
                rect.x = 0x240;
                rect.y = g_screen_row_height + 0x130;
                rect.w = g_screen_list_image_width;
                rect.h = g_screen_row_height * g_screen_list_visible_rows;
                menu_gfx_enqueue_draw_move(&rect, 0x240, 0x130, ot_index - 1);
            }
        }
    }
    if (g_screen_list_text_table != 0) {
        if (g_menu_list_scroll_offset != 0) {
            rect.x = command->list.x;
            rect.y = command->list.y - 5;
            rect.w = g_screen_list_image_width * 4;
            rect.h = g_screen_row_height * g_screen_list_visible_rows + 10;
            menu_gfx_enqueue_textured_quad(&rect, 0, g_menu_list_scroll_offset + 0x2B, 0, g_menu_gfx_semitransparency,
                g_screen_window_tpage, g_screen_window_clut, g_menu_gfx_otag_index);
        } else {
            rect.x = command->list.x;
            rect.y = command->list.y;
            rect.w = g_screen_list_image_width * 4;
            rect.h = g_screen_row_height * g_screen_list_visible_rows;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_menu_gfx_semitransparency, g_screen_window_tpage,
                g_screen_window_clut, g_menu_gfx_otag_index);
        }
    }
    if (g_screen_use_alternate_palette == 0) {
        if (g_menu_scroll_base_index != 0) {
            rect.x = g_screen_window_right_x;
            rect.y = command->list.arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_scroll_base_index < g_menu_list_entry_count - g_screen_list_visible_rows) {
            rect.x = g_screen_window_right_x;
            rect.y = command->list.arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xE0, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_list_entry_count > g_screen_list_visible_rows) {
            i = command->list.arrow_up_y + 0xF;
            j = command->list.arrow_down_y - (s16)(i + 5);
            i += j * g_menu_list_selected_index / (g_menu_list_entry_count - 1);
            rect.x = g_screen_window_right_x;
            rect.y = i;
            rect.w = 8;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
    }
    if (command->list.show_cursor != 0) {
        marker.x = command->list.x - command->list.cursor_x_offset;
        marker.y = command->list.y + (g_menu_list_selected_index - g_menu_scroll_base_index) * g_screen_row_height;
        screen_draw_sliding_cursor(&marker, &g_screen_list_cursor_anim, g_screen_use_alternate_palette);
    }
    command = menu_next_instruction(command);
    g_screen_scroll_row_offset = 0;
    rows = g_screen_list_visible_rows + 1;
    if (g_menu_list_scroll_offset == 0) {
        rows--;
    }
    row_commands = command;
    for (i = 0; i < rows; i++, g_screen_scroll_row_offset++) {
        if (g_menu_scroll_base_index + i > g_menu_list_entry_count - 1 && g_menu_list_scroll_offset == 0) {
            break;
        }
        command = row_commands;
        for (j = 0; j < g_screen_list_row_record_count; j++) {
            command = g_menu_opcode_handlers[command->header.opcode](command);
        }
    }
    g_screen_use_scroll_position = 0;
    if (old_cursor != g_menu_list_selected_index) {
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
    return command;
}

/* Set the WORLD menu sprite color from an RGB triple; a null color leaves it unchanged. */
void screen_set_sprite_color(const rgb24_t* src) {
    if (src != 0) {
        g_screen_sprite_color.r = src->r;
        g_screen_sprite_color.g = src->g;
        g_screen_sprite_color.b = src->b;
    }
}
