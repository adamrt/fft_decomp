/* The two fading translucent letterbox panels of the event dialog overlay. */

#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Builds and submits the two translucent letterbox panels used by the event
 * dialogue overlay.
 *
 * Banner tasks in slots 13 and 14 request a panel when display->x is zero;
 * display->y selects which of the two opens. Each panel fades its
 * gradient in by 0xc per frame up to 0x78 and back out again, alternating
 * between two primitive buffers so the previous frame stays queued.
 *
 */
void menu_draw_thread_status_indicators_variant(void) {
    menu_gradient_panel_prims_t* prims;
    menu_status_thread_t* display;
    menu_status_thread_t* descriptor;
    s32 active = 0;
    s32 i;
    s32 j;
    s32 k;
    s32 flip;
    s16 y;

    if (menu_thread_is_running_variant(EQUIP_THREAD_SLOT_UNIT_STATUS) != 0) {
        if (g_threads[EQUIP_THREAD_SLOT_UNIT_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            display = (menu_status_thread_t*)g_threads[EQUIP_THREAD_SLOT_UNIT_STATUS].function_parameter_1;
            active = (-(display->x == 0)) & 13;
        }
    }
    if (menu_thread_is_running_variant(EQUIP_THREAD_SLOT_COMPARISON_STATUS) != 0) {
        if (g_threads[EQUIP_THREAD_SLOT_COMPARISON_STATUS].task_id == THREAD_TASK_UNIT_STATUS_BANNER) {
            descriptor = (menu_status_thread_t*)g_threads[EQUIP_THREAD_SLOT_COMPARISON_STATUS].function_parameter_1;
            if (descriptor->x == 0) {
                active = 14;
                display = descriptor;
            }
        }
    }
    /* Both requests jump past the fade-out updates; the target shares one
     * state[1] store between them and the fall-through path. */
    if (active != 0) {
        if ((display->y == 0x90) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_equip_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
            g_equip_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_IN;
            goto draw_panels;
        } else if ((display->y == 0) && !(display->flags & MENU_STATUS_PANEL_HIDDEN)) {
            g_equip_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_IN;
            g_equip_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
            goto draw_panels;
        }
    }
    if (g_equip_menu_indicator_state[0] == MENU_INDICATOR_STATE_FADING_IN) {
        g_equip_menu_indicator_state[0] = MENU_INDICATOR_STATE_FADING_OUT;
    }
    if (g_equip_menu_indicator_state[1] == MENU_INDICATOR_STATE_FADING_IN) {
        g_equip_menu_indicator_state[1] = MENU_INDICATOR_STATE_FADING_OUT;
    }
draw_panels:
    for (i = 0; i < 2; i++) {
        flip = g_equip_menu_indicator_buffer_index[i] ^ 1;
        g_equip_menu_indicator_buffer_index[i] = flip;
        prims = &g_equip_thread_indicator_packets[i][flip];
        SetDrawMode(&prims->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_menu_draw_area_template);
        SetTile(&prims->center);
        setRGB0(&prims->center, 0, 0, 0);
        SetSemiTrans(&prims->center, 1);
        y = 0xb2;
        if (i == 0) {
            y = 0x23;
        }
        setXY0(&prims->center, 0x80, y);
        setWH(&prims->center, 0x100, 0x32);
        for (j = 0; j < 9; j++) {
            SetTile(&prims->top[j]);
            setRGB0(&prims->top[j], 0, 0, 0);
            SetSemiTrans(&prims->top[j], 1);
            y = 0xa9;
            setWH(&prims->top[j], 0x100, 1);
            if (i == 0) {
                y = 0x1a;
            }
            setXY0(&prims->top[j], 0x80, y + j);
            SetTile(&prims->bottom[j]);
            setRGB0(&prims->bottom[j], 0, 0, 0);
            SetSemiTrans(&prims->bottom[j], 1);
            y = 0xe4;
            setWH(&prims->bottom[j], 0x100, 1);
            if (i == 0) {
                y = 0x55;
            }
            setXY0(&prims->bottom[j], 0x80, y + j);
        }
        if (g_equip_menu_indicator_state[i] == MENU_INDICATOR_STATE_HIDDEN) {
            setRGB0(&prims->center, 0, 0, 0);
        } else {
            if (g_equip_menu_indicator_state[i] == MENU_INDICATOR_STATE_FADING_IN) {
                if (g_equip_menu_indicator_brightness[i] < 0x78) {
                    g_equip_menu_indicator_brightness[i] += 0xc;
                }
            }
            if (g_equip_menu_indicator_state[i] == MENU_INDICATOR_STATE_FADING_OUT) {
                if (g_equip_menu_indicator_brightness[i] != 0) {
                    g_equip_menu_indicator_brightness[i] -= 0xc;
                } else {
                    g_equip_menu_indicator_state[i] = MENU_INDICATOR_STATE_HIDDEN;
                }
            }
            setRGB0(&prims->center, g_equip_menu_indicator_brightness[i], g_equip_menu_indicator_brightness[i],
                g_equip_menu_indicator_brightness[i]);
            g_equip_menu_indicator_edge_brightness[i] = g_equip_menu_indicator_brightness[i];
            for (j = 0; j < 9; j++) {
                if (g_equip_menu_indicator_edge_brightness[i] != 0) {
                    g_equip_menu_indicator_edge_brightness[i] -= 0xc;
                }
                k = 8 - j;
                (k + prims->top)->r0 = g_equip_menu_indicator_edge_brightness[i];
                (k + prims->top)->g0 = g_equip_menu_indicator_edge_brightness[i];
                (k + prims->top)->b0 = g_equip_menu_indicator_edge_brightness[i];
                setRGB0(&prims->bottom[j], g_equip_menu_indicator_edge_brightness[i],
                    g_equip_menu_indicator_edge_brightness[i], g_equip_menu_indicator_edge_brightness[i]);
                gfx_draw_or_append_gpu_primitive(k + prims->top);
                gfx_draw_or_append_gpu_primitive(&prims->bottom[j]);
            }
            gfx_draw_or_append_gpu_primitive(&prims->center);
            gfx_draw_or_append_gpu_primitive(&prims->draw_mode);
        }
    }
}
