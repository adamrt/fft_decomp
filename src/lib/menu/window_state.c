/* Menu clipping commands and window transition scale. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* JOBSTTS and WORLD have distinct clipping-reset commands. */
#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_EQUIP)
/* Stream command closing a zoomed window: when the zoom transition queued a
 * cropped draw area, queue the full draw area again at the next
 * ordering-table entry and clear the lock. Unlike JOBSTTS's own copy, the
 * one BUNIT and EQUIP link does not advance the ordering-table index. */
menu_instruction_t* menu_opcode_13_clear_temporary_draw_area(menu_instruction_t* data) {
    if (g_gfx_otag_index_locked != 0) {
        g_gfx_draw_area.y = 0;
        menu_gfx_enqueue_draw_area(&g_gfx_draw_area, g_menu_gfx_otag_index + 1);
        g_gfx_otag_index_locked = 0;
    }
    return menu_next_instruction(data);
}
#endif

/* WORLD keeps its draw-area commands in its own unit. */
#ifndef FFT_WORLD
/* Queue the instruction draw area unless the zoom transition already queued one. */
menu_instruction_t* menu_opcode_14_set_draw_area_if_unlocked(menu_instruction_t* data) {
    RECT area;

    if (g_gfx_otag_index_locked == 0) {
        area.x = data->draw_area.x;
        area.y = data->draw_area.y;
        area.w = data->draw_area.width;
        area.h = data->draw_area.height;
        menu_gfx_enqueue_draw_area(&area, g_menu_gfx_otag_index - 1);
    }
    return menu_next_instruction(data);
}

/* Queue the full draw area at the next ordering-table slot when unlocked. */
menu_instruction_t* menu_opcode_15_repeat_draw_area(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked == 0) {
        s32 otag_index = g_menu_gfx_otag_index;
        g_gfx_draw_area.y = 0;
        menu_gfx_enqueue_draw_area(&g_gfx_draw_area, otag_index + 1);
    }
    return menu_next_instruction(instruction);
}
#endif

/* Set the current frame of the window open/close scaling transition. */
void menu_set_window_scale_step(s32 value) {
    g_gfx_transition_frame = value;
}

/* Return the current frame of the window open/close scaling transition. */
s32 menu_get_window_scale_step(void) {
    return g_gfx_transition_frame;
}

/* JOBSTTS advances the transition in its script runner. */
#ifndef FFT_EVENT_JOBSTTS
/* Command 0x16: advance the window zoom transition frame (up to 4, the open window), then skip
 * the one-byte command. */
menu_instruction_t* menu_opcode_16_advance_window_scale_step(menu_instruction_t* data) {
    if (g_gfx_transition_frame < 4) {
        g_gfx_transition_frame++;
    }
    return (menu_instruction_t*)((u8*)data + 1);
}
#endif
