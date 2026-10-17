/* Command stream dispatch and the palette-mode script run. */

#include "fft/menu_script.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_window.h"
#include "fft/status_screen.h"
#include "psx/types.h"

/* Dispatch slots before this program's end opcode; zero entries stay unhandled. */
menu_opcode_handler_t g_menu_opcode_handlers[STATUS_SCREEN_SCRIPT_END] = {
    menu_opcode_00_draw_window_frame,             /* 0x00 */
    menu_opcode_01_if_group,                      /* 0x01 */
    menu_opcode_02_if_else_group,                 /* 0x02 */
    0,                                            /* 0x03 */
    menu_opcode_04_draw_textured_quad,            /* 0x04 */
    menu_opcode_04_draw_textured_quad,            /* 0x05 */
    0,                                            /* 0x06 */
    0,                                            /* 0x07 */
    0,                                            /* 0x08 */
    menu_opcode_09_set_fixed_clut,                /* 0x09 */
    menu_opcode_0a_set_clut,                      /* 0x0a */
    0,                                            /* 0x0b */
    menu_opcode_0c_set_texture_page,              /* 0x0c */
    menu_opcode_0d_draw_formatted_number,         /* 0x0d */
    menu_opcode_0e_set_semi_trans,                /* 0x0e */
    menu_opcode_0f_set_draw_priority_if_unlocked, /* 0x0f */
    menu_opcode_10_draw_scrollable_list,          /* 0x10 */
    menu_opcode_11_draw_background_tiles,         /* 0x11 */
    menu_opcode_12_draw_opening_window_frame,     /* 0x12 */
    menu_opcode_13_clear_temporary_draw_area,     /* 0x13 */
    menu_opcode_14_set_draw_area_if_unlocked,     /* 0x14 */
    menu_opcode_15_repeat_draw_area,              /* 0x15 */
};

/* Dispatch every command of a command stream through the handler table until
 * the end opcode, after resetting the ordering-table index, its lock and the
 * semi-transparency state; `flags` is the input the handlers see. Then
 * advances the transition frame, up to 10. */
void menu_script_run(void* script, s32 flags) {
    menu_instruction_t* instruction = script;

    g_gfx_otag_index_locked = 0;
    g_menu_gfx_otag_index = 0;
    g_menu_gfx_semitransparency = 0;
    g_menu_script_input = flags;
    while (instruction->header.opcode != STATUS_SCREEN_SCRIPT_END) {
        instruction = g_menu_opcode_handlers[instruction->header.opcode](instruction);
    }
    if (g_gfx_transition_frame < 10) {
        g_gfx_transition_frame = g_gfx_transition_frame + 1;
    }
}

/* Select the palette set for `event_mode` and run the command stream. */
void menu_run_script_with_palette_mode(void* script, s32 flags, s32 event_mode) {
    menu_set_palette_mode(event_mode);
    menu_script_run(script, flags);
}
