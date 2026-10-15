#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Execute or skip the following instruction group according to the selected
 * predicate, using a literal argument or the current scrolling row. */
menu_instruction_t* world_menu_script_conditional(menu_instruction_t* script) {
    menu_instruction_t* instruction = script;
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[instruction->conditional.predicate].value;
    s32 count = instruction->conditional.count;
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = instruction->conditional.value;
    } else {
        arg = g_menu_scroll_base_index + g_world_menu_scroll_row_offset;
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
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
            count -= 1;
        }
    }
    return instruction;
}

/* Execute the selected branch and skip the other by encoded instruction
 * lengths, using a literal predicate argument or the current scrolling row. */
menu_instruction_t* world_menu_script_if_else(menu_instruction_t* instruction) {
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[instruction->if_else.predicate].value;
    s32 count_true = instruction->if_else.count_true;
    s32 count_false = instruction->if_else.count_false;
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = instruction->if_else.value;
    } else {
        arg = g_menu_scroll_base_index + g_world_menu_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
        }
    }
    instruction = menu_next_instruction(instruction);
    if (predicate(arg) != 0) {
        count_true -= 1;
        while (count_true != -1) {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
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
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
            count_false -= 1;
        }
    }
    return instruction;
}

/* Draws the rectangle described by a 6-byte command header (x, y, w, h)
 * followed by its payload, and returns the next command. */
menu_instruction_t* world_menu_script_draw_tile(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;

    if (g_world_menu_use_scroll_position == 0) {
        y = instruction->tile.y;
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + instruction->tile.y + g_menu_list_scroll_offset;
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
 * and returns the next command. */
menu_instruction_t* world_menu_script_draw_sprite(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;
    rgb24_t* color;

    if (g_world_menu_use_scroll_position == 0) {
        y = instruction->sprite.sprite.y;
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + instruction->sprite.sprite.y - g_menu_list_scroll_offset;
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
        color = &g_world_menu_sprite_color;
    }
    menu_gfx_enqueue_textured_quad(&rect, instruction->sprite.sprite.u, instruction->sprite.sprite.v, color,
        g_menu_gfx_semitransparency, g_world_menu_texture_page, g_world_menu_clut, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Draw the command's two endpoints with its RGB color, then advance. */
menu_instruction_t* world_menu_script_draw_line(menu_instruction_t* instruction) {
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
menu_instruction_t* world_menu_script_draw_callback_sprite(menu_instruction_t* instruction) {
    menu_icon_source_t* (*handler)(s32);
    menu_icon_source_t* entry;
    s32 index;

    handler = g_world_menu_script_callbacks[instruction->callback_sprite.callback_index].icon;
    if (g_world_menu_use_scroll_position == 0) {
        index = instruction->callback_sprite.row;
    } else {
        index = g_menu_scroll_base_index + g_world_menu_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            index -= 1;
        }
    }
    entry = handler(index);
    if (entry != 0) {
        g_world_menu_script_sprite_command.sprite.x = instruction->callback_sprite.x;
        g_world_menu_script_sprite_command.sprite.y = instruction->callback_sprite.y;
        /* The icon source stores 16-bit values; the menu script copies their low bytes. */
        g_world_menu_script_sprite_command.sprite.w = entry->w;
        g_world_menu_script_sprite_command.sprite.h = entry->h;
        g_world_menu_script_sprite_command.sprite.u = entry->u;
        g_world_menu_script_sprite_command.sprite.v = entry->v;
        g_world_menu_clut = entry->clut;
        g_world_menu_texture_page = entry->tpage;
        world_menu_script_draw_sprite((menu_instruction_t*)&g_world_menu_script_sprite_command);
    }
    return menu_next_instruction(instruction);
}

/*
 * Draw a menu window's 5x9/5x7 border pieces around its rectangle and tile
 * the interior through world_menu_parse_draw_tiled_rectangle.
 *
 * Source 2 takes the signed rectangle from the shared window record; other
 * sources use the command's byte operands. Source 1 selects the alternate
 * border texture coordinates. The copied return statement keeps the command
 * pointer's reference count above the draw priority's, so global allocation
 * places them in s6 and s7 like the target.
 */
menu_instruction_t* world_menu_parse_draw_window_frame(menu_instruction_t* instruction) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority;
    s16 u_left;
    s16 u_middle;
    s16 u_right;
    s16 v_bottom;
    u16 clut;
    s16 v_top;
    s32 column;
    s32 row;

    priority = g_menu_gfx_otag_index;
    clut = g_menu_palette_clut;
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
    g_world_menu_window_interior_command.rectangle_source = instruction->window.rectangle_source;
    g_world_menu_window_right_x = instruction->window.width + left + 5;
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

    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    }

    if (instruction->window.rectangle_source == 2) {
        g_world_menu_window_rect.x = g_menu_window_rect_source->rect.x;
        g_world_menu_window_rect.y = g_menu_window_rect_source->rect.y;
        g_world_menu_window_rect.w = g_menu_window_rect_source->rect.w;
        g_world_menu_window_rect.h = g_menu_window_rect_source->rect.h;
    } else {
        g_world_menu_window_interior_command.x = instruction->window.x;
        g_world_menu_window_interior_command.y = instruction->window.y;
        g_world_menu_window_interior_command.width = instruction->window.width;
        g_world_menu_window_interior_command.height = instruction->window.height;
    }
    world_menu_parse_draw_tiled_rectangle((menu_instruction_t*)&g_world_menu_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    instruction = menu_next_instruction(instruction);
    return instruction;
}

/*
 * Apply the centered opening-animation clip before processing a window command.
 *
 * Steps 0-3 scale width and height by the signed percentage table. Later
 * steps disable the temporary clip; the downstream command still runs.
 */
menu_instruction_t* world_menu_handle_window_command_with_scaled_clip(menu_instruction_t* instruction) {
    RECT clip;
    s32 scale;

    if (g_gfx_transition_frame < 4) {
        if (g_gfx_transition_frame < 3) {
            g_menu_cmd_stream_input = 0;
        }
        scale = g_world_menu_window_scale_table[g_gfx_transition_frame];
        clip.w = instruction->window.width * scale / 100;
        clip.h = instruction->window.height * scale / 100;
        clip.x = instruction->window.x + (instruction->window.width >> 1) - (clip.w >> 1);
        clip.y = instruction->window.y + (instruction->window.height >> 1) - (clip.h >> 1);
        gfx_add_draw_area_primitive(&clip, g_menu_gfx_otag_index - 1);
        g_gfx_otag_index_locked = 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    return world_menu_parse_draw_window_frame(instruction);
}

/*
 * Tile a menu rectangle with 16-pixel sprites and clipped edge pieces.
 *
 * Source 2 uses the signed shared rectangle; other sources use byte operands.
 */
menu_instruction_t* world_menu_parse_draw_tiled_rectangle(menu_instruction_t* instruction) {
    RECT rect;
    s32 base_x;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority;
    u16 clut;
    s32 full_columns;
    s32 row;
    s32 column;
    s32 base_y;

    priority = g_menu_gfx_otag_index;
    clut = g_menu_palette_clut;
    if (instruction->window.rectangle_source == 2) {
        base_x = g_world_menu_window_rect.x;
        base_y = g_world_menu_window_rect.y;
        full_columns = g_world_menu_window_rect.w / 16;
        partial_width = g_world_menu_window_rect.w % 16;
        full_rows = g_world_menu_window_rect.h / 16;
        partial_height = g_world_menu_window_rect.h % 16;
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
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
        }
    }
    if (partial_height == 0) {
        return menu_next_instruction(instruction);
    }
    rect.x = base_x;
    rect.y = base_y + row * 16;
    rect.w = partial_width;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = base_x + partial_width + column * 16;
        rect.y = base_y + row * 16;
        rect.w = 16;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage, clut, priority);
    }
    return menu_next_instruction(instruction);
}

/* Menu script opcode: compute a CLUT id from the packed operands and record it.
 * Returns the next script instruction pointer.
 *
 * Built at -O1: the unscheduled prologue and load order are the -O1 signature
 * of the menu-script handlers (see world_menu_script_* in src/world). The
 * reassigned s32 y defeats plus->ior folding (gives addu). */
menu_instruction_t* world_menu_script_set_fixed_clut(menu_instruction_t* instruction) {
    s32 x;
    s32 y;

    x = instruction->fixed_clut.x_div_16;
    x <<= 4;
    y = instruction->fixed_clut.y_low;
    y += instruction->fixed_clut.y_high << 8;
    g_world_menu_clut = GetClut(x, y);
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: pick the CLUT x/y operands from bytes 2..4 or,
 * when g_world_menu_use_alternate_palette selects the alternate palette, bytes 5..7, and record the
 * resulting CLUT id. Advances by the instruction length (byte 1), like
 * world_menu_script_set_texture_page. */
menu_instruction_t* world_menu_script_set_clut(menu_instruction_t* instruction) {
    if (g_world_menu_use_alternate_palette != 0) {
        g_world_menu_clut = GetClut(instruction->clut.alternate_x_div_16 << 4,
            instruction->clut.alternate_y_low + (instruction->clut.alternate_y_high << 8));
    } else {
        g_world_menu_clut = GetClut(instruction->clut.normal_x_div_16 << 4,
            instruction->clut.normal_y_low + (instruction->clut.normal_y_high << 8));
    }
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: set the texture page from the operand bytes and
 * advance by the instruction length (byte 1), like world_menu_script_set_semi_trans. */
menu_instruction_t* world_menu_script_set_texture_page(menu_instruction_t* instruction) {
    g_world_menu_texture_page = GetTPage(instruction->texture_page.depth, instruction->texture_page.y_blend >> 4,
        instruction->texture_page.x_div_16 << 4, instruction->texture_page.y_blend << 8);
    return menu_next_instruction(instruction);
}
